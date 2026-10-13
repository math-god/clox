#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

VM vm;

static Value clockNative(int argCount, Value* args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void resetStack() {
    vm.stackCount = 0;
    vm.stackCapacity = STACK_INIT_CAPACITY;
    vm.stackBottom = RESIZE_ARRAY(Value, vm.stackBottom, 0, STACK_INIT_CAPACITY);
    vm.stackTop = vm.stackBottom;
    vm.stackGrowCount = 0;
    vm.frameCount = 0;
}

static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        int line = getLine(&frame->function->chunk.lines, instruction);
        fprintf(stderr, "[line %d] in ", line);

        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s\n", function->name->chars);
        }
    }

    resetStack();
}

static void defineNative(const char* name, NativeFn function) {
    push(allocateString(name, (int)strlen(name)));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, &vm.stackBottom[0], vm.stackBottom[1]);
    pop();
    pop();
}

void initVM() {
    resetStack();
    vm.objects = NULL;
    initTable(&vm.strings);
    initTable(&vm.globals);

    defineNative("clock", clockNative);
}

void freeVM() {
    freeObjects();
    freeTable(&vm.strings);
    freeTable(&vm.globals);
}

void push(Value value) {
    if (vm.stackCapacity < vm.stackCount + 1) {
        int oldCapacity = vm.stackCapacity;
        vm.stackCapacity = GROW_CAPACITY(vm.stackCapacity);
        vm.stackBottom = RESIZE_ARRAY(Value, vm.stackBottom, oldCapacity, vm.stackCapacity);
        vm.stackTop = &vm.stackBottom[oldCapacity];
        vm.stackGrowCount++;
    }

    *vm.stackTop = value;
    vm.stackTop++;
    vm.stackCount++;
}

Value pop() {
    if (vm.stackGrowCount > 0 &&
        (vm.stackCount < (vm.stackCapacity >> 1) - STACK_SHRINK_THRESHOLD(vm.stackGrowCount))) {
        int oldCapacity = vm.stackCapacity;
        vm.stackCapacity = vm.stackCapacity >> 1;
        vm.stackBottom = RESIZE_ARRAY(Value, vm.stackBottom, oldCapacity, vm.stackCapacity);
        vm.stackTop = vm.stackBottom + vm.stackCount;
        vm.stackGrowCount--;
    }

    vm.stackTop--;
    vm.stackCount--;
    return *vm.stackTop;
}

/*Value popFrame(CallFrame frame) {
    if (vm.stackGrowCount > 0 &&
        (vm.stackCount < (vm.stackCapacity >> 1) - STACK_SHRINK_THRESHOLD(vm.stackGrowCount))) {
        int oldCapacity = vm.stackCapacity;
        vm.stackCapacity = vm.stackCapacity >> 1;
        vm.stackBottom = RESIZE_ARRAY(Value, vm.stackBottom, oldCapacity, vm.stackCapacity);
        vm.stackTop = vm.stackBottom + vm.stackCount;
        vm.stackGrowCount--;
    }

    vm.stackTop--;
    vm.stackCount--;
    return *vm.stackTop;
}*/

static Value peek(int distance) { return vm.stackTop[-1 - distance]; }

static bool call(ObjFunction* function, int argCount) {
    if (argCount != function->arity) {
        runtimeError("Expected %d arguments but got %d.", function->arity, argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->function = function;
    frame->ip = function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
        case OBJ_FUNCTION: return call(AS_FUNCTION(callee), argCount);
        case OBJ_NATIVE: {
            NativeFn native = AS_NATIVE(callee);
            Value result = native(argCount, vm.stackTop - argCount);
            vm.stackTop -= argCount + 1;
            push(result);
            return true;
        }
        default: break; // non-callable
        }
    }

    runtimeError("Can only call functions and classes");
    return false;
}

static bool isFalsey(Value value) { return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)); }

static void concatenate() {
    ObjString* popped = AS_STRING(pop());
    ObjString* lastVal = AS_STRING(STACK_LAST_VALUE);

    int length = lastVal->length + popped->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, lastVal->chars, lastVal->length);
    memcpy(chars + lastVal->length, popped->chars, popped->length);
    chars[length] = '\0';

    STACK_LAST_VALUE = takeString(chars, length);
}

static uint8_t readByte(CallFrame* frame) { return *frame->ip++; }
// constant array index
static uint32_t readConstantIndex(CallFrame* frame) {
    uint32_t index = 0;
    for (int offset = 0; offset <= 1 << (frame->function->chunk.constantMode + 1); offset += 8) {
        index |= readByte(frame) << offset;
    }

    return index;
}

static InterpretResult run() {
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define PUSH_CONSTANT() push(frame->function->chunk.constants.values[readConstantIndex(frame)]);
#define BINARY_OP(valueType, op)                                                          \
    do {                                                                                  \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                                 \
            runtimeError("Operands must be numbers.");                                    \
            return INTERPRET_RUNTIME_ERROR;                                               \
        }                                                                                 \
        Value popped = pop();                                                             \
        STACK_LAST_VALUE = valueType((AS_NUMBER(STACK_LAST_VALUE) op AS_NUMBER(popped))); \
    } while (false);
#define READ_STRING() AS_STRING(frame->function->chunk.constants.values[readConstantIndex(frame)])

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("        ");
        for (Value* slot = vm.stackBottom; slot < vm.stackTop; slot++) {
            printf("|[");
            printValue(*slot);
            printf("]");
        }
        printf("\n");
#endif
#ifdef DEBUG_INSTRUCTIONS
        disassembleInstruction(&frame->function->chunk, (int)(frame->ip - frame->function->chunk.code));
#endif

        uint8_t instruction;
        switch (instruction = readByte(frame)) {
            case OP_PRINT: {
                printValue(pop());
                printf("\n");
                break;
            }
            case OP_RETURN: {
                Value result = pop();
                vm.frameCount--;
                if (vm.frameCount == 0) {
                    pop();
                    return INTERPRET_OK;
                }

                uint16_t usedStack = vm.stackTop - frame->slots;
                for (int i = 0; i < usedStack; ++i) pop(); // inefficient way to clear stack

                push(result);
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                STACK_LAST_VALUE = NUMBER_VAL(-AS_NUMBER(STACK_LAST_VALUE));
                break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    Value popped = pop();
                    STACK_LAST_VALUE = NUMBER_VAL(AS_NUMBER(STACK_LAST_VALUE) + AS_NUMBER(popped));
                } else {
                    runtimeError("Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }
                BINARY_OP(NUMBER_VAL, +);
                break;
            case OP_SUBSTRACT:
                BINARY_OP(NUMBER_VAL, -);
                break;
            case OP_MULTIPLY:
                BINARY_OP(NUMBER_VAL, *);
                break;
            case OP_DIVIDE:
                BINARY_OP(NUMBER_VAL, /);
                break;
            case OP_CONSTANT:
                PUSH_CONSTANT();
                break;
            case OP_NIL:
                push(NIL_VAL);
                break;
            case OP_TRUE:
                push(BOOL_VAL(true));
                break;
            case OP_FALSE:
                push(BOOL_VAL(false));
                break;
            case OP_NOT:
                STACK_LAST_VALUE = BOOL_VAL(isFalsey(STACK_LAST_VALUE));
                break;
            case OP_EQUAL: {
                Value popped = pop();
                STACK_LAST_VALUE = BOOL_VAL(valuesEqual(STACK_LAST_VALUE, popped));
                break;
            }
            case OP_GREATER:
                BINARY_OP(BOOL_VAL, >);
                break;
            case OP_LESS:
                BINARY_OP(BOOL_VAL, <);
                break;
            case OP_POP:
                pop();
                break;
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value* key = &OBJ_VAL(name);
                key->hash = hashString(name->chars, name->length);
                if (tableSet(&vm.globals, key, peek(0))) {
                    tableDelete(&vm.globals, key);
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value* key = &OBJ_VAL(name);
                key->hash = hashString(name->chars, name->length);
                Value val;  // out param
                if (!tableGet(&vm.globals, key, &val)) {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                STACK_LAST_VALUE = val;
                break;
            }
            case OP_GET_LOCAL: {
                uint8_t slot = readByte(frame);
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = readByte(frame);
                frame->slots[slot] = peek(0);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                Value* key = &OBJ_VAL(name);
                key->hash = hashString(name->chars, name->length);
                tableSet(&vm.globals, key, pop());
                pop();
                break;
            }
            case OP_SWITCH_TO_16: {
                frame->function->chunk.constantMode = 2;
                break;
            }
            case OP_SWITCH_TO_24: {
                frame->function->chunk.constantMode = 3;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(0))) frame->ip += offset;
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int argCount = readByte(frame);
                if (!callValue(peek(argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
        }
    }

#undef PUSH_CONSTANT
#undef BINARY_OP
#undef READ_STRING
#undef READ_SHORT
}

InterpretResult interpret(const char* source) {
    ObjFunction* function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    call(function, 0);

    return run();
}