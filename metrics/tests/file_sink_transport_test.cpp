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
