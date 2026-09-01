#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) (type*)allocateObject(sizeof(type), objectType)
#define ALLOCATE_STR(length) (ObjString*)allocateObject(sizeof(ObjString) + length, OBJ_STRING)

static Obj* allocateObject(size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;

    object->next = vm.objects;
    vm.objects = object;
    return object;
}

static Value wrap(ObjString* string, uint32_t hash, bool intern) {
    Value key = OBJ_VAL(string);
    key.hash = hash;
    if (intern) tableSet(&vm.strings, &key, NIL_VAL);
    return key;
}

static ObjString* allocate(const char* chars, int length) {
    int terminatedLength = length + 1;
    ObjString* string = ALLOCATE_STR(terminatedLength);
    string->length = length;
    strncpy(string->chars, chars, terminatedLength);
    string->chars[length] = '\0';
    return string;
}

Value allocateString(const char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        return wrap(interned, hash, false);
    }

    ObjString* allocated = allocate(chars, length);
    return wrap(allocated, hash, true);
}

// frees chars if string was interned
Value takeString(char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return wrap(interned, hash, false);
    }

    ObjString* allocated = allocate(chars, length);
    return wrap(allocated, hash, true);
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    }
}

/*ObjString* copyString(const char* chars, int length) {
    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length);
}*/