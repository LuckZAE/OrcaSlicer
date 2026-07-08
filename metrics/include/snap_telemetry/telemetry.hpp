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
#define SNAP_TRACK(name, ...) \
  do { ::snap::TelemetryClient::instance().track((name), __VA_ARGS__); } while(0)
