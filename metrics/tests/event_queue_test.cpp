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
