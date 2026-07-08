add_executable(snap_telemetry_tests
  smoke_test.cpp
  types_test.cpp
  event_context_test.cpp
  event_queue_test.cpp
  file_sink_transport_test.cpp
  batch_uploader_test.cpp
  telemetry_client_test.cpp
  http_transport_test.cpp)
# snap_telemetry brings nlohmann_json + the include/ dir transitively.
target_link_libraries(snap_telemetry_tests PRIVATE GTest::gtest_main snap_telemetry)
# Static-link the MinGW runtime so the test exe has no external DLL deps —
# gtest_discover_tests runs it at build time where libwinpthread/libstdc++
# DLLs may not be on PATH.
target_link_options(snap_telemetry_tests PRIVATE -static -static-libgcc -static-libstdc++)
include(GoogleTest)
gtest_discover_tests(snap_telemetry_tests)
