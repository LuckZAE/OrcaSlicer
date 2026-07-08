#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
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
  BatchUploader u(q,t,c);
  // 挂通知回调，每次 push 唤醒 uploader→实现真正的按量触发
  q.set_notify([&u]{ u.notify_queue_has_data(); });
  u.start();
  for(int i=0;i<5;i++) q.push(ev());
  for(int i=0;i<100 && t.received.load()<5;i++) std::this_thread::sleep_for(std::chrono::milliseconds(10));
  u.stop();
  EXPECT_EQ(t.received.load(), 5);
}
TEST(BatchUploader, SpoolsToDiskWhenSendFailsRepeatedly) {
  auto c=cfg(); EventQueue q(1000); FakeTransport t; t.ok=false;
  BatchUploader u(q,t,c);
  q.set_notify([&u]{ u.notify_queue_has_data(); });
  u.start();
  for(int i=0;i<5;i++) q.push(ev());
  u.stop();
  auto spool = std::filesystem::path(c.data_dir)/"spool.jsonl";
  EXPECT_TRUE(std::filesystem::exists(spool));
  // 验证 spool 内容：至少写入一个事件
  std::ifstream in(spool); std::string line; int n=0;
  while(std::getline(in,line)) if(!line.empty()) n++;
  EXPECT_GT(n, 0);
}
