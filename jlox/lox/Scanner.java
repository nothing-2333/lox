package lox;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import static lox.TokenType.*;

class Scanner {
    private final String source;
    private final List<Token> tokens = new ArrayList<>();
    private int start = 0;      // start 与 current 构成双指针，扫描一个 Token
    private int current = 0;
    private int line = 1;
    private static final Map<String, TokenType> keywords;    // 保留字
   
    static {
        keywords = new HashMap<>();
        keywords.put("and",    AND);
        keywords.put("class",  CLASS);
        keywords.put("else",   ELSE);
        keywords.put("false",  FALSE);
        keywords.put("for",    FOR);
        keywords.put("fun",    FUN);
        keywords.put("if",     IF);
        keywords.put("nil",    NIL);
        keywords.put("or",     OR);
        keywords.put("print",  PRINT);
        keywords.put("return", RETURN);
        keywords.put("super",  SUPER);
        keywords.put("this",   THIS);
        keywords.put("true",   TRUE);
        keywords.put("var",    VAR);
        keywords.put("while",  WHILE);
    }

    Scanner(String source)
    {
        this.source = source;
    }

    List<Token> scanTokens()
    {
        while (!isAtEnd()) {
            start = current;    // 移动 start 到下一个 Token 起始字符
            scanToken();
        }

        tokens.add(new Token(EOF, "", null, line));
        return tokens;
    }

    private void scanToken()
    {
        char c = advance();
        switch (c) {
            case '(': addToken(LEFT_PAREN); break;
            case ')': addToken(RIGHT_PAREN); break;
            case '{': addToken(LEFT_BRACE); break;
            case '}': addToken(RIGHT_BRACE); break;
            case ',': addToken(COMMA); break;
            case '.': addToken(DOT); break;
            case '-': addToken(MINUS); break;
            case '+': addToken(PLUS); break;
            case ';': addToken(SEMICOLON); break;
            case '*': addToken(STAR); break;
            
            case '!':
                addToken(match('=') ? BANG_EQUAL : BANG);
                break;
            case '=':
                addToken(match('=') ? EQUAL_EQUAL : EQUAL);
                break;
            case '<':
                addToken(match('=') ? LESS_EQUAL : LESS);
                break;
            case '>':
                addToken(match('=') ? GREATER_EQUAL : GREATER);
                break;
            case '/':
                if (match('/'))
                {
                    while (peek() != '\n' && !isAtEnd()) advance();
                }
                else addToken(SLASH);
                break;

            case ' ':
            case '\r':
            case '\t':
                break;
            case '\n':
                line++;
                break;

            case '"': string(); break;

            default:
                if (isDigit(c))
                {
                    number();
                }
                else if (isAlpha(c))
                {
                    identifier();
                }
                else 
                {
                    Lox.error(line, "Unexpected character");
                }
                break;
        }
    }

    private void identifier()
    {
        while (isAlphaNumeric(peek())) advance();

        String text = source.substring(start, current);
        TokenType type = keywords.get(text);
        if (type == null) type = IDENTIFIER;
        addToken(type);
    }

    private void number()
    {
        while (isDigit(peek())) advance();

        if (peek() == '.' && isDigit(peekNext()))
        {
            advance();
            while (isDigit(peek())) advance();
        }

        addToken(NUMBER, Double.parseDouble(source.substring(start, current)));
    }

    private void string()
    {
        while (peek() != '"' && !isAtEnd()) {
            if (peek() == '\n') line++;
            advance();
        }

        if (isAtEnd())
        {
            Lox.error(line, "Unterminated string.");
            return ;
        }

        advance();

        String value = source.substring(start + 1, current - 1);
        addToken(STRING, value);
    }

    // 一个有条件的 advance 只有当前字符是我们正在寻找的字符时，我们才会消费
    private boolean match(char expected)
    {
        if (isAtEnd()) return false;
        if (source.charAt(current) != expected) return false;

        current++;  // current 永远指向下一个将要读取的字符
        return true;
    }

    // 看一下当前的字符是什么 lookahead(前瞻)
    private char peek()
    {
        if (isAtEnd()) return '\0';
        return source.charAt(current);
    }
    // 再多看一个字符
    private char peekNext()
    {
        if (current + 1 >= source.length()) return '\0';
        return source.charAt(current + 1); 
    }

    private boolean isAlphaNumeric(char c)
    {
        return isAlpha(c) || isDigit(c);
    }

    private boolean isAlpha(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    private boolean isDigit(char c)
    {
        return c >= '0' && c <= '9';
    }

    private boolean isAtEnd()
    {
        return current >= source.length();
    }

    private char advance()
    {
        return source.charAt(current++);
    }

    // 重载
    private void addToken(TokenType type)
    {
        addToken(type, null);
    }
    private void addToken(TokenType type, Object literal)
    {
        String text = source.substring(start, current);
        tokens.add(new Token(type, text, literal, line));
    }


}
