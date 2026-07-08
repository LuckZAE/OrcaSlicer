#include <gtest/gtest.h>
#include "snap_telemetry/http_transport.hpp"
using namespace snap;
static Event ev(const std::string& uid){
  Event e; e.name="slice_completed"; e.ts=1719739200123LL;
  e.props={{"duration_ms",1234}};
  e.ctx={{"install_id","inst-1"},{"user_id",uid}};
  return e;
}
TEST(HttpTransport, BuildsPostHogPayload) {
  auto j = HttpTransport::build_payload("KEY", {ev("u9"), ev("")});
  EXPECT_EQ(j.at("api_key"), "KEY");
  ASSERT_EQ(j.at("batch").size(), 2u);
  EXPECT_EQ(j["batch"][0]["event"], "slice_completed");
  EXPECT_EQ(j["batch"][0]["properties"]["duration_ms"], 1234);
  EXPECT_EQ(j["batch"][0]["distinct_id"], "u9");      // 有 user_id
  EXPECT_EQ(j["batch"][1]["distinct_id"], "inst-1");  // 无 user_id 退回 install_id
}
TEST(HttpTransport, SendPostsToEndpoint) {
  std::string seen_url, seen_body;
  HttpTransport t("http://localhost:8000/i/v0/e/", "KEY",
    [&](const std::string& u, const std::string& b){ seen_url=u; seen_body=b; return true; });
  EXPECT_TRUE(t.send({ev("u1")}));
  EXPECT_EQ(seen_url, "http://localhost:8000/i/v0/e/");
  EXPECT_NE(seen_body.find("\"api_key\":\"KEY\""), std::string::npos);
}
