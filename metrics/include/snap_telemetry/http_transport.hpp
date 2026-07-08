#pragma once
#include <string>
#include <functional>
#include "snap_telemetry/transport.hpp"
namespace snap {
class HttpTransport : public ITransport {
public:
  using PostFn = std::function<bool(const std::string& url, const std::string& body)>;
  HttpTransport(std::string endpoint, std::string api_key, PostFn post)
    : endpoint_(std::move(endpoint)), api_key_(std::move(api_key)), post_(std::move(post)) {}
  bool send(const std::vector<Event>& batch) override;
  static nlohmann::json build_payload(const std::string& api_key, const std::vector<Event>& batch);
private:
  std::string endpoint_, api_key_; PostFn post_;
};
}
