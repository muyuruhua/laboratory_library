# AFL++ 完整工具链安装指南

本文档说明如何安装完整的 AFL++ 工具链，包括 `afl-as` 等所有必需工具。

## 快速安装（推荐）

### Ubuntu/Debian 系统

```bash
# 1. 更新系统并安装依赖
sudo apt-get update
sudo apt-get install -y build-essential python3-dev automake cmake git flex bison \
    libglib2.0-dev libpixman-1-dev python3-setuptools cargo libgtk-3-dev

# 2. 安装 LLVM（推荐 LLVM 18 或更新版本，最低要求 LLVM 14）
# 尝试安装 LLVM 18，如果失败则使用系统默认版本
sudo apt-get install -y lld-18 llvm-18 llvm-18-dev clang-18 || \
    sudo apt-get install -y lld llvm llvm-dev clang

# 3. 安装 GCC 插件支持（用于 afl-gcc）
GCC_VERSION=$(gcc --version | head -n1 | sed 's/\..*//' | sed 's/.* //')
sudo apt-get install -y gcc-${GCC_VERSION}-plugin-dev \
    libstdc++-${GCC_VERSION}-dev

# 4. 克隆 AFL++ 仓库（如果还没有）
cd ~/code
git clone https://github.com/AFLplusplus/AFLplusplus.git
cd AFLplusplus
git submodule update --init

# 5. 编译 AFL++（选择以下之一）
# 选项 A: 编译所有功能（推荐，包括二进制模糊测试支持）
make distrib

# 选项 B: 仅编译源代码模糊测试所需的功能
# make source-only

# 选项 C: 仅编译基本功能
# make all

# 6. 安装到系统（可选，但推荐）
sudo make install
```

### 验证安装

安装完成后，验证工具是否可用：

```bash
# 检查 afl-cc（主编译器）
which afl-cc
afl-cc --version

# 检查 afl-as（汇编器，这是关键！）
which afl-as
ls -la $(which afl-as)  # 应该指向 AFL++ 的 afl-as

# 检查其他工具
which afl-fuzz
which afl-clang-fast
which afl-showmap
```

### 如果 afl-as 未找到

如果 `afl-as` 未找到，可能的原因和解决方法：

1. **未运行 `make install`**：
   ```bash
   # 在 AFL++ 源码目录中
   sudo make install
   ```

2. **afl-as 未编译**：
   ```bash
   # 确保运行了 make all 或 make distrib
   cd ~/code/AFLplusplus
   make clean
   make all
   ```

3. **PATH 未设置**：
   ```bash
   # 检查安装路径（默认是 /usr/local/bin）
   ls -la /usr/local/bin/afl-*
   
   # 如果文件存在但命令找不到，检查 PATH
   echo $PATH
   
   # 临时添加到 PATH（如果需要）
   export PATH=/usr/local/bin:$PATH
   ```

4. **使用本地编译的版本**：
   ```bash
   # 如果不想安装到系统，可以在 AFL++ 源码目录中使用
   cd ~/code/AFLplusplus
   export PATH=$(pwd):$PATH
   ./afl-cc --version
   ```

## 详细说明

### 编译目标说明

- **`make all`**: 编译主要的 AFL++ 二进制文件和 LLVM/GCC 插桩工具
- **`make source-only`**: 编译源代码模糊测试所需的所有功能（不包括 QEMU、FRIDA 等二进制模式）
- **`make distrib`**: 编译所有功能（包括二进制模糊测试支持：FRIDA、QEMU、Unicorn 等）
- **`make install`**: 将编译好的工具安装到系统路径（默认 `/usr/local/bin`）

### 关键组件

安装完整工具链后，以下组件应该可用：

1. **编译器包装器**：
   - `afl-cc` - 主编译器（自动选择最佳插桩方式）
   - `afl-clang-fast` - LLVM 模式编译器（快速）
   - `afl-clang-lto` - LLVM LTO 模式编译器（最快，推荐）
   - `afl-gcc-fast` - GCC 插件模式编译器

2. **汇编器**：
   - `afl-as` - AFL++ 的汇编器（用于传统插桩）

3. **模糊测试工具**：
   - `afl-fuzz` - 主模糊测试工具
   - `afl-showmap` - 显示代码覆盖率
   - `afl-cmin` - 最小化测试用例集
   - `afl-tmin` - 最小化单个测试用例

4. **LLVM 插件**（在 `/usr/local/lib/afl/` 或类似路径）：
   - `afl-llvm-pass.so` - LLVM 插桩插件
   - `SanitizerCoveragePCGUARD.so` - PCGUARD 模式插件
   - `SanitizerCoverageLTO.so` - LTO 模式插件

### 使用本地编译的版本（不安装到系统）

如果你不想安装到系统路径，可以：

```bash
cd ~/code/AFLplusplus
make all  # 或 make distrib

# 使用本地版本
export PATH=$(pwd):$PATH
export AFL_PATH=$(pwd)

# 现在可以使用本地编译的工具
./afl-cc --version
./afl-fuzz --version
```

### 故障排除

#### 问题 1: "Cannot find 'afl-as'"

**原因**: `afl-as` 未编译或未在 PATH 中。

**解决方法**:
```bash
# 确保已编译
cd ~/code/AFLplusplus
make all

# 检查是否生成了 afl-as
ls -la afl-as

# 如果存在，添加到 PATH
export PATH=$(pwd):$PATH

# 或者安装到系统
sudo make install
```

#### 问题 2: LLVM 版本不兼容

**原因**: 系统 LLVM 版本太旧或未安装。

**解决方法**:
```bash
# 检查 LLVM 版本
llvm-config --version

# 如果版本 < 14，需要安装更新的版本
# Ubuntu 22.04+ 通常有 LLVM 14+
sudo apt-get install -y llvm-18 llvm-18-dev clang-18

# 指定 LLVM 版本编译
export LLVM_CONFIG=llvm-config-18
make all
```

#### 问题 3: GCC 插件编译失败

**原因**: 缺少 GCC 插件开发包。

**解决方法**:
```bash
# 安装对应版本的 GCC 插件开发包
GCC_VERSION=$(gcc --version | head -n1 | sed 's/\..*//' | sed 's/.* //')
sudo apt-get install -y gcc-${GCC_VERSION}-plugin-dev

# 重新编译
make clean
make all
```

## 针对你的云服务器环境

根据你的错误信息，你的服务器上缺少 `afl-as`。按照以下步骤操作：

```bash
# 1. 进入 AFL++ 目录
cd ~/code/AFLplusplus-dev

# 2. 检查是否已编译
ls -la afl-as

# 3. 如果不存在，编译 AFL++
make all

# 4. 检查编译结果
ls -la afl-as afl-cc afl-fuzz

# 5. 如果编译成功，安装到系统
sudo make install

# 6. 验证安装
which afl-as
afl-as --version
```

如果 `make all` 失败，请检查：

1. **LLVM 是否安装**:
   ```bash
   which llvm-config
   llvm-config --version
   ```

2. **GCC 插件支持**:
   ```bash
   GCC_VERSION=$(gcc --version | head -n1 | sed 's/\..*//' | sed 's/.* //')
   dpkg -l | grep gcc-${GCC_VERSION}-plugin-dev
   ```

3. **编译错误信息**:
   ```bash
   make all 2>&1 | tee build.log
   # 查看 build.log 了解具体错误
   ```

## 参考资源

- [AFL++ 官方安装文档](docs/INSTALL.md)
- [LLVM 模式说明](instrumentation/README.llvm.md)
- [GCC 插件模式说明](instrumentation/README.gcc_plugin.md)

