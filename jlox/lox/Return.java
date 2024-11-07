package lox;

class Return extends RuntimeException {
    final Object value;

    Return(Object value)
    {
        super(null, null, false, false); // 禁用了一些我们不需要的JVM机制，不需要像堆栈跟踪这样的开销
        this.value = value;
    }
}
