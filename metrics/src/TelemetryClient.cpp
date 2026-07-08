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
  if (ready_.load()) return;   // 防止二次 init 触发 use-after-free
  cfg_ = std::move(cfg);
  transport_ = std::move(transport);
  consent_ = std::move(consent);
  ctx_ = std::make_unique<EventContext>(cfg_);
  queue_ = std::make_unique<EventQueue>(cfg_.queue_cap);
  uploader_ = std::make_unique<BatchUploader>(*queue_, *transport_, cfg_);
  // 让 EventQueue 的每次 push 唤醒 BatchUploader，实现按量批量上报
  queue_->set_notify([this]{ uploader_->notify_queue_has_data(); });
  uploader_->start();
  ready_.store(true);
}
EventContext& TelemetryClient::context(){ return *ctx_; }
void TelemetryClient::track(const std::string& name, nlohmann::json props){
  if (!ready_.load()) return;
  if (consent_ && !consent_->is_allowed()) return;
  if (!sampled()) return;
  if (!props.is_object()) props = nlohmann::json::object();   // 空/null props → {}
  Event e; e.name=name; e.ts=now_ms(); e.props=std::move(props); e.ctx=ctx_->build();
  queue_->push(std::move(e));
}
void TelemetryClient::flush(){
  if (!ready_.load()) return;
  uploader_->stop(); uploader_->start();   // stop 内已 drain+send，再重启线程
}
void TelemetryClient::shutdown(){
  if (!ready_.exchange(false)) return;
  // 析构顺序：必须先停 uploader（依赖 queue_），再依次释放子对象
  uploader_->stop();
  uploader_.reset();    // ~BatchUploader 完成后，queue_ 不再被引用
  queue_.reset();
  ctx_.reset();
  transport_.reset();
  consent_.reset();
}
}
