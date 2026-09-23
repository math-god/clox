#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"
#include "line.h"

typedef enum {
	OP_CONSTANT,
	OP_NEGATE,
	OP_NIL,
	OP_TRUE,
	OP_FALSE,
	OP_ADD,
	OP_NOT,
	OP_EQUAL,
	OP_GREATER,
	OP_LESS,
	OP_SUBSTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_RETURN,
	OP_PRINT,
	OP_POP,
	OP_SET_GLOBAL,
	OP_GET_GLOBAL,
	OP_GET_LOCAL,
	OP_SET_LOCAL,
	OP_DEFINE_GLOBAL,
	OP_SWITCH_TO_16,
	OP_SWITCH_TO_24,

} OpCode;

typedef struct {
	int count;
	int capacity;
	uint8_t* code;
	LineArray lines;
	ValueArray constants;
	int constantMode; // bytes
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
void writeInstruction(Chunk* chunk, uint8_t instruction, uint8_t bytes[], int length, int line);
int writeConstant(Chunk* chunk, Value value, int line);

#endif