# 原策略 vs Lattice-MAB 策略：变异选择机制对比

## 概述

AFL++ 在 `havoc` 阶段需要选择变异操作类型。本文档详细对比了**原策略**和**Lattice-MAB 策略**的变异选择机制。

---

## 一、原策略（Original Strategy）

### 1.1 选择机制

**完全随机选择**，基于预定义的变异操作数组。

### 1.2 代码实现

```2232:2262:AFLplusplus-dev/src/afl-fuzz-one.c
    retry_havoc_step: {

      u32 r, item;
      u32 selected_mutation;
      
      /* Try to use lattice-MAB strategy if enabled */
      if (afl->lattice_mab_ctx && afl->lattice_mab_ctx->enabled) {
        
        selected_mutation = lattice_mab_select_mutation(afl->lattice_mab_ctx, afl,
                                                       afl->input_mode, afl->fuzz_mode);
        
        /* If lattice-MAB returned 0, fallback to original strategy */
        if (selected_mutation == 0 || selected_mutation >= MUT_MAX) {
          
          r = rand_below(afl, rand_max);
          selected_mutation = mutation_array[r];
          
        } else {
          
          /* Use lattice-MAB selected mutation */
          r = selected_mutation;  /* For compatibility with switch statement */
          
        }
        
      } else {
        
        /* Original strategy */
        r = rand_below(afl, rand_max);
        selected_mutation = mutation_array[r];
        
      }
```

### 1.3 选择流程

1. **根据输入类型选择变异数组**：
   - **文本模式 (TEXT)**: `text_array` 或 `binary_array`
   - **二进制模式 (BINARY)**: `mutation_strategy_exploration_binary` 或 `mutation_strategy_exploitation_binary`
   - **默认模式 (DEFAULT)**: `binary_array` 或 `text_array`

2. **随机选择**：
   ```c
   r = rand_below(afl, rand_max);  // 随机生成索引
   selected_mutation = mutation_array[r];  // 从数组中随机选择
   ```

3. **特点**：
   - ✅ **简单直接**：无需额外计算
   - ✅ **公平性**：所有变异操作被选中的概率相等
   - ❌ **无学习能力**：不考虑历史性能
   - ❌ **效率低**：可能频繁选择低效的变异操作

### 1.4 变异操作类型

共有 **37 种变异操作**（MUT_MAX = 37）：

- `MUT_FLIPBIT` (0): 翻转单个位
- `MUT_INTERESTING8` (1): 设置为有趣的 8 位值
- `MUT_INTERESTING16` (2): 设置为有趣的 16 位值（小端）
- `MUT_ARITH8` (7): 随机增加字节值
- `MUT_ARITH16` (10): 随机增加 16 位值
- `MUT_RAND8` (16): 随机设置字节值
- `MUT_CLONE_COPY` (17): 克隆字节块
- `MUT_DEL` (25): 删除字节
- `MUT_INSERTONE` (28): 插入字节
- ... 等等

---

## 二、Lattice-MAB 策略

### 2.1 选择机制

**基于多臂老虎机（MAB）和格理论（Lattice Theory）的智能选择**，根据历史性能和效率动态调整选择概率。

### 2.2 代码实现

```619:728:AFLplusplus-dev/src/afl-lattice-mab.c
/* Select mutation using lattice properties + MAB */
u32 lattice_mab_select_mutation(lattice_mab_context_t *ctx, afl_state_t *afl,
                               u32 input_mode, u32 fuzz_mode) {

  (void)input_mode;  /* Suppress unused parameter warning */
  (void)fuzz_mode;   /* Suppress unused parameter warning */
  if (!ctx || !afl || !ctx->enabled) {
    
    /* Fallback to original strategy */
    if (ctx) { ctx->original_selections++; }
    return 0;  /* Will trigger original selection */
    
  }
  
  ctx->lattice_selections++;
  ctx->total_mutations++;
  
  /* Use MAB to select base mutation */
  u32 base_mutation = mab_select_mutation(&ctx->mab, afl);
  
  /* Apply lattice-based refinement (only if we have enough data) */
  mutation_vector_t *selected_vec = &ctx->lattice.vectors[base_mutation];
  
  /* Skip neighbor exploration if base mutation is efficient (to save computation) */
  u32 best_mutation = base_mutation;
  double best_reward = ctx->mab.arms[base_mutation].avg_reward;
  
  /* Explore neighbors more frequently to find better mutations */
  bool should_explore_neighbors = false;
  if (selected_vec->usage_count > 0) {
    
    double base_efficiency = best_reward / (double)(selected_vec->usage_count + 1);
    /* Explore if base is inefficient OR if we haven't explored much */
    if (base_efficiency < MIN_EFFICIENCY_RATIO * 5 || selected_vec->usage_count < 20) {
      
      should_explore_neighbors = true;  /* Explore more frequently */
      
    }
    
  } else {
    
    should_explore_neighbors = true;  /* Always explore if base is unexplored */
    
  }
  
  if (should_explore_neighbors) {
    
    /* Find nearest neighbors */
    u32 neighbors[LATTICE_NEIGHBOR_RADIUS];
    u32 neighbor_count = 0;
    find_lattice_neighbors(&ctx->lattice, selected_vec, neighbors,
                          LATTICE_NEIGHBOR_RADIUS, &neighbor_count);
    
    /* Consider neighbors if they have better rewards AND efficiency */
    for (u32 i = 0; i < neighbor_count; ++i) {
      
      u32 neighbor_idx = neighbors[i];
      if (neighbor_idx < ctx->mab.arm_count) {
        
        double neighbor_reward = ctx->mab.arms[neighbor_idx].avg_reward;
        
        /* Calculate neighbor efficiency for comparison */
        mutation_vector_t *neighbor_vec = &ctx->lattice.vectors[neighbor_idx];
        double neighbor_efficiency = 0.0;
        if (neighbor_vec->usage_count > 0) {
          
          neighbor_efficiency = neighbor_reward / (double)(neighbor_vec->usage_count + 1);
          
        }
        
        double base_efficiency = 0.0;
        if (selected_vec->usage_count > 0) {
          
          base_efficiency = best_reward / (double)(selected_vec->usage_count + 1);
          
        }
        
        /* Prefer neighbors with better rewards or efficiency */
        if (neighbor_reward > best_reward * 1.1 || 
            (neighbor_efficiency > base_efficiency * 1.05 && neighbor_reward >= best_reward * 0.9)) {
          
          /* Increased exploration probability */
          if (rand_below(afl, 100) < NEIGHBOR_EXPLORE_PROB) {
            
            best_mutation = neighbor_idx;
            best_reward = neighbor_reward;
            
          }
          
        } else if (neighbor_reward > best_reward * 1.3 || 
                   (neighbor_efficiency > base_efficiency * 1.15 && neighbor_reward >= best_reward)) {
          
          /* For significant improvements, always switch */
          best_mutation = neighbor_idx;
          best_reward = neighbor_reward;
          
        }
        
      }
      
    }
    
  }
  
  /* Ensure valid mutation type */
  if (best_mutation >= MUT_MAX) { best_mutation = MUT_FLIPBIT; }
  
  return best_mutation;
  
}
```

### 2.3 选择流程（两阶段）

#### 阶段 1：MAB 基础选择

使用 **UCB (Upper Confidence Bound)** 算法选择基础变异操作：

```342:421:AFLplusplus-dev/src/afl-lattice-mab.c
/* Select mutation using MAB */
u32 mab_select_mutation(mab_selector_t *mab, afl_state_t *afl) {

  if (!mab || !afl) { return MUT_FLIPBIT; }
  
  mab->total_pulls++;
  
  u32 selected_arm = 0;
  
  switch (mab->strategy_type) {
    
    case 0: {  /* UCB (Upper Confidence Bound) with efficiency weighting and direct filtering */
      
      double max_ucb = -1e10;
      for (u32 i = 0; i < mab->arm_count && i < MUT_MAX; ++i) {
        
        /* Direct efficiency filtering: skip arms with very low efficiency */
        if (mab->arms[i].pull_count > EFFICIENCY_THRESHOLD) {
          
          double efficiency = mab->arms[i].avg_reward / (double)mab->arms[i].pull_count;
          
          /* Skip arms with efficiency below minimum threshold */
          if (efficiency < MIN_EFFICIENCY_RATIO) {
            
            continue;  /* Skip this arm completely */
            
          }
          
        }
        
        /* Update UCB before selection */
        if (mab->arms[i].pull_count > 0) {
          
          /* Reduced exploration term to favor exploitation */
          double exploration = MAB_ALPHA * 
                              sqrt(log((double)mab->total_pulls) / 
                                   (double)mab->arms[i].pull_count);
          
          /* Efficiency factor: penalize arms with high pull count but low reward */
          double efficiency_factor = 1.0;
          if (mab->arms[i].pull_count > EFFICIENCY_THRESHOLD) {
            
            /* Calculate efficiency: reward per pull */
            double efficiency = mab->arms[i].avg_reward / (double)mab->arms[i].pull_count;
            
          /* Progressive penalty based on efficiency (more lenient) */
          if (efficiency < MIN_EFFICIENCY_RATIO) {
            
            efficiency_factor = 0.5;  /* Reduce UCB by 50% for very inefficient arms */
            
          } else if (efficiency < MIN_EFFICIENCY_RATIO * 3) {
            
            efficiency_factor = 0.7;  /* Reduce UCB by 30% for moderately inefficient arms */
            
          } else if (efficiency < MIN_EFFICIENCY_RATIO * 5) {
            
            efficiency_factor = 0.85;  /* Reduce UCB by 15% for slightly inefficient arms */
            
          }
            
          }
          
          mab->arms[i].ucb_value = (mab->arms[i].avg_reward + exploration) * efficiency_factor;
          
        } else {
          
          /* Unexplored arms get moderate UCB to encourage exploration */
          mab->arms[i].ucb_value = 1.0;  /* Increased from 0.5 to encourage more exploration */
          
        }
        
        if (mab->arms[i].ucb_value > max_ucb) {
          
          max_ucb = mab->arms[i].ucb_value;
          selected_arm = i;
          
        }
        
      }
      break;
```

**UCB 公式**：
```
UCB(i) = (avg_reward(i) + exploration_term) × efficiency_factor

其中：
- avg_reward(i): 变异操作 i 的平均奖励
- exploration_term = α × sqrt(ln(total_pulls) / pull_count(i))
- efficiency_factor: 基于效率的惩罚因子（0.5 ~ 1.0）
```

#### 阶段 2：格理论邻居探索

在 MAB 选择的基础上，探索**格空间中的邻居变异操作**：

1. **查找邻居**：基于格理论，找到与基础变异操作"相似"的邻居
2. **效率比较**：比较邻居和基础变异操作的效率
3. **智能切换**：
   - 如果邻居奖励 > 基础奖励 × 1.1，以 5% 概率切换
   - 如果邻居奖励 > 基础奖励 × 1.3，直接切换

### 2.4 奖励计算

每次变异操作执行后，根据结果计算奖励：

```531:616:AFLplusplus-dev/src/afl-lattice-mab.c
/* Calculate reward for a mutation based on coverage gain and efficiency */
double calculate_mutation_reward(afl_state_t *afl, u32 mutation_type,
                                u32 new_edges, u32 new_paths) {

  (void)mutation_type;  /* Suppress unused parameter warning */
  if (!afl) { return 0.0; }
  
  /* Base reward from new coverage (increased weight to emphasize coverage discovery) */
  double reward = (double)new_edges * COVERAGE_REWARD_WEIGHT + (double)new_paths * (COVERAGE_REWARD_WEIGHT * 0.5);
  
  /* Bonus for finding crashes */
  if (afl->saved_crashes > 0) {
    
    reward += 100.0;
    
  }
  
  /* Efficiency-based reward: balance between coverage discovery and efficiency */
  if (afl->lattice_mab_ctx && mutation_type < LATTICE_DIMENSION) {
    
    mutation_vector_t *vec = &afl->lattice_mab_ctx->lattice.vectors[mutation_type];
    
    /* Calculate historical efficiency: total coverage per total usage */
    double historical_efficiency = 0.0;
    if (vec->usage_count > 0) {
      
      /* Use historical average reward as proxy for efficiency */
      historical_efficiency = vec->avg_reward;
      
      /* Efficiency bonus: reward mutations that have good historical performance */
      if (historical_efficiency > 0.0) {
        
        reward += historical_efficiency * EFFICIENCY_REWARD_WEIGHT;
        
      }
      
    } else {
      
      /* Larger bonus for unexplored mutations to encourage exploration */
      reward += 1.0;  /* Increased from 0.1 to encourage more exploration */
      
    }
    
    /* Adaptive penalty: only penalize if mutation is clearly inefficient */
    if (vec->usage_count > EFFICIENCY_THRESHOLD) {
      
      /* Calculate current efficiency ratio */
      double current_efficiency = 0.0;
      if (vec->usage_count > 0) {
        
        current_efficiency = historical_efficiency;
        
      }
      
      /* Only apply penalty if efficiency is significantly below threshold */
      if (current_efficiency < MIN_EFFICIENCY_RATIO && (new_edges + new_paths) == 0) {
        
        /* Reduced penalty to avoid premature abandonment */
        double penalty = EFFICIENCY_PENALTY_FACTOR * 
                        (double)vec->usage_count / 100.0;  /* Reduced from 50.0 to 100.0 */
        reward -= penalty;
        
      }
      
    }
    
    /* Reduced penalty for zero coverage: only if usage is very high */
    if (vec->usage_count > EFFICIENCY_THRESHOLD * 2 && (new_edges + new_paths) == 0) {
      
      /* More lenient progressive penalty */
      double progressive_penalty = EFFICIENCY_PENALTY_FACTOR * 
                                  (double)vec->usage_count / 80.0;  /* Reduced from 30.0 to 80.0 */
      reward -= progressive_penalty;
      
    }
    
  }
  
  /* Normalize reward */
  reward = reward / 1000.0;
  
  /* Ensure reward is non-negative (but can be small) */
  if (reward < 0.0) { reward = 0.0; }
  
  return reward;
  
}
```

**奖励组成**：
1. **覆盖率奖励**：`new_edges × 10.0 + new_paths × 5.0`
2. **崩溃奖励**：`+100.0`（如果发现崩溃）
3. **效率奖励**：`historical_efficiency × 25.0`
4. **探索奖励**：`+1.0`（未探索的变异操作）
5. **效率惩罚**：根据使用次数和效率进行惩罚

### 2.5 特点

- ✅ **学习能力**：根据历史性能动态调整
- ✅ **效率优化**：优先选择高效的变异操作
- ✅ **探索与利用平衡**：UCB 算法平衡探索和利用
- ✅ **格理论优化**：通过邻居探索发现更好的变异操作
- ⚠️ **计算开销**：需要维护统计信息和计算 UCB 值

---

## 三、关键差异对比

| 维度 | 原策略 | Lattice-MAB 策略 |
|------|--------|------------------|
| **选择方式** | 完全随机 | 基于历史性能的智能选择 |
| **学习能力** | ❌ 无 | ✅ 有（MAB 算法） |
| **效率考虑** | ❌ 不考虑 | ✅ 优先选择高效变异操作 |
| **探索机制** | 随机探索 | UCB 算法平衡探索和利用 |
| **邻居优化** | ❌ 无 | ✅ 格理论邻居探索 |
| **计算复杂度** | O(1) | O(n)，n = 变异操作数量 |
| **适应性** | 静态 | 动态适应 |

---

## 四、选择流程图

### 原策略流程

```
开始
  ↓
根据输入类型选择 mutation_array
  ↓
随机生成索引 r = rand(0, rand_max)
  ↓
selected_mutation = mutation_array[r]
  ↓
执行变异操作
  ↓
结束
```

### Lattice-MAB 策略流程

```
开始
  ↓
阶段1: MAB 基础选择
  ├─ 计算每个变异操作的 UCB 值
  ├─ 过滤低效率变异操作
  ├─ 选择 UCB 值最高的变异操作
  └─ base_mutation = argmax(UCB)
  ↓
阶段2: 格理论邻居探索
  ├─ 判断是否需要探索邻居
  ├─ 查找格空间中的邻居
  ├─ 比较邻居和基础变异操作的效率
  └─ 如果邻居更好，则切换
  ↓
best_mutation = 最终选择的变异操作
  ↓
执行变异操作
  ↓
计算奖励并更新统计信息
  ↓
结束
```

---

## 五、实际效果

### 原策略
- **优点**：简单、公平、无偏
- **缺点**：可能频繁选择低效变异操作，浪费执行次数

### Lattice-MAB 策略
- **优点**：智能选择，优先使用高效变异操作，提高覆盖率效率
- **缺点**：需要一定的学习时间，初期可能不如原策略

---

## 六、总结

**原策略**采用**完全随机**的方式选择变异操作，简单但效率较低。

**Lattice-MAB 策略**采用**智能学习**的方式：
1. 使用 **MAB 算法**根据历史性能选择基础变异操作
2. 使用 **格理论**探索相似的邻居变异操作
3. 根据**覆盖率效率**动态调整选择概率

这使得 Lattice-MAB 策略能够：
- 🎯 **优先选择高效的变异操作**
- 📈 **提高覆盖率效率**（单位执行次数获得的覆盖率）
- 🔄 **平衡探索和利用**（既探索新变异，又利用已知高效变异）

