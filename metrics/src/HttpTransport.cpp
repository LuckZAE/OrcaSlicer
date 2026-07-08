#include "snap_telemetry/http_transport.hpp"
namespace snap {
nlohmann::json HttpTransport::build_payload(const std::string& api_key,
                                            const std::vector<Event>& batch){
  nlohmann::json arr = nlohmann::json::array();
  for (const auto& e : batch){
    std::string uid = e.ctx.value("user_id", std::string());
    std::string did = !uid.empty() ? uid : e.ctx.value("install_id", std::string());
    arr.push_back({{"event",e.name},{"properties",e.props},
                   {"timestamp",e.ts},{"distinct_id",did}});
  }
  return nlohmann::json{{"api_key",api_key},{"batch",arr}};
}
bool HttpTransport::send(const std::vector<Event>& batch){
  if (!post_) return false;
  try { return post_(endpoint_, build_payload(api_key_, batch).dump()); }
  catch (...) { return false; }
}
}
