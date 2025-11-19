#!/bin/bash

# AFL++ 变异策略对比测试脚本
# 用于对比原有策略和基于格理论+MAB的新策略

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 配置
TEST_TIME=${TEST_TIME:-3600}  # 测试时间（秒），默认1小时
TARGET_BINARY=${TARGET_BINARY:-""}
INPUT_DIR=${INPUT_DIR:-"./testcases"}
OUTPUT_DIR_BASE=${OUTPUT_DIR_BASE:-"./comparison_results"}
NUM_RUNS=${NUM_RUNS:-3}  # 每个策略运行次数

echo -e "${GREEN}AFL++ 变异策略对比测试${NC}"
echo "=================================="
echo "测试时间: ${TEST_TIME}秒"
echo "运行次数: ${NUM_RUNS}次/策略"
echo ""

if [ -z "$TARGET_BINARY" ]; then
    echo -e "${RED}错误: 请设置TARGET_BINARY环境变量${NC}"
    echo "用法: TARGET_BINARY=/path/to/target ./compare_strategies.sh"
    exit 1
fi

if [ ! -f "$TARGET_BINARY" ]; then
    echo -e "${RED}错误: 目标二进制文件不存在: $TARGET_BINARY${NC}"
    exit 1
fi

# 创建输出目录
mkdir -p "$OUTPUT_DIR_BASE"
RESULTS_DIR="$OUTPUT_DIR_BASE/$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULTS_DIR"

echo "结果将保存到: $RESULTS_DIR"
echo ""

# 函数：提取统计信息
extract_stats() {
    local output_dir=$1
    local stats_file="$output_dir/fuzzer_stats"
    
    if [ ! -f "$stats_file" ]; then
        echo "0,0,0,0,0,0"
        return
    fi
    
    # 提取关键指标
    local execs=$(grep "execs_done" "$stats_file" | awk '{print $3}')
    local edges=$(grep "edges_found" "$stats_file" | awk '{print $3}')
    local crashes=$(grep "unique_crashes" "$stats_file" | awk '{print $3}')
    local paths=$(grep "paths_total" "$stats_file" | awk '{print $3}')
    local cycles=$(grep "cycles_done" "$stats_file" | awk '{print $3}')
    local exec_time=$(grep "exec_timeout" "$stats_file" | awk '{print $3}')
    
    echo "$execs,$edges,$crashes,$paths,$cycles,$exec_time"
}

# 函数：运行测试
run_test() {
    local strategy_name=$1
    local use_lattice_mab=$2
    local run_num=$3
    local output_dir="$RESULTS_DIR/${strategy_name}_run${run_num}"
    
    echo -e "${YELLOW}运行测试: $strategy_name (运行 #$run_num)${NC}"
    
    mkdir -p "$output_dir"
    
    # 设置环境变量
    export AFL_LATTICE_MAB=$use_lattice_mab
    
    # 运行afl-fuzz
    timeout $TEST_TIME afl-fuzz \
        -i "$INPUT_DIR" \
        -o "$output_dir" \
        -- "$TARGET_BINARY" @@ \
        > "$output_dir/fuzzer.log" 2>&1 || true
    
    # 提取统计信息
    local stats=$(extract_stats "$output_dir")
    echo "$strategy_name,$run_num,$stats" >> "$RESULTS_DIR/results.csv"
    
    echo -e "${GREEN}完成: $strategy_name (运行 #$run_num)${NC}"
    echo ""
}

# 创建结果CSV文件
echo "strategy,run,execs,edges,crashes,paths,cycles,exec_timeout" > "$RESULTS_DIR/results.csv"

# 运行测试
for run in $(seq 1 $NUM_RUNS); do
    # 测试原有策略
    run_test "original" "0" "$run"
    
    # 测试新策略
    run_test "lattice_mab" "1" "$run"
done

# 生成对比报告
echo -e "${GREEN}生成对比报告...${NC}"

python3 << EOF
import csv
import statistics

results_file = "$RESULTS_DIR/results.csv"

# 读取结果
original_results = []
lattice_mab_results = []

with open(results_file, 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        if row['strategy'] == 'original':
            original_results.append({
                'execs': int(row['execs']),
                'edges': int(row['edges']),
                'crashes': int(row['crashes']),
                'paths': int(row['paths']),
                'cycles': int(row['cycles'])
            })
        elif row['strategy'] == 'lattice_mab':
            lattice_mab_results.append({
                'execs': int(row['execs']),
                'edges': int(row['edges']),
                'crashes': int(row['crashes']),
                'paths': int(row['paths']),
                'cycles': int(row['cycles'])
            })

def calc_stats(data, key):
    values = [d[key] for d in data]
    return {
        'mean': statistics.mean(values),
        'median': statistics.median(values),
        'stdev': statistics.stdev(values) if len(values) > 1 else 0
    }

metrics = ['execs', 'edges', 'crashes', 'paths', 'cycles']

report = f"""
========================================
AFL++ 变异策略对比测试报告
========================================

测试配置:
- 测试时间: {TEST_TIME}秒
- 运行次数: {NUM_RUNS}次/策略
- 目标程序: {TARGET_BINARY}

结果对比:
"""

for metric in metrics:
    orig_stats = calc_stats(original_results, metric)
    new_stats = calc_stats(lattice_mab_results, metric)
    
    improvement = ((new_stats['mean'] - orig_stats['mean']) / orig_stats['mean'] * 100) if orig_stats['mean'] > 0 else 0
    
    report += f"""
{metric.upper()}:
  原有策略: 平均={orig_stats['mean']:.2f}, 中位数={orig_stats['median']:.2f}, 标准差={orig_stats['stdev']:.2f}
  新策略:   平均={new_stats['mean']:.2f}, 中位数={new_stats['median']:.2f}, 标准差={new_stats['stdev']:.2f}
  改进:     {improvement:+.2f}%
"""

report += f"""
详细结果已保存到: {RESULTS_DIR}/results.csv
"""

print(report)

# 保存报告
with open(f"{RESULTS_DIR}/report.txt", "w") as f:
    f.write(report)
EOF

echo -e "${GREEN}对比报告已生成: $RESULTS_DIR/report.txt${NC}"
echo ""
echo -e "${GREEN}测试完成！${NC}"

