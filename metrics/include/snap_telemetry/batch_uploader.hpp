#pragma once
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include "snap_telemetry/event_queue.hpp"
#include "snap_telemetry/transport.hpp"
#include "snap_telemetry/types.hpp"
namespace snap {
class BatchUploader {
public:
  BatchUploader(EventQueue& q, ITransport& t, Config cfg);
  ~BatchUploader();
  void start();
  void stop();
  // 由 EventQueue::push() 调用，唤醒 cv_ 实现按量批量上报
  void notify_queue_has_data();
private:
  void run();
  bool send_with_retry(const std::vector<Event>& batch);
  void spool(const std::vector<Event>& batch);
  void replay_spool();
  static constexpr int kMaxRetries = 5;
  EventQueue& q_; ITransport& t_; Config cfg_;
  // IMPORTANT: cv_m_/cv_ before th_ before running_ — reverse destructor
  // order ensures the thread is fully stopped before cv/running_ are torn down.
  std::mutex cv_m_;
  std::condition_variable cv_;
  std::thread th_;
  std::atomic<bool> running_{false};
};
}
