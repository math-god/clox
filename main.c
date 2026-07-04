#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "line.h"

int main(int argc, char const *argv[])
{	
	Chunk chunk;
	initChunk(&chunk);

	for (int i = 0; i < 65600; ++i)
	{
		writeConstant(&chunk, i + 10 + i, 1);
	}
	writeConstant(&chunk, 1.2, 1);
	writeChunk(&chunk, OP_RETURN, 1);

	writeConstant(&chunk, 1.9, 2);
	writeChunk(&chunk, OP_RETURN, 2);

	printIntArray(chunk.lines.lines, chunk.lines.count);

	disassembleChunk(&chunk, "test chunk");
	freeChunk(&chunk);

	return 0;
}