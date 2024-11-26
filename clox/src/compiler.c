#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "memory.h"

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

typedef void (*ParseFn)(bool canAssign); // 函数指针，见名知意

// 解析规则
typedef struct 
{
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

// 局部作用域
typedef struct 
{
    Token name;
    int depth;
    bool isCaptrued;        // 是否是某个函数的上值
} Local;

// 上值
typedef struct
{
    uint8_t index;      // 上一个函数栈的索引
    bool isLocal;       // 
} Upvalue;

// 顶层代码，还是函数主体
typedef enum
{
    TYPE_FUNCTION,
    TYPE_SCRIPT,
    TYPE_METHOD,
    TYPE_INITIALIZER,
} FunctionType;

// 编译器
typedef struct Compiler
{
    struct Compiler* enclosing;     // 指向上一层环境

    ObjFunction* function;
    FunctionType type;              // 将整个程序包裹在一个隐式的函数中，编译器是在编译层层嵌套的函数

    Local locals[UINT8_COUNT];
    int localCount;                 // 局部变量的数量
    Upvalue upvalues[UINT8_COUNT];  // 上值
    int scopeDepth;                 // 当前作用域的深度
} Compiler;

// 编译器正在编译的类
typedef struct ClassCompiler
{
    struct ClassCompiler* enclosing;    // 上一层类
    bool hasSuperclass;                 // 是否有父类
} ClassCompiler;


Parser parser;                          // 解析器也是全局的
Compiler* current = NULL;               // 编译器的局部作用域
ClassCompiler* currentClass = NULL;     // 当前处于的类

// 当前编译到的字节码
static Chunk* currentChunk()
{
    return &current->function->chunk;
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

// 判断正在处理的Token类型
static bool check(TokenType type)
{
    return parser.current.type == type;
}

// 检验是否是预期的类型，不是就报错，是就吃掉
static void consume(TokenType type, const char* message)
{
    if (check(type))
    {
        advance();
        return;
    }
    
    errorAtCurrent(message);
}

// 检验是否是预期的类型，不是就返回false，是就吃掉
static bool match(TokenType type)
{
    if (!check(type)) return false;
    advance();
    return true;
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

// 循环指令
static void emitLoop(int loopStart)
{
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2; // 2是OP_LOOP后两字节的偏移，因为是往回跳
    if (offset > UINT16_MAX) error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
};


// jump指令
static int emitJump(uint8_t instruction)
{
    emitByte(instruction);
    emitByte(0xff); // 等待回填的跳转偏移
    emitByte(0xff);
    return currentChunk()->count - 2;   // 回填地址
}

// 向字节码中添加OP_RETURN
static void emitReturn()
{
    if (current->type == TYPE_INITIALIZER)
    {
        emitBytes(OP_GET_LOCAL, 0);    // 如果是类的初始化方法，隐式的返回实例
    }
    else
    {
        emitByte(OP_NIL);
    }

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

// 回填
static void patchJump(int offset)
{
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX)
    {
        error("Too much code to jump over.");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;  // 大端
    currentChunk()->code[offset + 1] = jump & 0xff;
}

// 初始化
static void initCompiler(Compiler* compiler, FunctionType type)
{
    compiler->enclosing = current;  // 指向上一个函数
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction(); // 蜜汁操作加一
    current = compiler;

    if (type != TYPE_SCRIPT)
    {
        // 局部函数名放入常量池
        current->function->name = copyString(parser.previous.start, parser.previous.length);    
    }

    Local* local = &current->locals[current->localCount++]; // 0供虚拟机自己内部使用
    local->depth = 0;
    local->isCaptrued = false;
    if (type != TYPE_FUNCTION)
    {
        local->name.start = "this";
        local->name.length = 4;
    }
    else
    {
        local->name.start = "";
        local->name.length = 0;
    }
}

// 编译的收尾
static ObjFunction* endCompiler()
{
    emitReturn();
    ObjFunction* function = current->function;

    #ifdef DEBUG_PRINT_CODE
    if (!parser.hadError)
    {
        disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
    }
    #endif

    current = current->enclosing;   // 还原回去

    return function;
}

// 作用域加一
static void beginScope()
{
    current->scopeDepth++;
}

// 作用域减一
static void endScope()
{
    current->scopeDepth--;

    while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth)
    {
        if (current->locals[current->localCount - 1].isCaptrued)
        {
            emitByte(OP_CLOSE_UPVALUE);
        }
        else
        {
            emitByte(OP_POP);   // 作用域消失时，局部变量出栈
        }
        current->localCount--;
    }
}

static void parsePrecedence(Precedence precedence);
static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static uint8_t identifierConstant(Token* name);
static int resolveLocal(Compiler* compiler, Token* name);
static void and_(bool canAssign);
static void or_(bool canAssign);
static uint8_t argumentList();
static int resolveUpvalue(Compiler* compiler, Token* name);
static Token syntheticToken(const char* text);

// 二元表达式
static void binary(bool canAssign)    // 中缀表达式
{
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);   // 二元表达式之间亦有优先级     2 * 3 + 4
    parsePrecedence((Precedence)(rule->precedence + 1));    // + 1 的原因是二元表达式是左结合的 ((1 + 2) + 3) + 4

    switch (operatorType)
    {
        case TOKEN_BANG_EQUAL:      emitBytes(OP_EQUAL, OP_NOT); break;     // !(a==b)
        case TOKEN_EQUAL_EQUAL:     emitByte(OP_EQUAL); break;
        case TOKEN_GREATER:         emitByte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL:   emitBytes(OP_LESS, OP_NOT); break;      // !(a<b)
        case TOKEN_LESS:            emitByte(OP_LESS); break;
        case TOKEN_LESS_EQUAL:      emitBytes(OP_GREATER, OP_NOT); break;   // !(a>b)

        case TOKEN_PLUS:            emitByte(OP_ADD); break;
        case TOKEN_MINUS:           emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR:            emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH:           emitByte(OP_DIVIDE); break;
        default: return; // 不是二元表达式
    }
}

// 函数调用
static void call(bool canAssign)
{
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

// 实例的.
static void dot(bool canAssign)
{
    consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint8_t name = identifierConstant(&parser.previous);

    if (canAssign && match(TOKEN_EQUAL))
    {
        expression();
        emitBytes(OP_SET_PROPERTY, name);
    }
    else if (match(TOKEN_LEFT_PAREN))
    {
        uint8_t argCount = argumentList();
        emitBytes(OP_INVOKE, name);     // 方法调用的优化-建立一个新的指令
        emitByte(argCount);
    }
    else
    {
        emitBytes(OP_GET_PROPERTY, name);
    }
}

// 解析false、nil或 true
static void literal(bool canAssign)   // 前缀
{
    switch (parser.previous.type)
    {   
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_NIL: emitByte(OP_NIL); break;
        case TOKEN_TRUE: emitByte(OP_TRUE); break;
        default: return;
    }
}

// 分组表达式识别
static void grouping(bool canAssign)  // 前缀表达式 （）只是用来调整添加字节码的顺序，本身不产生字节码
{
    expression();   // 递归启动！
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

// TOKEN_NUMBER对应的处理函数
static void number(bool canAssign)
{
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

// 字符串Token的处理函数
static void string(bool canAssign)
{
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

// 变量赋值或者获取
static void namedVariable(Token name, bool canAssign)
{
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1)
    {
        getOp = OP_GET_LOCAL;   // 函数作用域
        setOp = OP_SET_LOCAL;
    }
    else if ((arg = resolveUpvalue(current, &name)) != -1)
    {
        getOp = OP_GET_UPVALUE; // 嵌套函数的上值
        setOp = OP_SET_UPVALUE;
    }
    else
    {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;  // 全局作用域
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL))
    {
        expression();
        emitBytes(setOp, (uint8_t)arg);
    }
    else
    {
        emitBytes(getOp, (uint8_t)arg);
    }

}

// 读取变量表达式
static void variable(bool canAssign)
{
    namedVariable(parser.previous, canAssign);
}

// this关键字
static void this_(bool canAssign)
{
    if (currentClass == NULL)
    {
        error("Can't use 'this' outside of a class.");
        return;
    }

    variable(false);    // this像局部变量一样编译
}

// super关键字
static void super_(bool conAssign)
{
    if (currentClass == NULL)
    {
        error("Can't use 'super' outside of a class.");
    }
    else if (!currentClass->hasSuperclass)
    {
        error("Can't use 'super' in a class with no superclass.");
    }

    consume(TOKEN_DOT, "Expect '.' after 'super'.");
    consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
    uint8_t name = identifierConstant(&parser.previous);

    namedVariable(syntheticToken("this"), false);
    if (match(TOKEN_LEFT_PAREN))
    {
        uint8_t argCount = argumentList();
        namedVariable(syntheticToken("super"), false);
        emitBytes(OP_SUPER_INVOKE, name);   // 优化调用
        emitByte(argCount);
    }
    else
    { 
        namedVariable(syntheticToken("super"), false);
        emitBytes(OP_GET_SUPER, name);
    }
}

// 一元表达式
static void unary(bool canAssign) // 前缀表达式 
{
    TokenType operatorType = parser.previous.type;

    parsePrecedence(PREC_UNARY);    // 设立要解析的优先级，防止过度解析     -a.b + c

    switch (operatorType)   // 还记得栈式虚拟机的执行字节码的方式吗
    {
        case TOKEN_BANG:    emitByte(OP_NOT); break;
        case TOKEN_MINUS:   emitByte(OP_NEGATE); break;
        default: return;
    }
}

ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]    = {grouping, call,   PREC_CALL},
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]           = {NULL,     dot,    PREC_CALL},
  [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
  [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
  [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     or_,    PREC_OR},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {super_,   NULL,   PREC_NONE},
  [TOKEN_THIS]          = {this_,    NULL,   PREC_NONE},
  [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
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

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);   // 优先判断是不是前缀表达式，如果是就递归解析

    /*
    经过上边的递归parser.current指向了缀表达式的下一个词，而precedence还是最初传进来的那个，前缀表示式不用查表获取优先级，如果查到的下一个
    Token的属于中缀表达式，那个将其解析，而中缀表达式无法嵌套只能叠加，所以用循环的方式，前缀表达式是递归在回溯
    */
    while (precedence <= getRule(parser.current.type)->precedence)  
    {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL))
    {
        error("Invalid assignment target.");
    }
}

// 把变量名存进常量池，返回索引
static uint8_t identifierConstant(Token* name)
{
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

// 判断两个名称Token相等
static bool identifiersEqual(Token* a, Token* b)
{
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

// 解析局部变量
static int resolveLocal(Compiler* compiler, Token* name)
{

    for (int i = compiler->localCount - 1; i >= 0; --i)
    {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name))
        {
            if (local->depth == -1)
            {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

// 为当前函数添加一个上值
static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal)
{
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; ++i)
    {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal)
        {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT)
    {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;  // 返回自己的上值序号
}

// 寻找上值变量
static int resolveUpvalue(Compiler* compiler, Token* name)
{
    if (compiler->enclosing == NULL) return -1;

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1)
    {
        compiler->enclosing->locals[local].isCaptrued = true;
        return addUpvalue(compiler, (uint8_t)local, true);      // 上值true
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);    // 递归寻找
    if (upvalue != -1)
    {
        return addUpvalue(compiler, (uint8_t)upvalue, false);   // 上上上*值false
    }

    return -1;
}

// 记住局部变量的位置
static void addLocal(Token name)
{
    if (current->localCount == UINT8_COUNT)
    {
        error("Too many local variables in function.");
        return;
    }

    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
    local->isCaptrued = false;
}

// 局部变量处理
static void declareVariable()
{
    if (current->scopeDepth == 0) return; // 全局变量不处理
    Token* name = &parser.previous;
    for (int i = current->localCount - 1; i >= 0; --i)
    {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth)
        {
            break;
        }

        if (identifiersEqual(name, &local->name))
        {
            error("Already a variable with this name in this scope.");
        }
    }

    addLocal(*name);
}

// 解析变量名
static uint8_t parseVariable(const char* errorMessage)
{
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (current->scopeDepth > 0) return 0;  // 局部变量不放入常量池

    return identifierConstant(&parser.previous);
}

// 变量的初始化式编译完成，再将其标记为已初始化
static void markInitialized()
{
    if (current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

// 定义一个变量
static void defineVariable(uint8_t global)
{
    if (current->scopeDepth > 0) // 局部变量不放入常量池
    {
        markInitialized();
        return; 
    }

    emitBytes(OP_DEFINE_GLOBAL, global); 
}

// 函数参数个数
static uint8_t argumentList()
{
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN))
    {
        do
        {
            expression();
            if (argCount == 255)
            {
                error("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

// &&
static void and_(bool canAssign)
{
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);   // 弹出栈中的true，看看另一个表达式
    parsePrecedence(PREC_AND);

    patchJump(endJump); // 栈中的false直接保留下来，返回
}

// || 
static void or_(bool canAssign)
{
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);    // 栈中的true直接保留，返回

    patchJump(elseJump);
    emitByte(OP_POP);   // false弹栈看下一个表达式

    parsePrecedence(PREC_OR);
    patchJump(endJump);
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

// 块 
static void block()
{
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
    {
        declaration();
    }
    
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

// 处理函数 
static void function(FunctionType type)
{
    Compiler compiler;
    initCompiler(&compiler, type);  // 每个函数都新开一个局部变量数组
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN))
    {
        do 
        {
            current->function->arity++;
            if (current->function->arity > 255)
            {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            uint8_t constant = parseVariable("Expect parameter name.");
            defineVariable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction* function = endCompiler();
    emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; ++i)    // 将内层函数所需要的上值保留下来
    {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

// 方法
static void method()
{
    consume(TOKEN_IDENTIFIER, "Expect method name.");
    uint8_t constant = identifierConstant(&parser.previous);

    FunctionType type = TYPE_METHOD;

    if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0)
    {
        type = TYPE_INITIALIZER;
    }

    function(type);

    emitBytes(OP_METHOD, constant);
}

// 创建一个Token
static Token syntheticToken(const char* text)
{
    Token token;
    token.start = text;
    token.length = (int)strlen(text);
    return token;
}

// 遇见类关键字
static void classDeclaration()
{
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    Token className = parser.previous;
    uint8_t nameConstant = identifierConstant(&parser.previous);    // 获取名称
    declareVariable();

    emitBytes(OP_CLASS, nameConstant);  // 定义类
    defineVariable(nameConstant);

    ClassCompiler classCompiler;    // 加入链表
    classCompiler.hasSuperclass = false;
    classCompiler.enclosing = currentClass;
    currentClass = &classCompiler;

    if (match(TOKEN_LESS))  // 继承
    {
        consume(TOKEN_IDENTIFIER, "Expect superclass name.");
        variable(false);

        if (identifiersEqual(&className, &parser.previous))
        {
            error("A class can't inherit from itself.");
        }

        beginScope();       // 在所有方法上加一个作用域存储super
        addLocal(syntheticToken("super"));
        defineVariable(0);

        namedVariable(className, false);
        emitByte(OP_INHERIT);
        classCompiler.hasSuperclass = true;
    }

    namedVariable(className, false);    // 存入栈
    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");

    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
    {
        method();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emitByte(OP_POP);                   // 弹出栈

    if (classCompiler.hasSuperclass) endScope();

    currentClass = currentClass->enclosing;     // 恢复
}

// 遇见函数关键字
static void funDeclaration()
{
    uint8_t global = parseVariable("Expect function name.");
    markInitialized();  // 函数内部可以引用自己
    function(TYPE_FUNCTION);
    defineVariable(global);
}


// var语句
static void varDeclaration()
{
    uint8_t global = parseVariable("Expect variable name.");

    if (match(TOKEN_EQUAL))
    {
        expression();
    }
    else
    {
        emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(global);
}

// 表达式语句
static void expressionStatement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

// for 
static void forStatement()
{
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON))
    {

    }
    else if (match(TOKEN_VAR))
    {
        varDeclaration();
    }
    else
    {
        expressionStatement();
    }

    int loopStart = currentChunk()->count;
    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON))
    {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        exitJump = emitJump(OP_JUMP_IF_FALSE);  // false就跳出去
        emitByte(OP_POP);
    }

    if (!match(TOKEN_RIGHT_PAREN))
    {
        int bodyJump = emitJump(OP_JUMP);   // 先不执行增量，跳转到body代码
        int incrementStart = currentChunk()->count; // 这里是第一跳，跳到增量
        expression();
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(loopStart);    // 这里在是第二跳，跳到开始
        loopStart = incrementStart;
        patchJump(bodyJump);
    }

    statement();
    emitLoop(loopStart);

    if (exitJump != -1)
    {
        patchJump(exitJump);
        emitByte(OP_POP);
    }

    endScope();
}

// 编译if语句
static void ifStatement()
{
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition."); 

    int thenJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);   // if
    statement();    
    int elseJump = emitJump(OP_JUMP);

    patchJump(thenJump);

    emitByte(OP_POP);   // else 
    if (match(TOKEN_ELSE)) statement(); 

    patchJump(elseJump);
}

// print语句
static void printStatement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

// 遇见return关键字
static void returnStatement()
{
    if (current->type == TYPE_SCRIPT)
    {
        error("Can't return from top-level code.");
    }

    if (match(TOKEN_SEMICOLON))
    {
        emitReturn();
    }
    else
    {
        if (current->type == TYPE_INITIALIZER)
        {
            error("Can't return a value from an initializer.");
        }

        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }

}

// while
static void whileStatement()
{
    int loopStart = currentChunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
}

// 发生恐慌后接着寻找这个语句的错误
static void synchronize()
{
    parser.panicMode = false;

    while (parser.previous.type != TOKEN_EOF)
    {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type)
        {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
        
            default:
                ;
        }

        advance();
    }
    
}

// 解析语句
static void declaration()
{
    if (match(TOKEN_VAR))
    {
        varDeclaration();
    }
    else if (match(TOKEN_FUN))
    {
        funDeclaration();
    }
    else if (match(TOKEN_CLASS))
    {
        classDeclaration();
    }
    else
    {
        statement();
    }

    if (parser.panicMode) synchronize();
}

// statement
static void statement()
{
    if (match(TOKEN_PRINT))
    {
        printStatement();
    }
    else if (match(TOKEN_LEFT_BRACE))
    {
        beginScope();
        block();
        endScope();
    }
    else if (match(TOKEN_IF))
    {
        ifStatement();
    }
    else if (match(TOKEN_WHILE))
    {
        whileStatement();
    }
    else if (match(TOKEN_FOR))
    {
        forStatement();
    }
    else if (match(TOKEN_RETURN))
    {
        returnStatement();
    }
    else
    {
        expressionStatement();
    }
}

/*lox代码编译成字节码
declaration    → classDecl
               | funDecl
               | varDecl
               | statement ;

statement      → exprStmt
               | forStmt
               | ifStmt
               | printStmt
               | returnStmt
               | whileStmt
               | block ;
 */
ObjFunction* compile(const char* source)
{
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    while (!match(TOKEN_EOF))
    {
        declaration();
    }
    
    ObjFunction* function =  endCompiler();
    return parser.hadError ? NULL : function;
}

void markCompilerRoots()
{
    Compiler* compiler = current;
    while (compiler != NULL)
    {
        markObject((Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}