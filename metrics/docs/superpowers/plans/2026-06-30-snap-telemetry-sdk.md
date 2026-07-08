# snap_telemetry SDK 实现计划（计划一 / 共二）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `C:\workspace\code\metrics` 构建一个自包含、可独立编译与单测的 C++ 遥测客户端 SDK（采集→有界缓冲→后台批量→可插拔上报），含 FileSink 与 HttpTransport(PostHog) 两个通道，及本地 Docker PostHog 验证环境。

**Architecture:** 单例门面 `TelemetryClient` 接收 `track()`，经 `IConsentProvider` 隐私门与采样后入 `EventQueue`（有界、丢最旧）；后台 `BatchUploader` 线程按量/按时取批，交给注入的 `ITransport` 发送，失败退避重试、超限落盘 spool。`EventContext` 自管匿名 install_id/session_id 并合并外部注入字段。所有外部依赖经接口注入，SDK 不依赖任何 OrcaSlicer 符号。

**Tech Stack:** C++17、CMake (FetchContent)、nlohmann/json、GoogleTest、std::thread。

## Global Constraints

- 语言标准：C++17；纯 SDK，零依赖任何现有 Snapmaker/OrcaSlicer 代码。
- 命名空间统一 `snap::`；事件名为 SDK 自有常量，不引用 `BP_*`。
- `track()` 必须 O(1) 非阻塞，绝不阻塞调用线程。
- 所有异常吞在 SDK 内，不外抛。
- **不采集 `pc_name` 或任何 PII**；`install_id` 为匿名 UUID。
- 隐私门为假时事件直接丢弃（不入队）。
- 单测目标覆盖率 ≥80%。
- 事件信封字段：`event`(string)、`ts`(epoch 毫秒 int64)、`props`(json object)、`ctx{app_ver, os, os_ver, install_id, session_id, user_id}`。

---

### Task 1: 项目骨架与构建系统

**Files:**
- Create: `CMakeLists.txt`
- Create: `cmake/deps.cmake`
- Create: `tests/CMakeLists.txt`
- Create: `tests/smoke_test.cpp`

**Interfaces:**
- Consumes: 无
- Produces: CMake 目标 `snap_telemetry`(static lib) 与 `snap_telemetry_tests`(可执行)；`ctest` 可运行。

- [ ] **Step 1: 写 deps（拉取 json + googletest）**

`cmake/deps.cmake`:
```cmake
include(FetchContent)
FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3)
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG v1.15.2)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json googletest)
```

- [ ] **Step 2: 写根 CMake**

`CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.16)
project(snap_telemetry LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(cmake/deps.cmake)

add_library(snap_telemetry STATIC)
target_sources(snap_telemetry PRIVATE
  src/EventContext.cpp src/EventQueue.cpp src/BatchUploader.cpp
  src/FileSinkTransport.cpp src/HttpTransport.cpp
  src/Config.cpp src/TelemetryClient.cpp)
target_include_directories(snap_telemetry PUBLIC include)
target_link_libraries(snap_telemetry PUBLIC nlohmann_json::nlohmann_json)

enable_testing()
add_subdirectory(tests)
```

- [ ] **Step 3: 写 tests CMake + 冒烟测试**

`tests/CMakeLists.txt`:
```cmake
add_executable(snap_telemetry_tests smoke_test.cpp)
target_link_libraries(snap_telemetry_tests PRIVATE snap_telemetry GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(snap_telemetry_tests)
```

`tests/smoke_test.cpp`:
```cpp
#include <gtest/gtest.h>
TEST(Smoke, BuildWorks) { EXPECT_EQ(1 + 1, 2); }
```

> 注：Task 1 引用了尚未创建的 src 文件。先把 `target_sources` 里除将创建文件外注释掉，或创建空 `.cpp`。本计划按任务顺序逐个补齐；执行 Task 1 时仅保留 smoke 可编译——把 `target_sources` 暂时清空，每个后续任务再加回对应文件。

- [ ] **Step 4: 配置并构建，验证 smoke 通过**

Run:
```bash
cd C:/workspace/code/metrics && cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
```
Expected: `Smoke.BuildWorks` PASS。

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt cmake/ tests/ && git commit -m "build: scaffold snap_telemetry CMake + googletest smoke"
```

---

### Task 2: 核心类型与接口

**Files:**
- Create: `include/snap_telemetry/types.hpp`
- Create: `include/snap_telemetry/transport.hpp`
- Create: `include/snap_telemetry/consent.hpp`
- Test: `tests/types_test.cpp`

**Interfaces:**
- Produces: `snap::Event{std::string name; int64_t ts; nlohmann::json props; nlohmann::json ctx; nlohmann::json to_json() const;}`；`snap::Config`；`snap::ITransport{virtual bool send(const std::vector<Event>&)=0;}`；`snap::IConsentProvider{virtual bool is_allowed()=0;}`；`snap::events::*` 常量。

- [ ] **Step 1: 写失败测试（事件序列化）**

`tests/types_test.cpp`:
```cpp
#include <gtest/gtest.h>
#include "snap_telemetry/types.hpp"
using snap::Event;
TEST(Event, ToJsonHasEnvelope) {
  Event e; e.name="app_start"; e.ts=1719739200123LL;
  e.props={{"k",1}}; e.ctx={{"os","Windows"}};
  auto j = e.to_json();
  EXPECT_EQ(j.at("event"), "app_start");
  EXPECT_EQ(j.at("ts"), 1719739200123LL);
  EXPECT_EQ(j.at("props").at("k"), 1);
  EXPECT_EQ(j.at("ctx").at("os"), "Windows");
}
```
把 `tests/CMakeLists.txt` 的 sources 加上 `types_test.cpp`。

- [ ] **Step 2: 运行测试验证失败**

Run: `cmake --build build && ctest --test-dir build -R Event --output-on-failure`
Expected: FAIL（编译不过：types.hpp 不存在）。

- [ ] **Step 3: 写类型与接口头**

`include/snap_telemetry/types.hpp`:
```cpp
#pragma once
#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>
namespace snap {
struct Event {
  std::string name;
  int64_t ts = 0;
  nlohmann::json props = nlohmann::json::object();
  nlohmann::json ctx = nlohmann::json::object();
  nlohmann::json to_json() const {
    return nlohmann::json{{"event",name},{"ts",ts},{"props",props},{"ctx",ctx}};
  }
};
struct Config {
  std::string data_dir = ".";        // install_id / spool 存放目录
  size_t batch_size = 20;
  int    flush_interval_s = 30;
  size_t queue_cap = 1000;
  double sample_rate = 1.0;
  std::string app_ver = "0.0.0";
};
namespace events {
  constexpr const char* kAppStart      = "app_start";
  constexpr const char* kSliceCompleted= "slice_completed";
  constexpr const char* kDeviceConnect = "device_connect";
}
}
```

`include/snap_telemetry/transport.hpp`:
```cpp
#pragma once
#include <vector>
#include "snap_telemetry/types.hpp"
namespace snap {
struct ITransport { virtual ~ITransport()=default;
  virtual bool send(const std::vector<Event>& batch) = 0; }; // true=成功
}
```

`include/snap_telemetry/consent.hpp`:
```cpp
#pragma once
namespace snap {
struct IConsentProvider { virtual ~IConsentProvider()=default;
  virtual bool is_allowed() = 0; };
}
```

- [ ] **Step 4: 运行测试验证通过**

Run: `cmake --build build && ctest --test-dir build -R Event --output-on-failure`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add include/ tests/ && git commit -m "feat: add core Event/Config types and ITransport/IConsentProvider interfaces"
```

---

### Task 3: EventContext（匿名 install_id 持久化 + session_id + 注入合并）

**Files:**
- Create: `include/snap_telemetry/event_context.hpp`
- Create: `src/EventContext.cpp`
- Test: `tests/event_context_test.cpp`

**Interfaces:**
- Consumes: `Config`
- Produces: `snap::EventContext`，方法 `void set_app(const std::string& app_ver, const std::string& os, const std::string& os_ver); void set_user_id(const std::string&); nlohmann::json build() const; std::string install_id() const;`。`install_id` 持久化在 `Config.data_dir + "/install_id"`，跨实例稳定。

- [ ] **Step 1: 写失败测试**

`tests/event_context_test.cpp`:
```cpp
#include <gtest/gtest.h>
#include <filesystem>
#include "snap_telemetry/event_context.hpp"
using snap::EventContext; using snap::Config;
static Config tmp_cfg() {
  Config c; c.data_dir=(std::filesystem::temp_directory_path()/
    ("snap_ctx_"+std::to_string(::testing::UnitTest::GetInstance()->random_seed()))).string();
  std::filesystem::create_directories(c.data_dir); return c;
}
TEST(EventContext, InstallIdStableAcrossInstances) {
  auto c = tmp_cfg();
  EventContext a(c); auto id1 = a.install_id();
  EventContext b(c); auto id2 = b.install_id();
  EXPECT_FALSE(id1.empty());
  EXPECT_EQ(id1, id2);                 // 持久化后稳定
}
TEST(EventContext, BuildMergesInjectedFields) {
  auto c = tmp_cfg();
  EventContext ctx(c);
  ctx.set_app("2.3.1","Windows","10.0.19045");
  ctx.set_user_id("u123");
  auto j = ctx.build();
  EXPECT_EQ(j.at("app_ver"),"2.3.1");
  EXPECT_EQ(j.at("os"),"Windows");
  EXPECT_EQ(j.at("os_ver"),"10.0.19045");
  EXPECT_EQ(j.at("user_id"),"u123");
  EXPECT_FALSE(j.at("install_id").get<std::string>().empty());
  EXPECT_FALSE(j.at("session_id").get<std::string>().empty());
  EXPECT_EQ(j.count("pc_name"), 0u);   // 明确不采 PII
}
```

- [ ] **Step 2: 运行验证失败**

Run: `cmake --build build && ctest --test-dir build -R EventContext --output-on-failure`
Expected: FAIL（头不存在）。

- [ ] **Step 3: 实现**

`include/snap_telemetry/event_context.hpp`:
```cpp
#pragma once
#include <string>
#include <mutex>
#include <nlohmann/json.hpp>
#include "snap_telemetry/types.hpp"
namespace snap {
class EventContext {
public:
  explicit EventContext(const Config& cfg);
  void set_app(const std::string& app_ver, const std::string& os, const std::string& os_ver);
  void set_user_id(const std::string& uid);
  nlohmann::json build() const;
  std::string install_id() const;
private:
  static std::string gen_uuid();
  std::string load_or_create_install_id(const std::string& dir);
  mutable std::mutex m_;
  std::string install_id_, session_id_, app_ver_, os_, os_ver_, user_id_;
};
}
```

`src/EventContext.cpp`:
```cpp
#include "snap_telemetry/event_context.hpp"
#include <fstream>
#include <random>
#include <sstream>
#include <filesystem>
namespace snap {
std::string EventContext::gen_uuid() {
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  std::uniform_int_distribution<uint32_t> d(0,15);
  const char* hex="0123456789abcdef";
  std::string u; const char* fmt="xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
  for (char c: std::string(fmt)) {
    if (c=='x') u+=hex[d(rng)];
    else if (c=='y') u+=hex[(d(rng)&0x3)|0x8];
    else u+=c;
  }
  return u;
}
std::string EventContext::load_or_create_install_id(const std::string& dir) {
  std::filesystem::path p = std::filesystem::path(dir)/"install_id";
  std::ifstream in(p);
  std::string id; if (in && std::getline(in,id) && !id.empty()) return id;
  id = gen_uuid();
  std::filesystem::create_directories(dir);
  std::ofstream(p) << id;
  return id;
}
EventContext::EventContext(const Config& cfg) {
  install_id_ = load_or_create_install_id(cfg.data_dir);
  session_id_ = gen_uuid();
  app_ver_ = cfg.app_ver;
}
void EventContext::set_app(const std::string& a,const std::string& o,const std::string& ov){
  std::lock_guard<std::mutex> lk(m_); app_ver_=a; os_=o; os_ver_=ov;
}
void EventContext::set_user_id(const std::string& uid){
  std::lock_guard<std::mutex> lk(m_); user_id_=uid;
}
std::string EventContext::install_id() const { std::lock_guard<std::mutex> lk(m_); return install_id_; }
nlohmann::json EventContext::build() const {
  std::lock_guard<std::mutex> lk(m_);
  return nlohmann::json{
    {"app_ver",app_ver_},{"os",os_},{"os_ver",os_ver_},
    {"install_id",install_id_},{"session_id",session_id_},{"user_id",user_id_}};
}
}
```
把 `EventContext.cpp` 加回根 CMake `target_sources`，`event_context_test.cpp` 加入 tests。

- [ ] **Step 4: 运行验证通过**

Run: `cmake --build build && ctest --test-dir build -R EventContext --output-on-failure`
Expected: PASS（两个用例）。

- [ ] **Step 5: Commit**

```bash
git add include/ src/EventContext.cpp tests/ CMakeLists.txt && git commit -m "feat: EventContext with persisted anon install_id, session_id, injected ctx (no pc_name)"
```

---

### Task 4: EventQueue（有界、线程安全、丢最旧 + dropped 计数）

**Files:**
- Create: `include/snap_telemetry/event_queue.hpp`
- Create: `src/EventQueue.cpp`
- Test: `tests/event_queue_test.cpp`

**Interfaces:**
- Consumes: `Event`
- Produces: `snap::EventQueue`，`explicit EventQueue(size_t cap)`；`void push(Event)`（满则丢队首、dropped++）；`std::vector<Event> drain(size_t max)`；`size_t size() const`；`uint64_t dropped() const`。

- [ ] **Step 1: 写失败测试**

`tests/event_queue_test.cpp`:
```cpp
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "snap_telemetry/event_queue.hpp"
using snap::EventQueue; using snap::Event;
static Event ev(int i){ Event e; e.name="e"; e.ts=i; return e; }
TEST(EventQueue, DropsOldestWhenFull) {
  EventQueue q(2);
  q.push(ev(1)); q.push(ev(2)); q.push(ev(3));   // 3 挤掉 1
  EXPECT_EQ(q.size(), 2u);
  EXPECT_EQ(q.dropped(), 1u);
  auto out = q.drain(10);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0].ts, 2);
  EXPECT_EQ(out[1].ts, 3);
}
TEST(EventQueue, DrainRespectsMax) {
  EventQueue q(10);
  for (int i=0;i<5;i++) q.push(ev(i));
  EXPECT_EQ(q.drain(3).size(), 3u);
  EXPECT_EQ(q.size(), 2u);
}
TEST(EventQueue, ConcurrentPushNoLoss) {
  EventQueue q(100000);
  std::vector<std::thread> ts;
  for (int t=0;t<4;t++) ts.emplace_back([&]{ for(int i=0;i<1000;i++) q.push(ev(i)); });
  for (auto& t: ts) t.join();
  EXPECT_EQ(q.size(), 4000u);
  EXPECT_EQ(q.dropped(), 0u);
}
```

- [ ] **Step 2: 运行验证失败**

Run: `cmake --build build && ctest --test-dir build -R EventQueue --output-on-failure`
Expected: FAIL。

- [ ] **Step 3: 实现**

`include/snap_telemetry/event_queue.hpp`:
```cpp
#pragma once
#include <deque>
#include <mutex>
#include <atomic>
#include <vector>
#include "snap_telemetry/types.hpp"
namespace snap {
class EventQueue {
public:
  explicit EventQueue(size_t cap): cap_(cap) {}
  void push(Event e);
  std::vector<Event> drain(size_t max);
  size_t size() const;
  uint64_t dropped() const { return dropped_.load(); }
private:
  mutable std::mutex m_;
  std::deque<Event> q_;
  size_t cap_;
  std::atomic<uint64_t> dropped_{0};
};
}
```

`src/EventQueue.cpp`:
```cpp
#include "snap_telemetry/event_queue.hpp"
namespace snap {
void EventQueue::push(Event e) {
  std::lock_guard<std::mutex> lk(m_);
  if (q_.size() >= cap_) { q_.pop_front(); dropped_.fetch_add(1); }
  q_.push_back(std::move(e));
}
std::vector<Event> EventQueue::drain(size_t max) {
  std::lock_guard<std::mutex> lk(m_);
  std::vector<Event> out;
  size_t n = std::min(max, q_.size());
  out.reserve(n);
  for (size_t i=0;i<n;i++){ out.push_back(std::move(q_.front())); q_.pop_front(); }
  return out;
}
size_t EventQueue::size() const { std::lock_guard<std::mutex> lk(m_); return q_.size(); }
}
```
加回 CMake sources + tests。

- [ ] **Step 4: 运行验证通过**

Run: `cmake --build build && ctest --test-dir build -R EventQueue --output-on-failure`
Expected: PASS（3 用例）。

- [ ] **Step 5: Commit**

```bash
git add include/ src/EventQueue.cpp tests/ CMakeLists.txt && git commit -m "feat: bounded thread-safe EventQueue with drop-oldest + dropped counter"
```

---

### Task 5: FileSinkTransport（原子追加写 JSONL）

**Files:**
- Create: `include/snap_telemetry/file_sink_transport.hpp`
- Create: `src/FileSinkTransport.cpp`
- Test: `tests/file_sink_transport_test.cpp`

**Interfaces:**
- Consumes: `ITransport`, `Event`
- Produces: `snap::FileSinkTransport: public ITransport`，`explicit FileSinkTransport(std::string path)`；`send` 把每个 Event 的 `to_json()` 作为一行追加写入文件，返回 true。

- [ ] **Step 1: 写失败测试**

`tests/file_sink_transport_test.cpp`:
```cpp
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "snap_telemetry/file_sink_transport.hpp"
using snap::FileSinkTransport; using snap::Event;
TEST(FileSink, AppendsOneJsonLinePerEvent) {
  auto p = (std::filesystem::temp_directory_path()/"snap_fs.jsonl").string();
  std::filesystem::remove(p);
  FileSinkTransport t(p);
  Event a; a.name="app_start"; a.ts=1; Event b; b.name="slice_completed"; b.ts=2;
  EXPECT_TRUE(t.send({a,b}));
  std::ifstream in(p); std::string l1,l2,l3;
  std::getline(in,l1); std::getline(in,l2);
  EXPECT_NE(l1.find("\"app_start\""), std::string::npos);
  EXPECT_NE(l2.find("\"slice_completed\""), std::string::npos);
  EXPECT_FALSE(std::getline(in,l3));   // 恰好两行
}
```

- [ ] **Step 2: 运行验证失败**

Run: `cmake --build build && ctest --test-dir build -R FileSink --output-on-failure`
Expected: FAIL。

- [ ] **Step 3: 实现**

`include/snap_telemetry/file_sink_transport.hpp`:
```cpp
#pragma once
#include <string>
#include <mutex>
#include "snap_telemetry/transport.hpp"
namespace snap {
class FileSinkTransport : public ITransport {
public:
  explicit FileSinkTransport(std::string path): path_(std::move(path)) {}
  bool send(const std::vector<Event>& batch) override;
private:
  std::string path_; std::mutex m_;
};
}
```

`src/FileSinkTransport.cpp`:
```cpp
#include "snap_telemetry/file_sink_transport.hpp"
#include <fstream>
namespace snap {
bool FileSinkTransport::send(const std::vector<Event>& batch) {
  std::lock_guard<std::mutex> lk(m_);
  std::ofstream out(path_, std::ios::app);
  if (!out) return false;
  for (const auto& e : batch) out << e.to_json().dump() << "\n";
  return out.good();
}
}
```
加回 CMake sources + tests。

- [ ] **Step 4: 运行验证通过**

Run: `cmake --build build && ctest --test-dir build -R FileSink --output-on-failure`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add include/ src/FileSinkTransport.cpp tests/ CMakeLists.txt && git commit -m "feat: FileSinkTransport appends one JSON line per event"
```

---

### Task 6: BatchUploader（后台线程：按量/按时触发 + 退避重试 + spool 落盘）

**Files:**
- Create: `include/snap_telemetry/batch_uploader.hpp`
- Create: `src/BatchUploader.cpp`
- Test: `tests/batch_uploader_test.cpp`

**Interfaces:**
- Consumes: `EventQueue&`, `ITransport&`, `Config`
- Produces: `snap::BatchUploader`，`BatchUploader(EventQueue&, ITransport&, Config)`；`void start(); void stop(); void flush_now();`。失败超过 `kMaxRetries` 后把该批 `to_json()` 落盘到 `Config.data_dir+"/spool.jsonl"`；`start()` 时若 spool 存在则先尝试重发并清空。

- [ ] **Step 1: 写失败测试（用 FakeTransport）**

`tests/batch_uploader_test.cpp`:
```cpp
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <filesystem>
#include "snap_telemetry/batch_uploader.hpp"
using namespace snap;
struct FakeTransport : ITransport {
  std::atomic<int> calls{0}; std::atomic<int> received{0}; bool ok=true;
  bool send(const std::vector<Event>& b) override {
    calls++; if(!ok) return false; received += (int)b.size(); return true; }
};
static Config cfg() {
  Config c; c.batch_size=5; c.flush_interval_s=60; c.queue_cap=1000;
  c.data_dir=(std::filesystem::temp_directory_path()/"snap_bu").string();
  std::filesystem::create_directories(c.data_dir);
  std::filesystem::remove(std::filesystem::path(c.data_dir)/"spool.jsonl");
  return c;
}
static Event ev(){ Event e; e.name="e"; return e; }
TEST(BatchUploader, FlushesWhenBatchSizeReached) {
  auto c=cfg(); EventQueue q(1000); FakeTransport t;
  BatchUploader u(q,t,c); u.start();
  for(int i=0;i<5;i++) q.push(ev());
  for(int i=0;i<100 && t.received.load()<5;i++) std::this_thread::sleep_for(std::chrono::milliseconds(10));
  u.stop();
  EXPECT_EQ(t.received.load(), 5);
}
TEST(BatchUploader, SpoolsToDiskWhenSendFailsRepeatedly) {
  auto c=cfg(); EventQueue q(1000); FakeTransport t; t.ok=false;
  BatchUploader u(q,t,c); u.start();
  for(int i=0;i<5;i++) q.push(ev());
  u.stop();   // stop 内会 flush
  auto spool = std::filesystem::path(c.data_dir)/"spool.jsonl";
  EXPECT_TRUE(std::filesystem::exists(spool));
}
```

- [ ] **Step 2: 运行验证失败**

Run: `cmake --build build && ctest --test-dir build -R BatchUploader --output-on-failure`
Expected: FAIL。

- [ ] **Step 3: 实现**

`include/snap_telemetry/batch_uploader.hpp`:
```cpp
#pragma once
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include "snap_telemetry/event_queue.hpp"
#include "snap_telemetry/transport.hpp"
#include "snap_telemetry/types.hpp"
namespace snap {
class BatchUploader {
public:
  BatchUploader(EventQueue& q, ITransport& t, Config cfg);
  ~BatchUploader();
  void start();
  void stop();
private:
  void run();
  bool send_with_retry(const std::vector<Event>& batch);
  void spool(const std::vector<Event>& batch);
  void replay_spool();
  static constexpr int kMaxRetries = 3;
  EventQueue& q_; ITransport& t_; Config cfg_;
  std::thread th_; std::atomic<bool> running_{false};
  std::mutex cv_m_; std::condition_variable cv_;
};
}
```

`src/BatchUploader.cpp`:
```cpp
#include "snap_telemetry/batch_uploader.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
namespace snap {
BatchUploader::BatchUploader(EventQueue& q, ITransport& t, Config cfg)
  : q_(q), t_(t), cfg_(std::move(cfg)) {}
BatchUploader::~BatchUploader(){ stop(); }
void BatchUploader::start(){
  if (running_.exchange(true)) return;
  replay_spool();
  th_ = std::thread(&BatchUploader::run, this);
}
void BatchUploader::stop(){
  if (!running_.exchange(false)) return;
  cv_.notify_all();
  if (th_.joinable()) th_.join();
  auto rest = q_.drain(q_.size());          // 退出前清空
  if (!rest.empty() && !send_with_retry(rest)) spool(rest);
}
void BatchUploader::run(){
  while (running_.load()){
    {
      std::unique_lock<std::mutex> lk(cv_m_);
      cv_.wait_for(lk, std::chrono::seconds(cfg_.flush_interval_s),
                   [&]{ return !running_.load() || q_.size() >= cfg_.batch_size; });
    }
    auto batch = q_.drain(cfg_.batch_size);
    if (batch.empty()) continue;
    if (!send_with_retry(batch)) spool(batch);
  }
}
bool BatchUploader::send_with_retry(const std::vector<Event>& batch){
  int delay_ms = 50;
  for (int i=0;i<kMaxRetries;i++){
    if (t_.send(batch)) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    delay_ms *= 2;
  }
  return false;
}
void BatchUploader::spool(const std::vector<Event>& batch){
  std::filesystem::create_directories(cfg_.data_dir);
  std::ofstream out(std::filesystem::path(cfg_.data_dir)/"spool.jsonl", std::ios::app);
  for (const auto& e : batch) out << e.to_json().dump() << "\n";
}
void BatchUploader::replay_spool(){
  auto p = std::filesystem::path(cfg_.data_dir)/"spool.jsonl";
  std::ifstream in(p); if (!in) return;
  std::vector<Event> batch; std::string line;
  while (std::getline(in,line)){
    if (line.empty()) continue;
    auto j = nlohmann::json::parse(line, nullptr, false);
    if (j.is_discarded()) continue;
    Event e; e.name=j.value("event",""); e.ts=j.value("ts",(int64_t)0);
    e.props=j.value("props",nlohmann::json::object());
    e.ctx=j.value("ctx",nlohmann::json::object());
    batch.push_back(std::move(e));
  }
  in.close();
  if (!batch.empty() && t_.send(batch)) std::filesystem::remove(p);
}
}
```
加回 CMake sources + tests。

- [ ] **Step 4: 运行验证通过**

Run: `cmake --build build && ctest --test-dir build -R BatchUploader --output-on-failure`
Expected: PASS（2 用例）。

- [ ] **Step 5: Commit**

```bash
git add include/ src/BatchUploader.cpp tests/ CMakeLists.txt && git commit -m "feat: BatchUploader background thread with size/time trigger, retry backoff, spool"
```

---

### Task 7: TelemetryClient 门面（init/track/flush/shutdown + 隐私门 + 采样）

**Files:**
- Create: `include/snap_telemetry/telemetry.hpp`
- Create: `src/TelemetryClient.cpp`
- Create: `src/Config.cpp`（空占位，便于将来扩展配置加载；本任务先建空文件以匹配 CMake）
- Test: `tests/telemetry_client_test.cpp`

**Interfaces:**
- Consumes: 全部前序模块
- Produces: 单例 `snap::TelemetryClient::instance()`；`void init(Config, std::unique_ptr<ITransport>, std::shared_ptr<IConsentProvider>); void track(const std::string& name, nlohmann::json props={}); EventContext& context(); void flush(); void shutdown();`。宏 `SNAP_TRACK(name, props)`。隐私门为假或采样未命中时丢弃。

- [ ] **Step 1: 写失败测试**

`tests/telemetry_client_test.cpp`:
```cpp
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "snap_telemetry/telemetry.hpp"
using namespace snap;
struct AllowConsent : IConsentProvider { bool v=true; bool is_allowed() override { return v; } };
static std::string countlines(const std::string& p){
  std::ifstream in(p); std::string l; int n=0; while(std::getline(in,l)) if(!l.empty()) n++;
  return std::to_string(n);
}
TEST(TelemetryClient, TracksWhenAllowed) {
  auto dir=(std::filesystem::temp_directory_path()/"snap_cli1").string();
  std::filesystem::create_directories(dir);
  auto sink=(std::filesystem::path(dir)/"out.jsonl").string(); std::filesystem::remove(sink);
  Config c; c.data_dir=dir; c.batch_size=1; c.app_ver="2.3.1";
  auto consent=std::make_shared<AllowConsent>();
  TelemetryClient::instance().init(c, std::make_unique<FileSinkTransport>(sink), consent);
  TelemetryClient::instance().context().set_app("2.3.1","Windows","10.0.19045");
  TelemetryClient::instance().track(events::kAppStart);
  TelemetryClient::instance().flush();
  TelemetryClient::instance().shutdown();
  EXPECT_EQ(countlines(sink), "1");
}
TEST(TelemetryClient, DropsWhenConsentDenied) {
  auto dir=(std::filesystem::temp_directory_path()/"snap_cli2").string();
  std::filesystem::create_directories(dir);
  auto sink=(std::filesystem::path(dir)/"out.jsonl").string(); std::filesystem::remove(sink);
  Config c; c.data_dir=dir; c.batch_size=1;
  auto consent=std::make_shared<AllowConsent>(); consent->v=false;
  TelemetryClient::instance().init(c, std::make_unique<FileSinkTransport>(sink), consent);
  TelemetryClient::instance().track(events::kAppStart);
  TelemetryClient::instance().flush();
  TelemetryClient::instance().shutdown();
  EXPECT_FALSE(std::filesystem::exists(sink));   // 一条都没写
}
```

- [ ] **Step 2: 运行验证失败**

Run: `cmake --build build && ctest --test-dir build -R TelemetryClient --output-on-failure`
Expected: FAIL。

- [ ] **Step 3: 实现**

`include/snap_telemetry/telemetry.hpp`:
```cpp
#pragma once
#include <memory>
#include <atomic>
#include "snap_telemetry/types.hpp"
#include "snap_telemetry/transport.hpp"
#include "snap_telemetry/consent.hpp"
#include "snap_telemetry/event_context.hpp"
#include "snap_telemetry/event_queue.hpp"
#include "snap_telemetry/batch_uploader.hpp"
#include "snap_telemetry/file_sink_transport.hpp"
namespace snap {
class TelemetryClient {
public:
  static TelemetryClient& instance();
  void init(Config cfg, std::unique_ptr<ITransport> transport,
            std::shared_ptr<IConsentProvider> consent);
  void track(const std::string& name, nlohmann::json props = nlohmann::json::object());
  EventContext& context();
  void flush();
  void shutdown();
private:
  TelemetryClient() = default;
  static int64_t now_ms();
  bool sampled() const;
  Config cfg_;
  std::unique_ptr<ITransport> transport_;
  std::shared_ptr<IConsentProvider> consent_;
  std::unique_ptr<EventContext> ctx_;
  std::unique_ptr<EventQueue> queue_;
  std::unique_ptr<BatchUploader> uploader_;
  std::atomic<bool> ready_{false};
};
}
#define SNAP_TRACK(name, ...) ::snap::TelemetryClient::instance().track((name), __VA_ARGS__)
```

`src/TelemetryClient.cpp`:
```cpp
#include "snap_telemetry/telemetry.hpp"
#include <chrono>
#include <random>
namespace snap {
TelemetryClient& TelemetryClient::instance(){ static TelemetryClient c; return c; }
int64_t TelemetryClient::now_ms(){
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
}
bool TelemetryClient::sampled() const {
  if (cfg_.sample_rate >= 1.0) return true;
  if (cfg_.sample_rate <= 0.0) return false;
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<double> d(0.0,1.0);
  return d(rng) < cfg_.sample_rate;
}
void TelemetryClient::init(Config cfg, std::unique_ptr<ITransport> transport,
                           std::shared_ptr<IConsentProvider> consent){
  cfg_ = std::move(cfg);
  transport_ = std::move(transport);
  consent_ = std::move(consent);
  ctx_ = std::make_unique<EventContext>(cfg_);
  queue_ = std::make_unique<EventQueue>(cfg_.queue_cap);
  uploader_ = std::make_unique<BatchUploader>(*queue_, *transport_, cfg_);
  uploader_->start();
  ready_.store(true);
}
EventContext& TelemetryClient::context(){ return *ctx_; }
void TelemetryClient::track(const std::string& name, nlohmann::json props){
  if (!ready_.load()) return;
  if (consent_ && !consent_->is_allowed()) return;
  if (!sampled()) return;
  Event e; e.name=name; e.ts=now_ms(); e.props=std::move(props); e.ctx=ctx_->build();
  queue_->push(std::move(e));
}
void TelemetryClient::flush(){
  if (!ready_.load()) return;
  uploader_->stop(); uploader_->start();   // stop 内已 drain+send，再重启线程
}
void TelemetryClient::shutdown(){
  if (!ready_.exchange(false)) return;
  uploader_->stop();
  uploader_.reset(); queue_.reset(); ctx_.reset(); transport_.reset(); consent_.reset();
}
}
```

`src/Config.cpp`:
```cpp
#include "snap_telemetry/types.hpp"
// 预留：未来从 meta-cfg / 本地文件加载配置覆盖。当前默认值在 types.hpp 内联。
namespace snap {}
```
加回 CMake sources（`TelemetryClient.cpp`、`Config.cpp`）+ tests。

- [ ] **Step 4: 运行验证通过**

Run: `cmake --build build && ctest --test-dir build -R TelemetryClient --output-on-failure`
Expected: PASS（2 用例）。

- [ ] **Step 5: Commit**

```bash
git add include/ src/TelemetryClient.cpp src/Config.cpp tests/ CMakeLists.txt && git commit -m "feat: TelemetryClient facade with consent gate, sampling, SNAP_TRACK macro"
```

---

### Task 8: HttpTransport（PostHog capture 载荷构建 + 注入式发送）

**Files:**
- Create: `include/snap_telemetry/http_transport.hpp`
- Create: `src/HttpTransport.cpp`
- Test: `tests/http_transport_test.cpp`

**Interfaces:**
- Consumes: `ITransport`, `Event`
- Produces: `snap::HttpTransport: public ITransport`，构造 `HttpTransport(std::string endpoint, std::string api_key, PostFn post)`，其中 `using PostFn = std::function<bool(const std::string& url, const std::string& body)>`（注入真实 HTTP，便于单测）；静态 `static nlohmann::json build_payload(const std::string& api_key, const std::vector<Event>&)` 产出 PostHog `{api_key, batch:[{event,properties,timestamp,distinct_id}]}`，`distinct_id` 取 `ctx.user_id` 非空则用之否则 `ctx.install_id`。

- [ ] **Step 1: 写失败测试（只测载荷构建 + 注入 PostFn）**

`tests/http_transport_test.cpp`:
```cpp
#include <gtest/gtest.h>
#include "snap_telemetry/http_transport.hpp"
using namespace snap;
static Event ev(const std::string& uid){
  Event e; e.name="slice_completed"; e.ts=1719739200123LL;
  e.props={{"duration_ms",1234}};
  e.ctx={{"install_id","inst-1"},{"user_id",uid}};
  return e;
}
TEST(HttpTransport, BuildsPostHogPayload) {
  auto j = HttpTransport::build_payload("KEY", {ev("u9"), ev("")});
  EXPECT_EQ(j.at("api_key"), "KEY");
  ASSERT_EQ(j.at("batch").size(), 2u);
  EXPECT_EQ(j["batch"][0]["event"], "slice_completed");
  EXPECT_EQ(j["batch"][0]["properties"]["duration_ms"], 1234);
  EXPECT_EQ(j["batch"][0]["distinct_id"], "u9");      // 有 user_id
  EXPECT_EQ(j["batch"][1]["distinct_id"], "inst-1");  // 无 user_id 退回 install_id
}
TEST(HttpTransport, SendPostsToEndpoint) {
  std::string seen_url, seen_body;
  HttpTransport t("http://localhost:8000/i/v0/e/", "KEY",
    [&](const std::string& u, const std::string& b){ seen_url=u; seen_body=b; return true; });
  EXPECT_TRUE(t.send({ev("u1")}));
  EXPECT_EQ(seen_url, "http://localhost:8000/i/v0/e/");
  EXPECT_NE(seen_body.find("\"api_key\":\"KEY\""), std::string::npos);
}
```

- [ ] **Step 2: 运行验证失败**

Run: `cmake --build build && ctest --test-dir build -R HttpTransport --output-on-failure`
Expected: FAIL。

- [ ] **Step 3: 实现**

`include/snap_telemetry/http_transport.hpp`:
```cpp
#pragma once
#include <string>
#include <functional>
#include "snap_telemetry/transport.hpp"
namespace snap {
class HttpTransport : public ITransport {
public:
  using PostFn = std::function<bool(const std::string& url, const std::string& body)>;
  HttpTransport(std::string endpoint, std::string api_key, PostFn post)
    : endpoint_(std::move(endpoint)), api_key_(std::move(api_key)), post_(std::move(post)) {}
  bool send(const std::vector<Event>& batch) override;
  static nlohmann::json build_payload(const std::string& api_key, const std::vector<Event>& batch);
private:
  std::string endpoint_, api_key_; PostFn post_;
};
}
```

`src/HttpTransport.cpp`:
```cpp
#include "snap_telemetry/http_transport.hpp"
namespace snap {
nlohmann::json HttpTransport::build_payload(const std::string& api_key,
                                            const std::vector<Event>& batch){
  nlohmann::json arr = nlohmann::json::array();
  for (const auto& e : batch){
    std::string uid = e.ctx.value("user_id", std::string());
    std::string did = !uid.empty() ? uid : e.ctx.value("install_id", std::string());
    arr.push_back({{"event",e.name},{"properties",e.props},
                   {"timestamp",e.ts},{"distinct_id",did}});
  }
  return nlohmann::json{{"api_key",api_key},{"batch",arr}};
}
bool HttpTransport::send(const std::vector<Event>& batch){
  if (!post_) return false;
  try { return post_(endpoint_, build_payload(api_key_, batch).dump()); }
  catch (...) { return false; }
}
}
```
加回 CMake sources + tests。

> 真实 HTTP（OrcaSlicer 接入时）：适配层构造 `HttpTransport` 时传入基于现有 HTTP 客户端实现的 `PostFn`（如 libcurl / wxHTTP）。SDK 本身不绑定具体网络库。

- [ ] **Step 4: 运行验证通过**

Run: `cmake --build build && ctest --test-dir build -R HttpTransport --output-on-failure`
Expected: PASS（2 用例）。

- [ ] **Step 5: Commit**

```bash
git add include/ src/HttpTransport.cpp tests/ CMakeLists.txt && git commit -m "feat: HttpTransport with PostHog capture payload + injectable PostFn"
```

---

### Task 9: 本地 Docker PostHog 验证环境 + README

**Files:**
- Create: `deploy/docker-compose.posthog.yml`
- Create: `deploy/README.md`
- Create: `README.md`

**Interfaces:**
- Consumes: HttpTransport（手动验证用）
- Produces: 可 `docker compose up` 起本地 PostHog；SDK README 说明构建/测试/集成。

- [ ] **Step 1: 写 docker-compose（本地 PostHog）**

`deploy/docker-compose.posthog.yml`:
```yaml
# 本地端到端验证用。生产部署以官方 chart/compose 为准。
services:
  db:
    image: postgres:15-alpine
    environment: { POSTGRES_DB: posthog, POSTGRES_USER: posthog, POSTGRES_PASSWORD: posthog }
    volumes: [ "pgdata:/var/lib/postgresql/data" ]
  redis:
    image: redis:7-alpine
  posthog:
    image: posthog/posthog:latest
    depends_on: [ db, redis ]
    ports: [ "8000:8000" ]
    environment:
      DATABASE_URL: postgres://posthog:posthog@db:5432/posthog
      REDIS_URL: redis://redis:6379/
      SECRET_KEY: local-dev-only-not-a-secret
      SITE_URL: http://localhost:8000
volumes: { pgdata: {} }
```

- [ ] **Step 2: 写 deploy/README（验证步骤）**

`deploy/README.md`:
```markdown
# 本地 PostHog 验证

1. `docker compose -f docker-compose.posthog.yml up -d`
2. 浏览器开 http://localhost:8000 ，注册本地账号，建项目，复制 Project API Key。
3. 把 SDK 的 HttpTransport endpoint 设为 `http://localhost:8000/i/v0/e/`、api_key 设为上面的 key。
4. 触发事件后在 PostHog "Activity / Events" 里应看到 app_start / slice_completed / device_connect。
5. 结束：`docker compose -f docker-compose.posthog.yml down`（加 `-v` 清数据卷）。
```

- [ ] **Step 3: 写 SDK README**

`README.md`:
```markdown
# snap_telemetry

Snapmaker 第一方客户端遥测 SDK（采集→有界缓冲→后台批量→可插拔上报）。

## 构建与测试
```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## 用法
```cpp
#include "snap_telemetry/telemetry.hpp"
snap::Config c; c.data_dir = "<app-data-dir>"; c.app_ver = "2.3.1";
auto consent = std::make_shared<MyConsent>();           // 桥接宿主隐私开关
snap::TelemetryClient::instance().init(
    c, std::make_unique<snap::FileSinkTransport>("telemetry.jsonl"), consent);
snap::TelemetryClient::instance().context().set_app("2.3.1","Windows","10.0.19045");
SNAP_TRACK(snap::events::kAppStart, {});
// 退出前：
snap::TelemetryClient::instance().shutdown();
```

切换到 PostHog：把 FileSinkTransport 换成 HttpTransport（见 deploy/README.md）。
不采集 pc_name 或任何 PII；隐私门为假时零上报。
```

- [ ] **Step 4: 验证（构建+全量测试绿）**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部用例 PASS。

- [ ] **Step 5: Commit**

```bash
git add deploy/ README.md && git commit -m "docs: add local PostHog docker validation env and SDK README"
```

---

## 计划二预告（OrcaSlicer 集成，本计划不含）

`src/snap_telemetry/telemetry_adapter.{hpp,cpp}`：实现 `IConsentProvider`→`get_privacy_policy()`；`PostFn` 基于 OrcaSlicer HTTP 客户端；注入 `app_ver`/`os_ver`(wx)/`user_id`；`GUI_App` on_init `init()` + 3 处 `SNAP_TRACK` + 退出 `shutdown()`；`src/CMakeLists.txt:118` 链入 `Snapmaker_Orca`。

## Self-Review 记录

- **Spec 覆盖**：§3 解耦原则(D1/D5/D6→事件名/transport/不碰 bury) ✓；§4 模块(全部模块 Task2–8) ✓；§5 信封+字段字典(Task2/3) ✓；§6 数据流/错误(Task6/7) ✓；§7 隐私门/install_id/os_ver/不采 pc_name(Task3/7) ✓；§8 原型 3 事件常量+FileSink+HttpTransport+docker(Task2/5/8/9) ✓；§9 测试(每 Task 自带) ✓；§10 PostHog 载荷(Task8) ✓。§11 集成属计划二。D2/D3 同意与上下文"源用现有"在计划二的适配层落地（本计划用 Fake/注入验证）。
- **占位扫描**：无 TBD/TODO；`src/Config.cpp` 为有意预留空实现，已注明。
- **类型一致**：`ITransport::send`、`EventContext::build/set_app/set_user_id`、`EventQueue::push/drain/dropped`、`BatchUploader::start/stop`、`TelemetryClient::init/track/context/flush/shutdown`、`HttpTransport::build_payload` 跨任务签名一致。
