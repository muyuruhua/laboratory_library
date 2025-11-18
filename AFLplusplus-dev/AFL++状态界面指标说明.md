# AFL++ 状态界面指标详细说明

## 界面概览

AFL++ 的状态界面实时显示模糊测试的进度和结果。界面分为左右两栏，包含多个关键指标。

## 顶部标题栏

```
american fuzzy lop ++4.35a {default} (./test-instr) [explore]
```

- **版本号**: `++4.35a` - AFL++ 的版本
- **模式**: `{default}` - 模糊测试模式（default/fast/explore 等）
- **目标程序**: `./test-instr` - 正在测试的程序
- **策略**: `[explore]` - 当前使用的探索策略

## 左侧栏：进程和进度信息

### 1. 进程时间（Process Timing）

```
run time: 0 days, 0 hrs, 7 min, 58 sec
last new find: none yet (odd, check syntax!)
last saved crash: none seen yet
last saved hang: none seen yet
```

- **run time**: 模糊测试已运行的总时间
- **last new find**: 距离上次发现新路径的时间
  - ⚠️ **"none yet (odd, check syntax!)"** 表示还没有发现新的代码路径
  - 这可能意味着：
    - 程序非常简单，所有路径已被探索完
    - 程序可能有语法问题或执行异常
    - 种子文件可能不合适
- **last saved crash**: 距离上次保存崩溃的时间
- **last saved hang**: 距离上次保存超时的时间

### 2. 周期进度（Cycle Progress）

```
now processing: 1.2134 (33.3%)
runs timed out: 0 (0.00%)
```

- **now processing**: 当前正在处理的队列项编号和百分比
- **runs timed out**: 超时的运行次数和百分比

### 3. 阶段进度（Stage Progress）

```
now trying: havoc
stage execs: 48/100 (48.00%)
total execs: 286k
exec speed: 617.8/sec
```

- **now trying**: 当前使用的变异策略（havoc、bitflip、arith 等）
- **stage execs**: 当前阶段的执行次数/总次数
- **total execs**: 累计总执行次数（286k = 286,000）
- **exec speed**: 每秒执行次数（617.8/sec）- **这个值越高越好**

### 4. 变异策略效果（Fuzzing Strategy Yields）

显示各种变异策略发现新路径的效果：

```
bit flips: 0/0, 0/0, 0/0
byte flips: 0/0, 0/0, 0/0
arithmetics: 0/0, 0/0, 0/0
known ints: 0/0, 0/0, 0/0
dictionary: 0/0, 0/0, 0/0, 0/0
havoc/splice: 0/286k, 0/0
```

格式：`新路径数/总执行数`

- **bit flips**: 位翻转策略
- **byte flips**: 字节翻转策略
- **arithmetics**: 算术运算策略
- **known ints**: 已知整数插入策略
- **dictionary**: 字典策略
- **havoc/splice**: 随机变异和拼接策略

如果都是 `0/0` 或 `0/286k`，说明这些策略还没有发现新的代码路径。

## 右侧栏：总体结果和覆盖率

### 1. 总体结果（Overall Results）

```
cycles done: 580
corpus count: 3
saved crashes: 0
saved hangs: 0
```

- **cycles done**: 完成的周期数（绿色表示正常）
  - 一个周期 = 遍历完整个队列一次
  - 580 个周期说明已经遍历了队列 580 次
- **corpus count**: 语料库中的测试用例数量（3 个）
- **saved crashes**: 保存的唯一崩溃数
- **saved hangs**: 保存的唯一超时数

### 2. 映射覆盖率（Map Coverage）

```
map density: 26.32% / 36.84%
count coverage: 51.29 bits/tuple
```

- **map density**: 覆盖率映射的密度
  - 第一个值（26.32%）显示当前覆盖率
  - 第二个值（36.84%）显示理论最大覆盖率
  - 红色表示覆盖率较低
- **count coverage**: 每个元组的平均位数（用于评估覆盖质量）

### 3. 发现详情（Findings in Depth）

```
favored items: 3 (100.00%)
new edges on: 3 (100.00%)
total crashes: 0 (0 saved)
total tmouts: 2 (0 saved)
```

- **favored items**: 优先处理的测试用例数量（100% 表示所有用例都是优先的）
- **new edges on**: 发现新边的测试用例数量
- **total crashes**: 总崩溃数（括号内是已保存的）
- **total tmouts**: 总超时数（2 个超时，但未保存）

### 4. 项目几何（Item Geometry）

```
levels: 1
pending: 0
pend fav: 0
own finds: 0
imported: 0
stability: 100.00%
```

- **levels**: 测试用例的层级深度
- **pending**: 待处理的测试用例数
- **pend fav**: 待处理的优先用例数
- **own finds**: 本地发现的用例数
- **imported**: 从其他实例导入的用例数
- **stability**: 稳定性百分比（100% 表示行为一致）

### 5. 底部状态

```
strategy: explore
state: finished...
[cpu000: 12%]
```

- **strategy**: 当前使用的策略（explore = 探索模式）
- **state**: 模糊测试状态
  - ⚠️ **"finished..."** (红色) 表示模糊测试已经完成/停止
  - 可能原因：
    - 所有路径已探索完
    - 没有新的发现
    - 程序过于简单
- **cpu000: 12%**: CPU 使用率（第一个核心）

## 关于你的结果分析

根据你看到的状态：

### ✅ 正常的部分：
1. **执行速度**: 617.8/sec - 速度正常
2. **已完成 580 个周期**: 说明 AFL++ 正常运行
3. **corpus count: 3**: 发现了 3 个不同的测试用例
4. **stability: 100%**: 程序行为稳定一致

### ⚠️ 需要注意的部分：
1. **state: finished...**: 模糊测试已经停止
2. **last new find: none yet (odd, check syntax!)**: 没有发现新的代码路径
3. **所有变异策略都是 0**: 没有策略发现新路径

### 为什么会这样？

`test-instr.c` 是一个非常简单的测试程序：
- 只有 3 个主要分支：`case '0'`, `case '1'`, `default`
- 程序逻辑简单，AFL++ 很快就探索完了所有可能的路径
- 没有复杂的解析逻辑，所以不会触发崩溃

这是**正常现象**！`test-instr.c` 就是用来验证 AFL++ 是否正常工作的简单测试程序。

## 如何判断模糊测试是否成功？

### 成功的标志：
- ✅ 执行速度 > 100/sec（你的 617.8/sec 很好）
- ✅ 发现新的代码路径（corpus count 增加）
- ✅ 发现崩溃（saved crashes > 0）
- ✅ 覆盖率增加（map density 提升）

### 可能的问题：
- ⚠️ 执行速度 < 10/sec → 目标程序太慢
- ⚠️ 长时间没有新发现 → 可能需要更好的种子文件
- ⚠️ 所有策略都是 0 → 可能需要字典文件或自定义变异器

## 下一步建议

1. **测试更复杂的程序**: `test-instr.c` 太简单，可以尝试模糊测试更复杂的程序
2. **查看语料库**: 检查 `output/queue/` 目录，看看 AFL++ 发现了哪些测试用例
3. **分析覆盖率**: 使用 `afl-showmap` 查看代码覆盖率详情
4. **尝试其他目标**: 找一个有实际解析逻辑的程序进行测试

## 常用命令

```bash
# 查看发现的测试用例
ls -la output/queue/

# 查看覆盖率详情
./afl-showmap -o coverage.map -- ./test-instr -f @@ < output/queue/id:000000,*

# 查看统计信息（机器可读格式）
cat output/fuzzer_stats

# 生成图表（需要 gnuplot）
./afl-plot output output_plot
```

## 总结

你的 AFL++ 运行是**正常的**！`test-instr.c` 程序太简单，所以 AFL++ 很快就探索完了所有路径并停止。这是预期的行为。要测试 AFL++ 的真正能力，需要使用更复杂的程序作为目标。

