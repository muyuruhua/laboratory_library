#!/bin/bash

# test-whiteBox.c 对比测试脚本
# 对比原策略和 Lattice-MAB 策略的模糊测试效果

# 不设置set -e，因为某些命令可能失败但需要继续执行

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build_whitebox_test"
TESTCASES_DIR="$BUILD_DIR/testcases"
RESULTS_DIR="$BUILD_DIR/results"
TARGET_PROGRAM="$SCRIPT_DIR/test-whiteBox.c"
TARGET_BINARY="$BUILD_DIR/test-whiteBox"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 测试时间（秒）
TEST_TIME=${TEST_TIME:-60}

echo "========================================"
echo "test-whiteBox.c 对比测试"
echo "原策略 vs Lattice-MAB 策略"
echo "========================================"
echo ""
echo "配置:"
echo "  测试时间: ${TEST_TIME}秒"
echo "  目标程序: $TARGET_PROGRAM"
echo "  构建目录: $BUILD_DIR"
echo ""

# 创建目录
mkdir -p "$BUILD_DIR"
mkdir -p "$TESTCASES_DIR"
mkdir -p "$RESULTS_DIR"

# 检查目标程序是否存在
if [ ! -f "$TARGET_PROGRAM" ]; then
    echo -e "${RED}错误: 找不到目标程序 $TARGET_PROGRAM${NC}"
    exit 1
fi

# 检查工具（优先使用本地编译的版本）
if [ -f "$SCRIPT_DIR/afl-clang-fast" ]; then
    AFL_CLANG_FAST="$SCRIPT_DIR/afl-clang-fast"
elif command -v afl-clang-fast &> /dev/null; then
    AFL_CLANG_FAST="afl-clang-fast"
else
    echo -e "${YELLOW}警告: 找不到afl-clang-fast，尝试使用普通编译器...${NC}"
    AFL_CLANG_FAST=""
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

echo "使用工具:"
echo "  afl-clang-fast: ${AFL_CLANG_FAST:-未找到，将使用普通编译器}"
echo "  afl-fuzz: $AFL_FUZZ"
echo ""

# 编译test-whiteBox.c
echo -e "${GREEN}[1/4] 编译 test-whiteBox.c...${NC}"
cd "$BUILD_DIR"

COMPILE_SUCCESS=0
COMPILE_METHOD=""

# 先尝试afl-clang-fast
if [ -n "$AFL_CLANG_FAST" ]; then
    if "$AFL_CLANG_FAST" -o test-whiteBox "$TARGET_PROGRAM" >/dev/null 2>&1; then
        if [ -f "$TARGET_BINARY" ]; then
            echo -e "${GREEN}✓ 使用afl-clang-fast编译成功（带插桩）${NC}"
            COMPILE_SUCCESS=1
            COMPILE_METHOD="afl-clang-fast"
        fi
    fi
fi

# 如果afl-clang-fast失败，使用普通编译器
if [ $COMPILE_SUCCESS -eq 0 ]; then
    echo -e "${YELLOW}提示: 使用普通编译器编译（无插桩，仅用于策略对比）...${NC}"
    
    # 尝试gcc
    if command -v gcc &> /dev/null; then
        if gcc -o test-whiteBox "$TARGET_PROGRAM" 2>&1 | head -10; then
            if [ -f "$TARGET_BINARY" ]; then
                echo -e "${GREEN}✓ 使用gcc编译成功（无插桩，仅用于策略对比）${NC}"
                COMPILE_SUCCESS=1
                COMPILE_METHOD="gcc"
            fi
        fi
    fi
    
    # 如果gcc也失败，尝试clang
    if [ $COMPILE_SUCCESS -eq 0 ] && command -v clang &> /dev/null; then
        if clang -o test-whiteBox "$TARGET_PROGRAM" 2>&1 | head -10; then
            if [ -f "$TARGET_BINARY" ]; then
                echo -e "${GREEN}✓ 使用clang编译成功（无插桩，仅用于策略对比）${NC}"
                COMPILE_SUCCESS=1
                COMPILE_METHOD="clang"
            fi
        fi
    fi
fi

if [ $COMPILE_SUCCESS -eq 0 ] || [ ! -f "$TARGET_BINARY" ]; then
    echo -e "${RED}错误: 编译失败，找不到可用的编译器${NC}"
    exit 1
fi

echo ""

# 创建初始测试用例（两个整数，用空格分隔）
echo -e "${GREEN}[2/4] 创建初始测试用例...${NC}"
echo "0 0" > "$TESTCASES_DIR/input1.txt"
echo "1 1" > "$TESTCASES_DIR/input2.txt"
echo "-1 -1" > "$TESTCASES_DIR/input3.txt"
echo "10 20" > "$TESTCASES_DIR/input4.txt"
echo -e "${GREEN}✓ 测试用例创建完成${NC}"

# 验证程序是否能正常运行
echo "  验证程序是否能正常运行..."
if echo "1 2" | "$TARGET_BINARY" >/dev/null 2>&1; then
    echo -e "  ${GREEN}✓ 程序可以正常运行${NC}"
else
    echo -e "  ${YELLOW}警告: 程序可能无法正常运行，但继续测试...${NC}"
fi
echo ""

# 函数：查找 fuzzer_stats 文件
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
    local stats_file=$1
    if [ ! -f "$stats_file" ]; then
        echo "0,0,0,0,0,0,0,0"
        return
    fi
    
    # 提取统计值，如果为空或不存在则使用默认值 0
    local execs=$(grep "^execs_done" "$stats_file" 2>/dev/null | awk '{print $3}' | grep -E '^[0-9]+$' || echo "0")
    local edges=$(grep "^edges_found" "$stats_file" 2>/dev/null | awk '{print $3}' | grep -E '^[0-9]+$' || echo "0")
    local crashes=$(grep "^unique_crashes" "$stats_file" 2>/dev/null | awk '{print $3}' | grep -E '^[0-9]+$' || echo "0")
    local paths=$(grep "^paths_total" "$stats_file" 2>/dev/null | awk '{print $3}' | grep -E '^[0-9]+$' || echo "0")
    local cycles=$(grep "^cycles_done" "$stats_file" 2>/dev/null | awk '{print $3}' | grep -E '^[0-9]+$' || echo "0")
    local exec_time=$(grep "^exec_timeout" "$stats_file" 2>/dev/null | awk '{print $3}' | grep -E '^[0-9]+$' || echo "0")
    local paths_favored=$(grep "^paths_favored" "$stats_file" 2>/dev/null | awk '{print $3}' | grep -E '^[0-9]+$' || echo "0")
    local paths_imported=$(grep "^paths_imported" "$stats_file" 2>/dev/null | awk '{print $3}' | grep -E '^[0-9]+$' || echo "0")
    
    # 确保所有值都是数字，如果不是则设为 0
    execs=${execs:-0}
    edges=${edges:-0}
    crashes=${crashes:-0}
    paths=${paths:-0}
    cycles=${cycles:-0}
    exec_time=${exec_time:-0}
    paths_favored=${paths_favored:-0}
    paths_imported=${paths_imported:-0}
    
    echo "$execs,$edges,$crashes,$paths,$cycles,$exec_time,$paths_favored,$paths_imported"
}

# 函数：运行测试
run_fuzz_test() {
    local strategy_name=$1
    local use_lattice_mab=$2
    local output_dir="$RESULTS_DIR/${strategy_name}"
    
    echo -e "${BLUE}--- 测试${strategy_name}策略 ---${NC}"
    
    # 生成唯一的 fuzzer ID，避免多次运行时的目录冲突
    local fuzzer_id="${strategy_name}_$$_$(date +%s)"
    
    # 清理旧输出（处理 NFS 文件系统的延迟删除问题）
    # 注意：由于使用了唯一的 fuzzer_id，每次运行会在 output_dir 下创建新的子目录
    # 所以理论上不会冲突，但为了保持目录整洁，我们尝试清理旧的子目录
    if [ -d "$output_dir" ]; then
        # 先尝试删除所有子目录（这些是旧的 fuzzer_id 目录）
        # 使用 find 命令逐个删除，避免 NFS 的 "Directory not empty" 错误
        find "$output_dir" -mindepth 1 -maxdepth 1 -type d -exec rm -rf {} + 2>&1 | \
            grep -v "Device or resource busy" | \
            grep -v "Stale file handle" | \
            grep -v "Directory not empty" || true
        
        # 等待 NFS 文件系统完成删除操作
        sleep 1
        
        # 如果目录仍然不为空，尝试再次删除（最多重试 3 次）
        local retry_count=0
        while [ -d "$output_dir" ] && [ $retry_count -lt 3 ]; do
            # 检查目录是否为空（使用 find 更可靠）
            if [ -n "$(find "$output_dir" -mindepth 1 -maxdepth 1 2>/dev/null | head -1)" ]; then
                find "$output_dir" -mindepth 1 -maxdepth 1 -exec rm -rf {} + 2>&1 | \
                    grep -v "Device or resource busy" | \
                    grep -v "Stale file handle" | \
                    grep -v "Directory not empty" || true
                sleep 1
                retry_count=$((retry_count + 1))
            else
                # 目录已为空，退出循环
                break
            fi
        done
        
        # 如果目录为空，尝试删除目录本身（如果失败也没关系，mkdir -p 会处理）
        if [ -d "$output_dir" ] && [ -z "$(find "$output_dir" -mindepth 1 -maxdepth 1 2>/dev/null | head -1)" ]; then
            rmdir "$output_dir" 2>/dev/null || true
        fi
    fi
    mkdir -p "$output_dir"
    
    # 设置环境变量
    if [ "$use_lattice_mab" = "1" ]; then
        export AFL_LATTICE_MAB=1
    else
        unset AFL_LATTICE_MAB
    fi
    
    # 运行 afl-fuzz
    # 注意：test-whiteBox.c 从标准输入读取，AFL++ 会自动将测试用例文件内容传递给标准输入
    echo "  运行时间: ${TEST_TIME}秒..."
    
    # 设置 AFL++ 环境变量
    export AFL_SKIP_CPUFREQ=1
    # 跳过 CPU 绑定检查，避免在虚拟化环境中扫描 /proc 目录时卡住
    export AFL_NO_AFFINITY=1
    # 允许在 core_pattern 配置不理想的情况下运行（用于测试环境）
    export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1
    # 减少输出（可选，如果需要更多调试信息可以注释掉）
    export AFL_QUIET=1
    
    # 运行 afl-fuzz（后台运行）
    # 使用 -S 参数指定唯一的 fuzzer ID，避免多次运行时的目录冲突
    # 设置执行超时为 1000ms（1秒），避免单个测试用例执行时间过长导致卡住
    timeout ${TEST_TIME}s "$AFL_FUZZ" -i "$TESTCASES_DIR" -o "$output_dir" -S "$fuzzer_id" -m none -t 1000 \
        -- "$TARGET_BINARY" > "$RESULTS_DIR/${strategy_name}.log" 2>&1 &
    local timeout_pid=$!
    
    # 等待几秒，检查是否正常启动
    sleep 3
    if ! kill -0 $timeout_pid 2>/dev/null; then
        echo -e "  ${RED}错误: timeout/afl-fuzz 进程已退出${NC}"
        if [ -f "$RESULTS_DIR/${strategy_name}.log" ]; then
            echo "  错误日志:"
            tail -30 "$RESULTS_DIR/${strategy_name}.log" | sed 's/^/    /'
        fi
        return 1
    fi
    
    # 等待timeout进程完成（timeout会自动在指定时间后终止afl-fuzz）
    # 等待时间比测试时间多5秒，给timeout足够时间清理
    local wait_timeout=$((TEST_TIME + 5))
    local waited=0
    while kill -0 $timeout_pid 2>/dev/null && [ $waited -lt $wait_timeout ]; do
        sleep 1
        waited=$((waited + 1))
    done
    
    # 获取退出码
    local exit_code=0
    if kill -0 $timeout_pid 2>/dev/null; then
        # timeout进程仍在运行，可能是afl-fuzz没有及时响应终止信号
        # 先等待一下，给timeout时间清理子进程
        sleep 2
        if kill -0 $timeout_pid 2>/dev/null; then
            # 如果仍然在运行，说明可能卡住了，需要强制终止
            kill -TERM $timeout_pid 2>/dev/null || true
            sleep 1
            if kill -0 $timeout_pid 2>/dev/null; then
                # 如果TERM信号无效，使用KILL
                kill -9 $timeout_pid 2>/dev/null || true
            fi
        fi
        wait $timeout_pid 2>/dev/null || true
        exit_code=124
    else
        # 等待timeout进程结束并获取退出码
        wait $timeout_pid 2>/dev/null
        exit_code=$?
    fi
    
    # 退出码124是timeout命令的正常退出码（表示afl-fuzz被timeout终止），这是正常的
    # 其他非零退出码可能是afl-fuzz本身的错误
    if [ $exit_code -ne 0 ] && [ $exit_code -ne 124 ]; then
        echo -e "  ${YELLOW}警告: afl-fuzz异常退出，退出码: $exit_code${NC}"
        if [ -f "$RESULTS_DIR/${strategy_name}.log" ]; then
            echo "  错误日志:"
            tail -30 "$RESULTS_DIR/${strategy_name}.log" | sed 's/^/    /'
        fi
    fi
    
    # 等待文件写入
    sleep 2
    
    # 提取统计信息
    local stats_file=$(find_fuzzer_stats "$output_dir")
    if [ -n "$stats_file" ] && [ -f "$stats_file" ]; then
        local stats=$(extract_stats "$stats_file")
        echo "  统计文件: $stats_file"
        IFS=',' read -r execs edges crashes paths cycles exec_time paths_favored paths_imported <<< "$stats"
        
        # 确保所有值都有默认值
        execs=${execs:-0}
        edges=${edges:-0}
        crashes=${crashes:-0}
        paths=${paths:-0}
        cycles=${cycles:-0}
        paths_favored=${paths_favored:-0}
        paths_imported=${paths_imported:-0}
        
        echo -e "  ${GREEN}执行次数: $execs${NC}"
        echo -e "  ${GREEN}发现边数: $edges${NC}"
        echo -e "  ${GREEN}崩溃数: $crashes${NC}"
        echo -e "  ${GREEN}路径数: $paths${NC}"
        echo -e "  ${GREEN}循环数: $cycles${NC}"
        echo -e "  ${GREEN}偏好路径: $paths_favored${NC}"
    else
        echo -e "  ${YELLOW}警告: 无法读取统计信息${NC}"
        echo "  输出目录内容:"
        ls -la "$output_dir" 2>/dev/null | head -10 || echo "    目录不存在或为空"
        
        # 检查日志文件中的错误
        if [ -f "$RESULTS_DIR/${strategy_name}.log" ]; then
            echo "  日志文件最后20行:"
            tail -20 "$RESULTS_DIR/${strategy_name}.log" | sed 's/^/    /'
        fi
        
        # 尝试查找任何输出目录
        echo "  搜索所有可能的输出目录:"
        find "$output_dir" -type d 2>/dev/null | head -5 | sed 's/^/    /'
        find "$output_dir" -name "fuzzer_stats" 2>/dev/null | head -5 | sed 's/^/    /'
    fi
    
    echo ""
}

# 运行对比测试
echo -e "${GREEN}[3/4] 运行对比测试（${TEST_TIME}秒/策略）...${NC}"
echo ""

# 测试原策略
run_fuzz_test "original" "0"

# 测试 Lattice-MAB 策略
run_fuzz_test "lattice_mab" "1"

# 生成对比报告
echo -e "${GREEN}[4/4] 生成对比报告...${NC}"

REPORT_FILE="$RESULTS_DIR/comparison_report.txt"
{
    echo "========================================"
    echo "test-whiteBox.c 对比测试报告"
    echo "生成时间: $(date)"
    echo "========================================"
    echo ""
    echo "测试配置:"
    echo "  目标程序: test-whiteBox.c"
    echo "  编译方式: $COMPILE_METHOD"
    echo "  测试时间: ${TEST_TIME}秒/策略"
    echo ""
    echo "----------------------------------------"
    echo "原策略结果:"
    echo "----------------------------------------"
    original_stats_file=$(find_fuzzer_stats "$RESULTS_DIR/original")
    if [ -n "$original_stats_file" ] && [ -f "$original_stats_file" ]; then
        original_stats=$(extract_stats "$original_stats_file")
        IFS=',' read -r execs edges crashes paths cycles exec_time paths_favored paths_imported <<< "$original_stats"
        
        # 确保所有值都有默认值
        execs=${execs:-0}
        edges=${edges:-0}
        crashes=${crashes:-0}
        paths=${paths:-0}
        cycles=${cycles:-0}
        paths_favored=${paths_favored:-0}
        paths_imported=${paths_imported:-0}
        
        echo "  执行次数: $execs"
        echo "  发现边数: $edges"
        echo "  崩溃数: $crashes"
        echo "  路径数: $paths"
        echo "  循环数: $cycles"
        echo "  偏好路径: $paths_favored"
        echo "  导入路径: $paths_imported"
    else
        echo "  无法读取统计信息"
    fi
    echo ""
    echo "----------------------------------------"
    echo "Lattice-MAB 策略结果:"
    echo "----------------------------------------"
    lattice_stats_file=$(find_fuzzer_stats "$RESULTS_DIR/lattice_mab")
    if [ -n "$lattice_stats_file" ] && [ -f "$lattice_stats_file" ]; then
        lattice_stats=$(extract_stats "$lattice_stats_file")
        IFS=',' read -r execs edges crashes paths cycles exec_time paths_favored paths_imported <<< "$lattice_stats"
        
        # 确保所有值都有默认值
        execs=${execs:-0}
        edges=${edges:-0}
        crashes=${crashes:-0}
        paths=${paths:-0}
        cycles=${cycles:-0}
        paths_favored=${paths_favored:-0}
        paths_imported=${paths_imported:-0}
        
        echo "  执行次数: $execs"
        echo "  发现边数: $edges"
        echo "  崩溃数: $crashes"
        echo "  路径数: $paths"
        echo "  循环数: $cycles"
        echo "  偏好路径: $paths_favored"
        echo "  导入路径: $paths_imported"
    else
        echo "  无法读取统计信息"
    fi
    echo ""
    echo "----------------------------------------"
    echo "对比分析:"
    echo "----------------------------------------"
    
    # 计算改进百分比
    if [ -n "$original_stats_file" ] && [ -f "$original_stats_file" ] && \
       [ -n "$lattice_stats_file" ] && [ -f "$lattice_stats_file" ]; then
        original_stats=$(extract_stats "$original_stats_file")
        lattice_stats=$(extract_stats "$lattice_stats_file")
        
        IFS=',' read -r orig_execs orig_edges orig_crashes orig_paths orig_cycles orig_time orig_favored orig_imported <<< "$original_stats"
        IFS=',' read -r latt_execs latt_edges latt_crashes latt_paths latt_cycles latt_time latt_favored latt_imported <<< "$lattice_stats"
        
        # 确保所有值都是数字，如果不是则设为 0
        orig_execs=${orig_execs:-0}
        orig_edges=${orig_edges:-0}
        orig_paths=${orig_paths:-0}
        latt_execs=${latt_execs:-0}
        latt_edges=${latt_edges:-0}
        latt_paths=${latt_paths:-0}
        
        # 计算改进百分比（使用 awk 避免 bc 依赖）
        if [ "$orig_execs" -gt 0 ] 2>/dev/null; then
            execs_improvement=$(awk "BEGIN {printf \"%.2f\", ($latt_execs - $orig_execs) * 100 / $orig_execs}" 2>/dev/null || echo "0.00")
            echo "  执行次数改进: ${execs_improvement}%"
        fi
        
        if [ "$orig_edges" -gt 0 ] 2>/dev/null; then
            edges_improvement=$(awk "BEGIN {printf \"%.2f\", ($latt_edges - $orig_edges) * 100 / $orig_edges}" 2>/dev/null || echo "0.00")
            echo "  边覆盖率改进: ${edges_improvement}%"
        fi
        
        if [ "$orig_paths" -gt 0 ] 2>/dev/null; then
            paths_improvement=$(awk "BEGIN {printf \"%.2f\", ($latt_paths - $orig_paths) * 100 / $orig_paths}" 2>/dev/null || echo "0.00")
            echo "  路径数改进: ${paths_improvement}%"
        fi
        
        # 计算循环数改进
        orig_cycles=${orig_cycles:-0}
        latt_cycles=${latt_cycles:-0}
        if [ "$orig_cycles" -gt 0 ] 2>/dev/null; then
            cycles_improvement=$(awk "BEGIN {printf \"%.2f\", ($latt_cycles - $orig_cycles) * 100 / $orig_cycles}" 2>/dev/null || echo "0.00")
            echo "  循环数改进: ${cycles_improvement}%"
        fi
    fi
    
    echo ""
    echo "详细日志:"
    echo "  原策略: $RESULTS_DIR/original.log"
    echo "  Lattice-MAB: $RESULTS_DIR/lattice_mab.log"
    echo ""
    echo "输出目录:"
    echo "  原策略: $RESULTS_DIR/original"
    echo "  Lattice-MAB: $RESULTS_DIR/lattice_mab"
    echo ""
} > "$REPORT_FILE"

cat "$REPORT_FILE"
echo ""
echo -e "${GREEN}✓ 对比测试完成！${NC}"
echo ""
echo "报告已保存到: $REPORT_FILE"
echo "结果目录: $RESULTS_DIR"

