#pragma once
// Snapmaker telemetry integration — thin adapter between snap_telemetry SDK
// and OrcaSlicer. This is the ONLY coupling point. Delete this directory
// and the SDK decouples cleanly.
//
// Also re-exports SNAP_TRACK so that OrcaSlicer compilation units which
// include this header can instrument events with a single include.
//
// NOMINMAX is defined HERE (not globally in CMake) because some OrcaSlicer
// TUs (e.g. BaseException.cpp) rely on windows.h min/max macros. Limiting the
// define to TUs that include this header keeps them working. Must appear
// before windows.h is pulled in, so this header should be included early.
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "snap_telemetry/telemetry.hpp"

namespace Slic3r { namespace GUI {

// Call once during GUI_App::on_init_inner(), before the main frame is shown.
// Reads existing privacy consent (get_privacy_policy) and injects os_ver/user_id.
void telemetry_init();

// Drain pending events and tear down the background uploader.
// Call during GUI_App::OnExit().
void telemetry_shutdown();

} }
