package lox;

import java.util.List;
import java.util.ArrayList;

class Interpreter implements Expr.Visitor<Object>, Stmt.Visitor<Void> { // Stmt 是对 Expr 的拓展
    final Environment globals = new Environment();  // 最外层的全局环境
    private Environment environment = globals;  // 当前作用域环境

    Interpreter()
    {
        globals.define("clock", new LoxCallable() { // 定义原生函数
            @Override
            public int arity() { return 0; }

            @Override
            public Object call(Interpreter interpreter, List<Object> arguments)
            {
                return (double)System.currentTimeMillis() / 1000.0;
            }

            @Override
            public String toString() { return "<native fn>"; }
        });
    }

    // 对外方法，为 AST 中的节点写出具体的处理，实现抽象方法
    void interpret(List<Stmt> statements)
    {
        try {
            for (Stmt statement : statements)
            execute(statement);
        } catch (RuntimeError error) {
            Lox.runtimeError(error);
        }
    }

    private void execute(Stmt stmt)     // 怎么样，名字起的不错吧
    {
        stmt.accept(this);
    }

    // Expr 的抽象方法实现
    @Override
    public Object visitLiteralExpr(Expr.Literal expr)
    {
        return expr.value;
    }

    @Override
    public Object visitGroupingExpr(Expr.Grouping expr)
    {
        return evaluate(expr.expression);
    }

    @Override
    public Object visitUnaryExpr(Expr.Unary expr)
    {
        Object right = evaluate(expr.right);

        switch (expr.operator.type) {
            case BANG:
                return !isTruthy(right);
            case MINUS:
                checkNumberOperand(expr.operator, right); // 运行时检查
                return -(double)right;
        }

        return null;
    }

    @Override
    public Object visitBinaryExpr(Expr.Binary expr)
    {
        Object left = evaluate(expr.left);
        Object right = evaluate(expr.right);

        switch (expr.operator.type)
        {
            case GREATER:
                checkNumberOperands(expr.operator, left, right);
                return (double)left > (double)right;
            case GREATER_EQUAL:
                checkNumberOperands(expr.operator, left, right);
                return (double)left >= (double)right;
            case LESS:
                checkNumberOperands(expr.operator, left, right);
                return (double)left < (double)right;
            case LESS_EQUAL:
                checkNumberOperands(expr.operator, left, right);
                return (double)left <= (double)right;
            case MINUS:
                checkNumberOperands(expr.operator, left, right);
                return (double)left - (double)right;
            case BANG_EQUAL:
                return !isEqual(left, right);
            case EQUAL_EQUAL:
                return isEqual(left, right);
            case PLUS:
                if (left instanceof Double && right instanceof Double)
                {
                    return (double)left + (double)right;
                }
                if (left instanceof String && right instanceof String)
                {
                    return (String)left + (String)right;
                }
                throw new RuntimeError(expr.operator, "Operands must be two numbers or two strings.");
            case SLASH:
                checkNumberOperands(expr.operator, left, right);
                return (double)left / (double)right;
            case STAR:
                checkNumberOperands(expr.operator, left, right);
                return (double)left * (double)right;
        }

        return null;
    }

    public Object visitCallExpr(Expr.Call expr)
    {
        Object callee = evaluate(expr.callee);

        List<Object> arguments = new ArrayList<>();
        for (Expr argument : expr.arguments)
        {
            arguments.add(evaluate(argument));
        }

        if (!(callee instanceof LoxCallable))   // 如果调用者不是一个函数，你怎么办呢，孩子
        {
            throw new RuntimeError(expr.paren, "Can only call functions and classes.");
        }

        LoxCallable function = (LoxCallable)callee;

        if (arguments.size() != function.arity())   // 小子，参数对不上！
        {
            throw new RuntimeError(expr.paren, "Expected " + function.arity() + " arguments but got " +
            arguments.size() + ".");
        }

        return function.call(this, arguments);
    }

    // 一元运算检查
    private void checkNumberOperand(Token operator, Object operand)
    {
        if (operand instanceof Double) return ;
        throw new RuntimeError(operator, "Operand must be a number.");
    }
    // 二元运算检查
    private void checkNumberOperands(Token operator, Object left, Object right)
    {
        if (left instanceof Double && right instanceof Double) return ;

        throw new RuntimeError(operator, "Operands must be numbers.");
    }

    private Object evaluate(Expr expr)
    {
        return expr.accept(this);
    }

    private boolean isTruthy(Object object)
    {
        if (object == null) return false;
        if (object instanceof Boolean) return (boolean)object;
        return true;
    }

    private boolean isEqual(Object a, Object b)
    {
        if (a == null && b == null) return true;
        if (a == null) return false;

        return a.equals(b);
    }

    // 将对象转换成字符串
    private String stringify(Object object)
    {
        if (object == null) return "nil";

        if (object instanceof Double)
        {
            String text = object.toString();
            if (text.endsWith(".0"))
            {
                text = text.substring(0, text.length() - 2);
            }
            return text;
        }
        return object.toString();
    }

    // Stmt 的抽象方法实现
    @Override
    public Void visitExpressionStmt(Stmt.Expression stmt)
    {
        evaluate(stmt.expression);
        return null;
    }

    @Override
    public Void visitPrintStmt(Stmt.Print stmt)
    {
        Object value = evaluate(stmt.expression);
        System.out.println(stringify(value));
        return null;
    }

    @Override
    public Void visitVarStmt(Stmt.Var stmt)
    {
        Object value = null;
        if (stmt.initializer != null)
        {
            value = evaluate(stmt.initializer);
        }

        environment.define(stmt.name.lexeme, value);
        return null;
    }

    @Override
    public Object visitVariableExpr(Expr.Variable expr)
    {
        return environment.get(expr.name);
    }

    @Override
    public Object visitAssignExpr(Expr.Assign expr)
    {
        Object value = evaluate(expr.value);
        environment.assign(expr.name, value);
        return value;
    }

    @Override
    public Void visitBlockStmt(Stmt.Block stmt)
    {
        executeBlock(stmt.statements, new Environment(environment));
        return null;
    }

    @Override
    public Void visitIfStmt(Stmt.If stmt)
    {
        if (isTruthy(evaluate(stmt.condition)))
        {
            execute(stmt.thenBranch);
        }
        else if (stmt.elseBranch != null)
        {
            execute(stmt.elseBranch);
        }

        return null;
    }

    @Override
    public Object visitLogicalExpr(Expr.Logical expr)
    {
        Object left = evaluate(expr.left);

        if (expr.operator.type == TokenType.OR) // 先检查特殊情况
        {
            if (isTruthy(left)) return left;
        }
        else
        {
            if (!isTruthy(left)) return left;
        }

        return evaluate(expr.right);
    }
    @Override
    public Void visitWhileStmt(Stmt.While stmt)
    {
        while (isTruthy(evaluate(stmt.condition))) {
            execute(stmt.body);
        }
        return null;
    }
    @Override
    public Void visitFunctionStmt(Stmt.Function stmt)
    {
        LoxFunction function = new LoxFunction(stmt, environment);
        environment.define(stmt.name.lexeme, function);
        return null;
    }
    @Override 
    public Void visitReturnStmt(Stmt.Return stmt)
    {
        Object value = null;
        if (stmt.value != null) value = evaluate(stmt.value);

        throw new Return(value);    // 我们用异常跳过多层的函数嵌套，这简直是天才的构思
    }

    void executeBlock(List<Stmt> statements, Environment environment)
    {
        Environment previous = this.environment;
        try {
            this.environment = environment; // environment 是继承了老环境的新环境（我不是rap）

            for (Stmt statement : statements)
            {
                execute(statement); // 开始递归
            }
        } finally {
            this.environment = previous;    // 恢复环境
        }
    }

}
