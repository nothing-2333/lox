package lox;

import java.util.ArrayList;
import java.util.List;

import static lox.TokenType.*;

class Parser {
    private static class ParserError extends RuntimeException {}

    private final List<Token> tokens;
    private int current = 0;

    Parser(List<Token> tokens)
    {
        this.tokens = tokens;
    }

    List<Stmt> parse()
    {
        List<Stmt> statements = new ArrayList<>();
        while (!isAtEnd()) {
            statements.add(declaration());
        }

        return statements;
    }

    // 声明语句判断，开始状态下降
    private Stmt declaration()
    {
        try {
            if (match(VAR)) return varDeclaration();

            return statement();
        } catch (ParserError error) {
            synchronize();
            return null;
        }
    }

    // 状态，我们需要语义分析了！
    private Stmt statement()
    {
        if (match(PRINT)) return printStatement();
        if (match(LEFT_BRACE)) return new Stmt.Block(block());

        return expressionStatement();
    }
    // print 语句
    private Stmt printStatement()
    {
        Expr value = expression();
        consume(SEMICOLON, "Expect ';' after value.");  // 吃掉！
        return new Stmt.Print(value);   // 一个新的 AST 节点！
    }
    // expression 语句
    private Stmt expressionStatement()
    {
        Expr expr = expression();
        consume(SEMICOLON, "Expect ';' after value.");
        return new Stmt.Expression(expr);
    }
    // 声明语句
    private Stmt varDeclaration()
    {
        Token name = consume(IDENTIFIER, "Expect variable name.");

        Expr initializer = null;    // 还记得正则表达式的 * 吗
        if (match(EQUAL))
        {
            initializer = expression();
        }

        consume(SEMICOLON, "Expect ';' after value.");
        return new Stmt.Var(name, initializer);
    }

    private List<Stmt> block()
    {
        List<Stmt> statements = new ArrayList<>();

        while (!check(RIGHT_BRACE) && !isAtEnd()) {
            statements.add(declaration());
        }

        consume(RIGHT_BRACE, "Exprct '}' after block");
        return statements;
    }
    
    /*
    谈一下我对上下文无关文法与递归下降的理解
    上下文无关文法的实现是如 AstPrinter.java 中定义的规则，如括号表达式 -> '(' expression ')' ，
    是几个特定种类的 Token 组合而来的规则，而递归下降是有优先级的排列这些规则，比如我想先解析括号表
    达式，再解析运算表达式，那就先进入括号表达式对应的函数，看看符不符合括号表达式的规则，不符合或者
    某个子项不符合，再跳转到运算表达式......而这个过程在感觉上有些像套被罩，只需要塞进去，抓住四角开
    抖，它自己就会铺好。而递归下降的出口在我们这里是 primary，“抓住了” primary，开递！
     */

    // 递归下降启动！（什么都启动只会害了你，孩子）
    private Expr expression()
    {
        return assignment();  
    }
    private Expr assignment()
    {
        Expr expr = equality();

        if (match(EQUAL))
        {
            Token equals = previous();
            Expr value = assignment();

            if (expr instanceof Expr.Variable)
            {
                Token name = ((Expr.Variable)expr).name;
                return new Expr.Assign(name, value);
            }

            error(equals, "Invalid assignment target.");
        }

        return expr;
    }
    private Expr equality()
    {
        Expr expr = comparison();

        while (match(BANG_EQUAL, EQUAL_EQUAL)) {
            Token operator = previous();
            Expr right = comparison();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }
    private Expr comparison()
    {
        Expr expr = term();

        while (match(GREATER, GREATER_EQUAL, LESS, LESS_EQUAL)) {
            Token operator = previous();
            Expr right = term();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }
    private Expr term()
    {
        Expr expr = factor();

        while (match(MINUS, PLUS)) {
            Token operator = previous();
            Expr right = factor();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }
    private Expr factor()
    {
        Expr expr = unary();

        while (match(SLASH, STAR)) {
            Token operator = previous();
            Expr right = unary();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }
    private Expr unary()
    {
        if (match(BANG, MINUS)) // 这里才是最先匹配的地方
        {
            Token operator = previous();
            Expr right = unary();
            return new Expr.Unary(operator, right);
        }

        return primary();
    }
    private Expr primary()
    {
        if (match(FALSE)) return new Expr.Literal(false);
        if (match(TRUE)) return new Expr.Literal(true);
        if (match(NIL)) return new Expr.Literal(null);

        if (match(NUMBER, STRING))
        {
            return new Expr.Literal(previous().literal);
        }

        if (match(IDENTIFIER))
        {
            return new Expr.Variable(previous());
        }

        if (match(LEFT_PAREN))
        {
            Expr expr = expression();
            consume(RIGHT_PAREN, "Expect ')' after expression.");
            return new Expr.Grouping(expr);
        }

        // 与任何一个都不必配
        throw error(peek(), "Expect expression.");
    }


    // 看看当前 Token 中是否是其中的一个（有没有感觉扫描字符与扫描 Token 有些相似），如果是的话，会被吃掉哦！
    private boolean match(TokenType... types)
    {
        for (TokenType type : types)
        {
            if (check(type))
            {
                advance();      // 吃掉！
                return true;
            }
        }

        return false;
    }

    private boolean check(TokenType type)
    {
        if (isAtEnd()) return false;
        return peek().type == type;
    }

    // 消耗当前 Token
    private Token advance()
    {
        if (!isAtEnd()) current++;
        return previous();
    }

    private boolean isAtEnd()
    {
        return peek().type == EOF;
    }

    private Token peek()
    {
        return tokens.get(current);
    }

    private Token previous()
    {
        return tokens.get(current - 1);
    }

    // 处理错误
    private Token consume(TokenType type, String message)
    {
        if (check(type)) return advance();

        throw error(peek(), message);
    }
    private ParserError error(Token token, String message)
    {
        Lox.error(token, message);
        return new ParserError();
    }

    // 不断丢掉 Token 直到找到以个边界
    private void synchronize()
    {
        advance();

        while (!isAtEnd()) {
            if (previous().type == SEMICOLON) return;

            switch (peek().type) {
                case CLASS:
                case FUN:
                case VAR:
                case FOR:
                case IF:
                case WHILE:
                case PRINT:
                case RETURN:
                    break;
            }

            advance();
        }
    }
}