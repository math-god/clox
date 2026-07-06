#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    initLineArray(&chunk->lines);
    initValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    freeLineArray(&chunk->lines);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;
    writeLineArray(&chunk->lines, line, 1);
}

/*void print_binary(unsigned int n) {
    // Starts from the Most Significant Bit (MSB)
    for (int i = (sizeof(n) * 8) - 1; i >= 0; i--) {
        printf("%d", (n >> i) & 1);
    }
    printf("\n");
}*/

void writeConstant(Chunk* chunk, Value value, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }

    // opcode write
    uint8_t byteOverflow = 0;
    if (chunk->constants.count >= 65536) {
        chunk->code[chunk->count] = OP_CONSTANT_LONGEST;
        chunk->count++;
        byteOverflow = OP_CONSTANT_LONGEST;
    } else if (chunk->constants.count >= 256) {
        chunk->code[chunk->count] = OP_CONSTANT_LONG;
        chunk->count++;
        byteOverflow = OP_CONSTANT_LONG;
    } else {
        chunk->code[chunk->count] = OP_CONSTANT;
        chunk->count++;
    }
    writeLineArray(&chunk->lines, line, 1);

    // const val write
    if (byteOverflow == 0) {
        writeValueArray(&chunk->constants, value);
        chunk->code[chunk->count] = chunk->constants.count - 1;
        chunk->count++;
        writeLineArray(&chunk->lines, line, 1);
    } else {
        writeValueArray(&chunk->constants, value);
        uint32_t number = chunk->constants.count - 1;
        uint32_t* ptr = (uint32_t*)&chunk->code[chunk->count];
        *ptr = number;

        if (byteOverflow == OP_CONSTANT_LONG) {
            chunk->count += 2;
            writeLineArray(&chunk->lines, line, 2);
        } else {
            chunk->count += 3;
            writeLineArray(&chunk->lines, line, 3);
        }
    }
}
