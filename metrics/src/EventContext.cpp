#include "snap_telemetry/event_context.hpp"
#include <fstream>
#include <random>
#include <sstream>
#include <filesystem>
namespace snap {
std::string EventContext::gen_uuid() {
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  std::uniform_int_distribution<uint32_t> d(0,15);
  const char* hex="0123456789abcdef";
  std::string u; const char* fmt="xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
  for (char c: std::string(fmt)) {
    if (c=='x') u+=hex[d(rng)];
    else if (c=='y') u+=hex[(d(rng)&0x3)|0x8];
    else u+=c;
  }
  return u;
}
std::string EventContext::load_or_create_install_id(const std::string& dir) {
  std::filesystem::path p = std::filesystem::path(dir)/"install_id";
  std::ifstream in(p);
  std::string id; if (in && std::getline(in,id) && !id.empty()) return id;
  id = gen_uuid();
  std::filesystem::create_directories(dir);
  std::ofstream(p) << id;
  return id;
}
EventContext::EventContext(const Config& cfg) {
  install_id_ = load_or_create_install_id(cfg.data_dir);
  session_id_ = gen_uuid();
  app_ver_ = cfg.app_ver;
}
void EventContext::set_app(const std::string& a,const std::string& o,const std::string& ov){
  std::lock_guard<std::mutex> lk(m_); app_ver_=a; os_=o; os_ver_=ov;
}
void EventContext::set_user_id(const std::string& uid){
  std::lock_guard<std::mutex> lk(m_); user_id_=uid;
}
std::string EventContext::install_id() const { std::lock_guard<std::mutex> lk(m_); return install_id_; }
nlohmann::json EventContext::build() const {
  std::lock_guard<std::mutex> lk(m_);
  return nlohmann::json{
    {"app_ver",app_ver_},{"os",os_},{"os_ver",os_ver_},
    {"install_id",install_id_},{"session_id",session_id_},{"user_id",user_id_}};
}
}
