#!/bin/bash

# AFL++ Lattice-MAB策略对比测试脚本
# 使用test-instr.c作为测试目标

# 不设置set -e，因为某些命令可能失败但需要继续执行

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 配置
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_TIME=${TEST_TIME:-300}  # 测试时间（秒），默认5分钟
NUM_RUNS=${NUM_RUNS:-3}  # 每个策略运行次数
BUILD_DIR="$SCRIPT_DIR/build_test"
RESULTS_DIR="$SCRIPT_DIR/comparison_results_$(date +%Y%m%d_%H%M%S)"
TESTCASES_DIR="$BUILD_DIR/testcases"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}AFL++ Lattice-MAB策略对比测试${NC}"
echo -e "${BLUE}使用test-instr.c作为测试目标${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "${YELLOW}配置:${NC}"
echo "  测试时间: ${TEST_TIME}秒"
echo "  运行次数: ${NUM_RUNS}次/策略"
echo "  构建目录: $BUILD_DIR"
echo "  结果目录: $RESULTS_DIR"
echo ""

# 创建目录
mkdir -p "$BUILD_DIR"
mkdir -p "$RESULTS_DIR"
mkdir -p "$TESTCASES_DIR"

# 检查工具（优先使用本地编译的版本）
if [ -f "$SCRIPT_DIR/afl-clang-fast" ]; then
    AFL_CLANG_FAST="$SCRIPT_DIR/afl-clang-fast"
elif command -v afl-clang-fast &> /dev/null; then
    AFL_CLANG_FAST="afl-clang-fast"
else
    echo -e "${RED}错误: 找不到afl-clang-fast${NC}"
    echo "请先编译AFL++: cd $SCRIPT_DIR && make"
    exit 1
fi

if [ -f "$SCRIPT_DIR/afl-fuzz" ]; then
    AFL_FUZZ="$SCRIPT_DIR/afl-fuzz"
elif command -v afl-fuzz &> /dev/null; then
    AFL_FUZZ="afl-fuzz"
else
    echo -e "${RED}错误: 找不到afl-fuzz${NC}"
    echo "请先编译AFL++: cd $SCRIPT_DIR && make"
    exit 1
fi

echo -e "${GREEN}使用工具:${NC}"
echo "  afl-clang-fast: $AFL_CLANG_FAST"
echo "  afl-fuzz: $AFL_FUZZ"
echo ""

echo -e "${GREEN}[1/5] 编译test-instr.c...${NC}"
cd "$BUILD_DIR"

# 尝试使用afl-clang-fast编译（需要完整工具链）
COMPILE_SUCCESS=0
COMPILE_METHOD=""

# 先尝试afl-clang-fast
if "$AFL_CLANG_FAST" -o test-instr "$SCRIPT_DIR/test-instr.c" >/dev/null 2>&1; then
    if [ -f "$BUILD_DIR/test-instr" ]; then
        echo -e "${GREEN}✓ 使用afl-clang-fast编译成功（带插桩）${NC}" | tee "$RESULTS_DIR/build.log"
        COMPILE_SUCCESS=1
        COMPILE_METHOD="afl-clang-fast"
    fi
fi

# 如果afl-clang-fast失败，使用普通编译器（无插桩，但可用于对比测试）
if [ $COMPILE_SUCCESS -eq 0 ]; then
    echo -e "${YELLOW}提示: afl-clang-fast需要完整工具链，尝试使用普通编译器...${NC}" | tee "$RESULTS_DIR/build.log"
    
    # 尝试gcc
    if command -v gcc &> /dev/null; then
        if gcc -o test-instr "$SCRIPT_DIR/test-instr.c" 2>&1 | tee -a "$RESULTS_DIR/build.log"; then
            if [ -f "$BUILD_DIR/test-instr" ]; then
                echo -e "${GREEN}✓ 使用gcc编译成功（无插桩，仅用于策略对比）${NC}" | tee -a "$RESULTS_DIR/build.log"
                COMPILE_SUCCESS=1
                COMPILE_METHOD="gcc"
            fi
        fi
    fi
    
    # 如果gcc也失败，尝试clang
    if [ $COMPILE_SUCCESS -eq 0 ] && command -v clang &> /dev/null; then
        if clang -o test-instr "$SCRIPT_DIR/test-instr.c" 2>&1 | tee -a "$RESULTS_DIR/build.log"; then
            if [ -f "$BUILD_DIR/test-instr" ]; then
                echo -e "${GREEN}✓ 使用clang编译成功（无插桩，仅用于策略对比）${NC}" | tee -a "$RESULTS_DIR/build.log"
                COMPILE_SUCCESS=1
                COMPILE_METHOD="clang"
            fi
        fi
    fi
fi

if [ $COMPILE_SUCCESS -eq 0 ] || [ ! -f "$BUILD_DIR/test-instr" ]; then
    echo -e "${RED}错误: 编译失败，找不到可用的编译器${NC}"
    echo -e "${YELLOW}调试信息:${NC}"
    echo "  尝试的命令: $AFL_CLANG_FAST"
    echo "  目标文件: $BUILD_DIR/test-instr"
    echo "  可用的编译器:"
    command -v gcc && echo "    - gcc: $(command -v gcc)" || echo "    - gcc: 未找到"
    command -v clang && echo "    - clang: $(command -v clang)" || echo "    - clang: 未找到"
    exit 1
fi

echo ""

# 创建初始测试用例
echo -e "${GREEN}[2/5] 创建初始测试用例...${NC}"
echo "0" > "$TESTCASES_DIR/input1.txt"
echo "1" > "$TESTCASES_DIR/input2.txt"
echo "2" > "$TESTCASES_DIR/input3.txt"
echo "A" > "$TESTCASES_DIR/input4.txt"
echo -e "${GREEN}测试用例创建完成！${NC}"
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

# 函数：提取统计信息
extract_stats() {
    local output_dir=$1
    local stats_file=$(find_fuzzer_stats "$output_dir")
    
    if [ -z "$stats_file" ] || [ ! -f "$stats_file" ]; then
        echo "0,0,0,0,0,0,0,0"
        return
    fi
    
    # 提取关键指标
    local execs=$(grep "^execs_done" "$stats_file" | awk '{print $3}' || echo "0")
    local edges=$(grep "^edges_found" "$stats_file" | awk '{print $3}' || echo "0")
    local crashes=$(grep "^unique_crashes" "$stats_file" | awk '{print $3}' || echo "0")
    local paths=$(grep "^paths_total" "$stats_file" | awk '{print $3}' || echo "0")
    local cycles=$(grep "^cycles_done" "$stats_file" | awk '{print $3}' || echo "0")
    local exec_time=$(grep "^exec_timeout" "$stats_file" | awk '{print $3}' || echo "0")
    local paths_favored=$(grep "^paths_favored" "$stats_file" | awk '{print $3}' || echo "0")
    local paths_imported=$(grep "^paths_imported" "$stats_file" | awk '{print $3}' || echo "0")
    
    echo "$execs,$edges,$crashes,$paths,$cycles,$exec_time,$paths_favored,$paths_imported"
}

# 函数：运行测试
run_test() {
    local strategy_name=$1
    local use_lattice_mab=$2
    local run_num=$3
    local output_dir="$RESULTS_DIR/${strategy_name}_run${run_num}"
    
    echo -e "${YELLOW}[运行测试] $strategy_name (运行 #$run_num)${NC}"
    
    mkdir -p "$output_dir"
    
    # 设置环境变量
    export AFL_LATTICE_MAB=$use_lattice_mab
    export AFL_SKIP_CPUFREQ=1  # 跳过CPU频率检查
    
    # 运行afl-fuzz
    timeout $TEST_TIME "$AFL_FUZZ" \
        -i "$TESTCASES_DIR" \
        -o "$output_dir" \
        -m none \
        -- "$BUILD_DIR/test-instr" @@ \
        > "$output_dir/fuzzer.log" 2>&1 || true
    
    # 等待一下确保文件写入完成
    sleep 2
    
    # 提取统计信息
    local stats=$(extract_stats "$output_dir")
    echo "$strategy_name,$run_num,$stats" >> "$RESULTS_DIR/results.csv"
    
    # 复制fuzzer_stats用于分析
    local stats_file=$(find_fuzzer_stats "$output_dir")
    if [ -n "$stats_file" ] && [ -f "$stats_file" ]; then
        cp "$stats_file" "$output_dir/fuzzer_stats_final"
    fi
    
    echo -e "${GREEN}✓ 完成: $strategy_name (运行 #$run_num)${NC}"
    echo ""
}

# 创建结果CSV文件
echo "strategy,run,execs,edges,crashes,paths,cycles,exec_timeout,paths_favored,paths_imported" > "$RESULTS_DIR/results.csv"

echo -e "${GREEN}[3/5] 开始运行对比测试...${NC}"
echo ""

# 运行测试
for run in $(seq 1 $NUM_RUNS); do
    echo -e "${BLUE}--- 运行批次 #$run ---${NC}"
    
    # 测试原有策略
    run_test "original" "0" "$run"
    
    # 短暂休息，避免资源竞争
    sleep 2
    
    # 测试新策略
    run_test "lattice_mab" "1" "$run"
    
    # 短暂休息
    sleep 2
done

echo -e "${GREEN}[4/5] 生成对比报告...${NC}"

# 生成对比报告
python3 << EOF
import csv
import statistics
import os

results_file = "$RESULTS_DIR/results.csv"

# 读取结果
original_results = []
lattice_mab_results = []

if os.path.exists(results_file):
    with open(results_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                data = {
                    'execs': int(row.get('execs', 0) or 0),
                    'edges': int(row.get('edges', 0) or 0),
                    'crashes': int(row.get('crashes', 0) or 0),
                    'paths': int(row.get('paths', 0) or 0),
                    'cycles': int(row.get('cycles', 0) or 0),
                    'exec_timeout': int(row.get('exec_timeout', 0) or 0),
                    'paths_favored': int(row.get('paths_favored', 0) or 0),
                    'paths_imported': int(row.get('paths_imported', 0) or 0)
                }
                
                if row['strategy'] == 'original':
                    original_results.append(data)
                elif row['strategy'] == 'lattice_mab':
                    lattice_mab_results.append(data)
            except (ValueError, KeyError) as e:
                print(f"Warning: 跳过无效行: {e}")

def calc_stats(data, key):
    if not data:
        return {'mean': 0, 'median': 0, 'stdev': 0, 'min': 0, 'max': 0}
    values = [d[key] for d in data if key in d]
    if not values:
        return {'mean': 0, 'median': 0, 'stdev': 0, 'min': 0, 'max': 0}
    return {
        'mean': statistics.mean(values),
        'median': statistics.median(values),
        'stdev': statistics.stdev(values) if len(values) > 1 else 0,
        'min': min(values),
        'max': max(values)
    }

def format_number(num):
    if num >= 1000000:
        return f"{num/1000000:.2f}M"
    elif num >= 1000:
        return f"{num/1000:.2f}K"
    else:
        return f"{num:.2f}"

metrics = [
    ('execs', '执行次数'),
    ('edges', '发现的边数'),
    ('crashes', '崩溃数'),
    ('paths', '路径总数'),
    ('cycles', '周期数'),
    ('paths_favored', '优先路径数'),
    ('paths_imported', '导入路径数')
]

report = f"""
{'='*60}
AFL++ Lattice-MAB策略对比测试报告
{'='*60}

测试配置:
  - 测试目标: test-instr.c
  - 测试时间: {TEST_TIME}秒/运行
  - 运行次数: {NUM_RUNS}次/策略
  - 结果目录: {RESULTS_DIR}

{'='*60}
结果对比
{'='*60}

"""

for metric_key, metric_name in metrics:
    orig_stats = calc_stats(original_results, metric_key)
    new_stats = calc_stats(lattice_mab_results, metric_key)
    
    if orig_stats['mean'] > 0:
        improvement = ((new_stats['mean'] - orig_stats['mean']) / orig_stats['mean'] * 100)
    else:
        improvement = 0.0 if new_stats['mean'] == 0 else 100.0
    
    improvement_str = f"{improvement:+.2f}%"
    if improvement > 0:
        improvement_str = f"\033[32m{improvement_str}\033[0m"  # 绿色
    elif improvement < 0:
        improvement_str = f"\033[31m{improvement_str}\033[0m"  # 红色
    
    report += f"""
{metric_name} ({metric_key}):
  ┌─────────────────────────────────────────────────────────┐
  │ 原有策略 (Original):                                     │
  │   平均: {format_number(orig_stats['mean']):>10}  │
  │   中位数: {format_number(orig_stats['median']):>8}  │
  │   标准差: {format_number(orig_stats['stdev']):>8}  │
  │   范围: [{format_number(orig_stats['min'])}, {format_number(orig_stats['max'])}] │
  ├─────────────────────────────────────────────────────────┤
  │ 新策略 (Lattice-MAB):                                    │
  │   平均: {format_number(new_stats['mean']):>10}  │
  │   中位数: {format_number(new_stats['median']):>8}  │
  │   标准差: {format_number(new_stats['stdev']):>8}  │
  │   范围: [{format_number(new_stats['min'])}, {format_number(new_stats['max'])}] │
  ├─────────────────────────────────────────────────────────┤
  │ 改进: {improvement_str:>47} │
  └─────────────────────────────────────────────────────────┘
"""

report += f"""
{'='*60}
详细数据
{'='*60}

原始数据已保存到: {RESULTS_DIR}/results.csv

各次运行的详细统计信息:
"""

# 添加每次运行的详细信息
for run in range(1, NUM_RUNS + 1):
    orig_run = [r for r in original_results if original_results.index(r) < len(original_results) and (original_results.index(r) // 1) == run - 1]
    new_run = [r for r in lattice_mab_results if lattice_mab_results.index(r) < len(lattice_mab_results) and (lattice_mab_results.index(r) // 1) == run - 1]
    
    if orig_run and new_run:
        report += f"\n运行 #{run}:\n"
        report += f"  原有策略: execs={orig_run[0]['execs']}, edges={orig_run[0]['edges']}, paths={orig_run[0]['paths']}\n"
        report += f"  新策略:   execs={new_run[0]['execs']}, edges={new_run[0]['edges']}, paths={new_run[0]['paths']}\n"

report += f"""
{'='*60}
总结
{'='*60}

测试完成！请查看上述对比结果。

如需查看更详细的信息，请检查:
  - {RESULTS_DIR}/results.csv (原始数据)
  - {RESULTS_DIR}/*/fuzzer_stats (每次运行的统计信息)
  - {RESULTS_DIR}/*/fuzzer.log (每次运行的日志)
"""

print(report)

# 保存报告（去除ANSI颜色代码）
report_plain = report
import re
report_plain = re.sub(r'\033\[[0-9;]*m', '', report_plain)
with open(f"{RESULTS_DIR}/report.txt", "w", encoding='utf-8') as f:
    f.write(report_plain)

# 同时保存带格式的版本
with open(f"{RESULTS_DIR}/report_formatted.txt", "w", encoding='utf-8') as f:
    f.write(report)
EOF

echo -e "${GREEN}[5/5] 测试完成！${NC}"
echo ""
echo -e "${GREEN}对比报告已生成:${NC}"
echo "  - $RESULTS_DIR/report.txt (纯文本)"
echo "  - $RESULTS_DIR/report_formatted.txt (带格式)"
echo "  - $RESULTS_DIR/results.csv (原始数据)"
echo ""
echo -e "${BLUE}查看报告:${NC}"
echo "  cat $RESULTS_DIR/report.txt"
echo ""

