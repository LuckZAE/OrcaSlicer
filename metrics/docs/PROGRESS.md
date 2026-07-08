# snap_telemetry 当前进度

- 更新日期：2026-07-08
- 涉及仓库：`C:\workspace\code\metrics`（SDK）+ `C:\workspace\code\OrcaSlicer`（接入）
- 最近提交：metrics `afa8cf9` / OrcaSlicer `5bf8cbc535`

---

## 一句话状态

第一方数据采集 SDK 已完成构建与单测，已通过薄适配层接入 OrcaSlicer 并埋好用户主链路 7 个事件；本地 5 容器 PostHog 栈跑通端到端上报。Sentry 可选 sink 仅在设计文档中，尚未实现。

---

## 已完成

### 1. SDK（metrics 仓库）
独立、解耦的 C++17 静态库，不依赖任何 OrcaSlicer 符号。

| 模块 | 文件 | 职责 |
|------|------|------|
| 类型与接口 | `types.hpp` / `transport.hpp` / `consent.hpp` | Event/Config、事件名常量、JSON 序列化、ITransport / IConsentProvider |
| 事件队列 | `event_queue.hpp` | 有界 MPSC、线程安全、丢最旧 + dropped 计数、`set_notify()` 唤醒上报线程 |
| 后台批量 | `batch_uploader.hpp` | size/time 双触发、5 次指数退避重试（50/100/200/400/800ms）、失败落盘 spool + 启动回放 |
| 门面 | `telemetry.hpp` | Meyers 单例；隐私门、采样（thread_local mt19937）、上下文注入；`SNAP_TRACK` 宏 |
| 传输 | `http_transport.hpp` / `file_sink_transport.hpp` | PostHog `/batch` 上报（可注入 PostFn）/ JSONL 文件 |
| 上下文 | `event_context.hpp` | 持久化匿名 install_id/session_id、注入 os/ver —— **不采 pc_name** |

### 2. 单元测试
- **15 / 15 通过**（8 个测试套件，2026-07-08 实跑 `build/tests/snap_telemetry_tests.exe`）
- 覆盖：types、event_queue（含并发）、batch_uploader、telemetry_client、http_transport、file_sink_transport、event_context、smoke

### 3. OrcaSlicer 接入（适配层 `src/snap_telemetry/`）
全仓库唯一耦合点；删除该目录 SDK 即完全解耦。

- 生命周期：`GUI_App::on_init_inner()` 调 `telemetry_init()`（隐私同意同步之后，触发 `app_start`）；`OnExit()` 调 `telemetry_shutdown()` 排空。
- 隐私门独立：`SlicerConsent` 读 `get_privacy_policy()`。
- 构建：`src/CMakeLists.txt` 链接 `../metrics/build_msvc` 的 `snap_telemetry.lib`（Release/Debug）。
- 端点/Key 可经环境变量覆盖：`SNAP_TELEMETRY_ENDPOINT` / `SNAP_TELEMETRY_KEY`。

### 4. 埋点清单（7 个事件）

| 事件 | 触发位置 | 关键属性 |
|------|----------|----------|
| `app_start` | `telemetry_adapter.cpp` (init) | — |
| `project_open` | `GUI_App::load_project` | file_ext |
| `project_opened` | `Plater::load_files` | file_ext, file_count, object_count |
| `slice_completed` | `Plater::on_process_completed` | duration_ms |
| `device_connect` | `DeviceManager::insert_local_device`（仅首次发现） | net_type |
| `connect_attempt` | `MachineObject::connect` | connection_type, is_anonymous, use_ssl |
| `connect_success` | `MachineObject::parse_json`（parse_msg_count==1） | connection_type |

用户主漏斗：`app_start → project_open → project_opened → slice_completed`
设备连接漏斗：`device_connect → connect_attempt → connect_success`

### 5. 本地 PostHog 栈（`deploy/`）
5 容器 docker-compose：`db`(postgres) + `redis` + `clickhouse`(23.12) + `kafka`(redpanda) + `posthog`，含 cluster/user/kafka XML 配置与 mock 收发脚本。
- 端到端链路 `/batch → Kafka → ClickHouse sharded_events` 已跑通（事件曾确认进入 team_id=1）。
- 原生 PostHog 面板（`/dashboard/1`，7 个 insight）已建。**注意：默认 7 天时间范围下事件稀疏会显示空，需把右上角时间范围调成 Last 24 hours / Today。**

---

## 待办 / 下一步

| 项 | 状态 | 说明 |
|----|------|------|
| Sentry 可选 sink | 🔲 未实现 | 设计文档（见下）提及作为可选第二通道，代码尚未落地 |
| 生产端点 | 🔲 待配置 | 当前默认 `http://localhost:8000/batch` + 本地 Key；上线前需通过环境变量切到真实端点 |
| 数据分区落地 | 🔲 仅设计 | 设计文档有海外/大陆数据分区建议，当前只有单本地实例 |
| 采样率 | ⚙️ 当前 1.0 | 生产需按量级下调 `sample_rate` |
| OrcaSlicer `MQTT.cpp` | ⚠️ 工作区游离 | 一行无关 log 级别改动（debug→warning），按用户决定暂留工作区未提交 |

---

## 相关文档（均已统一在本目录）

- 设计方案：[`superpowers/specs/2026-06-30-snapmaker-telemetry-client-design.md`](superpowers/specs/2026-06-30-snapmaker-telemetry-client-design.md)
- SDK 实现计划：[`superpowers/plans/2026-06-30-snap-telemetry-sdk.md`](superpowers/plans/2026-06-30-snap-telemetry-sdk.md)
- 连接流程埋点设计：[`superpowers/specs/2026-07-03-connect-flow-telemetry-design.md`](superpowers/specs/2026-07-03-connect-flow-telemetry-design.md)
