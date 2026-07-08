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
TEST(TelemetryClient, EmptyPropsSerializeAsObject) {
  auto dir=(std::filesystem::temp_directory_path()/"snap_cli3").string();
  std::filesystem::create_directories(dir);
  auto sink=(std::filesystem::path(dir)/"out.jsonl").string(); std::filesystem::remove(sink);
  Config c; c.data_dir=dir; c.batch_size=1;
  TelemetryClient::instance().init(c, std::make_unique<FileSinkTransport>(sink),
                                   std::make_shared<AllowConsent>());
  TelemetryClient::instance().track(events::kAppStart, {});   // 空 props
  TelemetryClient::instance().flush();
  TelemetryClient::instance().shutdown();
  std::ifstream in(sink); std::string line; std::getline(in,line);
  auto j = nlohmann::json::parse(line);
  ASSERT_TRUE(j.at("props").is_object());   // 必须是 {} 而非 null
}
