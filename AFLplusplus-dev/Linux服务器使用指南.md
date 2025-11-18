# AFL++ 在 Linux 服务器上的使用指南

## 问题：`afl-cc: command not found`

你的 AFL++ 已经编译好了（可以看到 `afl-cc` 和 `afl-fuzz` 文件存在），但是它们不在系统的 PATH 环境变量中。

## 解决方案

### 方案 1：使用相对路径（最简单，推荐）

直接在 AFL++ 目录下使用 `./` 前缀：

```bash
# 编译 test-instr.c
./afl-cc -o test-instr test-instr.c

# 运行模糊测试
./afl-fuzz -i seeds -o output -- ./test-instr
```

### 方案 2：将当前目录添加到 PATH（当前会话有效）

```bash
# 添加到当前 shell 的 PATH
export PATH="$PWD:$PATH"

# 现在可以直接使用 afl-cc
afl-cc -o test-instr test-instr.c
```

### 方案 3：永久添加到 PATH（推荐用于长期使用）

编辑你的 shell 配置文件：

```bash
# 如果是 bash
echo 'export PATH="$HOME/code/AFLplusplus-dev:$PATH"' >> ~/.bashrc
source ~/.bashrc

# 如果是 zsh
echo 'export PATH="$HOME/code/AFLplusplus-dev:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

### 方案 4：安装到系统路径（需要 sudo 权限）

```bash
# 在 AFL++ 目录下执行
sudo make install

# 安装后，afl-cc 就可以在任何地方使用了
```

## 完整的模糊测试流程（使用 test-instr.c）

### 步骤 1：编译 test-instr.c

```bash
# 进入 AFL++ 目录
cd ~/code/AFLplusplus-dev

# 使用相对路径编译
./afl-cc -o test-instr test-instr.c

# 验证编译是否成功
ls -la test-instr
file test-instr
```

### 步骤 2：准备测试种子文件

```bash
# 创建种子目录
mkdir -p seeds

# 创建一些简单的种子文件
echo "0" > seeds/input1.txt
echo "1" > seeds/input2.txt
echo "2" > seeds/input3.txt

# 验证种子文件
cat seeds/*
```

### 步骤 3：处理 core_pattern 配置（重要！）

在运行模糊测试之前，需要处理系统的 core_pattern 配置：

```bash
# 检查当前配置
cat /proc/sys/kernel/core_pattern

# 如果输出包含 "|"（管道符号），说明需要处理
```

**情况 1：可以修改 core_pattern（有 sudo 权限且文件系统可写）**

```bash
# 尝试使用 sysctl 修改
sudo sysctl -w kernel.core_pattern=core

# 验证修改
cat /proc/sys/kernel/core_pattern
# 应该输出：core
```

**情况 2：无法修改 core_pattern（只读文件系统或没有权限）**

如果遇到 "Read-only file system" 错误，使用环境变量：

```bash
# 设置环境变量忽略检查
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1

# 注意：这可能导致崩溃检测不准确，但模糊测试仍可正常运行
```

### 步骤 4：运行模糊测试

```bash
# 创建输出目录
mkdir -p output

# 运行 afl-fuzz（test-instr 从 stdin 读取输入）
./afl-fuzz -i seeds -o output -- ./test-instr

# 如果程序从文件读取输入，使用：
# ./afl-fuzz -i seeds -o output -- ./test-instr -f @@
```

### 步骤 5：查看模糊测试结果

模糊测试运行后，你会看到一个实时统计界面，显示：
- **exec/s**: 每秒执行次数
- **paths total**: 发现的路径总数
- **cycles done**: 完成的周期数
- **uniq crashes**: 唯一崩溃数
- **uniq hangs**: 唯一超时数

按 `Ctrl+C` 停止模糊测试。

### 步骤 6：查看发现的崩溃

```bash
# 查看崩溃文件
ls -la output/crashes/

# 查看崩溃内容
cat output/crashes/id:000000,*

# 复现崩溃
cat output/crashes/id:000000,* | ./test-instr
```

### 步骤 7：使用 gdb 调试崩溃（可选）

```bash
# 使用 gdb 调试
gdb ./test-instr
(gdb) run < output/crashes/id:000000,*
(gdb) bt  # 查看堆栈跟踪
(gdb) quit
```

## test-instr.c 程序说明

`test-instr.c` 是一个简单的测试程序，用于验证 AFL++ 是否正常工作：

- **功能**：从 stdin、命令行参数或文件读取输入
- **逻辑**：根据输入的第一个字符输出不同消息
- **用途**：验证 AFL++ 的代码覆盖率追踪功能

程序支持的输入方式：
1. 命令行参数：`./test-instr "0"`
2. 标准输入：`echo "1" | ./test-instr`
3. 文件输入：`./test-instr -f input.txt`

## 快速测试命令

```bash
# 1. 编译
cd ~/code/AFLplusplus-dev
./afl-cc -o test-instr test-instr.c

# 2. 处理 core_pattern 配置
# 如果可以修改：
# sudo sysctl -w kernel.core_pattern=core
# 如果无法修改（只读文件系统），使用环境变量：
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1

# 3. 准备种子
mkdir -p seeds && echo "0" > seeds/input1.txt && echo "1" > seeds/input2.txt

# 4. 运行模糊测试（运行几秒钟后按 Ctrl+C 停止）
./afl-fuzz -i seeds -o output -- ./test-instr

# 5. 查看结果
ls output/crashes/ output/hangs/
```

## 常见问题

### Q: 运行 afl-fuzz 时提示 "core_pattern" 相关错误，程序无法启动

**错误信息示例：**
```
[-] Your system is configured to send core dump notifications to an
    external utility. This will cause issues...
[-] PROGRAM ABORT : Pipe at the beginning of 'core_pattern'
```

**原因：** 系统的 `core_pattern` 配置为将核心转储发送到外部工具（通常以 `|` 开头），AFL++ 需要直接写入核心转储文件才能检测崩溃。

**解决方案（按优先级）：**

#### 方案 1：使用 sysctl 修改（如果 /proc 文件系统可写）

```bash
# 查看当前的 core_pattern 设置
cat /proc/sys/kernel/core_pattern

# 尝试使用 sysctl 修改（需要 sudo 权限）
sudo sysctl -w kernel.core_pattern=core

# 验证修改是否成功
cat /proc/sys/kernel/core_pattern
```

**注意：** 这个修改在系统重启后会失效。如果想永久生效，需要修改系统配置：
```bash
# 编辑 /etc/sysctl.conf
sudo vi /etc/sysctl.conf
# 添加一行：kernel.core_pattern = core
# 然后执行：sudo sysctl -p
```

#### 方案 2：使用环境变量（当无法修改 core_pattern 时）

**如果遇到 "Read-only file system" 错误**（如你的情况），说明 `/proc/sys/kernel/core_pattern` 是只读的，无法直接修改。此时可以使用环境变量来绕过检查：

```bash
# 设置环境变量忽略 core_pattern 检查
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1

# 然后运行模糊测试
./afl-fuzz -i seeds -o output -- ./test-instr -f @@

# 或者在一行中设置
AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 ./afl-fuzz -i seeds -o output -- ./test-instr -f @@
```

**重要说明：**
- 使用此选项后，AFL++ 仍然可以运行并发现新的代码路径
- 但是**可能无法正确检测崩溃**（crash detection 可能不准确）
- 对于学习和测试目的，这通常是可以接受的
- 如果确实发现了崩溃，可以通过其他方式验证（如手动运行程序）

**验证崩溃的方法：**
```bash
# 即使使用了环境变量，如果 AFL++ 报告了崩溃，可以手动验证
cat output/crashes/id:000000,* | ./test-instr

# 或者使用 gdb 调试
gdb ./test-instr
(gdb) run < output/crashes/id:000000,*
```

#### 方案 3：联系系统管理员

如果是共享服务器且需要完整的崩溃检测功能，可能需要联系系统管理员修改系统级别的配置。管理员可以：
- 修改 `/etc/sysctl.conf` 或 `/etc/sysctl.d/` 中的配置
- 或者修改 systemd 的配置（如果系统使用 systemd）

### Q: 模糊测试速度很慢
A: 可以尝试：
- 使用 `afl-clang-fast` 而不是 `afl-cc`（需要 LLVM）
- 检查系统负载
- 使用 `-m none` 选项禁用内存限制（如果目标程序需要）

### Q: 没有发现崩溃
A: 这是正常的，`test-instr.c` 是一个简单的测试程序，可能不会触发崩溃。可以尝试：
- 运行更长时间
- 使用更复杂的测试目标
- 检查程序是否有实际的漏洞

## 下一步

成功运行 `test-instr.c` 后，你可以：
1. 尝试对更复杂的程序进行模糊测试
2. 学习 AFL++ 的高级功能（字典、自定义变异器等）
3. 阅读 `docs/fuzzing_in_depth.md` 了解更深入的模糊测试技巧

