#!/bin/bash

# 快速测试脚本 - 用于验证Lattice-MAB策略是否正常工作

# 不设置set -e，因为timeout命令在超时后会返回非零退出码

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build_quick_test"
TESTCASES_DIR="$BUILD_DIR/testcases"

echo "快速测试 Lattice-MAB 策略..."
echo ""

# 创建目录
mkdir -p "$BUILD_DIR"
mkdir -p "$TESTCASES_DIR"

# 检查工具（优先使用本地编译的版本）
if [ -f "$SCRIPT_DIR/afl-clang-fast" ]; then
    AFL_CLANG_FAST="$SCRIPT_DIR/afl-clang-fast"
elif command -v afl-clang-fast &> /dev/null; then
    AFL_CLANG_FAST="afl-clang-fast"
else
    echo "错误: 找不到afl-clang-fast"
    echo "请先编译AFL++: cd $SCRIPT_DIR && make"
    exit 1
fi

if [ -f "$SCRIPT_DIR/afl-fuzz" ]; then
    AFL_FUZZ="$SCRIPT_DIR/afl-fuzz"
elif command -v afl-fuzz &> /dev/null; then
    AFL_FUZZ="afl-fuzz"
else
    echo "错误: 找不到afl-fuzz"
    echo "请先编译AFL++: cd $SCRIPT_DIR && make"
    exit 1
fi

echo "使用工具:"
echo "  afl-clang-fast: $AFL_CLANG_FAST"
echo "  afl-fuzz: $AFL_FUZZ"
echo ""

# 编译test-instr.c
echo "[1/3] 编译test-instr.c..."
cd "$BUILD_DIR"

# 尝试使用afl-clang-fast编译（需要完整工具链）
COMPILE_SUCCESS=0
COMPILE_METHOD=""

# 先尝试afl-clang-fast
if "$AFL_CLANG_FAST" -o test-instr "$SCRIPT_DIR/test-instr.c" >/dev/null 2>&1; then
    if [ -f "$BUILD_DIR/test-instr" ]; then
        echo "✓ 使用afl-clang-fast编译成功（带插桩）"
        COMPILE_SUCCESS=1
        COMPILE_METHOD="afl-clang-fast"
    fi
fi

# 如果afl-clang-fast失败，使用普通编译器（无插桩，但可用于对比测试）
if [ $COMPILE_SUCCESS -eq 0 ]; then
    echo "提示: afl-clang-fast需要完整工具链，尝试使用普通编译器..."
    
    # 尝试gcc
    if command -v gcc &> /dev/null; then
        if gcc -o test-instr "$SCRIPT_DIR/test-instr.c" 2>&1 | head -10; then
            if [ -f "$BUILD_DIR/test-instr" ]; then
                echo "✓ 使用gcc编译成功（无插桩，仅用于策略对比）"
                COMPILE_SUCCESS=1
                COMPILE_METHOD="gcc"
            fi
        fi
    fi
    
    # 如果gcc也失败，尝试clang
    if [ $COMPILE_SUCCESS -eq 0 ] && command -v clang &> /dev/null; then
        if clang -o test-instr "$SCRIPT_DIR/test-instr.c" 2>&1 | head -10; then
            if [ -f "$BUILD_DIR/test-instr" ]; then
                echo "✓ 使用clang编译成功（无插桩，仅用于策略对比）"
                COMPILE_SUCCESS=1
                COMPILE_METHOD="clang"
            fi
        fi
    fi
fi

if [ $COMPILE_SUCCESS -eq 0 ] || [ ! -f "$BUILD_DIR/test-instr" ]; then
    echo "错误: 编译失败，找不到可用的编译器"
    echo "调试信息:"
    echo "  尝试的命令: $AFL_CLANG_FAST"
    echo "  目标文件: $BUILD_DIR/test-instr"
    echo "  可用的编译器:"
    command -v gcc && echo "    - gcc: $(command -v gcc)" || echo "    - gcc: 未找到"
    command -v clang && echo "    - clang: $(command -v clang)" || echo "    - clang: 未找到"
    exit 1
fi
echo ""

# 创建测试用例
echo "[2/3] 创建测试用例..."
echo "0" > "$TESTCASES_DIR/input1.txt"
echo "1" > "$TESTCASES_DIR/input2.txt"
echo "✓ 测试用例创建完成"
echo ""

# 函数：查找fuzzer_stats文件
find_fuzzer_stats() {
    local output_dir=$1
    # 尝试多个可能的位置
    if [ -f "$output_dir/fuzzer_stats" ]; then
        echo "$output_dir/fuzzer_stats"
    elif [ -f "$output_dir/default/fuzzer_stats" ]; then
        echo "$output_dir/default/fuzzer_stats"
    else
        # 搜索所有子目录
        find "$output_dir" -name "fuzzer_stats" -type f 2>/dev/null | head -1
    fi
}

# 测试原有策略
echo "[3/3] 运行测试（30秒）..."
echo ""
echo "--- 测试原有策略 ---"
export AFL_LATTICE_MAB=0
export AFL_SKIP_CPUFREQ=1
export AFL_QUIET=1  # 减少输出

# 运行afl-fuzz并保存日志
timeout 30 "$AFL_FUZZ" -i "$TESTCASES_DIR" -o "$BUILD_DIR/output_original" -m none -- "$BUILD_DIR/test-instr" @@ > "$BUILD_DIR/original.log" 2>&1 || true

# 等待文件写入
sleep 2

# 查找统计文件
STATS_FILE=$(find_fuzzer_stats "$BUILD_DIR/output_original")

if [ -n "$STATS_FILE" ] && [ -f "$STATS_FILE" ]; then
    execs=$(grep "^execs_done" "$STATS_FILE" | awk '{print $3}' || echo "0")
    edges=$(grep "^edges_found" "$STATS_FILE" | awk '{print $3}' || echo "0")
    paths=$(grep "^paths_total" "$STATS_FILE" | awk '{print $3}' || echo "0")
    echo "原有策略: execs=$execs, edges=$edges, paths=$paths"
    echo "  统计文件: $STATS_FILE"
else
    echo "警告: 无法读取原有策略的统计信息"
    echo "  输出目录内容:"
    ls -la "$BUILD_DIR/output_original" 2>/dev/null || echo "    输出目录不存在"
    if [ -d "$BUILD_DIR/output_original" ]; then
        find "$BUILD_DIR/output_original" -type f -name "*stats*" 2>/dev/null | head -5
    fi
    echo "  最后10行日志:"
    tail -10 "$BUILD_DIR/original.log" 2>/dev/null || echo "    日志文件不存在"
fi

echo ""
echo "--- 测试新策略 (Lattice-MAB) ---"
export AFL_LATTICE_MAB=1

# 运行afl-fuzz并保存日志
timeout 30 "$AFL_FUZZ" -i "$TESTCASES_DIR" -o "$BUILD_DIR/output_lattice_mab" -m none -- "$BUILD_DIR/test-instr" @@ > "$BUILD_DIR/lattice_mab.log" 2>&1 || true

# 等待文件写入
sleep 2

# 查找统计文件
STATS_FILE=$(find_fuzzer_stats "$BUILD_DIR/output_lattice_mab")

if [ -n "$STATS_FILE" ] && [ -f "$STATS_FILE" ]; then
    execs=$(grep "^execs_done" "$STATS_FILE" | awk '{print $3}' || echo "0")
    edges=$(grep "^edges_found" "$STATS_FILE" | awk '{print $3}' || echo "0")
    paths=$(grep "^paths_total" "$STATS_FILE" | awk '{print $3}' || echo "0")
    echo "新策略: execs=$execs, edges=$edges, paths=$paths"
    echo "  统计文件: $STATS_FILE"
else
    echo "警告: 无法读取新策略的统计信息"
    echo "  输出目录内容:"
    ls -la "$BUILD_DIR/output_lattice_mab" 2>/dev/null || echo "    输出目录不存在"
    if [ -d "$BUILD_DIR/output_lattice_mab" ]; then
        find "$BUILD_DIR/output_lattice_mab" -type f -name "*stats*" 2>/dev/null | head -5
    fi
    echo "  最后10行日志:"
    tail -10 "$BUILD_DIR/lattice_mab.log" 2>/dev/null || echo "    日志文件不存在"
fi

echo ""
echo "✓ 快速测试完成！"
echo ""
echo "调试信息:"
echo "  - 原有策略日志: $BUILD_DIR/original.log"
echo "  - 新策略日志: $BUILD_DIR/lattice_mab.log"
echo "  - 输出目录: $BUILD_DIR/output_*"
echo ""
echo "如需运行完整对比测试，请使用: ./test_lattice_mab.sh"

