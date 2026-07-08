#include "snap_telemetry/event_queue.hpp"
namespace snap {
void EventQueue::push(Event e) {
  std::function<void()> notify;
  {
    std::lock_guard<std::mutex> lk(m_);
    if (q_.size() >= cap_) { q_.pop_front(); dropped_.fetch_add(1); }
    q_.push_back(std::move(e));
    notify = notify_;   // 拷贝出来，在锁外调用避免死锁
  }
  if (notify) notify();
}
std::vector<Event> EventQueue::drain(size_t max) {
  std::lock_guard<std::mutex> lk(m_);
  std::vector<Event> out;
  size_t n = std::min(max, q_.size());
  out.reserve(n);
  for (size_t i=0;i<n;i++){ out.push_back(std::move(q_.front())); q_.pop_front(); }
  return out;
}
size_t EventQueue::size() const { std::lock_guard<std::mutex> lk(m_); return q_.size(); }
void EventQueue::set_notify(std::function<void()> fn) {
  std::lock_guard<std::mutex> lk(m_);
  notify_ = std::move(fn);
}
}
