package tool;

import java.io.IOException;
import java.io.PrintWriter;
import java.util.Arrays;
import java.util.List;

public class GenerateAst
{
    public static void main(String[] args) throws IOException
    {
        if (args.length != 1)
        {
            System.err.println("Usage: generate_ast <output directory>");
            System.exit(64);
        }
        String outputDir = args[0];
        defineAst(outputDir, "Expr", Arrays.asList(
            "Binary         : Expr left, Token operator, Expr right",
            "Grouping       : Expr expression",
            "Literal        : Object value",
            "Unary          : Token operator, Expr right"
        ));
    }

    private static void defineAst(String outputDir, String baseName, List<String> types) throws IOException
    {
        String path = outputDir + "/" + baseName + ".java";
        PrintWriter writer = new PrintWriter(path, "UTF-8");

        writer.println("package lox;");
        writer.println();
        writer.println("import java.util.List;");
        writer.println();
        writer.println("abstract class " + baseName + "\n{");

        // 定义访问者接口类
        defineVisitor(writer, baseName, types);
        

        // AST 中各种类
        for (String type : types)
        {
            writer.println();
            String className = type.split(":")[0].trim();
            String fields = type.split(":")[1].trim();
            defineType(writer, baseName, className, fields);
        }

        // 访问者模式，定义一个接受泛型的 accept 方法，让AST 中各种类继承，实现多态
        writer.println();
        writer.println("    abstract <R> R accept(Visitor<R> visitor);");

        writer.println("}");
        writer.close();
    }

    private static void defineVisitor(PrintWriter writer, String baseName, List<String> types)
    {
        writer.println("    interface Visitor<R>\n    {");

        for (String type : types)
        {
            String typeName = type.split(":")[0].trim();
            writer.println("        R visit" + typeName + baseName + "(" +
                typeName + " " + baseName.toLowerCase() + ");");
        }

        writer.println("    }");
    }

    private static void defineType(PrintWriter writer, String baseName, String className, String fieldList)
    {
        writer.println("    static class " + className + " extends " + baseName + "\n    {");

        // 构造函数
        writer.println("        " + className + "(" + fieldList + ")\n        {");

        // 在 fields 中存储的参数
        String[] fields = fieldList.split(", ");
        for (String field : fields)
        {
            String name = field.split(" ")[1];
            writer.println("            this." + name + " = " + name + ";");
        }

        writer.println("        }");

        
        writer.println();
        writer.println("        @Override");
        writer.println("        <R> R accept(Visitor<R> visitor)\n        {");
        writer.println("            return visitor.visit" + className + baseName + "(this);");
        writer.println("        }"); 

        // Fields.
        writer.println();
        for (String field : fields)
        {
            writer.println("        final " + field + ";");
        }

        writer.println("    }");
    }
}