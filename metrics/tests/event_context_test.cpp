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
