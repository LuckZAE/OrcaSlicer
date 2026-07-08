// snap_telemetry 真实 demo：初始化 SDK → 触发 3 个事件 → flush → 写出 telemetry.jsonl
#include <iostream>
#include <filesystem>
#include "snap_telemetry/telemetry.hpp"

using namespace snap;

struct DemoConsent : IConsentProvider { bool is_allowed() override { return true; } };

int main() {
  auto dir = std::filesystem::current_path().string() + "/demo-data";
  std::filesystem::create_directories(dir);
  auto sink = dir + "/telemetry.jsonl";
  std::filesystem::remove(sink);

  Config c; c.data_dir = dir; c.app_ver = "2.3.1-demo"; c.batch_size = 1;

  TelemetryClient::instance().init(
    c, std::make_unique<FileSinkTransport>(sink), std::make_shared<DemoConsent>());
  TelemetryClient::instance().context().set_app("2.3.1-demo", "Windows", "10.0.19045");
  TelemetryClient::instance().context().set_user_id("demo-user-42");

  SNAP_TRACK(events::kAppStart, {});
  SNAP_TRACK(events::kSliceCompleted, {{"duration_ms", 1234}, {"object_count", 3}});
  SNAP_TRACK(events::kDeviceConnect, {{"net_type", "wifi"}});

  TelemetryClient::instance().flush();
  TelemetryClient::instance().shutdown();

  std::cout << "Demo done.\n  file: " << sink
            << "\n  size: " << std::filesystem::file_size(sink) << " bytes\n";
  return 0;
}
