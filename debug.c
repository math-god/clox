#include <stdio.h>

#include "debug.h"
#include "value.h"
#include "line.h"

void disassembleChunk(Chunk* chunk, const char* name) {
	printf("== %s == \n", name);
	for (int offset = 0; offset < chunk->count;) {
		offset = disassembleInstruction(chunk, offset);
	}
}

static int simpleInstruction(const char* name, int offset) {
	printf("%s\n", name);
	return offset + 1;
}
 
static int constantInstruction(const char* name, Chunk* chunk, int offset, int constSize) {
	uint32_t constant = 0;
	int offsetIncr = 0;
	if (constSize == 1) {
		constant = chunk->code[offset + 1];
		offsetIncr = 2;
	} else if (constSize == 2) {
		constant = chunk->code[offset + 1] | (chunk->code[offset + 2] << 8);
		offsetIncr = 3;
	} else if (constSize == 3) {
		constant = chunk->code[offset + 1] | (chunk->code[offset + 2] << 8) | (chunk->code[offset + 3] << 16);
		offsetIncr = 4;
	}

	printf("%-16s %4d '", name, constant);
	printValue(chunk->constants.values[constant]);
	printf("'\n");
	return offset + offsetIncr;
}

int disassembleInstruction(Chunk* chunk, int offset) {
	printf("%04d ", offset);
	if (offset > 0 && getLine(&chunk->lines, offset) == getLine(&chunk->lines, offset - 1))
	{
		printf("   | ");
	} else {
		printf("%4d ", getLine(&chunk->lines, offset));
	}
	uint8_t instruction = chunk->code[offset];
	switch (instruction) {
	case OP_RETURN:
		return simpleInstruction("OP_RETURN", offset);
	case OP_CONSTANT:
		return constantInstruction("OP_CONSTANT", chunk, offset, 1);
	case OP_CONSTANT_LONG:
		return constantInstruction("OP_CONSTANT_LONG", chunk, offset, 2);
	case OP_CONSTANT_LONGEST:
		return constantInstruction("OP_CONSTANT_LONGEST", chunk, offset, 3);
	default:
		printf("Unknown opcode %d\n", instruction);
		return offset + 1;
	}
}

void printIntArray(int* ref, int num) {
	printf("Data debug:\n");

	for (int i = 0; i < num; ++i)
	{
		printf("%d ", ref[i]);
	}

	printf("\n");
}