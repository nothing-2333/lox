package lox;

import java.util.HashMap;
import java.util.Map;

class Environment {
    final Environment enclosing;    // 添加对上层环境的引用，形成环境链
    private final Map<String, Object> values = new HashMap<>();

    Environment()
    {
        enclosing = null;
    }

    Environment(Environment enclosing)
    {
        this.enclosing = enclosing;
    }

    // 定义一个变量
    void define(String name, Object value)
    {
        values.put(name, value);
    }

    // 获取一个变量
    Object get(Token nama)
    {
        if (values.containsKey(nama.lexeme))
        {
            return values.get(nama.lexeme);
        }

        if (enclosing != null) return enclosing.get(nama);

        throw new RuntimeError(nama, "Undefine variable '" + nama.lexeme + "'.");
    }

    // 赋值一个变量
    void assign(Token name, Object value)
    {
        if (values.containsKey(name.lexeme))
        {
            values.put(name.lexeme, value);
            return ;
        }

        if (enclosing != null)
        {
            enclosing.assign(name, value);
            return ;
        }

        throw new RuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
    }
}
