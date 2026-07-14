#include <stdio.h>
#include <time.h>

#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "line.h"
#include "vm.h"

// 0.0001 -      push(-pop());
// 0.00004 -  *(vm.stackTop - 1) = -(*(vm.stackTop - 1));
// 2.5 diff

// 0.000128 - old add
// 0.000093 - new add
// 1.4 diff

// 0.000129 - old sub
// 0.000091 - new sub
// 1.4 diff

// 0.000130 - old mul
// 0.000094 - new mul
// 1.4 diff

// 0.000157 - old div
// 0.000115 - new div
// 1.4 diff

int main(int argc, char const *argv[]) {
    int size = 10000;

    float arr[size];

    for (int j = 0; j < size; ++j) {
        initVM();

        Chunk chunk;
        initChunk(&chunk);

        for (int i = 0; i < 10000; ++i) {
            writeConstant(&chunk, 100, 1);
        }

        for (int i = 0; i < 9999; ++i) {
            writeChunk(&chunk, OP_DIVIDE, 2);
        }
        writeChunk(&chunk, OP_RETURN, 10);

        clock_t start = clock();
        interpret(&chunk);
        clock_t end = clock();

        freeChunk(&chunk);
        freeVM();
  
        arr[j] = (float)(end - start) / CLOCKS_PER_SEC;
    }

    float sum = 0;
    for (int j = 0; j < size; ++j) {
        sum += arr[j];
    }
    printf("AVG TIME: %f\n", sum / size);

    return 0;
}