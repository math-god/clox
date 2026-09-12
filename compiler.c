#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "object.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

#ifdef DEBUG_PARSER
// ordering: parsePrecedence,number,grouping,unary,binary,literal,string,variable
int parserFuncCalls[8] = {0};
#endif

typedef enum {
    GET,
    SET,
    DEFINE,
} VariableOp;

static OpCode varGetOp[] = {OP_GET_GLOBAL, OP_GET_GLOBAL_LONG, OP_GET_GLOBAL_LONGEST};
static OpCode varSetOp[] = {OP_SET_GLOBAL, OP_SET_GLOBAL_LONG, OP_SET_GLOBAL_LONGEST};
static OpCode varDefineOp[] = {OP_DEFINE_GLOBAL, OP_DEFINE_GLOBAL_LONG, OP_DEFINE_GLOBAL_LONGEST};

static OpCode getVarOp(int length, VariableOp op) {
    switch (op) {
        case GET:
            return varGetOp[--length];
        case SET:
            return varSetOp[--length];
        case DEFINE:
            return varDefineOp[--length];
        default: return -1; // Unreachable
    }
}

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_OR,
    PREC_AND,
    PREC_EQUALITY,
    PREC_COMPARISON,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY,
    PREC_CALL,
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

Parser parser;
Chunk* compilingChunk;

static Chunk* currentChunk() { return compilingChunk; }

static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char* message) { errorAt(&parser.previous, message); }

static void errorAtCurrent(const char* message) { errorAt(&parser.current, message); }

static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(message);
}

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

static void emitByte(uint8_t byte) { writeChunk(currentChunk(), byte, parser.previous.line); }

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitReturn() { emitByte(OP_RETURN); }

static int emitConstant(Value value) {
    int constant = writeConstant(currentChunk(), value, parser.previous.line);

    // 24 bits
    if (constant > 0xFFFFFF) error("Too many constants in one chunk.");

    return constant;
}

static void endCompiler() {
    emitReturn();

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
static int identifierConstant(Token* name);

static void binary(bool canAssign) {
#ifdef DEBUG_PARSER
    int callNum = ++parserFuncCalls[4];
    printf("binary() call #%d\n", callNum);
#endif

    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_PLUS:
            emitByte(OP_ADD);
            break;
        case TOKEN_MINUS:
            emitByte(OP_SUBSTRACT);
            break;
        case TOKEN_STAR:
            emitByte(OP_MULTIPLY);
            break;
        case TOKEN_SLASH:
            emitByte(OP_DIVIDE);
            break;
        case TOKEN_BANG_EQUAL:
            emitBytes(OP_EQUAL, OP_NOT);
            break;
        case TOKEN_EQUAL_EQUAL:
            emitByte(OP_EQUAL);
            break;
        case TOKEN_GREATER:
            emitByte(OP_GREATER);
            break;
        case TOKEN_GREATER_EQUAL:
            emitBytes(OP_LESS, OP_NOT);
            break;
        case TOKEN_LESS:
            emitByte(OP_LESS);
            break;
        case TOKEN_LESS_EQUAL:
            emitBytes(OP_GREATER, OP_NOT);
            break;
        default:
            return;  // Unreachable
    }

#ifdef DEBUG_PARSER
    printf("binary() return #%d\n", callNum);
#endif
}

static void literal(bool canAssign) {
#ifdef DEBUG_PARSER
    int callNum = ++parserFuncCalls[5];
    printf("literal() call #%d\n", callNum);
#endif

    switch (parser.previous.type) {
        case TOKEN_FALSE:
            emitByte(OP_FALSE);
            break;
        case TOKEN_NIL:
            emitByte(OP_NIL);
            break;
        case TOKEN_TRUE:
            emitByte(OP_TRUE);
            break;
        default:
            return;  // Unreachable
    }

#ifdef DEBUG_PARSER
    printf("literal() return #%d\n", callNum);
#endif
}

static void grouping(bool canAssign) {
#ifdef DEBUG_PARSER
    int callNum = ++parserFuncCalls[2];
    printf("grouping() call #%d\n", callNum);
#endif

    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");

#ifdef DEBUG_PARSER
    printf("grouping() return #%d\n", callNum);
#endif
}

static void number(bool canAssign) {
#ifdef DEBUG_PARSER
    printf("number() call #%d\n", ++parserFuncCalls[1]);
#endif

    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void string(bool canAssign) {
#ifdef DEBUG_PARSER
    printf("string() call #%d\n", ++parserFuncCalls[6]);
#endif

    emitConstant(allocateString(parser.previous.start + 1, parser.previous.length - 2));
}

static void namedVariable(Token name, bool canAssign) {
    int arg = identifierConstant(&name);
    bool isAssignment = canAssign && match(TOKEN_EQUAL);
    if (isAssignment) expression();

    uint8_t bytes[3];  // 3 bytes arr is enough to store constant index
    uint8_t length = arg >= 65536 ? 3 : arg >= 256 ? 2 : 1;
    for (int i = 0; i < length; i++) {
        bytes[i] = (uint8_t)arg << 8 * i;
    }

    writeInstruction(currentChunk(), getVarOp(length, isAssignment ? SET : GET), bytes, length, parser.previous.line);
}

static void variable(bool canAssign) {
#ifdef DEBUG_PARSER
    printf("variable() call #%d\n", ++parserFuncCalls[7]);
#endif

    namedVariable(parser.previous, canAssign);
}

static void unary(bool canAssign) {
#ifdef DEBUG_PARSER
    int callNum = ++parserFuncCalls[3];
    printf("unary() call #%d\n", callNum);
#endif

    TokenType operatorType = parser.previous.type;

    parsePrecedence(PREC_UNARY);

    switch (operatorType) {
        case TOKEN_MINUS:
            emitByte(OP_NEGATE);
            break;
        case TOKEN_BANG:
            emitByte(OP_NOT);
            break;
        default:
            return;  // Unreachable
    }

#ifdef DEBUG_PARSER
    printf("unary() return #%d\n", callNum);
#endif
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static void parsePrecedence(Precedence precedence) {
#ifdef DEBUG_PARSER
    int callNum = ++parserFuncCalls[0];
    printf("parsePrecedence(%d) call #%d\n", precedence, callNum);
#endif

    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);
    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }

#ifdef DEBUG_PARSER
    printf("parsePrecedence(%d) return #%d\n", precedence, callNum);
#endif
}

static int identifierConstant(Token* name) { return emitConstant(allocateString(name->start, name->length)); }

static int parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);
    return identifierConstant(&parser.previous);
}

static void defineVariable(int global) {
    uint8_t bytes[3];  // 3 bytes arr is enough to store constant index
    uint8_t length = global >= 65536 ? 3 : global >= 256 ? 2 : 1;
    for (int i = 0; i < length; i++) {
        bytes[i] = (uint8_t)global << 8 * i;
    }

    writeInstruction(currentChunk(), getVarOp(length, DEFINE), bytes, length, parser.previous.line);
}

static ParseRule* getRule(TokenType type) { return &rules[type]; }

static void expression() { parsePrecedence(PREC_ASSIGNMENT); }

static void varDeclaration() {
    uint8_t global = parseVariable("Expect variable name.");

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NIL);
    }

    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(global);
}

static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

static void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;

        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:;
        }
    }
}

static void declaration() {
    if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        statement();
    }

    if (parser.panicMode) synchronize();
}

static void statement() {
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else {
        expressionStatement();
    }
}

bool compile(const char* source, Chunk* chunk) {
    initScanner(source);
    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }

    endCompiler();
    return !parser.hadError;
}