# AnyChat SDK 构建指南

本项目使用 **CMake Presets** 标准化跨平台构建过程，支持 Windows MSVC、Linux GCC/Clang、macOS Clang 等多种编译器。

## 目录

- [系统要求](#系统要求)
- [快速开始](#快速开始)
- [平台构建](#平台构建)
- [CMake Presets](#cmake-presets)
- [构建脚本](#构建脚本)
- [高级用法](#高级用法)

---

## 系统要求

### 通用要求

- **CMake** >= 3.20
- **Ninja** 构建系统
- **Python** 3.7+ (用于构建脚本)

### 平台特定要求

#### Linux
- GCC 9+ 或 Clang 10+
- 依赖库：
  ```bash
  # Ubuntu/Debian
  sudo apt install cmake ninja-build libssl-dev

  # Fedora/RHEL
  sudo dnf install cmake ninja-build
  ```

#### macOS
- Xcode 12+ (包含 Apple Clang)
- 依赖库（通过 Homebrew）:
  ```bash
  brew install cmake ninja
  ```

#### Windows
- Visual Studio 2019+ (MSVC v142+)
- 依赖库：建议使用 vcpkg
  ```powershell
  vcpkg install curl:x64-windows libwebsockets:x64-windows sqlite3:x64-windows
  ```

#### Android
- Android NDK r21+
- 设置环境变量：`export ANDROID_NDK_HOME=/path/to/ndk`

#### iOS
- macOS with Xcode 12+

#### WebAssembly
- Emscripten SDK 3.0+
- 激活 Emscripten：`source /path/to/emsdk/emsdk_env.sh`

---

## 快速开始

### 1. 使用 CMake Presets（推荐）

列出可用的预设：
```bash
cmake --list-presets
```

配置 + 构建 + 测试（一行命令）：
```bash
# Linux GCC Release
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release -j
ctest --preset linux-gcc-release

# macOS Clang Debug
cmake --preset macos-clang-debug
cmake --build --preset macos-clang-debug -j
ctest --preset macos-clang-debug

# Windows MSVC Release
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release -j
ctest --preset windows-msvc-release
```

### 2. 使用构建脚本（更简单）

```bash
# 自动检测平台和编译器
python3 tools/build-native.py --test

# 指定编译器（Linux）
python3 tools/build-native.py --compiler clang --config debug --test

# 清理后重新构建
python3 tools/build-native.py --clean --config release -j 8
```

---

## 平台构建

### Native（Linux / macOS / Windows）

```bash
# 基本构建（自动检测平台和编译器）
python3 tools/build-native.py

# 列出可用预设
python3 tools/build-native.py --list-presets

# 指定配置
python3 tools/build-native.py --config debug

# Linux: 指定编译器
python3 tools/build-native.py --compiler gcc     # GCC
python3 tools/build-native.py --compiler clang   # Clang

# 运行测试
python3 tools/build-native.py --test

# 完整示例
python3 tools/build-native.py --compiler clang --config debug --clean --test -j 8
```

### Android

```bash
# 构建 ARM64
python3 tools/build-android.py --abi arm64-v8a

# 构建 ARMv7
python3 tools/build-android.py --abi armeabi-v7a

# 构建所有 ABI
python3 tools/build-android.py --abi all

# Debug 构建
python3 tools/build-android.py --config debug --abi all

# 列出可用预设
python3 tools/build-android.py --list-presets
```

### iOS

```bash
# Release 构建
python3 tools/build-ios.py

# Debug 构建
python3 tools/build-ios.py --config debug

# 清理重建
python3 tools/build-ios.py --clean
```

### WebAssembly

```bash
# 激活 Emscripten
source /path/to/emsdk/emsdk_env.sh

# Release 构建
python3 tools/build-web.py

# Debug 构建
python3 tools/build-web.py --config debug
```

---

## CMake Presets

### 可用预设列表

#### Linux
- `linux-gcc-debug` - Linux GCC Debug
- `linux-gcc-release` - Linux GCC Release
- `linux-clang-debug` - Linux Clang Debug
- `linux-clang-release` - Linux Clang Release

#### macOS
- `macos-clang-debug` - macOS Clang Debug (Universal Binary: arm64 + x86_64)
- `macos-clang-release` - macOS Clang Release (Universal Binary: arm64 + x86_64)

#### Windows
- `windows-msvc-debug` - Windows MSVC Debug
- `windows-msvc-release` - Windows MSVC Release

#### Android
- `android-arm64-debug` / `android-arm64-release` - ARM64 (arm64-v8a)
- `android-armv7-debug` / `android-armv7-release` - ARMv7 (armeabi-v7a)

#### iOS
- `ios-debug` - iOS Debug (Device + Simulator)
- `ios-release` - iOS Release (Device + Simulator)

#### WebAssembly
- `web-debug` - WebAssembly Debug
- `web-release` - WebAssembly Release

### Preset 结构

所有预设定义在项目根目录的 `CMakePresets.json` 文件中，包含：

- **configurePresets**: 配置阶段预设（编译器、工具链、构建类型）
- **buildPresets**: 构建阶段预设
- **testPresets**: 测试阶段预设

预设继承关系：
```
base (hidden)
 ├─ debug-base (hidden)
 │   ├─ linux-gcc-debug
 │   ├─ linux-clang-debug
 │   ├─ macos-clang-debug
 │   └─ ...
 └─ release-base (hidden)
     ├─ linux-gcc-release
     ├─ linux-clang-release
     └─ ...
```

---

## 构建脚本

### `tools/build-native.py`

Native 平台（Linux / macOS / Windows）构建脚本。

**用法**:
```bash
python3 tools/build-native.py [options]
```

**选项**:
- `--preset PRESET` - 指定 CMake preset（自动检测如不指定）
- `--compiler {gcc,clang,msvc}` - 指定编译器（仅 Linux）
- `--config {debug,release}` - 构建配置（默认 release）
- `-j, --jobs N` - 并行任务数（默认 CPU 核心数）
- `--test` - 构建后运行测试
- `--clean` - 构建前清理目录
- `--list-presets` - 列出可用预设

### `tools/build-android.py`

Android 平台构建脚本。

**用法**:
```bash
python3 tools/build-android.py [options]
```

**选项**:
- `--abi {arm64-v8a,armeabi-v7a,all}` - 目标 ABI（默认 arm64-v8a）
- `--config {debug,release}` - 构建配置（默认 release）
- `-j, --jobs N` - 并行任务数
- `--clean` - 构建前清理目录
- `--list-presets` - 列出可用预设

**环境变量**:
- `ANDROID_NDK_HOME` - Android NDK 路径（必需）

### `tools/build-ios.py`

iOS 平台构建脚本。

**用法**:
```bash
python3 tools/build-ios.py [options]
```

**选项**:
- `--config {debug,release}` - 构建配置（默认 release）
- `--clean` - 构建前清理目录
- `--list-presets` - 列出可用预设

### `tools/build-web.py`

WebAssembly 平台构建脚本。

**用法**:
```bash
python3 tools/build-web.py [options]
```

**选项**:
- `--config {debug,release}` - 构建配置（默认 release）
- `--clean` - 构建前清理目录
- `--list-presets` - 列出可用预设

**环境变量**:
- `EMSDK` - Emscripten SDK 路径（必需）

---

## 高级用法

### 直接使用 CMake

如果不想用 Python 脚本，可以直接使用 CMake:

```bash
# 1. 配置
cmake --preset linux-gcc-release

# 2. 构建
cmake --build --preset linux-gcc-release --parallel 8

# 3. 测试
ctest --preset linux-gcc-release --output-on-failure

# 4. 安装（可选）
cmake --install build/linux-gcc-release --prefix /usr/local
```

### 自定义预设

创建 `CMakeUserPresets.json`（不提交到 Git）来定义个人预设：

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "my-custom-preset",
      "inherits": "linux-gcc-debug",
      "cacheVariables": {
        "CMAKE_CXX_FLAGS": "-Wall -Wextra -O0 -g3"
      }
    }
  ]
}
```

### 交叉编译

对于 Android 和 iOS，CMake 自动使用 toolchain 文件：

- **Android**: `$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake`
- **iOS**: `cmake/ios.toolchain.cmake`（项目自带）

### 构建产物

所有构建产物位于 `build/<preset-name>/` 目录：

```
build/
├── linux-gcc-release/       # Linux GCC Release
│   ├── bin/
│   │   └── anychat_core_tests
│   └── lib/
│       └── libanychat_core.a
├── android-arm64-release/   # Android ARM64 Release
│   └── ...
└── ...
```

### 持续集成（CI）

示例 GitHub Actions workflow:

```yaml
name: Build

on: [push, pull_request]

jobs:
  linux:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install dependencies
        run: sudo apt install libcurl4-openssl-dev libwebsockets-dev libsqlite3-dev ninja-build
      - name: Build
        run: python3 tools/build-native.py --test

  macos:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install dependencies
        run: brew install curl libwebsockets sqlite ninja
      - name: Build
        run: python3 tools/build-native.py --test

  windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Setup MSVC
        uses: ilammy/msvc-dev-cmd@v1
      - name: Install dependencies
        run: vcpkg install curl:x64-windows libwebsockets:x64-windows sqlite3:x64-windows
      - name: Build
        run: python tools/build-native.py --test
```

---

## 故障排查

### 常见问题

1. **找不到 Ninja**
   ```bash
   # Linux
   sudo apt install ninja-build
   # macOS
   brew install ninja
   # Windows
   choco install ninja
   ```

2. **CMake 版本过低**
   - 需要 CMake 3.20+
   - 从 https://cmake.org/download/ 下载最新版

3. **找不到依赖库**
   - 检查是否安装了 libcurl、libwebsockets、sqlite3
   - 设置 `CMAKE_PREFIX_PATH` 指向依赖安装路径

4. **Android NDK 未设置**
   ```bash
   export ANDROID_NDK_HOME=/path/to/android-ndk
   ```

5. **Emscripten 未激活**
   ```bash
   source /path/to/emsdk/emsdk_env.sh
   ```

### 清理构建

```bash
# 删除所有构建目录
rm -rf build/

# 或使用脚本清理
python3 tools/build-native.py --clean
```

---

## 参考资料

- [CMake Presets Documentation](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
- [CMake Toolchains](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html)
- [Android NDK CMake Guide](https://developer.android.com/ndk/guides/cmake)
- [Emscripten Documentation](https://emscripten.org/docs/getting_started/index.html)
