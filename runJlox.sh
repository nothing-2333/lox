# 检查bin目录是否存在
if [ ! -d "jlox/bin" ]; then
    echo "jlox 中 bin 目录不存在，正在创建..."
    mkdir jlox/bin
    echo "成功创建."
fi

# 编译脚本生成 AST 树的子类
javac -d jlox/bin -sourcepath ./jlox  ./jlox/tool/GenerateAst.java
java -cp jlox/bin tool.GenerateAst ./jlox/lox/

# 编译、运行 lox
javac -d jlox/bin -sourcepath ./jlox  ./jlox/lox/Lox.java
java -cp jlox/bin lox.Lox

# 编译与运行 AstPrinter
# javac -d jlox/bin -sourcepath ./jlox  ./jlox/lox/AstPrinter.java
# java -cp jlox/bin lox.AstPrinter



