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
