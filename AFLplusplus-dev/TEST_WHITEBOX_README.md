# test-whiteBox.c 对比测试说明

本文档说明如何使用 `test_whitebox.sh` 脚本对 `test-whiteBox.c` 进行原策略和 Lattice-MAB 策略的对比测试。

## 目标程序说明

`test-whiteBox.c` 是一个白盒测试程序，具有以下特点：

- **输入格式**: 从标准输入读取两个整数（用空格分隔）
- **功能**: 根据输入的两个整数的正负性和奇偶性，输出不同的情况
- **分支数量**: 包含多个条件分支，适合用于测试代码覆盖率

### 程序逻辑

1. **正负性判断**（4种情况）:
   - X > 0 && Y > 0
   - X > 0 && Y <= 0
   - X <= 0 && Y > 0
   - X <= 0 && Y <= 0

2. **奇偶性判断**（4种情况）:
   - X 和 Y 都是偶数
   - X 是偶数，Y 是奇数
   - X 是奇数，Y 是偶数
   - X 和 Y 都是奇数

## 使用方法

### 基本用法

```bash
cd ~/code/AFLplusplus-dev
./test_whitebox.sh
```

### 自定义测试时间

默认测试时间为 60 秒。可以通过环境变量自定义：

```bash
TEST_TIME=120 ./test_whitebox.sh  # 每个策略测试 120 秒
```

### 完整示例

```bash
# 1. 进入 AFL++ 目录
cd ~/code/AFLplusplus-dev

# 2. 确保脚本有执行权限
chmod +x test_whitebox.sh

# 3. 运行测试（默认 60 秒）
./test_whitebox.sh

# 或者指定更长的测试时间
TEST_TIME=300 ./test_whitebox.sh  # 5 分钟
```

## 测试流程

脚本会自动执行以下步骤：

1. **编译 test-whiteBox.c**
   - 优先尝试使用 `afl-clang-fast`（带插桩）
   - 如果失败，自动使用 `gcc` 或 `clang`（无插桩，仅用于策略对比）

2. **创建初始测试用例**
   - `0 0`
   - `1 1`
   - `-1 -1`
   - `10 20`

3. **运行对比测试**
   - 先运行原策略（`AFL_LATTICE_MAB=0`）
   - 再运行 Lattice-MAB 策略（`AFL_LATTICE_MAB=1`）

4. **生成对比报告**
   - 统计信息对比
   - 性能改进分析

## 输出说明

### 测试输出

测试过程中会显示：

```
========================================
test-whiteBox.c 对比测试
原策略 vs Lattice-MAB 策略
========================================

[1/4] 编译 test-whiteBox.c...
✓ 使用afl-clang-fast编译成功（带插桩）

[2/4] 创建初始测试用例...
✓ 测试用例创建完成

[3/4] 运行对比测试（60秒/策略）...

--- 测试original策略 ---
  运行时间: 60秒...
  执行次数: 12345
  发现边数: 8
  崩溃数: 0
  路径数: 12
  ...

--- 测试lattice_mab策略 ---
  运行时间: 60秒...
  执行次数: 13567
  发现边数: 8
  崩溃数: 0
  路径数: 15
  ...

[4/4] 生成对比报告...
```

### 结果文件

测试完成后，会在 `build_whitebox_test/results/` 目录下生成：

- **`comparison_report.txt`**: 详细的对比报告
- **`original.log`**: 原策略的运行日志
- **`lattice_mab.log`**: Lattice-MAB 策略的运行日志
- **`original/`**: 原策略的输出目录（包含 `fuzzer_stats`、测试用例等）
- **`lattice_mab/`**: Lattice-MAB 策略的输出目录

### 查看报告

```bash
# 查看对比报告
cat build_whitebox_test/results/comparison_report.txt

# 查看详细统计信息
cat build_whitebox_test/results/original/default/fuzzer_stats
cat build_whitebox_test/results/lattice_mab/default/fuzzer_stats
```

## 关键指标说明

### 执行次数 (execs_done)
- 在测试时间内执行的测试用例总数
- **更高 = 更好**：表示模糊测试速度更快

### 发现边数 (edges_found)
- 发现的代码覆盖率（边数）
- **更高 = 更好**：表示代码覆盖率更高

### 路径数 (paths_total)
- 发现的唯一执行路径数
- **更高 = 更好**：表示发现了更多的程序行为

### 崩溃数 (unique_crashes)
- 发现的唯一崩溃数
- 对于 `test-whiteBox.c`，正常情况下应该为 0

### 循环数 (cycles_done)
- 完成的模糊测试循环数
- **更高 = 更好**：表示测试更深入

## 故障排除

### 问题 1: 编译失败

**症状**: 显示 "错误: 编译失败"

**解决方法**:
```bash
# 检查编译器是否可用
which gcc
which clang

# 手动编译测试
gcc -o test-whiteBox test-whiteBox.c
./test-whiteBox
# 输入: 1 2
```

### 问题 2: 无法读取统计信息

**症状**: 显示 "警告: 无法读取统计信息"

**解决方法**:
```bash
# 检查输出目录
ls -la build_whitebox_test/results/original/

# 查找 fuzzer_stats 文件
find build_whitebox_test/results -name "fuzzer_stats"

# 检查日志文件
cat build_whitebox_test/results/original.log
```

### 问题 3: AFL++ 工具未找到

**症状**: 显示 "错误: 找不到afl-fuzz"

**解决方法**:
```bash
# 确保已编译 AFL++
cd ~/code/AFLplusplus-dev
make

# 或者安装到系统
sudo make install

# 检查工具
which afl-fuzz
which afl-clang-fast
```

### 问题 4: 测试用例格式问题

**症状**: 程序无法正确读取输入

**解决方法**:
- 确保测试用例文件包含两个整数，用空格分隔
- 例如: `1 2`、`-10 20`、`0 0`

## 性能对比分析

测试完成后，报告会显示性能改进百分比：

- **执行次数改进**: Lattice-MAB 策略相比原策略的执行次数变化
- **边覆盖率改进**: 代码覆盖率的变化
- **路径数改进**: 发现的路径数变化

### 示例报告片段

```
对比分析:
  执行次数改进: 9.90%
  边覆盖率改进: 0.00%
  路径数改进: 25.00%
```

## 注意事项

1. **插桩 vs 无插桩**: 
   - 如果使用 `afl-clang-fast` 编译，程序会有插桩，可以获得准确的代码覆盖率
   - 如果使用普通编译器，程序无插桩，只能对比策略选择行为，无法获得准确的覆盖率

2. **测试时间**: 
   - 较短的测试时间（如 30 秒）可能无法充分展示策略差异
   - 建议至少测试 60 秒以上，以获得更可靠的结果

3. **系统资源**: 
   - 模糊测试会消耗 CPU 资源
   - 确保系统有足够的资源运行测试

## 相关文件

- `test-whiteBox.c`: 目标测试程序
- `test_whitebox.sh`: 对比测试脚本
- `build_whitebox_test/`: 测试输出目录
- `docs/LATTICE_MAB_STRATEGY.md`: Lattice-MAB 策略详细说明

