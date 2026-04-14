# 异步回调错误处理（已废弃）

本文档原先描述的 `parent + handle error buffer` 方案已经不再适用，仅保留此文件作为迁移说明。

## 当前实现

- 异步错误通过各模块回调结构中的 `on_error` 直接返回。
- `core/src/c_api/handles_c.h` 中各类子 handle 只保留 `impl` 指针，不再持有 `parent`。
- `AnyChatClient_T` 不再维护异步错误缓冲区，也不再维护用于异步回调的 token 共享缓冲区。

## 调整原因

- 错误已经沿当前异步回调链直接透传，不再需要在 client handle 上保存中间错误字符串。
- token 等结果数据在各自成功回调中即时拷贝即可，不再依赖共享缓冲区跨回调存活。
- 旧设计会增加 handle 定义复杂度，也容易让后续实现误以为仍需维护共享状态。

## 现在应遵循的约定

- 失败路径直接调用 `on_error(code, message)`。
- 成功路径在回调内把需要暴露给 C API 的数据转换为对应的 C 结构后立即回调。
- 不要再为异步错误或 token 结果新增挂在 `AnyChatClient_T` 上的共享缓冲区。

## 相关代码

- `core/src/c_api/auth_c.cpp`
- `core/src/c_api/client_c.cpp`
- `core/src/c_api/handles_c.h`
- `core/src/c_api/utils_c.h`

## 更新时间

- 2026-04-14
