#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "memory.h"
#include "vm.h"

VM vm;

static void resetStack() {
    vm.stackCount = 0;
    vm.stackCapacity = STACK_INIT_CAPACITY;
    vm.stackBottom = RESIZE_ARRAY(Value, vm.stackBottom, 0, STACK_INIT_CAPACITY);
    vm.stackTop = vm.stackBottom;
    vm.stackGrowCount = 0;
}

void initVM() { resetStack(); }

void freeVM() {}

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

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT(size)                                                  \
    (vm.chunk->constants.values[size == 1   ? READ_BYTE()                    \
                                : size == 2 ? READ_BYTE() | READ_BYTE() << 8 \
                                            : READ_BYTE() | READ_BYTE() << 8 | READ_BYTE() << 16])
    
#define BINARY_OP(op)                                 \
    do {                                              \
        double b = pop();                             \
        *(vm.stackTop - 1) = *(vm.stackTop - 1) op b; \
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
                *(vm.stackTop - 1) = -(*(vm.stackTop - 1));
                break;
            case OP_ADD:
                BINARY_OP(+);
                break;
            case OP_SUBSTRACT:
                BINARY_OP(-);
                break;
            case OP_MULTIPLY:
                BINARY_OP(*);
                break;
            case OP_DIVIDE:
                BINARY_OP(/);
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
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(Chunk* chunk) {
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;
    return run();
}