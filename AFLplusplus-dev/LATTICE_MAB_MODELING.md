# Lattice-MAB 对 AFL++ 变异策略的建模说明

## 问题：是否对所有 37 种变异策略都建模了？

**答案：是的，Lattice-MAB 对所有 37 种变异策略都进行了建模。**

## AFL++ 的 37 种变异类型

根据 `afl-mutations.h` 的定义，AFL++ 共有 **37 种变异类型**（从 0 到 36）：

```c
enum {
  /* 00 */ MUT_FLIPBIT,              // 比特翻转
  /* 01 */ MUT_INTERESTING8,         // 8位有趣值
  /* 02 */ MUT_INTERESTING16,        // 16位有趣值（小端）
  /* 03 */ MUT_INTERESTING16BE,      // 16位有趣值（大端）
  /* 04 */ MUT_INTERESTING32,        // 32位有趣值（小端）
  /* 05 */ MUT_INTERESTING32BE,      // 32位有趣值（大端）
  /* 06 */ MUT_ARITH8_,              // 8位算术运算（减法）
  /* 07 */ MUT_ARITH8,               // 8位算术运算（加法）
  /* 08 */ MUT_ARITH16_,             // 16位算术运算（减法，小端）
  /* 09 */ MUT_ARITH16BE_,           // 16位算术运算（减法，大端）
  /* 10 */ MUT_ARITH16,              // 16位算术运算（加法，小端）
  /* 11 */ MUT_ARITH16BE,            // 16位算术运算（加法，大端）
  /* 12 */ MUT_ARITH32_,             // 32位算术运算（减法，小端）
  /* 13 */ MUT_ARITH32BE_,           // 32位算术运算（减法，大端）
  /* 14 */ MUT_ARITH32,              // 32位算术运算（加法，小端）
  /* 15 */ MUT_ARITH32BE,            // 32位算术运算（加法，大端）
  /* 16 */ MUT_RAND8,                // 随机8位值
  /* 17 */ MUT_CLONE_COPY,           // 克隆并复制
  /* 18 */ MUT_CLONE_FIXED,          // 克隆并固定
  /* 19 */ MUT_OVERWRITE_COPY,       // 覆盖并复制
  /* 20 */ MUT_OVERWRITE_FIXED,      // 覆盖并固定
  /* 21 */ MUT_BYTEADD,              // 字节加法
  /* 22 */ MUT_BYTESUB,              // 字节减法
  /* 23 */ MUT_FLIP8,                // 8位翻转
  /* 24 */ MUT_SWITCH,               // 字节交换
  /* 25 */ MUT_DEL,                  // 删除
  /* 26 */ MUT_SHUFFLE,              // 打乱
  /* 27 */ MUT_DELONE,               // 删除一个字节
  /* 28 */ MUT_INSERTONE,            // 插入一个字节
  /* 29 */ MUT_ASCIINUM,             // ASCII 数字
  /* 30 */ MUT_INSERTASCIINUM,       // 插入 ASCII 数字
  /* 31 */ MUT_EXTRA_OVERWRITE,      // 额外覆盖
  /* 32 */ MUT_EXTRA_INSERT,         // 额外插入
  /* 33 */ MUT_AUTO_EXTRA_OVERWRITE, // 自动额外覆盖
  /* 34 */ MUT_AUTO_EXTRA_INSERT,    // 自动额外插入
  /* 35 */ MUT_SPLICE_OVERWRITE,     // 拼接覆盖
  /* 36 */ MUT_SPLICE_INSERT,        // 拼接插入
  
  MUT_MAX  // = 37
};
```

## Lattice-MAB 的建模

### 1. 维度定义

在 `afl-lattice-mab.h` 中：

```c
#define LATTICE_DIMENSION 37  /* Number of mutation types (MUT_MAX) */
```

**说明**：`LATTICE_DIMENSION` 被明确定义为 37，对应 AFL++ 的所有变异类型。

### 2. Lattice 初始化

在 `init_mutation_lattice()` 函数中：

```c
void init_mutation_lattice(mutation_lattice_t *lattice) {
    // ...
    lattice->vector_count = LATTICE_DIMENSION;  // = 37
    
    /* Create vectors for all mutation types */
    for (u32 i = 0; i < LATTICE_DIMENSION && i < MUT_MAX; ++i) {
        lattice->vectors[i] = create_mutation_vector(i);
    }
    // ...
}
```

**说明**：循环从 0 到 36（共 37 次），为每种变异类型创建一个向量。

### 3. MAB 初始化

在 `mab_init()` 函数中：

```c
void mab_init(mab_selector_t *mab, u32 strategy_type) {
    // ...
    mab->arm_count = LATTICE_DIMENSION;  // = 37
    
    /* Initialize all arms */
    for (u32 i = 0; i < mab->arm_count && i < MUT_MAX; ++i) {
        mab->arms[i].mutation_type = i;
        // 初始化每个 arm 的统计信息
    }
    // ...
}
```

**说明**：为所有 37 种变异类型创建 MAB arm，每个 arm 对应一种变异策略。

### 4. 向量表示

每种变异类型使用 **one-hot 编码**表示：

```c
mutation_vector_t create_mutation_vector(u32 mutation_type) {
    // ...
    /* Create vector representation: one-hot encoding */
    for (u32 i = 0; i < LATTICE_DIMENSION; ++i) {
        vec.dimension[i] = (i == mutation_type) ? 1 : 0;
        vec.position[i] = (i == mutation_type) ? 1 : 0;
    }
    // ...
}
```

**说明**：
- 每种变异类型是一个 37 维向量
- 使用 one-hot 编码：对应位置为 1，其他位置为 0
- 例如：`MUT_FLIPBIT (0)` = `[1, 0, 0, ..., 0]`
- 例如：`MUT_INTERESTING8 (1)` = `[0, 1, 0, ..., 0]`

### 5. 邻居矩阵

Lattice 还构建了一个 **37×37 的邻居矩阵**：

```c
size_t matrix_size = LATTICE_DIMENSION * LATTICE_DIMENSION * sizeof(u32);
lattice->neighbor_matrix = (u32 *)malloc(matrix_size);  // 37×37 矩阵

/* Initialize adjacency: vectors are neighbors if distance < threshold */
for (u32 i = 0; i < LATTICE_DIMENSION; ++i) {
    for (u32 j = 0; j < LATTICE_DIMENSION; ++j) {
        // 计算向量距离，判断是否为邻居
    }
}
```

**说明**：37×37 的矩阵记录了所有变异类型之间的邻居关系。

## 建模完整性验证

### 代码验证

1. **维度匹配**：
   - `LATTICE_DIMENSION = 37`
   - `MUT_MAX = 37`
   - ✅ 匹配

2. **初始化循环**：
   ```c
   for (u32 i = 0; i < LATTICE_DIMENSION && i < MUT_MAX; ++i)
   ```
   - 循环范围：0 到 36（共 37 次）
   - ✅ 覆盖所有变异类型

3. **数组大小**：
   - `lattice->vectors[LATTICE_DIMENSION]` = `vectors[37]`
   - `mab->arms[LATTICE_DIMENSION]` = `arms[37]`
   - ✅ 足够存储所有变异类型

4. **边界检查**：
   ```c
   if (mutation_type >= LATTICE_DIMENSION) { return; }
   ```
   - ✅ 确保不会越界

## 37 种变异类型的分类

### 按功能分类

1. **位操作**（3 种）：
   - MUT_FLIPBIT (0)
   - MUT_FLIP8 (23)
   - MUT_SWITCH (24)

2. **有趣值替换**（5 种）：
   - MUT_INTERESTING8 (1)
   - MUT_INTERESTING16 (2)
   - MUT_INTERESTING16BE (3)
   - MUT_INTERESTING32 (4)
   - MUT_INTERESTING32BE (5)

3. **算术运算**（10 种）：
   - MUT_ARITH8_ (6), MUT_ARITH8 (7)
   - MUT_ARITH16_ (8), MUT_ARITH16 (10)
   - MUT_ARITH16BE_ (9), MUT_ARITH16BE (11)
   - MUT_ARITH32_ (12), MUT_ARITH32 (14)
   - MUT_ARITH32BE_ (13), MUT_ARITH32BE (15)

4. **随机值**（1 种）：
   - MUT_RAND8 (16)

5. **克隆操作**（2 种）：
   - MUT_CLONE_COPY (17)
   - MUT_CLONE_FIXED (18)

6. **覆盖操作**（2 种）：
   - MUT_OVERWRITE_COPY (19)
   - MUT_OVERWRITE_FIXED (20)

7. **字节操作**（2 种）：
   - MUT_BYTEADD (21)
   - MUT_BYTESUB (22)

8. **删除操作**（2 种）：
   - MUT_DEL (25)
   - MUT_DELONE (27)

9. **插入操作**（2 种）：
   - MUT_INSERTONE (28)
   - MUT_INSERTASCIINUM (30)

10. **打乱操作**（1 种）：
    - MUT_SHUFFLE (26)

11. **ASCII 操作**（1 种）：
    - MUT_ASCIINUM (29)

12. **额外操作**（4 种）：
    - MUT_EXTRA_OVERWRITE (31)
    - MUT_EXTRA_INSERT (32)
    - MUT_AUTO_EXTRA_OVERWRITE (33)
    - MUT_AUTO_EXTRA_INSERT (34)

13. **拼接操作**（2 种）：
    - MUT_SPLICE_OVERWRITE (35)
    - MUT_SPLICE_INSERT (36)

**总计：37 种**

## Lattice-MAB 的建模方式

### 1. 向量空间建模

- **维度**：37 维离散向量空间
- **表示**：每种变异类型是一个 one-hot 向量
- **位置**：向量在 37 维空间中的位置

### 2. 格（Lattice）结构

- **节点**：37 个变异向量
- **边**：通过距离计算确定邻居关系
- **密度**：计算向量在空间中的分布密度

### 3. MAB 建模

- **Arm 数量**：37 个（每个对应一种变异类型）
- **奖励跟踪**：为每个 arm 跟踪历史性能
- **选择策略**：UCB 或 Epsilon-Greedy

### 4. 邻居关系

- **邻居矩阵**：37×37 的邻接矩阵
- **距离计算**：基于向量之间的欧氏距离
- **邻居搜索**：根据距离阈值找到最近邻

## 总结

### ✅ **完全建模**

Lattice-MAB 策略**对所有 37 种 AFL++ 变异策略都进行了建模**：

1. ✅ **Lattice 向量**：37 个向量，每个对应一种变异类型
2. ✅ **MAB Arms**：37 个 arm，每个对应一种变异类型
3. ✅ **邻居矩阵**：37×37 矩阵，记录所有变异类型之间的关系
4. ✅ **统计跟踪**：为每种变异类型跟踪使用次数、奖励、效率等

### 建模特点

- **完整性**：覆盖所有 37 种变异类型
- **结构化**：使用向量空间和格结构组织
- **可扩展**：如果 AFL++ 增加新的变异类型，只需修改 `LATTICE_DIMENSION` 即可

### 验证方法

可以通过以下方式验证：

```c
// 在代码中添加验证
assert(LATTICE_DIMENSION == 37);
assert(MUT_MAX == 37);
assert(lattice->vector_count == 37);
assert(mab->arm_count == 37);
```

**结论**：Lattice-MAB 完全建模了 AFL++ 的所有 37 种变异策略。

