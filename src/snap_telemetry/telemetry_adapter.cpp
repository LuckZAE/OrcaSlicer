// Must come before any include: nlohmann/json vs windows.h min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "snap_telemetry/telemetry_adapter.hpp"
#include "snap_telemetry/telemetry.hpp"
#include "snap_telemetry/http_transport.hpp"
#include "snap_telemetry/consent.hpp"
#include "snap_telemetry/types.hpp"

#include <memory>
#include <string>
#include <cstdlib>
#include <curl/curl.h>

// Existing privacy consent flag
#include "bury_cfg/bury_point.hpp"

// data_dir()
#include "libslic3r/Utils.hpp"

// wx helper for OS version
#include <wx/utils.h>

#ifndef SLIC3R_BUILD_ID
#error "SLIC3R_BUILD_ID must be defined by CMake"
#endif

namespace {
std::string env_or(const char* name, const char* fallback) {
    const char* v = std::getenv(name);
    return v && *v ? std::string(v) : std::string(fallback);
}
}

// ── Consent provider ──
struct SlicerConsent : snap::IConsentProvider {
  bool is_allowed() override { return get_privacy_policy(); }
};

// ── HTTP POST via libcurl ──
static bool curl_post_fn(const std::string& url, const std::string& body) {
  CURL* curl = curl_easy_init();
  if (!curl) return false;

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 2000L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  struct curl_slist* headers = curl_slist_append(nullptr, "Content-Type: application/json");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  CURLcode res = curl_easy_perform(curl);
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return res == CURLE_OK && http_code >= 200 && http_code < 300;
}

namespace Slic3r { namespace GUI {

void telemetry_init() {
  using namespace snap;

  Config cfg;
  cfg.data_dir         = Slic3r::data_dir();
  cfg.app_ver          = SLIC3R_BUILD_ID;
  cfg.batch_size       = 20;
  cfg.flush_interval_s = 30;
  cfg.queue_cap        = 1000;
  cfg.sample_rate      = 1.0;

  auto transport = std::make_unique<HttpTransport>(
    env_or("SNAP_TELEMETRY_ENDPOINT", "http://localhost:8000/batch"),
    env_or("SNAP_TELEMETRY_KEY",
           "phc_JvAYmzJomqPeZAYN7SM7Za6Xir3lrrHHwW0ljWP1oQf"),
    curl_post_fn
  );

  auto consent = std::make_shared<SlicerConsent>();

  TelemetryClient::instance().init(cfg, std::move(transport), consent);

  // Inject host context
  auto& ctx = TelemetryClient::instance().context();
  std::string os_ver = wxGetOsDescription().ToStdString();
#ifdef _WIN32
  std::string os_name = "Windows";
#elif defined(__APPLE__)
  std::string os_name = "macOS";
#else
  std::string os_name = "Linux";
#endif
  ctx.set_app(cfg.app_ver, os_name, os_ver);

  SNAP_TRACK("app_start", {});
}

void telemetry_shutdown() {
  snap::TelemetryClient::instance().shutdown();
}

} }
