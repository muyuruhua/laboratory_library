# Lattice-MAB策略对比测试使用说明

## 概述

本测试脚本使用`test-instr.c`作为测试目标，对比原有变异策略和基于格理论+多臂老虎机的新策略的性能差异。

## 前置要求

1. **已编译的AFL++**
   - 确保`afl-clang-fast`和`afl-fuzz`在PATH中
   - 如果还未编译，请先编译AFL++：
     ```bash
     cd AFLplusplus-dev
     make
     sudo make install  # 可选，安装到系统路径
     ```

2. **Python 3**
   - 用于生成对比报告

3. **足够的磁盘空间**
   - 测试结果会保存在`comparison_results_*`目录中

## 快速开始

### 基本使用

```bash
cd AFLplusplus-dev
./test_lattice_mab.sh
```

### 自定义配置

可以通过环境变量自定义测试参数：

```bash
# 设置测试时间（秒），默认300秒（5分钟）
export TEST_TIME=600

# 设置运行次数，默认3次
export NUM_RUNS=5

# 运行测试
./test_lattice_mab.sh
```

### 完整示例

```bash
# 运行10分钟测试，每个策略运行5次
TEST_TIME=600 NUM_RUNS=5 ./test_lattice_mab.sh
```

## 测试流程

脚本会自动执行以下步骤：

1. **编译test-instr.c**
   - 使用`afl-clang-fast`编译测试程序
   - 编译输出保存在`build_test/`目录

2. **创建初始测试用例**
   - 自动创建4个初始测试用例（包含'0', '1', '2', 'A'）

3. **运行对比测试**
   - 交替运行原有策略和新策略
   - 每次运行独立的模糊测试会话

4. **收集统计数据**
   - 从每次运行的`fuzzer_stats`文件中提取关键指标

5. **生成对比报告**
   - 自动生成详细的对比报告
   - 包含平均值、中位数、标准差等统计信息

## 输出结果

测试完成后，会在`comparison_results_YYYYMMDD_HHMMSS/`目录中生成：

### 文件结构

```
comparison_results_20250101_120000/
├── results.csv                    # CSV格式的原始数据
├── report.txt                     # 纯文本格式的对比报告
├── report_formatted.txt           # 带格式的对比报告
├── build.log                      # 编译日志
├── original_run1/                 # 原有策略第1次运行结果
│   ├── fuzzer_stats              # 统计信息
│   ├── fuzzer.log                # 运行日志
│   └── queue/                    # 发现的测试用例
├── lattice_mab_run1/              # 新策略第1次运行结果
│   ├── fuzzer_stats
│   ├── fuzzer.log
│   └── queue/
├── original_run2/                 # 原有策略第2次运行结果
│   └── ...
└── ...
```

### 关键指标说明

报告会对比以下指标：

- **执行次数 (execs)**: 总共执行的测试用例数量
- **发现的边数 (edges)**: 代码覆盖中发现的边数
- **崩溃数 (crashes)**: 发现的唯一崩溃数量
- **路径总数 (paths)**: 发现的唯一执行路径数量
- **周期数 (cycles)**: 完成的模糊测试周期数
- **优先路径数 (paths_favored)**: 被标记为优先的路径数
- **导入路径数 (paths_imported)**: 从其他模糊器导入的路径数

## 解读结果

### 改进百分比

报告中的"改进"列显示新策略相对于原有策略的性能变化：

- **正值（绿色）**: 新策略表现更好
- **负值（红色）**: 新策略表现较差
- **接近0**: 两种策略性能相近

### 统计指标

- **平均**: 所有运行的平均值
- **中位数**: 所有运行的中位数（更抗异常值）
- **标准差**: 数据离散程度（越小越稳定）
- **范围**: 最小值和最大值

## 注意事项

1. **测试时间**: 
   - 较短的测试时间（<5分钟）可能无法充分展示策略差异
   - 建议至少运行5-10分钟以获得有意义的结果

2. **运行次数**:
   - 至少运行3次以获得统计意义
   - 更多运行次数可以提高结果的可信度

3. **系统负载**:
   - 测试会占用CPU资源
   - 建议在系统空闲时运行

4. **磁盘空间**:
   - 每次运行会产生测试用例和日志
   - 确保有足够的磁盘空间（建议至少1GB）

## 故障排除

### 编译失败

如果编译失败，检查：
- AFL++是否正确编译
- `afl-clang-fast`是否在PATH中
- 是否有编译错误（查看`build.log`）

### 运行失败

如果测试运行失败，检查：
- `afl-fuzz`是否在PATH中
- 是否有足够的磁盘空间
- 系统资源是否充足

### 结果异常

如果结果看起来异常：
- 检查`fuzzer.log`中的错误信息
- 确认测试时间是否足够
- 尝试增加运行次数

## 高级用法

### 只运行一次快速测试

```bash
TEST_TIME=60 NUM_RUNS=1 ./test_lattice_mab.sh
```

### 长时间稳定性测试

```bash
TEST_TIME=3600 NUM_RUNS=10 ./test_lattice_mab.sh
```

### 分析特定运行的结果

```bash
# 查看某次运行的统计信息
cat comparison_results_*/original_run1/fuzzer_stats

# 查看发现的测试用例
ls comparison_results_*/lattice_mab_run1/queue/
```

## 贡献

如果发现测试脚本的问题或有改进建议，欢迎提交Issue或Pull Request。

