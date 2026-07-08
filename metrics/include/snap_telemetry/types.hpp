#pragma once
#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>
namespace snap {
struct Event {
  std::string name;
  int64_t ts = 0;
  nlohmann::json props = nlohmann::json::object();
  nlohmann::json ctx = nlohmann::json::object();
  nlohmann::json to_json() const {
    return nlohmann::json{{"event",name},{"ts",ts},{"props",props},{"ctx",ctx}};
  }
};
struct Config {
  std::string data_dir = ".";        // install_id / spool 存放目录
  size_t batch_size = 20;
  int    flush_interval_s = 30;
  size_t queue_cap = 1000;
  double sample_rate = 1.0;
  std::string app_ver = "0.0.0";
};
namespace events {
  constexpr const char* kAppStart      = "app_start";
  constexpr const char* kSliceCompleted= "slice_completed";
  constexpr const char* kDeviceConnect = "device_connect";
  constexpr const char* kConnectAttempt= "connect_attempt";
  constexpr const char* kConnectSuccess= "connect_success";
  constexpr const char* kProjectOpen   = "project_open";
  constexpr const char* kProjectOpened = "project_opened";
}
}
