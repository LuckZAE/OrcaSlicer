# 设备连接主链路埋点（connect_attempt / connect_success）设计

- 日期：2026-07-03
- 状态：已评审通过，待进入实现计划
- 仓库：OrcaSlicer（埋点）+ metrics（SDK 事件常量）
- 依赖：计划二已完成的 snap_telemetry SDK 集成（`src/snap_telemetry/`）

---

## 1. 背景

计划二已完成 SDK 集成，现有埋点：`app_start`、`slice_completed`、`device_connect`（设备发现）。
本次给"设备连接主链路"补两个节点，构成完整连接漏斗，用于分析联机转化率与耗时。

## 2. 范围

**做**：给设备连接主链路加 `connect_attempt`（发起连接）+ `connect_success`（连接成功）两个埋点。
**不做**：账号登录、绑定、断线/重连、失败上报（用户已明确排除）。

## 3. 漏斗

```
device_connect (发现，已有) → connect_attempt (尝试，新增) → connect_success (成功，新增)
```

- 三步转化率：每步丢多少用户。
- `connect_attempt` → `connect_success` 耗时：PostHog 按 `dev_id` + 时间戳 join 后端算（方案 A，客户端无状态）。

## 4. 事件定义（方案 A：纯离散事件）

| 事件名 | 触发点 | props |
|---|---|---|
| `connect_attempt` | `MachineObject::connect()` 调 `connect_printer` 前 | `dev_id`、`connection_type`(lan/cloud)、`is_anonymous`、`use_ssl` |
| `connect_success` | MachineObject 收到设备**首次**推送消息（`parse_msg_count == 1`） | `dev_id`、`connection_type` |

## 5. 关键设计决策

### 5.1 方案 A（离散事件，后端算耗时）而非 B/C
- **A（选定）**：每节点独立事件，客户端无状态、多设备并发零复杂度、与现有埋点风格一致；耗时由 PostHog 后端按 `dev_id`+时间戳 join。
- B（客户端算耗时）：需维护 `dev_id→start_time` map + 超时清理，有状态易出 bug。
- C（单事件 + status 枚举）：与现有离散命名风格不一致，漏斗需先按 status 拆分。

### 5.2 `connect_success` 用 `parse_msg_count == 1` 判首次
MachineObject 已有 `parse_msg_count` 成员（每条推送 ++，初始 0）。收首条设备消息时值为 1，即"首次联机成功"信号——零新成员、零状态管理。在消息处理点（`set_online_state(true)` 后、`parse_msg_count++` 后）判断 `== 1` 上报。

### 5.3 `dev_id`（设备序列号）采集
设备标识、非个人标识（PII），用于漏斗 join 必需。与已有匿名 `install_id`（UUID）同类，符合现有隐私基线（不采 `pc_name`/账号）。`user_id` 已由 `ctx` 在登录后自动附加，事件不重复带。

## 6. 埋点位置（OrcaSlicer）

| 事件 | 文件 | 位置 |
|---|---|---|
| `connect_attempt` | `src/slic3r/GUI/DeviceManager.cpp` `MachineObject::connect()` | 调 `m_agent->connect_printer(...)` 前 |
| `connect_success` | `src/slic3r/GUI/DeviceManager.cpp` 消息处理 | `set_online_state(true)` + `parse_msg_count++` 后，`if (parse_msg_count == 1)` |

两处各加一行 `SNAP_TRACK`，通过已 `#include "snap_telemetry/telemetry_adapter.hpp"`（计划二已加）。

## 7. SDK 侧改动（metrics）

`include/snap_telemetry/types.hpp` 的 `snap::events` 命名空间加：
```cpp
constexpr const char* kConnectAttempt = "connect_attempt";
constexpr const char* kConnectSuccess = "connect_success";
```
与 `kAppStart`/`kSliceCompleted`/`kDeviceConnect` 并列。无需改其他模块。

## 8. 不改的东西
- `MachineObject::connect` 连接逻辑不动、MQTT 层不动、`is_connected()` 不动。
- 不加新成员变量、不加状态管理、不加 once 标志（复用 `parse_msg_count`）。
- 隐私门、采样、批量上报等机制沿用 SDK 现有设计。

## 9. 验证
- 构建 OrcaSlicer `Snapmaker_Orca.dll` 零错误。
- 运行 OrcaSlicer 连接一台设备 → PostHog 面板出现 `connect_attempt` + `connect_success`，`dev_id` 一致，`connection_type` 正确。
