#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"
#include "table.h"
#include "object.h"

#define STACK_INIT_CAPACITY 256
#define STACK_SHRINK_THRESHOLD(growCount) (2 << (3 + growCount))
#define STACK_LAST_VALUE (*(vm.stackTop - 1))
#define FRAMES_MAX 64

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    Value* stackBottom;
    Value* stackTop;
    uint32_t stackCount;
    uint32_t stackCapacity;
    uint16_t stackGrowCount;
    Obj* objects;
    Table globals;
    Table strings;
    ObjUpvalue* openUpvalues;
} VM;

typedef enum { INTERPRET_OK, INTERPRET_COMPILE_ERROR, INTERPRET_RUNTIME_ERROR } InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();

#endif