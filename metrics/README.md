# snap_telemetry

Snapmaker 第一方客户端遥测 SDK（采集→有界缓冲→后台批量→可插拔上报）。
完全自包含、可脱离 OrcaSlicer 独立编译与单测。

## 构建与测试

```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

依赖由 CMake FetchContent 自动拉取（nlohmann/json、GoogleTest）。测试可执行文件静态链接 MinGW 运行时，无外部 DLL 依赖。

## 用法

```cpp
#include "snap_telemetry/telemetry.hpp"

snap::Config c; c.data_dir = "<app-data-dir>"; c.app_ver = "2.3.1";
auto consent = std::make_shared<MyConsent>();           // 桥接宿主隐私开关
snap::TelemetryClient::instance().init(
    c, std::make_unique<snap::FileSinkTransport>("telemetry.jsonl"), consent);
snap::TelemetryClient::instance().context().set_app("2.3.1","Windows","10.0.19045");

SNAP_TRACK(snap::events::kAppStart, {});
SNAP_TRACK(snap::events::kSliceCompleted, {{"duration_ms", 1234}, {"object_count", 3}});

// 退出前：
snap::TelemetryClient::instance().shutdown();
```

## 上报通道（可插拔 ITransport）

- `FileSinkTransport` — 追加写本地 JSONL（零依赖默认，调试用）。
- `HttpTransport` — 按 PostHog capture 协议批量 POST；构造时注入 `PostFn`（宿主的 HTTP 客户端）。切换到 PostHog 见 `deploy/README.md`。

## 隐私

- 不采集 `pc_name` 或任何 PII；`install_id` 为匿名 UUID。
- 隐私门（`IConsentProvider`）为假时，事件直接丢弃、零上报。

## 事件信封

```json
{ "event":"slice_completed", "ts":1719739200123,
  "props":{"duration_ms":1234},
  "ctx":{"app_ver":"2.3.1","os":"Windows","os_ver":"10.0.19045",
         "install_id":"<anon-uuid>","session_id":"<uuid>","user_id":""} }
```
