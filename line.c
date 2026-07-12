#include <stdio.h>

#include "line.h"
#include "memory.h"

void initLineArray(LineArray* array) {
    array->lines = NULL;
    array->capacity = 0;
    array->count = 0;
}

void writeLineArray(LineArray* array, int line, int bytesCount) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->lines = RESIZE_ARRAY(int, array->lines, oldCapacity, array->capacity);
    }

    // init
    if (array->count == 0) {
        array->lines[0] = line;
        array->lines[1] = 0;
        array->count = array->count + 2;
        return;
    } else {
        // incr
        for (int i = 0; i < array->count; i = i + 2) {
            if (line == array->lines[i]) {
                array->lines[i + 1] = array->lines[i + 1] + bytesCount;
                return;
            }
        }

        // adding
        array->lines[array->count] = line;
        array->lines[array->count + 1] = array->lines[array->count - 1] + bytesCount;
        array->count = array->count + 2;
    }
}

void freeLineArray(LineArray* array) {
    FREE_ARRAY(int, array->lines, array->capacity);
    initLineArray(array);
}

int getLine(LineArray* array, int instrOffset) {
    for (int i = 1; i < array->count; i = i + 2) {
        if (instrOffset <= array->lines[i]) {
            return array->lines[i - 1];
        }
    }

    return -1;
}