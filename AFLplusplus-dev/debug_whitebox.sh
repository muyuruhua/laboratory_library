#!/bin/bash

# 快速诊断脚本 - 检查 test-whiteBox.c 是否能正常工作

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build_whitebox_test"
TARGET_PROGRAM="$SCRIPT_DIR/test-whiteBox.c"
TARGET_BINARY="$BUILD_DIR/test-whiteBox"

echo "=== test-whiteBox.c 诊断脚本 ==="
echo ""

# 1. 检查源文件
echo "[1] 检查源文件..."
if [ ! -f "$TARGET_PROGRAM" ]; then
    echo "  错误: 找不到 $TARGET_PROGRAM"
    exit 1
fi
echo "  ✓ 源文件存在"
echo ""

# 2. 编译程序
echo "[2] 编译程序..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if command -v gcc &> /dev/null; then
    gcc -o test-whiteBox "$TARGET_PROGRAM" 2>&1
    if [ -f "$TARGET_BINARY" ]; then
        echo "  ✓ 编译成功 (gcc)"
    else
        echo "  ✗ 编译失败"
        exit 1
    fi
else
    echo "  ✗ 找不到 gcc"
    exit 1
fi
echo ""

# 3. 测试程序基本功能
echo "[3] 测试程序基本功能..."
echo "  测试输入: '1 2'"
if echo "1 2" | "$TARGET_BINARY" >/dev/null 2>&1; then
    echo "  ✓ 程序可以执行"
    echo "  输出:"
    echo "1 2" | "$TARGET_BINARY" 2>&1 | sed 's/^/    /'
else
    echo "  ✗ 程序执行失败"
    exit 1
fi
echo ""

# 4. 测试多个输入
echo "[4] 测试多个输入..."
for input in "0 0" "1 1" "-1 -1" "10 20"; do
    echo "  输入: '$input'"
    if echo "$input" | "$TARGET_BINARY" >/dev/null 2>&1; then
        echo "    ✓ 成功"
    else
        echo "    ✗ 失败"
    fi
done
echo ""

# 5. 测试 AFL++ 工具
echo "[5] 检查 AFL++ 工具..."
if [ -f "$SCRIPT_DIR/afl-fuzz" ]; then
    AFL_FUZZ="$SCRIPT_DIR/afl-fuzz"
    echo "  ✓ 找到 afl-fuzz: $AFL_FUZZ"
else
    echo "  ✗ 找不到 afl-fuzz"
    exit 1
fi

if [ -f "$SCRIPT_DIR/afl-clang-fast" ]; then
    AFL_CLANG_FAST="$SCRIPT_DIR/afl-clang-fast"
    echo "  ✓ 找到 afl-clang-fast: $AFL_CLANG_FAST"
else
    echo "  ⚠ 找不到 afl-clang-fast，将使用普通编译器"
    AFL_CLANG_FAST=""
fi
echo ""

# 6. 测试 AFL++ 编译
echo "[6] 测试 AFL++ 编译..."
if [ -n "$AFL_CLANG_FAST" ]; then
    if "$AFL_CLANG_FAST" -o test-whiteBox-instr "$TARGET_PROGRAM" 2>&1 | head -20; then
        if [ -f "$BUILD_DIR/test-whiteBox-instr" ]; then
            echo "  ✓ AFL++ 编译成功"
            TARGET_BINARY="$BUILD_DIR/test-whiteBox-instr"
        else
            echo "  ⚠ AFL++ 编译可能失败，使用普通编译版本"
        fi
    else
        echo "  ⚠ AFL++ 编译失败，使用普通编译版本"
    fi
fi
echo ""

# 7. 测试 AFL++ 执行
echo "[7] 测试 AFL++ 执行程序..."
TESTCASES_DIR="$BUILD_DIR/testcases"
mkdir -p "$TESTCASES_DIR"
echo "1 2" > "$TESTCASES_DIR/test1.txt"

export AFL_SKIP_CPUFREQ=1
export AFL_NO_AFFINITY=1  # 跳过CPU绑定检查，避免在虚拟化环境中扫描 /proc 目录时卡住
echo "  运行 afl-showmap 测试..."
if [ -f "$SCRIPT_DIR/afl-showmap" ]; then
    OUTPUT_DIR="$BUILD_DIR/test_output"
    mkdir -p "$OUTPUT_DIR"
    
    if echo "1 2" | "$SCRIPT_DIR/afl-showmap" -m none -o "$OUTPUT_DIR/map.txt" -- "$TARGET_BINARY" 2>&1 | head -10; then
        if [ -f "$OUTPUT_DIR/map.txt" ]; then
            echo "  ✓ afl-showmap 成功"
            echo "  覆盖率信息:"
            cat "$OUTPUT_DIR/map.txt" | head -5 | sed 's/^/    /'
        else
            echo "  ⚠ afl-showmap 执行但未生成输出"
        fi
    else
        echo "  ⚠ afl-showmap 执行失败"
    fi
else
    echo "  ⚠ 找不到 afl-showmap"
fi
echo ""

# 8. 测试 afl-fuzz 启动
echo "[8] 测试 afl-fuzz 启动（5秒）..."
OUTPUT_DIR="$BUILD_DIR/fuzz_test"
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

timeout 5s "$AFL_FUZZ" -i "$TESTCASES_DIR" -o "$OUTPUT_DIR" -m none \
    -- "$TARGET_BINARY" > "$BUILD_DIR/fuzz_test.log" 2>&1 &
FUZZ_PID=$!

sleep 2
if kill -0 $FUZZ_PID 2>/dev/null; then
    echo "  ✓ afl-fuzz 进程正在运行"
    kill $FUZZ_PID 2>/dev/null
    wait $FUZZ_PID 2>/dev/null
else
    echo "  ✗ afl-fuzz 进程已退出"
    echo "  错误日志:"
    tail -30 "$BUILD_DIR/fuzz_test.log" | sed 's/^/    /'
fi

# 检查输出目录
sleep 1
if [ -d "$OUTPUT_DIR" ]; then
    echo "  输出目录内容:"
    find "$OUTPUT_DIR" -type f -name "fuzzer_stats" 2>/dev/null | head -5 | sed 's/^/    /'
    if [ -f "$OUTPUT_DIR/default/fuzzer_stats" ]; then
        echo "  ✓ 找到 fuzzer_stats"
    else
        echo "  ⚠ 未找到 fuzzer_stats"
        echo "  目录结构:"
        find "$OUTPUT_DIR" -type d 2>/dev/null | head -10 | sed 's/^/    /'
    fi
fi
echo ""

echo "=== 诊断完成 ==="
echo ""
echo "如果所有测试都通过，可以运行: ./test_whitebox.sh"

