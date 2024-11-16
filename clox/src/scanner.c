#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

// lox扫描仪你值得拥有（我要从现在开始改掉英文后加空格的习惯）
typedef struct 
{
    const char* start;  // 灵魂配料-双指针
    const char* current;
    int line;
} Scanner;

Scanner scanner;    // 扫描器是个全局变量

void initScanner(const char* source) // 任何事物都应该有个初始化不是吗？
{
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
} 

// 判断是否是一个数字
static bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

// 判断是否是标识符的开头
static bool isAlpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

// 判断是否到达代码的结尾
static bool isAtEnd()
{
    return *scanner.current == '\0';    // 还记得在main读取文件时加的'\0'吗
}

// 创造一个Token，记录扫描器当前的信息到Token中
static Token makeToken(TokenType type)
{
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);  // 将扫描器的东西拿来用
    token.line = scanner.line;
    return token;
}

// 创造一个错误的Token
static Token errorToken(const char* message)
{
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    return token;
}

// 检查关键字
static TokenType checkKeyword(int start, int length, const char* rest, TokenType type)
{
    // 优先检查长度
    if (scanner.current - scanner.start == start + length && memcmp(scanner.start + start, rest, length) == 0) 
        return type;

    return TOKEN_IDENTIFIER;
}

// 返回标识符类型-变量or关键字
static TokenType identifierType()
{
    switch (scanner.start[0])
    {
        case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
        case 'c': return checkKeyword(1, 4, "lass", TOKEN_CLASS);
        case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner.current - scanner.start > 1)
            {
                switch (scanner.start[1])
                {
                    case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
                    case 'u': return checkKeyword(2, 1, "n", TOKEN_FUN);
                }
            }
        case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);
        case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
        case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
        case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
        case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
        case 's': return checkKeyword(1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (scanner.current - scanner.start > 1)
            {
                switch (scanner.start[1])
                {
                    case 'h': return checkKeyword(2, 2, "is", TOKEN_THIS);
                    case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
        case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
        case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
    }

    return TOKEN_IDENTIFIER;
}

// 消费一个字符并将其返回
static char advance()
{
    scanner.current++;  // 没有类确实有点不方便，不能全是类，也不能没有类
    return scanner.current[-1];
}

// current指针所在字符
static char peek()
{
    return *scanner.current;
}

// current指针下一个字符
static char peekNext()
{
    if (!isAtEnd()) return '\0';    // 不要越界
    return scanner.current[1];
}

// 识别一个字符串
static Token string()
{
    while (peek() != '"' && !isAtEnd())
    {
        if (peek() == '\n') scanner.line++;
        advance();
    }

    if (isAtEnd()) return errorToken("Unterminated string.");

    advance();
    return makeToken(TOKEN_STRING);
    
}

// 识别一个数
static Token number()
{
    while (isDigit(peek())) advance();

    if (peek() == '.' && isDigit(peekNext()))
    {
        advance();  // 跳过'.'

        while (isDigit(peek())) advance();
    }
    
    return makeToken(TOKEN_NUMBER); // 存的数仍是字符串
}

// 识别标识符
static Token identifier()
{
    while (isAlpha(peek()) || isDigit(peek())) advance();
    return makeToken(identifierType());
}

// 检查是否是期待的字符，如何是消费它，如果不是就不理会
static bool match(char expected)
{
    if (isAtEnd()) return false;
    if (*scanner.current != expected) return false;

    scanner.current++;
    return true;
}

// 空白字符舍去
static void skipWhitespace()
{
    for (;;)
    {
        char c = peek();
        switch (c)
        {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':   // 注释优先处理
                if (peekNext() == '/')
                {
                    while (peek() != '\n' && !isAtEnd()) advance();
                }
                else return;
                break;
            default:
                return;
        }
    }
}

// 扫描一个词
Token scanToken()
{
    skipWhitespace();

    scanner.start = scanner.current;    // 双指针的重置

    if (isAtEnd()) return makeToken(TOKEN_EOF);

    char c = advance();
    if (isAlpha(c)) return identifier();
    if (isDigit(c)) return number();

    switch (c)
    {
        case '(': return makeToken(TOKEN_LEFT_PAREN);
        case ')': return makeToken(TOKEN_RIGHT_PAREN);
        case '{': return makeToken(TOKEN_LEFT_BRACE);
        case '}': return makeToken(TOKEN_RIGHT_BRACE);
        case ';': return makeToken(TOKEN_SEMICOLON);
        case ',': return makeToken(TOKEN_COMMA);
        case '.': return makeToken(TOKEN_DOT);
        case '-': return makeToken(TOKEN_MINUS);
        case '+': return makeToken(TOKEN_PLUS);
        case '/': return makeToken(TOKEN_SLASH);
        case '*': return makeToken(TOKEN_STAR);

        case '!': return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=': return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<': return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>': return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);

        case '"': return string();
        
    }

    return errorToken("Unexpected character.");
}