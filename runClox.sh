if [ ! -d "clox/bin" ]; then
    echo "clox 中 bin 目录不存在，正在创建..."
    mkdir clox/bin
    echo "成功创建."
fi

if [ ! -d "clox/build" ]; then
    echo "clox 中 build 目录不存在，正在创建..."
    mkdir clox/build
    echo "成功创建."
fi

cd clox/build

cmake ..
cmake --build .


../bin/clox