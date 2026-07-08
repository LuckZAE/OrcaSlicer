#pragma once
#include <string>
#include <mutex>
#include "snap_telemetry/transport.hpp"
namespace snap {
class FileSinkTransport : public ITransport {
public:
  explicit FileSinkTransport(std::string path): path_(std::move(path)) {}
  bool send(const std::vector<Event>& batch) override;
private:
  std::string path_; std::mutex m_;
};
}
