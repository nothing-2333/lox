package lox;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Stack;

class Resolver implements Expr.Visitor<Object>, Stmt.Visitor<Void> {    // 是时候揭开语义分析神秘的面纱了
    private final Interpreter interpreter;  
    private final Stack<Map<String, Boolean>> scopes = new Stack<>();
    private FunctionType currentFunction = FunctionType.NONE;
    private ClassType currentClass = ClassType.NONE;

    // 这个文件总的来说是静态分析，在定义变量与变量赋值时记录了他们对应的作用域的层数；定义了一个变量定义的错误

    /*语义分析感悟
    语义分析其实是遍历一遍树，在真正执行前，静态的进行一次检查，return 没用在函数里、break 没用在循环里、this
    用在类中......
    */

    Resolver(Interpreter interpreter)
    {
        this.interpreter = interpreter;
    }

    // 是否处在一个函数内
    private enum FunctionType   
    {
        NONE,
        FUNCTION,
        INITIALIZER,
        METHOD
    }

    // 是否在一个类内
    private enum ClassType
    {
        NONE,
        CLASS,
        SUBCLASS
    }

    /* 访问者模式思考
    这个设计模式中，不变的是元素类，每个元素类定义了它的属性，而变化的部分是处理不同元素类属性的方法，
    所有的元素类会有一个统一的 accept 方法， accept 方法会接受一个实现了所有处理不同元素属性的方法的
    访问者。
    */

    @Override
    public Void visitBlockStmt(Stmt.Block stmt)
    {
        beginScope();
        resolve(stmt.statements);
        endScope();
        return null;
    }

    @Override 
    public Void visitVarStmt(Stmt.Var stmt)
    {
        declare(stmt.name);
        if (stmt.initializer != null)
        {
            resolve(stmt.initializer);
        }
        define(stmt.name);
        return null;
    }

    @Override
    public Void visitVariableExpr(Expr.Variable expr)
    {
        if (!scopes.isEmpty() && scopes.peek().get(expr.name.lexeme) == Boolean.FALSE)
        {
            Lox.error(expr.name, "Can't read local variable in its own initializer.");
        }

        resolveLocal(expr, expr.name);
        return null;
    }

    @Override 
    public Void visitAssignExpr(Expr.Assign expr)
    {
        resolve(expr.value);
        resolveLocal(expr, expr.name);
        return null;
    }

    @Override
    public Void visitFunctionStmt(Stmt.Function stmt)
    {
        declare(stmt.name);
        define(stmt.name);

        resolveFunction(stmt, FunctionType.FUNCTION);
        return null;
    }

    @Override 
    public Void visitExpressionStmt(Stmt.Expression stmt)
    {
        resolve(stmt.expression);
        return null;
    }

    @Override
    public Void visitIfStmt(Stmt.If stmt)
    {
        resolve(stmt.condition);
        resolve(stmt.thenBranch);
        if (stmt.elseBranch != null) resolve(stmt.elseBranch);
        return null;
    }

    @Override
    public Void visitPrintStmt(Stmt.Print stmt)
    {
        resolve(stmt.expression);
        return null;
    }

    @Override
    public Void visitReturnStmt(Stmt.Return stmt)
    {
        if (currentFunction == FunctionType.NONE)   // 如果不在一个函数内 return 就报错
        {
            Lox.error(stmt.ketword, "Can't return from top-level code.");
        }

        if (stmt.value != null)
        {
            if (currentFunction == FunctionType.INITIALIZER)
            {
                Lox.error(stmt.ketword, "Can't return a value from an initializer.");
            }

            resolve(stmt.value);
        }

        return null;
    }

    @Override
    public Void visitWhileStmt(Stmt.While stmt)
    {
        resolve(stmt.condition);
        resolve(stmt.body);
        return null;
    }

    @Override
    public Void visitBinaryExpr(Expr.Binary expr)
    {
        resolve(expr.left);
        resolve(expr.right);
        return null;
    }

    @Override
    public Void visitCallExpr(Expr.Call expr)
    {
        resolve(expr.callee);

        for (Expr argument : expr.arguments)
        {
            resolve(argument);
        }

        return null;
    }

    @Override
    public Void visitGroupingExpr(Expr.Grouping expr)
    {
        resolve(expr.expression);
        return null;
    }

    @Override
    public Void visitLiteralExpr(Expr.Literal expr)
    {
        return null;
    }

    @Override
    public Void visitLogicalExpr(Expr.Logical expr)
    {
        resolve(expr.left);
        resolve(expr.right);
        return null;
    }

    @Override
    public Void visitUnaryExpr(Expr.Unary expr)
    {
        resolve(expr.right);
        return null;
    }

    @Override
    public Void visitClassStmt(Stmt.Class stmt)
    {
        ClassType enclosingClass = currentClass;
        currentClass = ClassType.CLASS;

        declare(stmt.name);
        declare(stmt.name);

        // 静态检查子类与父类名称相同
        if (stmt.superclass != null && stmt.name.lexeme.equals(stmt.superclass.name.lexeme))
        {
            Lox.error(stmt.superclass.name, "A class can't inherit from itself.");
        }

        if (stmt.superclass != null)
        {
            currentClass = ClassType.SUBCLASS;      // super 关键字的静态检查
            resolve(stmt.superclass);
        }

        if (stmt.superclass != null)    // 为 super 单独开一个环境，解决继承时 super 指向问题
        {
            beginScope();
            scopes.peek().put("super", true);
        }

        beginScope();
        scopes.peek().put("this", true);

        for (Stmt.Function method : stmt.methods)
        {
            FunctionType declaration = FunctionType.METHOD;

            if (method.name.lexeme.equals("init"))
            {
                declaration = FunctionType.INITIALIZER;
            }

            resolveFunction(method, declaration);
        }

        endScope();

        if (stmt.superclass != null) endScope();

        currentClass = enclosingClass;
        return null;
    }
    @Override
    public Void visitGetExpr(Expr.Get expr)
    {
        resolve(expr.object);
        return null;
    }
    @Override
    public Void visitSetExpr(Expr.Set expr)
    {
        resolve(expr.value);
        resolve(expr.object);
        return null;
    }
    @Override
    public Void visitThisExpr(Expr.This expr)
    {
        if (currentClass == ClassType.NONE)
        {
            Lox.error(expr.keyword, "Can't use 'this' outside of a class.");
            return null;
        }

        resolveLocal(expr, expr.keyword);
        return null;
    }
    @Override
    public Void visitSuperExpr(Expr.Super expr)
    {
        if (currentClass == ClassType.NONE)
        {
            Lox.error(expr.keyword, "Can't use 'super' outside of a class.");
        }
        else if (currentClass != ClassType.SUBCLASS)
        {
            Lox.error(expr.keyword, "Can't use 'super' in a class with no superclass.");
        }

        resolveLocal(expr, expr.keyword);
        return null;
    }

    private void resolveFunction(Stmt.Function function, FunctionType type)
    {
        FunctionType enclosingFunction = currentFunction;
        currentFunction = type;

        beginScope();
        for (Token param : function.params)
        {
            declare(param);
            define(param);
        }
        resolve(function.body);
        endScope();

        currentFunction = enclosingFunction;    // 应对函数嵌套
    }
    
    // 让解释器可以直接跳到这个作用域
    private void resolveLocal(Expr expr, Token name)
    {
        for (int i = scopes.size() - 1; i >= 0; --i)
        {
            if (scopes.get(i).containsKey(name.lexeme))
            {
                interpreter.resolve(expr, scopes.size() - 1 - i); // 传递给解释器
                return;
            }
        }
    }

    private void define(Token name)
    {
        if (scopes.isEmpty()) return;
        scopes.peek().put(name.lexeme, true);   // 已就绪
    }

    private void declare(Token name)
    {
        if (scopes.isEmpty()) return;

        Map<String, Boolean> scope = scopes.peek();
        if (scope.containsKey(name.lexeme)) // 在局部作用域中重复声明是一种错误
        {
            Lox.error(name, "Already a variable with this name in this scope.");
        }

        scope.put(name.lexeme, false);  // false 来表明该变量“尚未就绪”
    }

    void resolve(List<Stmt> statements)
    {
        for (Stmt statement : statements)
        {
            resolve(statement);
        }
    }
    private void resolve(Stmt stmt)
    {
        stmt.accept(this);
    }
    private void resolve(Expr expr)
    {
        expr.accept(this);
    }

    // 创建一块新的作用域
    private void beginScope()
    {
        scopes.push(new HashMap<String, Boolean>());    // 全局作用域并不在搜索范围内
    }
    // 退出作用域
    private void endScope()
    {
        scopes.pop();
    }
}
