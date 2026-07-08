#pragma once
#include <vector>
#include "snap_telemetry/types.hpp"
namespace snap {
struct ITransport { virtual ~ITransport()=default;
  virtual bool send(const std::vector<Event>& batch) = 0; }; // true=成功
}
