#ifndef clox_line_h
#define clox_line_h

#include "common.h"

typedef struct {
	int capacity;
	int count;
	int* lines;
} LineArray;

void initLineArray(LineArray* array);
void writeLineArray(LineArray* array, int line, int bytesCount);
void freeLineArray(LineArray* array);
int getLine(LineArray* array, int instrOffset);

#endif