# Lattice-MAB 策略优化说明

## 问题分析

根据测试结果，原始的 Lattice-MAB 策略存在以下问题：

1. **执行次数增加 7.36%**：Lattice-MAB (91305) > 原策略 (85049)
2. **循环数增加 24.66%**：Lattice-MAB (91) > 原策略 (73)
3. **资源消耗更高**：虽然覆盖率相同（都是 17 条边），但消耗了更多计算资源

## 根本原因

1. **奖励函数只考虑覆盖率，不考虑效率**
   - 原奖励函数：`reward = new_edges * 10.0 + new_paths * 5.0`
   - 没有惩罚低效率的变异操作
   - 没有考虑"单位执行次数的覆盖率"

2. **探索参数过高**
   - `MAB_ALPHA = 0.1`：UCB 探索参数过高，导致过度探索
   - `MAB_EPSILON = 0.1`：10% 的时间在随机探索
   - 邻居探索概率 30%：过于激进

3. **缺少效率惩罚机制**
   - 没有惩罚执行次数多但覆盖率低的策略
   - 没有考虑变异操作的执行成本

## 优化方案

### 1. 降低探索参数

```c
// 优化前
#define MAB_ALPHA 0.1         /* UCB exploration parameter */
#define MAB_EPSILON 0.1      /* Epsilon-greedy parameter */

// 优化后
#define MAB_ALPHA 0.03        /* UCB exploration parameter (reduced for efficiency) */
#define MAB_EPSILON 0.05      /* Epsilon-greedy parameter (reduced for efficiency) */
```

**效果**：减少不必要的探索，更多时间用于利用已知的高效策略。

### 2. 改进奖励函数，加入效率因子

```c
// 新增效率奖励
double efficiency = (double)(new_edges + new_paths) / (double)(vec->usage_count + 1);
reward += efficiency * 5.0;

// 新增效率惩罚
if (vec->usage_count > 100 && (new_edges + new_paths) == 0) {
    reward -= EFFICIENCY_PENALTY_FACTOR * (double)vec->usage_count / 1000.0;
}
```

**效果**：
- 奖励那些用更少执行次数获得相同覆盖率的策略
- 惩罚那些执行次数多但覆盖率低的策略

### 3. 减少邻居探索概率

```c
// 优化前
if (rand_below(afl, 100) < 30) {  /* 30% chance */

// 优化后
if (rand_below(afl, 100) < NEIGHBOR_EXPLORE_PROB) {  /* 10% chance */
#define NEIGHBOR_EXPLORE_PROB 10
```

**效果**：减少不必要的邻居探索，降低资源消耗。

### 4. 提高邻居选择阈值

```c
// 优化前
if (neighbor_reward > best_reward * 0.9) {  /* 只要 90% 就考虑 */

// 优化后
if (neighbor_reward > best_reward * 1.1) {  /* 必须 10% 更好才考虑 */
```

**效果**：只在邻居明显更好时才选择，避免频繁切换。

### 5. 添加 UCB 效率因子

```c
// 新增效率因子
double efficiency_factor = 1.0;
if (arm->pull_count > 50 && arm->avg_reward < 0.01) {
    efficiency_factor = 0.8;  /* 减少 UCB 20% */
}
arm->ucb_value = (arm->avg_reward + exploration) * efficiency_factor;
```

**效果**：降低那些尝试多次但收益低的策略的 UCB 值，减少选择概率。

## 预期效果

优化后的策略应该：

1. **执行次数减少**：通过减少探索和惩罚低效策略，总执行次数应该降低
2. **循环数减少**：更高效的策略选择应该减少不必要的循环
3. **保持覆盖率**：在减少资源消耗的同时，保持相同的代码覆盖率
4. **提高效率**：单位执行次数的覆盖率应该提高

## 测试建议

重新编译并测试：

```bash
cd ~/code/AFLplusplus-dev

# 重新编译
make clean
make

# 运行对比测试
./test_whitebox.sh
```

## 进一步优化方向

如果优化后仍然不够理想，可以考虑：

1. **动态调整探索参数**：根据测试进度动态调整 `MAB_ALPHA` 和 `MAB_EPSILON`
2. **执行时间感知**：在奖励函数中加入执行时间因子
3. **覆盖率密度**：考虑"覆盖率密度"（覆盖率/执行次数）作为主要指标
4. **自适应阈值**：根据历史性能动态调整邻居选择阈值

## 配置参数说明

| 参数 | 原值 | 优化值 | 说明 |
|------|------|--------|------|
| `MAB_ALPHA` | 0.1 | 0.03 | UCB 探索参数，越小越偏向利用 |
| `MAB_EPSILON` | 0.1 | 0.05 | Epsilon-greedy 探索概率 |
| `NEIGHBOR_EXPLORE_PROB` | 30% | 10% | 邻居探索概率 |
| 邻居选择阈值 | 0.9 | 1.1 | 邻居必须明显更好才选择 |
| 效率惩罚阈值 | 无 | 100次 | 超过100次无收益则惩罚 |

## 注意事项

1. **平衡探索与利用**：过度减少探索可能导致错过更好的策略
2. **测试环境差异**：不同目标程序可能需要不同的参数设置
3. **长期效果**：短期测试可能无法完全体现优化效果，建议进行更长时间的测试

