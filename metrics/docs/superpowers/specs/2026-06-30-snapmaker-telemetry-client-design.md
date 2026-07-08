# Snapmaker 第一方数据采集客户端（snap_telemetry）设计方案

- 日期：2026-06-30
- 状态：已评审通过，待进入实现计划
- 适用仓库：`C:\workspace\code\metrics`（新建 SDK）+ `OrcaSlicer`（薄适配层接入）
- 变更记录：2026-06-30 增补 —— `os` 拆分为 `os`/`os_ver`；明确**不采** `pc_name`（匿名基线+出境合规）；原型纳入 `HttpTransport` + 本地 Docker 自托管 PostHog 端到端验证；新增海外/大陆数据分区落地建议。

---

## 1. 背景与问题

OrcaSlicer 是 Snapmaker 维护的桌面切片软件分支（upstream: `Snapmaker/OrcaSlicer`）。现状：

- `src/bury_cfg/bury_point.{cpp,hpp}`：仅定义事件名常量（`BP_START_SOFT`、`BP_SLICE_DURATION`、`BP_DEIVCE_CONNECT`…）、隐私同意开关、时间戳工具。**没有任何上报逻辑**，部分埋点（如切片时长 `Plater.cpp:13954`）处于注释禁用状态。
- 真实事件上报走 `NetworkAgent::track_event()` → `bambu_network_track_event`，落在 **BambuLab 闭源网络插件**里，非 Snapmaker 自有**。
- `sentry_wrapper/` 负责崩溃上报，Sentry 项目为 Snapmaker 自有（DSN 在 `SentryWrapper.cpp:88`，ingest 在美区）。
- 隐私同意已打通：`GUI_App.cpp:7158` 设置同意 → `SentryWrapper.cpp:345` 据此放行；Web 侧通过 SSWCP `sw_SubUserUpdatePrivacy` / `sw_GetUserUpdatePrivacy` 联动。

**问题**：Snapmaker 没有第一方、可控、可分析的数据采集能力——产品使用数据流经 Bambu 闭源插件，埋点系统半成品且禁用。

## 2. 任务范围

- **做**：在 OrcaSlicer 客户端搭建 Snapmaker 第一方数据采集 SDK——采集 → 缓冲 → 批量 → 可插拔上报 → 隐私门；并在本地用 Docker 自托管 PostHog 验证整条上报链路。
- **不做（本次）**：生产级后端 ingestion 服务运维 / 数据仓库 / 看板搭建（以选型与分区落地建议形式写入第 10 节）。
- **交付物**：(1) 本设计文档；(2) 可运行的最小原型纵向切片（含本地 Docker PostHog 验证）。

## 3. 核心原则（解耦）

> SDK 完全自包含、零依赖任何现有 Snapmaker 代码。与 OrcaSlicer 的全部耦合收敛到**单一薄适配层**；只能由适配层通过依赖注入向 SDK 提供外部信息，SDK 绝不反向调用现有符号。

删除适配层后，OrcaSlicer 即回到原样，`bury_point` 一字未动。

| # | 维度 | 决策 |
|---|---|---|
| D1 | 事件命名 | **全新**一套事件常量，不引用 `bury_point.hpp` |
| D2 | 隐私门 | 数据源用**现有** `get_privacy_policy()`，但经 `IConsentProvider` 由适配层注入；SDK 不依赖 bury_point |
| D3 | 上下文 | 数据源用**现有**登录态等；`user_id`/`app_ver`/`os_ver` 经适配层 `setContext()` 注入；SDK 自管匿名 `install_id` / `session_id`；**不采 `pc_name`** |
| D4 | 集成隔离 | 适配层置于**独立新目录** `src/snap_telemetry/`；SDK 作独立 static lib 链入；不碰现有模块内部符号 |
| D5 | 上报通道 | 原型做 **`FileSinkTransport`（零依赖默认）+ `HttpTransport`（PostHog capture 协议）**，并用本地 Docker 自托管 PostHog 验证端到端；Sentry 列为未来独立实现（自带配置，不复用 `SentryWrapper`） |
| D6 | 原型埋点 | 3 个点位只通过适配层调用新 SDK API，完全不触碰 `bury_point` 与 Bambu `track_event` |

## 4. 架构与模块边界

```
metrics/                              ← 独立 SDK 仓库（可脱离 OrcaSlicer 编译/单测）
  include/snap_telemetry/
    telemetry.hpp                     公共门面 + SNAP_TRACK 宏
    transport.hpp                     ITransport 接口
    consent.hpp                       IConsentProvider 接口
    types.hpp                         Event / Context / Config 结构
  src/
    TelemetryClient.cpp               门面: init/track/flush/shutdown，单例
    EventQueue.cpp                    线程安全有界缓冲（满了丢最旧 + dropped 计数）
    BatchUploader.cpp                 后台线程: 满 N 条 或 每 T 秒 → 取批 → transport.send
    EventContext.cpp                  install_id/session_id 自管 + 外部注入字段合并
    FileSinkTransport.cpp             桩 sink：原子追加写本地 JSONL（零依赖默认）
    HttpTransport.cpp                 PostHog capture 协议批量 POST（原型纳入，验证用）
    Config.cpp                        配置默认值与加载
  tests/                              GoogleTest 单测
  deploy/
    docker-compose.posthog.yml        本地自托管 PostHog（端到端验证用）
  CMakeLists.txt                      产出 static lib：snap_telemetry
  README.md

OrcaSlicer/src/snap_telemetry/        ← 唯一耦合点（编进 Snapmaker_Orca 目标）
  telemetry_adapter.{hpp,cpp}
    - 实现 IConsentProvider → 调用现有 get_privacy_policy()
    - setContext(): app_ver、os、os_ver、user_id(若登录) 注入；不采 pc_name
    - 选择并配置 Transport（FileSink / HttpTransport）
    - 提供 init() / shutdown() 供 GUI_App 调用
```

依赖：`nlohmann/json`（OrcaSlicer 已用）+ `std::thread`；`HttpTransport` 用一个轻量 HTTP 客户端（优先复用 OrcaSlicer 已有的，SDK 内以接口隔离）。

### 模块职责

- **TelemetryClient**：门面/单例。`init(Config, ITransport, IConsentProvider)`、`track(name, props)`、`flush(timeout)`、`shutdown()`。
- **EventQueue**：有界 MPSC 缓冲；`push` O(1) 非阻塞；满则丢最旧并累加 `dropped`。
- **BatchUploader**：独立后台线程；触发条件「攒满 N 条」或「每 T 秒」；调用 `ITransport::send(batch)`；失败指数退避重试，超上限落盘 spool。
- **EventContext**：自管匿名 `install_id`（首次生成存本地、UUID、无 PII）与 `session_id`；合并适配层注入的 `app_ver`/`os`/`os_ver`/`user_id`。
- **ITransport**：`send(const std::vector<Event>&) -> Result`。实现：`FileSinkTransport`、`HttpTransport`（原型）；未来 `SentryTransport`。
- **IConsentProvider**：`bool is_allowed()`。适配层实现，桥接现有同意开关。
- **Config**：`endpoint`、`api_key`、`batch_size=20`、`flush_interval_s=30`、`queue_cap=1000`、`sample_rate=1.0`、`spool_path`。原型硬编码/本地配置，未来从 `meta-cfg` 远程覆盖。

## 5. 事件模型与采集 API

统一事件信封：

```json
{
  "event": "slice_completed",
  "ts": 1719739200123,
  "props": { "duration_ms": 1234, "object_count": 3 },
  "ctx": {
    "app_ver": "2.x.y",
    "os": "Windows",
    "os_ver": "10.0.19045",
    "install_id": "<匿名 uuid>",
    "session_id": "<本次启动 uuid>",
    "user_id": "<可空>"
  }
}
```

### 字段字典

| 字段 | 含义 | 谁填 |
|---|---|---|
| `event` | 事件名，区分"发生了什么"（如 `slice_completed`） | 埋点处指定 |
| `ts` | 事件发生时刻，Unix epoch **毫秒** | SDK |
| `props` | 该事件特有属性（随事件而变；如切片耗时、对象数） | 埋点处传入 |
| `ctx.app_ver` | 软件版本号，用于按版本对比 | 适配层注入 |
| `ctx.os` | 操作系统平台（`Windows`/`macOS`/`Linux`） | SDK 判断 |
| `ctx.os_ver` | 系统版本/build（如 `10.0.19045`） | 适配层注入（wx 获取） |
| `ctx.install_id` | **匿名**安装 ID（随机 UUID，无 PII），用于匿名去重/留存 | SDK 自管 |
| `ctx.session_id` | 本次启动会话 ID，串联同一次运行内的事件 | SDK 每次启动生成 |
| `ctx.user_id` | 登录用户 ID（仅登录 `id.snapmaker` 时有，登出清空，可空） | 适配层注入 |

> **明确不采 `pc_name`（主机名）**：主机名常含真实姓名，属个人信息（PII），与匿名基线冲突且加重大陆出境合规负担；机器去重由匿名 `install_id` 完成。

采集 API（埋点处一行）：

```cpp
SNAP_TRACK("slice_completed", {{"duration_ms", elapsed}, {"object_count", n}});
// 等价：snap::TelemetryClient::instance().track("slice_completed", props);
```

事件名由 **SDK 自有常量**定义（如 `snap::events::kAppStart`），**不复用** `BP_*`。

## 6. 数据流与错误处理

1. `track()`（调用线程，O(1) 非阻塞）：`IConsentProvider::is_allowed()` 为假→直接丢弃；否则补 `ctx`、按 `sample_rate` 采样、`EventQueue::push`。**绝不阻塞 UI / 切片线程。**
2. `BatchUploader` 后台线程：满 N 条**或**每 T 秒→取一批→`transport.send(batch)`。
3. 失败：指数退避重试；超上限→落盘到 spool 文件（离线缓冲），下次启动重发。
4. 队列有界：满→丢最旧 + `dropped` 计数（防内存膨胀）。
5. 关闭：`flush()` + join（带超时）。
6. **所有异常吞在 SDK 内**，不外抛、不崩溃 slicer。

## 7. 隐私与配置（D2/D3：源用现有，注入解耦）

- **隐私门**：适配层实现 `IConsentProvider::is_allowed()` → 返回 `get_privacy_policy()`。同意=false 时事件连队都不入。无需新 UI（沿用 `GUI_App.cpp:7158` 既有同意流）。
- **install_id**：SDK 自管匿名 UUID，首次启动生成存本地，无 PII。
- **session_id**：SDK 每次启动生成。
- **os / os_ver**：`os` 由 SDK 判断平台；`os_ver`（精确 build 号）由适配层经 `wxGetOsDescription()` / `wxPlatformInfo` 注入。
- **user_id**：适配层在登录（`id.snapmaker`）后 `setContext({user_id})`，登出清除。
- **不采 `pc_name`**：见第 5 节说明。
- **Config 默认值**：`batch=20`、`flush=30s`、`queue_cap=1000`、`sample_rate=1.0`；原型本地配置，未来 `meta-cfg` 远程覆盖。

## 8. 原型纵向切片（可 demo 的最小集）

采 3 个真实事件（**均经适配层调用新 SDK API**）：

| 事件（新名） | 埋点位置 | props 示例 |
|---|---|---|
| `app_start` | `GUI_App` on_init | `{}` |
| `slice_completed` | `Plater` 切片完成（`Plater.cpp:13954` 附近，新增调用，不复用注释代码） | `{duration_ms, object_count}` |
| `device_connect` | `DeviceManager` 连接成功 | `{net_type}` |

**两路验证**：
1. `FileSinkTransport` → 本地 `telemetry.jsonl` 出现这 3 类事件且带完整 `ctx`（离线、零依赖验证）。
2. `HttpTransport` → 指向本地 Docker 自托管 PostHog（`deploy/docker-compose.posthog.yml`），在 **PostHog 面板里看到事件流**（端到端、可视化 demo，同时验证未来后端选型）。

**Demo 成功标准**：切换 transport，两路都能看到 3 类事件，`ctx` 字段齐全、隐私关闭时零上报。

## 9. 测试（GoogleTest，独立于 slicer）

- `EventQueue`：并发 push/pop、有界丢弃、`dropped` 计数。
- `BatchUploader`：按量触发、按时触发、退避重试、spool 落盘+重载。
- `IConsentProvider` 为假 → 不入队。
- `FakeTransport` 校验批次内容与信封 schema。
- `HttpTransport`：对 mock HTTP server 验证 PostHog 载荷格式（event/properties/timestamp/distinct_id）。
- 端到端：track N 条 → FileSink → 校验 JSONL 行数与字段。
- 目标覆盖率 ≥80%。

## 10. 后端落地建议（写入文档，本次不做生产运维）

### 10.1 平台选型：自托管 PostHog（推荐）

- `HttpTransport` 按 PostHog capture 协议：`POST https://<host>/i/v0/e/`，body `{api_key, batch:[{event, properties, timestamp, distinct_id}]}`。
- `distinct_id = install_id`（匿名）或 `user_id`；信封 `props` → PostHog `properties` 直接映射。
- PostHog 自带漏斗/留存/趋势/会话面板，自托管满足数据自有与合规。
- 原型即用其官方 `docker-compose` 在本地验证；上生产时同一套部署到服务器即可。
- 备选：自建 `api.snapmaker.com/events` 轻量 ingestion → ClickHouse → Grafana/Metabase（数据自有最彻底，但需后端团队搭建）。

### 10.2 海外 / 大陆：数据分区落地（data residency）

- **按区域路由 endpoint**：大陆用户 → 国内区服务器（阿里云/腾讯云国内区）；海外用户 → 海外区服务器（AWS/GCP 海外区）。对齐现有 `api.snapmaker.cn` / `api.snapmaker.com` 双域名体系。
- **SDK 侧零改动**：只是 `Config.endpoint` 不同，由适配层按区域选择；transport 实现不变。
- **区域判定**：优先跟随账号体系（登录 `.cn` vs `.com`）；未登录用 locale / 安装渠道粗判并给默认。
- **为何必须分区**：大陆访问海外延迟高、链路不稳；更关键的是**大陆个人信息出境**需走合规评估（标准合同/安全评估）。
- **降低出境与合规压力的根本手段 = 最小化 + 匿名化采集**：只采匿名 `install_id`、不采 `pc_name` 原文，数据不构成"个人信息"，跨境与合规负担大幅下降（与第 5/7 节决策一致）。

## 11. OrcaSlicer 集成步骤（最小侵入）

1. `snap_telemetry` 作 static lib，在 `src/CMakeLists.txt:118` 链入 `Snapmaker_Orca` 目标。
2. 新增 `src/snap_telemetry/telemetry_adapter.{hpp,cpp}`（编进 `Snapmaker_Orca`）：实现 consent provider、注入 context（含 os_ver）、配置 Transport、暴露 init/shutdown。
3. `GUI_App` on_init 调 `init()`；3 个埋点处各加一行 `SNAP_TRACK(...)`；退出处 `flush()/shutdown()`。
4. **不动** Bambu `track_event` 路径（共存）；**不改** `bury_point` 与 OrcaSlicer 主体逻辑。

## 12. 非目标 / YAGNI

- 不做远程配置拉取（仅预留 Config 接口）。
- 不做实时流 / WebSocket 上报。
- 不做 Sentry transport 的原型实现（仅接口预留）。
- 不采集 `pc_name` 或任何 PII；不做用户级精确画像。
- 不做生产后端的运维/扩容/告警（仅本地 Docker 验证 + 选型建议）。
- 不重构现有 `bury_point` 或 Bambu 上报路径。

## 13. 风险与缓解

| 风险 | 缓解 |
|---|---|
| 后台线程在 slicer 退出时未 join 导致崩溃 | `shutdown()` 带超时 join；析构兜底 |
| 高频事件压垮队列 | 有界队列丢最旧 + 采样率 + `dropped` 计数可观测 |
| install_id 被误当 PII | 匿名 UUID、隐私政策声明；同意=false 全程不采；明确不采 pc_name |
| 大陆个人信息出境合规 | 数据分区落地（.cn/.com 路由）+ 最小化/匿名化采集 |
| 与 OrcaSlicer 构建集成失败 | SDK 先独立编译/单测通过，再以 static lib 链入，问题隔离 |
| 本地 Docker PostHog 资源占用 | 仅验证期启动；CI/常态用 mock HTTP server 替代 |
