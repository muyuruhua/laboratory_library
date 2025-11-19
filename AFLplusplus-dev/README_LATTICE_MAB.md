# Lattice-MAB 变异策略集成说明

## 编译状态

✅ **核心工具已成功编译**：
- `afl-fuzz` - 主模糊测试工具
- `afl-cc` / `afl-clang-fast` - 编译器包装器

⚠️ **注意**：编译过程中可能出现一些警告和可选的插件编译失败，这些不影响核心功能的使用。

## 快速开始

### 1. 验证编译结果

```bash
# 检查工具是否可用
./afl-fuzz
./afl-clang-fast --version
```

### 2. 运行快速测试

```bash
# 快速验证（30秒）
./quick_test.sh

# 完整对比测试（默认5分钟，3次运行）
./test_lattice_mab.sh
```

### 3. 使用新策略

新策略默认启用。可以通过环境变量控制：

```bash
# 启用新策略（默认）
export AFL_LATTICE_MAB=1
./afl-fuzz -i input_dir -o output_dir -- ./target @@

# 禁用新策略（使用原有策略）
export AFL_LATTICE_MAB=0
./afl-fuzz -i input_dir -o output_dir -- ./target @@
```

## 编译问题排查

如果遇到编译问题，可以尝试：

1. **只编译核心工具**（跳过测试）：
   ```bash
   make afl-fuzz afl-cc
   ```

2. **忽略测试错误**：
   测试阶段的错误（如 `test_build` 失败）不影响核心功能，可以忽略。

3. **检查依赖**：
   - 确保有基本的编译工具（gcc/clang）
   - Python 3（用于生成报告）

## 功能说明

### 原有策略
- 基于预定义数组的加权随机选择
- 根据输入类型（TEXT/BINARY）和模式（exploration/exploitation）选择不同策略

### 新策略（Lattice-MAB）
- 将变异操作形式化为高维向量
- 使用格理论建模策略空间
- 基于多臂老虎机（MAB）进行智能选择
- 根据历史性能动态调整选择概率

详细文档请参考：`docs/LATTICE_MAB_STRATEGY.md`

