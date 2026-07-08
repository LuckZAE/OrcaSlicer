add_executable(snap_demo demo_main.cpp)
target_link_libraries(snap_demo PRIVATE snap_telemetry)
target_link_options(snap_demo PRIVATE -static -static-libgcc -static-libstdc++)

add_executable(snap_http_demo http_demo_main.cpp)
target_link_libraries(snap_http_demo PRIVATE snap_telemetry)
target_link_options(snap_http_demo PRIVATE -static -static-libgcc -static-libstdc++)
