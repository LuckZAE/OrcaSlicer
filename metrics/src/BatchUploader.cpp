#include "snap_telemetry/batch_uploader.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
namespace snap {
BatchUploader::BatchUploader(EventQueue& q, ITransport& t, Config cfg)
  : q_(q), t_(t), cfg_(std::move(cfg)) {}
BatchUploader::~BatchUploader(){ stop(); }
void BatchUploader::start(){
  if (running_.exchange(true)) return;
  replay_spool();
  th_ = std::thread(&BatchUploader::run, this);
}
void BatchUploader::stop(){
  if (!running_.exchange(false)) return;
  cv_.notify_all();
  if (th_.joinable()) th_.join();
  // 清空队列中剩余事件——join 后 worker 已退出，drain+safe
  auto rest = q_.drain(q_.size());
  if (!rest.empty() && !send_with_retry(rest)) spool(rest);
}
void BatchUploader::notify_queue_has_data() { cv_.notify_one(); }
void BatchUploader::run(){
  while (running_.load()){
    {
      std::unique_lock<std::mutex> lk(cv_m_);
      cv_.wait_for(lk, std::chrono::seconds(cfg_.flush_interval_s),
                   [&]{ return !running_.load() || q_.size() >= cfg_.batch_size; });
    }
    if (!running_.load()) break;
    auto batch = q_.drain(cfg_.batch_size);
    if (batch.empty()) continue;
    if (!send_with_retry(batch)) spool(batch);
  }
}
bool BatchUploader::send_with_retry(const std::vector<Event>& batch){
  int delay_ms = 50;
  for (int i=0;i<kMaxRetries;i++){
    if (t_.send(batch)) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    delay_ms *= 2;
  }
  return false;
}
void BatchUploader::spool(const std::vector<Event>& batch){
  std::filesystem::create_directories(cfg_.data_dir);
  std::ofstream out(std::filesystem::path(cfg_.data_dir)/"spool.jsonl", std::ios::app);
  for (const auto& e : batch) out << e.to_json().dump() << "\n";
}
void BatchUploader::replay_spool(){
  auto p = std::filesystem::path(cfg_.data_dir)/"spool.jsonl";
  std::ifstream in(p); if (!in) return;
  std::vector<Event> batch; std::string line;
  while (std::getline(in,line)){
    if (line.empty()) continue;
    auto j = nlohmann::json::parse(line, nullptr, false);
    if (j.is_discarded()) continue;
    Event e; e.name=j.value("event",""); e.ts=j.value("ts",(int64_t)0);
    e.props=j.value("props",nlohmann::json::object());
    e.ctx=j.value("ctx",nlohmann::json::object());
    batch.push_back(std::move(e));
  }
  in.close();
  if (!batch.empty() && t_.send(batch)) std::filesystem::remove(p);
}
}
