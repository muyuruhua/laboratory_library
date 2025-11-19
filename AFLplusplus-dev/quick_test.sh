#!/bin/bash

# 快速测试脚本 - 用于验证Lattice-MAB策略是否正常工作

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build_quick_test"
TESTCASES_DIR="$BUILD_DIR/testcases"

echo "快速测试 Lattice-MAB 策略..."
echo ""

# 创建目录
mkdir -p "$BUILD_DIR"
mkdir -p "$TESTCASES_DIR"

# 检查工具
if ! command -v afl-clang-fast &> /dev/null; then
    echo "错误: 找不到afl-clang-fast"
    echo "请先编译AFL++: cd $SCRIPT_DIR && make"
    exit 1
fi

if ! command -v afl-fuzz &> /dev/null; then
    echo "错误: 找不到afl-fuzz"
    echo "请先编译AFL++: cd $SCRIPT_DIR && make"
    exit 1
fi

# 编译test-instr.c
echo "[1/3] 编译test-instr.c..."
cd "$BUILD_DIR"
afl-clang-fast -o test-instr "$SCRIPT_DIR/test-instr.c" 2>&1 | head -20

if [ ! -f "$BUILD_DIR/test-instr" ]; then
    echo "错误: 编译失败"
    exit 1
fi
echo "✓ 编译成功"
echo ""

# 创建测试用例
echo "[2/3] 创建测试用例..."
echo "0" > "$TESTCASES_DIR/input1.txt"
echo "1" > "$TESTCASES_DIR/input2.txt"
echo "✓ 测试用例创建完成"
echo ""

# 测试原有策略
echo "[3/3] 运行测试（30秒）..."
echo ""
echo "--- 测试原有策略 ---"
export AFL_LATTICE_MAB=0
export AFL_SKIP_CPUFREQ=1
timeout 30 afl-fuzz -i "$TESTCASES_DIR" -o "$BUILD_DIR/output_original" -m none -- "$BUILD_DIR/test-instr" @@ > /dev/null 2>&1 || true

if [ -f "$BUILD_DIR/output_original/fuzzer_stats" ]; then
    execs=$(grep "^execs_done" "$BUILD_DIR/output_original/fuzzer_stats" | awk '{print $3}' || echo "0")
    edges=$(grep "^edges_found" "$BUILD_DIR/output_original/fuzzer_stats" | awk '{print $3}' || echo "0")
    paths=$(grep "^paths_total" "$BUILD_DIR/output_original/fuzzer_stats" | awk '{print $3}' || echo "0")
    echo "原有策略: execs=$execs, edges=$edges, paths=$paths"
else
    echo "警告: 无法读取原有策略的统计信息"
fi

echo ""
echo "--- 测试新策略 (Lattice-MAB) ---"
export AFL_LATTICE_MAB=1
timeout 30 afl-fuzz -i "$TESTCASES_DIR" -o "$BUILD_DIR/output_lattice_mab" -m none -- "$BUILD_DIR/test-instr" @@ > /dev/null 2>&1 || true

if [ -f "$BUILD_DIR/output_lattice_mab/fuzzer_stats" ]; then
    execs=$(grep "^execs_done" "$BUILD_DIR/output_lattice_mab/fuzzer_stats" | awk '{print $3}' || echo "0")
    edges=$(grep "^edges_found" "$BUILD_DIR/output_lattice_mab/fuzzer_stats" | awk '{print $3}' || echo "0")
    paths=$(grep "^paths_total" "$BUILD_DIR/output_lattice_mab/fuzzer_stats" | awk '{print $3}' || echo "0")
    echo "新策略: execs=$execs, edges=$edges, paths=$paths"
else
    echo "警告: 无法读取新策略的统计信息"
fi

echo ""
echo "✓ 快速测试完成！"
echo ""
echo "如需运行完整对比测试，请使用: ./test_lattice_mab.sh"

