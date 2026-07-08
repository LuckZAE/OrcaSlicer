#include "snap_telemetry/file_sink_transport.hpp"
#include <fstream>
namespace snap {
bool FileSinkTransport::send(const std::vector<Event>& batch) {
  std::lock_guard<std::mutex> lk(m_);
  std::ofstream out(path_, std::ios::app);
  if (!out) return false;
  for (const auto& e : batch) out << e.to_json().dump() << "\n";
  return out.good();
}
}
