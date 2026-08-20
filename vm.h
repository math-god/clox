#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"

#define STACK_INIT_CAPACITY 256
#define STACK_SHRINK_THRESHOLD(growCount) (2 << 3 + growCount)
#define STACK_LAST_VALUE (*(vm.stackTop - 1))

typedef struct {
    Chunk* chunk;
    uint8_t* ip;
    Value* stackBottom;
    Value* stackTop;
    uint32_t stackCount;
    uint32_t stackCapacity;
    uint16_t stackGrowCount;
    Obj* objects;
} VM;

typedef enum { INTERPRET_OK, INTERPRET_COMPILE_ERROR, INTERPRET_RUNTIME_ERROR } InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();

#endif