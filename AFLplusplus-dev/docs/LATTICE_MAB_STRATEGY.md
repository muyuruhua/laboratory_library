# AFL++ 基于格理论和多臂老虎机的变异操作选择策略

## 原有框架的变异操作选择策略原理

### 1. 基于数组的加权随机选择

AFLplusplus使用预定义的策略数组来实现变异操作的选择：

- **策略数组**：定义了多个数组，如`mutation_strategy_exploration_binary`、`mutation_strategy_exploitation_binary`等
- **数组大小**：每个数组大小为256（`MUT_STRATEGY_ARRAY_SIZE`）
- **加权机制**：通过重复某些操作类型来实现加权。例如，如果`MUT_FLIPBIT`在数组中出现8次，而`MUT_INTERESTING8`只出现4次，那么`FLIPBIT`被选中的概率是`INTERESTING8`的两倍

### 2. 根据输入类型和模式选择策略

策略选择依赖于两个关键因素：

- **`afl->input_mode`**：
  - `1` = TEXT（文本输入）
  - `2` = BINARY（二进制输入）
  - `0` = DEFAULT/GENERIC（默认/通用）

- **`afl->fuzz_mode`**：
  - `0` = exploration（探索模式，关注新覆盖）
  - `1` = exploitation（利用模式，关注崩溃）

根据这两个因素的组合，选择不同的策略数组：
- 文本输入 + 探索模式 → `binary_array`
- 文本输入 + 利用模式 → `text_array`
- 二进制输入 + 探索模式 → `mutation_strategy_exploration_binary`
- 二进制输入 + 利用模式 → `mutation_strategy_exploitation_binary`

### 3. 随机选择机制

在havoc阶段，通过以下方式选择变异操作：

```c
u32 r = rand_below(afl, rand_max);
u32 selected_mutation = mutation_array[r];
```

然后根据`selected_mutation`的值执行相应的变异操作。

### 4. MOpt（多目标优化）

AFLplusplus还包含MOpt模式，使用概率分布来选择算法，但这主要用于特定的优化场景。

## 新的优化策略：基于格理论和MAB

### 1. 变异向量（Mutation Vectors）

每个变异操作被形式化为一个高维向量：

- **维度**：37维（对应`MUT_MAX`个变异类型）
- **表示**：使用one-hot编码，每个向量在对应维度上为1，其他为0
- **属性**：
  - `mutation_type`：变异类型枚举值
  - `dimension[]`：向量表示
  - `position[]`：在格空间中的位置
  - `magnitude`：向量大小
  - `usage_count`：使用次数
  - `avg_reward`：平均奖励

### 2. 格理论（Lattice Theory）

策略空间被建模为离散格：

- **格结构**：包含所有变异向量的集合
- **邻接矩阵**：定义向量之间的邻接关系（距离小于阈值的向量互为邻居）
- **几何性质**：
  - **向量分布**：分析向量在空间中的分布密度
  - **正交性**：检查向量之间的正交关系
  - **最近邻**：找到与当前向量最近的邻居向量

### 3. 多臂老虎机（MAB）

使用MAB算法进行智能策略选择：

- **策略类型**：
  - **UCB（Upper Confidence Bound）**：平衡探索和利用
  - **Epsilon-Greedy**：以ε概率探索，以1-ε概率利用
  - **Thompson Sampling**：基于贝叶斯推理的选择

- **奖励机制**：
  - 基于覆盖增益计算奖励
  - 新边（new edges）：权重10
  - 新路径（new paths）：权重5
  - 崩溃发现：额外奖励100

- **更新机制**：
  - 每次执行后更新对应臂的奖励
  - 维护滑动窗口的平均奖励
  - 动态调整探索率

### 4. 集成策略

结合格理论和MAB：

1. **MAB选择基础变异**：使用MAB算法选择基础变异操作
2. **格理论优化**：在格空间中查找最近邻，考虑具有相似或更好奖励的邻居
3. **自适应选择**：根据历史性能动态调整选择概率

## 使用方法

### 启用新策略

默认情况下，新策略是启用的。可以通过环境变量控制：

```bash
# 启用（默认）
export AFL_LATTICE_MAB=1

# 禁用（使用原有策略）
export AFL_LATTICE_MAB=0
```

### 对比测试

#### 使用test-instr.c进行对比测试（推荐）

我们提供了专门的测试脚本来使用`test-instr.c`进行对比测试：

```bash
# 快速测试（30秒，用于验证功能）
./quick_test.sh

# 完整对比测试（默认5分钟，3次运行）
./test_lattice_mab.sh

# 自定义测试时间和运行次数
TEST_TIME=600 NUM_RUNS=5 ./test_lattice_mab.sh
```

测试脚本会自动：
1. 编译`test-instr.c`
2. 创建初始测试用例
3. 交替运行原有策略和新策略
4. 收集统计数据
5. 生成详细的对比报告

详细使用说明请参考：`TEST_LATTICE_MAB_README.md`

#### 使用自定义目标进行对比测试

运行通用对比测试脚本（见`compare_strategies.sh`）来比较新旧策略的性能：

```bash
TARGET_BINARY=/path/to/target \
INPUT_DIR=/path/to/testcases \
TEST_TIME=3600 \
./compare_strategies.sh
```

## 性能指标

对比测试会收集以下指标：

1. **覆盖指标**：
   - 发现的边数（edges）
   - 发现的路径数（paths）
   - 覆盖率增长速率

2. **效率指标**：
   - 执行次数
   - 平均执行时间
   - 变异操作分布

3. **效果指标**：
   - 发现的崩溃数
   - 发现的超时数
   - 唯一崩溃数

4. **策略指标**：
   - Lattice-MAB选择次数
   - 原始策略选择次数
   - 平均奖励值

## 实现细节

### 文件结构

- `include/afl-lattice-mab.h`：头文件，定义数据结构和函数接口
- `src/afl-lattice-mab.c`：实现文件，包含所有核心算法
- `src/afl-fuzz-one.c`：修改了havoc阶段的变异选择逻辑
- `src/afl-fuzz-bitmap.c`：添加了反馈更新机制
- `src/afl-fuzz-state.c`：添加了初始化代码

### 关键函数

- `lattice_mab_init()`：初始化lattice-MAB系统
- `lattice_mab_select_mutation()`：选择变异操作
- `lattice_mab_update()`：更新反馈
- `mab_select_mutation()`：MAB选择
- `find_lattice_neighbors()`：查找最近邻

## 未来改进

1. **更精细的奖励函数**：考虑更多因素（执行时间、路径深度等）
2. **动态格结构**：根据反馈动态调整格结构
3. **多目标优化**：同时优化覆盖和崩溃发现
4. **机器学习集成**：使用ML模型预测变异效果

