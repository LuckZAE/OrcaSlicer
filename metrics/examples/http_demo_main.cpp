// snap_telemetry HTTP e2e demo：初始化 SDK → HttpTransport → 真 PostHog /batch → 落 ClickHouse
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>
#include "snap_telemetry/telemetry.hpp"
#include "snap_telemetry/http_transport.hpp"

using namespace snap;

// ============================================================
// curl 实现的 PostFn（写 body 到临时文件避免 shell 转义问题）
// ============================================================
static bool curl_post(const std::string& url, const std::string& body) {
    std::ofstream("snap_http_body.tmp", std::ios::binary) << body;
    std::string cmd = "curl -s -m 5 -X POST \"" + url +
        "\" -H \"Content-Type: application/json\" -d @snap_http_body.tmp";
    int ret = std::system(cmd.c_str());
    std::remove("snap_http_body.tmp");
    return ret == 0;
}

// ============================================================
struct AlwaysConsent : IConsentProvider { bool is_allowed() override { return true; } };

int main() {
    std::cout << "snap_telemetry HTTP e2e demo\n";
    std::cout << "  target: http://localhost:8000/batch\n";
    std::cout << "  api_key: phc_JZIax0Jy7Ghc9YCgHC62OgPYDFlOoMNEP4CfzIfx8cr\n\n";

    Config c;
    c.data_dir      = "./demo-data";
    c.app_ver       = "2.3.1-http-demo";
    c.batch_size    = 2;           // 攒 2 条就发
    c.flush_interval_s = 3;
    c.queue_cap     = 100;
    c.sample_rate   = 1.0;

    TelemetryClient::instance().init(
        c,
        std::make_unique<HttpTransport>(
            "http://localhost:8000/batch",
            "phc_JZIax0Jy7Ghc9YCgHC62OgPYDFlOoMNEP4CfzIfx8cr",
            curl_post),
        std::make_shared<AlwaysConsent>());

    TelemetryClient::instance().context().set_app("2.3.1-http-demo", "Windows", "10.0.19045");
    TelemetryClient::instance().context().set_user_id("e2e-user");

    // 发送 8 个不同事件（batch_size=2 → 4 次 HttpTransport::send）
    std::cout << "[1] SNAP_TRACK  app_start\n";
    SNAP_TRACK(events::kAppStart, {{"app_ver","2.3.1-http-demo"},{"os","Windows"}});
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "[2] SNAP_TRACK  slice_completed  #1\n";
    SNAP_TRACK(events::kSliceCompleted, {{"duration_ms",1234},{"object_count",3}});
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "[3] SNAP_TRACK  device_connect  wifi\n";
    SNAP_TRACK(events::kDeviceConnect, {{"net_type","wifi"}});
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "[4] SNAP_TRACK  slice_completed  #2\n";
    SNAP_TRACK(events::kSliceCompleted, {{"duration_ms",5678},{"object_count",7}});
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "[5] SNAP_TRACK  slice_completed  #3\n";
    SNAP_TRACK(events::kSliceCompleted, {{"duration_ms",9012},{"object_count",12}});
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "[6] SNAP_TRACK  device_connect  ethernet\n";
    SNAP_TRACK(events::kDeviceConnect, {{"net_type","ethernet"}});
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "[7] SNAP_TRACK  slice_completed  #4\n";
    SNAP_TRACK(events::kSliceCompleted, {{"duration_ms",3456},{"object_count",5}});
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "[8] SNAP_TRACK  slice_completed  #5\n";
    SNAP_TRACK(events::kSliceCompleted, {{"duration_ms",7890},{"object_count",9}});
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 多等几秒让 batch_uploader 把剩余事件 flush 掉
    std::cout << "\n[=] waiting for BatchUploader flush...\n";
    std::this_thread::sleep_for(std::chrono::seconds(8));

    // 主动 flush 一次剩余
    TelemetryClient::instance().flush();
    std::this_thread::sleep_for(std::chrono::seconds(3));

    TelemetryClient::instance().shutdown();
    std::cout << "\nDemo done. 8 events sent via HttpTransport to PostHog \n"
              << "  → check http://localhost:8000/activity/live_events\n"
              << "  → or Insights for app_start / slice_completed / device_connect\n";
    return 0;
}
