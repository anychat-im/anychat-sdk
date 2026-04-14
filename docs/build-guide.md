# AnyChat SDK 构建指南

本仓库当前的 Native 构建流程统一使用 `python3 tools/build-native.py`。
该脚本内部通过 `CMakePresets.json` 驱动 CMake，并在 Windows 上自动初始化
MSVC 的 VC 编译环境；文档不再将底层 CMake 命令作为标准构建方式。

## 目录

- [构建约定](#构建约定)
- [第三方依赖](#第三方依赖)
- [系统要求](#系统要求)
- [快速开始](#快速开始)
- [常用命令](#常用命令)
- [Native 预设](#native-预设)
- [构建输出](#构建输出)
- [故障排查](#故障排查)

---

## 构建约定

- Native 平台（Linux / macOS / Windows）统一使用 `python3 tools/build-native.py`
- 脚本会自动选择当前宿主机对应的 CMake preset
- Windows 下脚本会自动查找并调用 `vcvars64.bat`，不需要手动预热 MSVC 环境
- `CMakePresets.json` 仍然是底层实现的一部分，但不作为文档推荐入口

---

## 第三方依赖

当前仓库中的第三方集成方式如下：

| 依赖 | 用途 | 集成方式 |
|------|------|----------|
| `curl` | HTTP 异步请求 | Git submodule（`thirdparty/curl`） |
| `glaze` | JSON 序列化 / 反序列化 | Git submodule（`thirdparty/glaze`） |
| `googletest` | 单元测试 | Git submodule（`thirdparty/googletest`） |
| `libwebsockets` | WebSocket | Git submodule（`thirdparty/libwebsockets`） |
| `sqlite3` | 本地持久化 | 仓库内源码集成（`thirdparty/sqlite3`） |
| `OpenSSL` | TLS（供 `curl` / `libwebsockets` 使用） | 各平台单独安装，由 CMake 查找 |

首次拉取代码后，先初始化 submodule：

```bash
git submodule update --init --recursive
```

说明：

- `sqlite3` 不是 submodule，而是直接随仓库源码一起维护
- `OpenSSL` 不随仓库分发，需在目标平台单独安装
- Windows 下如果找到了 OpenSSL 运行库，CMake 会将相关 DLL 复制到运行目录

---

## 系统要求

### 通用要求

- Git
- CMake 3.20+
- Ninja
- Python 3.7+

### Linux

- GCC 9+ 或 Clang 10+
- OpenSSL 开发包

示例：

```bash
sudo apt install build-essential cmake ninja-build python3 libssl-dev
```

### macOS

- Xcode 12+（包含 Apple Clang）
- OpenSSL

示例：

```bash
brew install cmake ninja openssl
```

如果 CMake 无法自动定位 Homebrew 的 OpenSSL，请设置 `OPENSSL_ROOT_DIR`。

### Windows

- Visual Studio 2019+ 或 Build Tools（包含 Desktop development with C++）
- CMake
- Ninja
- Python 3
- 单独安装 OpenSSL

说明：

- `tools/build-native.py` 会自动查找 Visual Studio 并调用 `vcvars64.bat`
- 如果 Visual Studio 安装在自定义位置，可设置 `VCVARS64_BAT`
- 如果 CMake 找不到 OpenSSL，可设置 `OPENSSL_ROOT_DIR`

---

## 快速开始

### 1. 初始化第三方代码

```bash
git submodule update --init --recursive
```

### 2. 安装平台工具链与 OpenSSL

按当前平台安装编译器、CMake、Ninja、Python 3 和 OpenSSL。

### 3. 执行构建

```bash
python3 tools/build-native.py --test
```

该命令会完成以下步骤：

1. 自动选择当前平台对应的 preset
2. 执行 CMake configure
3. 执行 CMake build
4. 在构建成功后运行对应 preset 的测试

默认行为：

- Linux: 默认选择 `linux-gcc-release`
- macOS: 默认选择 `macos-clang-release`
- Windows: 默认选择 `windows-msvc-release`

---

## 常用命令

### 默认 Release 构建

```bash
python3 tools/build-native.py
```

### 构建并运行测试

```bash
python3 tools/build-native.py --test
```

### Debug 构建

```bash
python3 tools/build-native.py --config debug
```

### Linux 上切换编译器

```bash
python3 tools/build-native.py --compiler gcc
python3 tools/build-native.py --compiler clang --config debug --test
```

`--compiler` 主要用于 Linux；Windows 会始终使用 MSVC，macOS 会始终使用 Clang。

### 指定 preset

```bash
python3 tools/build-native.py --preset windows-msvc-debug
```

### 列出可用 preset

```bash
python3 tools/build-native.py --list-presets
```

### 清理后重建

```bash
python3 tools/build-native.py --clean --test -j 8
```

---

## Native 预设

当前 Native 相关 preset 如下：

| 平台 | Debug | Release |
|------|-------|---------|
| Linux GCC | `linux-gcc-debug` | `linux-gcc-release` |
| Linux Clang | `linux-clang-debug` | `linux-clang-release` |
| macOS Clang | `macos-clang-debug` | `macos-clang-release` |
| Windows MSVC | `windows-msvc-debug` | `windows-msvc-release` |

脚本内部通过这些 preset 驱动 CMake，但日常使用建议仍然直接调用
`python3 tools/build-native.py`。

---

## 构建输出

- 当前 `CMakePresets.json` 的 `binaryDir` 固定为仓库根目录下的 `build/`
- 构建中间文件和产物都会生成在 `build/` 下
- `tools/build-native.py --clean` 会在 configure 前清理对应的构建目录

注意：

- 不同 Native preset 当前共享同一个 `build/` 目录
- 在不同 preset 之间切换时，建议带上 `--clean` 重新配置

---

## 故障排查

### 1. 提示 submodule 未初始化

执行：

```bash
git submodule update --init --recursive
```

顶层 `CMakeLists.txt` 会在以下目录缺失时直接报错：

- `thirdparty/curl`
- `thirdparty/glaze`
- `thirdparty/googletest`
- `thirdparty/libwebsockets`

### 2. 提示 `thirdparty/sqlite3` 缺失

`sqlite3` 不是 submodule，而是直接集成在仓库中的源码目录。请确认以下文件存在：

- `thirdparty/sqlite3/sqlite3.c`
- `thirdparty/sqlite3/sqlite3.h`
- `thirdparty/sqlite3/CMakeLists.txt`

### 3. 找不到 OpenSSL

请先在当前平台安装 OpenSSL，再确保 CMake 可以定位到它。

必要时可设置：

```bash
OPENSSL_ROOT_DIR=/path/to/openssl
```

Windows 下也可以将该变量指向 OpenSSL 安装根目录。

### 4. Windows 下找不到 `vcvars64.bat`

请确认已安装 Visual Studio 或 Build Tools 的 C++ 工具链。

如果安装路径不是默认位置，可设置：

```bash
VCVARS64_BAT=C:\Path\To\vcvars64.bat
```

### 5. 找不到 Ninja 或 CMake 版本过低

- 需要 CMake 3.20+
- 需要可执行的 `ninja`

可以先检查：

```bash
cmake --version
ninja --version
python3 --version
```

---

## 参考资料

- [CMake Presets Documentation](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
- [CMake Toolchains](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html)
