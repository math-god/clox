#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->constantMode = 1;
    initLineArray(&chunk->lines);
    initValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    freeLineArray(&chunk->lines);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

/*static void print_binary(unsigned int n) {
    // Starts from the Most Significant Bit (MSB)
    for (int i = (sizeof(n) * 8) - 1; i >= 0; i--) {
        printf("%d", (n >> i) & 1);
    }
    printf("\n");
}*/

#define WRITE_OP(op, line)                      \
    do {                                        \
        chunk->code[chunk->count] = op;         \
        chunk->count++;                         \
        writeLineArray(&chunk->lines, line, 1); \
    } while (false);

// write 1 byte
void writeChunk(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = RESIZE_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }

    WRITE_OP(byte, line);
}

void writeInstruction(Chunk* chunk, uint8_t opCode, uint8_t bytes[], int length, int line) {
    if (chunk->capacity < chunk->count + length + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = RESIZE_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }

    WRITE_OP(opCode, line);
    if (length < 1) return;

    // arg write
    for (int i = 0; i < length; i++) {
        chunk->code[chunk->count] = bytes[i];
        chunk->count++;
    }

    writeLineArray(&chunk->lines, line, length);
}

// write up to 4 bytes (opcode + const)
// returns constant index
int writeConstant(Chunk* chunk, Value value, int line) {
    uint8_t desiredMemory = 1;
    if (chunk->constants.count >= 65536) {
        desiredMemory += 3;
        if (chunk->constantMode == 2) WRITE_OP(OP_SWITCH_TO_24, line);
    } else if (chunk->constants.count >= 256) {
        desiredMemory += 2;
        if (chunk->constantMode == 1) WRITE_OP(OP_SWITCH_TO_16, line);
    } else {
        desiredMemory += 1;
    }

    if (chunk->capacity < chunk->count + desiredMemory) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = RESIZE_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }

    WRITE_OP(OP_CONSTANT, line);

    // const val write
    if (desiredMemory == 2) {
        writeValueArray(&chunk->constants, value);
        chunk->code[chunk->count] = chunk->constants.count - 1;
        chunk->count++;
        writeLineArray(&chunk->lines, line, 1);
    } else {
        writeValueArray(&chunk->constants, value);
        uint32_t number = chunk->constants.count - 1;
        uint32_t* ptr = (uint32_t*)&chunk->code[chunk->count];
        *ptr = number;

        if (desiredMemory == 3) {
            chunk->count += 2;
            writeLineArray(&chunk->lines, line, 2);
        } else {
            chunk->count += 3;
            writeLineArray(&chunk->lines, line, 3);
        }
    }

    return chunk->constants.count - 1;
}
