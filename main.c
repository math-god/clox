#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "line.h"

int main(int argc, char const *argv[])
{	
	Chunk chunk;
	initChunk(&chunk);

	int constant1 = addConstant(&chunk, 1.2);
	writeChunk(&chunk, OP_CONSTANT, 1);
	writeChunk(&chunk, constant1, 1);

	writeChunk(&chunk, OP_RETURN, 1);


	int constant2 = addConstant(&chunk, 1.9);
	writeChunk(&chunk, OP_CONSTANT, 2);
	writeChunk(&chunk, constant2, 2);

	writeChunk(&chunk, OP_RETURN, 2);

	printIntArray(chunk.lines.lines, chunk.lines.count);

	disassembleChunk(&chunk, "test chunk");
	freeChunk(&chunk);

	return 0;
}