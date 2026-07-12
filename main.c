#include <stdio.h>

#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "line.h"
#include "vm.h"

int main(int argc, char const *argv[]) {
    initVM();

    Chunk chunk;
    initChunk(&chunk);

    double arr[2000];
    for (int i = 0; i < 1500; ++i) {
        arr[i] = i;
        writeConstant(&chunk, i, 1);
    }

    int sum = 0;
    for (int i = 0; i < 1500; ++i) {
        writeChunk(&chunk, OP_ADD, 2);
        sum += arr[i];
    }

    writeChunk(&chunk, OP_RETURN, 10);

    interpret(&chunk);
    printf("\n%d\n", sum);
    freeChunk(&chunk);
    freeVM();

    return 0;
}