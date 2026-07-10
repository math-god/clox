#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "line.h"
#include "vm.h"

int main(int argc, char const *argv[])
{	
	initVM();

	Chunk chunk;
	initChunk(&chunk);

// 1 * 2 + 3
	writeConstant(&chunk, 1, 1);
	writeConstant(&chunk, 2, 1);
	writeChunk(&chunk, OP_MULTIPLY, 1);

	writeConstant(&chunk, 3, 1);
	writeChunk(&chunk, OP_ADD, 1);

// 1 + 2 * 3
	writeConstant(&chunk, 2, 2);
	writeConstant(&chunk, 3, 2);
	writeChunk(&chunk, OP_MULTIPLY, 2);

	writeConstant(&chunk, 1, 2);
	writeChunk(&chunk, OP_ADD, 2);

// 3 - 2 - 1
	writeConstant(&chunk, 3, 3);
	writeConstant(&chunk, 2, 3);
	writeChunk(&chunk, OP_SUBSTRACT, 3);

	writeConstant(&chunk, 1, 3);
	writeChunk(&chunk, OP_SUBSTRACT, 3);

// 1 + 2 * 3 - 4 / -5

	writeConstant(&chunk, 2, 4);
	writeConstant(&chunk, 3, 4);
	writeChunk(&chunk, OP_MULTIPLY, 4);

	writeConstant(&chunk, 4, 4);
	writeConstant(&chunk, 5, 4);
	writeChunk(&chunk, OP_NEGATE, 4);

	writeChunk(&chunk, OP_DIVIDE, 4);

	writeChunk(&chunk, OP_SUBSTRACT, 4);

	writeConstant(&chunk, 1, 4);
	writeChunk(&chunk, OP_ADD, 4);


	writeChunk(&chunk, OP_RETURN, 10);

	interpret(&chunk);

	freeChunk(&chunk);
	freeVM();

	return 0;
}