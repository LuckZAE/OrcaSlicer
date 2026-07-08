cmake_minimum_required(VERSION 3.16)
project(snap_telemetry LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 与 OrcaSlicer 对齐：MSVC 动态运行时 (/MD)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
include(cmake/deps.cmake)
find_package(Threads REQUIRED)

# snap_telemetry static library — source files added per task as modules land.
add_library(snap_telemetry STATIC)
target_sources(snap_telemetry PRIVATE
  src/EventContext.cpp
  src/EventQueue.cpp
  src/FileSinkTransport.cpp
  src/BatchUploader.cpp
  src/TelemetryClient.cpp
  src/HttpTransport.cpp
  src/Config.cpp)
target_include_directories(snap_telemetry PUBLIC include)
target_link_libraries(snap_telemetry PUBLIC nlohmann_json::nlohmann_json Threads::Threads)

enable_testing()
add_subdirectory(tests)
add_subdirectory(examples)
