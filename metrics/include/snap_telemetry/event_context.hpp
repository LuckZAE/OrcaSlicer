#pragma once
#include <string>
#include <mutex>
#include <nlohmann/json.hpp>
#include "snap_telemetry/types.hpp"
namespace snap {
class EventContext {
public:
  explicit EventContext(const Config& cfg);
  void set_app(const std::string& app_ver, const std::string& os, const std::string& os_ver);
  void set_user_id(const std::string& uid);
  nlohmann::json build() const;
  std::string install_id() const;
private:
  static std::string gen_uuid();
  std::string load_or_create_install_id(const std::string& dir);
  mutable std::mutex m_;
  std::string install_id_, session_id_, app_ver_, os_, os_ver_, user_id_;
};
}
