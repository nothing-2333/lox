#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

// 此法分析器
typedef struct 
{
    Token current;  // 好吧，这也是双指针
    Token previous;
    bool hadError;
    bool panicMode; // 避免级联效应 
} Parser;

// 优先级
typedef enum    // 优先级从低到高
{
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_TERM,        // + -
  PREC_FACTOR,      // * /
  PREC_UNARY,       // ! -
  PREC_CALL,        // . ()
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(); // 函数指针，见名知意

// 解析规则
typedef struct 
{
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

Parser parser; // 解析器也是全局的
Chunk* compilingChunk;   // 存储解析出的字节码

static Chunk* currentChunk()
{
    return compilingChunk;
}

// 报告错误的实现
static void errorAt(Token* token, const char* message)
{
    if (parser.panicMode) return;
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF)
    {
        fprintf(stderr, " at end");
    }
    else if (token->type == TOKEN_ERROR)
    {

    }
    else
    {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

// 报告错误的封装
static void errorAtCurrent(const char* message)
{
    errorAt(&parser.current, message);
}
static void error(const char* message)
{
    errorAt(&parser.previous, message);
}

// 移动parser的前后指针
static void advance()   // 词法是语法的进一步抽象（我就说吧，很相似）
{
    parser.previous = parser.current;

    for (;;)
    {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser.current.start);
    }
}

// 检验是否是预期的类型，不是就报错
static void consume(TokenType type, const char* message)
{
    if (parser.current.type == type)
    {
        advance();
        return;
    }
    
    errorAtCurrent(message);
}

// 向字节码中写入追加一个字节
static void emitByte(uint8_t byte)
{
    writeChunk(currentChunk(), byte, parser.previous.line);
}

// 向字节码中写入追加两个字节
static void emitBytes(uint8_t byte1, uint8_t byte2)
{
    emitByte(byte1);
    emitByte(byte2);
}

// 向字节码中添加OP_RETURN
static void emitReturn()
{
    emitByte(OP_RETURN);
}

// 常数要加入常量池，字节码中只存储索引
static uint8_t makeConstant(Value value)
{
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX)
    {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

// 向字节码中添加常数
static void emitConstant(Value value)
{
    emitBytes(OP_CONSTANT, makeConstant(value));
}

// 编译的收尾
static void endCompiler()
{
    emitReturn();
    #ifdef DEBUG_PRINT_CODE
    if (!parser.hadError)
    {
        disassembleChunk(currentChunk(), "code");
    }
    #endif
}

static void parsePrecedence(Precedence precedence);
static ParseRule* getRule(TokenType type);
static void expression();

// 二元表达式
static void binary()    // 中缀表达式
{
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);   // 二元表达式之间亦有优先级     2 * 3 + 4
    parsePrecedence((Precedence)(rule->precedence + 1));    // + 1 的原因是二元表达式是左结合的 ((1 + 2) + 3) + 4

    switch (operatorType)
    {
        case TOKEN_PLUS:            emitByte(OP_ADD); break;
        case TOKEN_MINUS:           emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR:            emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH:           emitByte(OP_DIVIDE); break;
        default: return; // 不是二元表达式
    }
}

// 分组表达式识别
static void grouping()  // 前缀表达式 （）只是用来调整添加字节码的顺序，本身不产生字节码
{
    expression();   // 递归启动！
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

// TOKEN_NUMBER对应的处理函数
static void number()
{
    double value = strtod(parser.previous.start, NULL);
    emitConstant(value);
}

// 一元表达式
static void unary() // 前缀表达式 
{
    TokenType operatorType = parser.previous.type;

    parsePrecedence(PREC_UNARY);    // 设立要解析的优先级，防止过度解析     -a.b + c

    switch (operatorType)   // 还记得栈式虚拟机的执行字节码的方式吗
    {
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        default: return;
    }
}

ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
  [TOKEN_BANG]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL_EQUAL]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_GREATER]       = {NULL,     NULL,   PREC_NONE},
  [TOKEN_GREATER_EQUAL] = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LESS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LESS_EQUAL]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_STRING]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_TRUE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};


// 确立解析表达式的优先级
static void parsePrecedence(Precedence precedence)  // 想念递归下降的第一天
{   
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL)
    {
        error("Expect expression.");
        return;
    }

    prefixRule();   // 优先判断是不是前缀表达式，如果是就递归解析

    /*
    经过上边的递归parser.current指向了缀表达式的下一个词，而precedence还是最初传进来的那个，前缀表示式不用查表获取优先级，如果查到的下一个
    Token的属于中缀表达式，那个将其解析，而中缀表达式无法嵌套只能叠加，所以用循环的方式，前缀表达式是递归在回溯
    */
    while (precedence <= getRule(parser.current.type)->precedence)  
    {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule();
    }
}

// 在表中查询
static ParseRule* getRule(TokenType type)
{
    return &rules[type];
}

// 解析表达式
static void expression()
{
    // 先假设为最低优先级，什么都要解析，后续再进行调整
    parsePrecedence(PREC_ASSIGNMENT);
}

// lox代码编译成字节码
bool compile(const char* source, Chunk* chunk)
{
    initScanner(source);
    compilingChunk = chunk; // 都是指针，真正的资源只有一个，在vm.c中

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    expression();
    consume(TOKEN_EOF, "Expect end of expression.");
    endCompiler();
    return !parser.hadError;
}