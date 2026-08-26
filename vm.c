#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

VM vm;

static void resetStack() {
    vm.stackCount = 0;
    vm.stackCapacity = STACK_INIT_CAPACITY;
    vm.stackBottom = RESIZE_ARRAY(Value, vm.stackBottom, 0, STACK_INIT_CAPACITY);
    vm.stackTop = vm.stackBottom;
    vm.stackGrowCount = 0;
}

static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = getLine(&vm.chunk->lines, instruction);
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack();
}

void initVM() {
    resetStack();
    vm.objects = NULL;
    initTable(&vm.strings);
}

void freeVM() {
    freeObjects();
    freeTable(&vm.strings);
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

static Value peek(int distance) { return vm.stackTop[-1 - distance]; }

static bool isFalsey(Value value) { return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)); }

static void concatenate() {
    ObjString* popped = AS_STRING(pop());
    ObjString* lastVal = AS_STRING(STACK_LAST_VALUE);

    int length = lastVal->length + popped->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, lastVal->chars, lastVal->length);
    memcpy(chars + lastVal->length, popped->chars, popped->length);
    chars[length] = '\0';

    STACK_LAST_VALUE = OBJ_VAL(takeString(chars, length));
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT(size)                                                  \
    (vm.chunk->constants.values[size == 1   ? READ_BYTE()                    \
                                : size == 2 ? READ_BYTE() | READ_BYTE() << 8 \
                                            : READ_BYTE() | READ_BYTE() << 8 | READ_BYTE() << 16])

#define BINARY_OP(valueType, op)                                                        \
    do {                                                                                \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                               \
            runtimeError("Operands must be numbers.");                                  \
            return INTERPRET_RUNTIME_ERROR;                                             \
        }                                                                               \
        Value popped = pop();                                                           \
        STACK_LAST_VALUE = valueType(AS_NUMBER(STACK_LAST_VALUE) op AS_NUMBER(popped)); \
    } while (false);

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
        disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_RETURN: {
                printValue(pop());
                printf("\n");
                return INTERPRET_OK;
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
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT(1);
                push(constant);
                break;
            }
            case OP_CONSTANT_LONG: {
                Value constant = READ_CONSTANT(2);
                push(constant);
                break;
            }
            case OP_CONSTANT_LONGEST: {
                Value constant = READ_CONSTANT(3);
                push(constant);
                break;
            }
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
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(const char* source) {
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();

    freeChunk(&chunk);
    return result;
}