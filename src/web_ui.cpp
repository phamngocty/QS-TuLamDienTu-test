// web_ui.cpp
#include "web_ui.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config_store.h"
#include "log_ring.h"
#include "pwm_test.h"
#include "lock_guard.h"

#include <Arduino.h>
#include "FS.h"
#include "LittleFS.h"
#include "ota_update.h"

// ...


// ===================== Serial log helpers =====================
static bool serOn = true; // có thể thêm /api/serial để bật/tắt nếu muốn
#define SLOGln(x)  do{ if(serOn) Serial.println(x); }while(0)
#define SLOGf(...) do{ if(serOn) Serial.printf(__VA_ARGS__); }while(0)

// ===================== Globals =====================
static AsyncWebServer server(80);
static DNSServer dns;
static uint32_t lastHit   = 0;
static bool     running   = false; // portal is running
static bool     holdPortal= false; // keep AP on while UI is open

// ===================== FS debug – danh sách file =====================
static void listFS() {
  SLOGln("[FS] Listing LittleFS:");
  File root = LittleFS.open("/");
  if (!root) { SLOGln("  (cannot open /)"); return; }
  File f = root.openNextFile();
  while (f) {
    SLOGf("  - %s (%u bytes)\n", f.name(), (unsigned)f.size());
    f = root.openNextFile();
  }
  SLOGf("[FS] total=%u used=%u\n",
        (unsigned)LittleFS.totalBytes(), (unsigned)LittleFS.usedBytes());
}
// Helper: lấy param từ query trước, nếu không có thì lấy từ POST body
auto getParam = [&](AsyncWebServerRequest* req, const char* key, const char* def = nullptr) -> String {
  if (req->hasParam(key))                    return req->getParam(key)->value();         // query string
  if (req->hasParam(key, true))              return req->getParam(key, true)->value();   // POST body
  return def ? String(def) : String();
};

// ===================== REST API =====================
static void handleAPI() {
  // --------- Config get/set ----------
  server.on("/api/get", HTTP_GET, [](AsyncWebServerRequest* req) {
    SLOGln("[API] GET /api/get");
    String js; CFG::exportJSON(js);
    req->send(200, "application/json", js);
    lastHit = millis();
  });
  // --------- Wi-Fi get/set (AP SSID/Password) ----------
  server.on("/api/wifi_get", HTTP_GET, [](AsyncWebServerRequest* req){
    DynamicJsonDocument d(256);
    const auto& c = CFG::get();
    d["ap_ssid"] = String(c.ap_ssid);
    d["ap_pass_len"] = (uint8_t)strlen(c.ap_pass);
    String out; serializeJson(d, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/wifi_set", HTTP_POST, [](AsyncWebServerRequest* req){}, nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t){
      DynamicJsonDocument d(256);
      DeserializationError e = deserializeJson(d, (const char*)data, len);
      if (e) { req->send(400, "text/plain", "BAD JSON"); return; }
      String ssid = d["ap_ssid"] | "";
      String pass = d["ap_pass"] | "";
      if (pass.length() && pass.length() < 8) { req->send(400, "text/plain", "pass>=8"); return; }
      auto c = CFG::get();
      if (ssid.length()) ssid.toCharArray(c.ap_ssid, sizeof(c.ap_ssid));
      if (pass.length()) pass.toCharArray(c.ap_pass, sizeof(c.ap_pass));
      CFG::set(c);
      req->send(200, "text/plain", "OK");
    }
  );

  // --------- Reboot ----------
  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* req){
    req->send(200, "text/plain", "REBOOT");
    req->client()->close(true);
    delay(400);
    ESP.restart();
  });


  server.on("/api/set", HTTP_POST, [](AsyncWebServerRequest* req) {
    SLOGln("[API] POST /api/set");
  }, NULL, [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    String body((char*)data, len);
    bool ok = CFG::importJSON(body);
    SLOGf("[API] /api/set → %s\n", ok ? "OK" : "BAD");
    req->send(ok ? 200 : 400, "text/plain", ok ? "OK" : "BAD");
    lastHit = millis();
  });

  // --------- Logs ----------
  server.on("/api/log", HTTP_GET, [](AsyncWebServerRequest* req) {
    SLOGln("[API] GET /api/log");
    String js; LOGR::readAllToJson(js);
    req->send(200, "application/json", js);
    lastHit = millis();
  });

  server.on("/api/clearlog", HTTP_POST, [](AsyncWebServerRequest* req) {
    SLOGln("[API] POST /api/clearlog");
    LOGR::clear();
    req->send(200, "text/plain", "OK");
    lastHit = millis();
  });

  // --------- Test output (cut 50ms) ----------
  server.on("/api/testcut", HTTP_POST, [](AsyncWebServerRequest* req) {
  String out = getParam(req, "out");
  SLOGf("[API] POST /api/testcut out=%s\n", out.c_str());
  if (out != "ign" && out != "inj") { req->send(400, "text/plain", "out? ign|inj"); return; }
  extern void CUT_testPulse(bool useIgn, uint16_t ms);
  CUT_testPulse(out == "ign", 50);
  req->send(200, "text/plain", "OK");
  lastHit = millis();
});


  // --------- Test RPM ----------
 // /api/testrpm?en=0|1&rpm=5000&ppr=1
server.on("/api/testrpm", HTTP_POST, [](AsyncWebServerRequest* req) {
  int   en  = getParam(req, "en",  "0").toInt();
  float rpm = getParam(req, "rpm", "0").toFloat();
  float ppr = getParam(req, "ppr", "1").toFloat();
  SLOGf("[API] POST /api/testrpm en=%d rpm=%.1f ppr=%.2f\n", en, rpm, ppr);
  PWMTEST::enable(en != 0);
  PWMTEST::setSim(rpm, ppr <= 0 ? 1 : ppr);
  req->send(200, "text/plain", "OK test r");
  lastHit = millis();
});

  // --------- Calibrate RPM ----------
  // /api/calib?true_rpm=XXXX
  server.on("/api/calib", HTTP_POST, [](AsyncWebServerRequest* req) {
    auto p = req->getParam("true_rpm", true);
    SLOGf("[API] POST /api/calib true_rpm=%s\n", p ? p->value().c_str() : "?");
    if (!p) { req->send(400, "text/plain", "true_rpm?"); return; }
    uint16_t true_rpm = p->value().toInt();
    uint32_t t0 = millis(), n = 0, sum = 0;
    while (millis() - t0 < 1000) { extern uint16_t RPM_get(); sum += RPM_get(); n++; delay(5); }
    float meas = (n ? (float)sum / n : 1.0f);
    auto cfg = CFG::get();
    cfg.rpm_scale = (meas > 0 ? (float)true_rpm / meas : 1.0f);
    CFG::set(cfg);
    req->send(200, "text/plain", "OK");
    lastHit = millis();
  });
  

  // --------- Wi-Fi hold / off ----------
  server.on("/api/wifi_hold", HTTP_POST, [](AsyncWebServerRequest* req) {
    int on = req->getParam("on", true) ? req->getParam("on", true)->value().toInt() : 1;
    SLOGf("[API] POST /api/wifi_hold on=%d\n", on);
    holdPortal = (on != 0);
    lastHit = millis();
    req->send(200, "text/plain", holdPortal ? "HOLD" : "RELEASE");
  });

  server.on("/api/wifi_off", HTTP_POST, [](AsyncWebServerRequest* req) {
    SLOGln("[API] POST /api/wifi_off");
    req->send(200, "text/plain", "OFF");
    SLOGln("[WEB] Wi-Fi AP OFF by user");
    server.end(); dns.stop(); WiFi.softAPdisconnect(true);
    running = false; holdPortal = false;
  });

  // --------- Lock: change pass ----------
  server.on("/api/lock_change_pass", HTTP_POST, [](AsyncWebServerRequest* req){}, nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t){
      DynamicJsonDocument d(128);
      DeserializationError e = deserializeJson(d, (const char*)data, len);
      if (e) { req->send(400, "text/plain", "BAD JSON"); return; }
      String oldp = d["old"] | "";
      String neo  = d["neo"] | "";
      if (neo.length()==0 || neo.length()>8) { req->send(400, "text/plain", "len 1..8"); return; }
      auto c = CFG::get();
      if (oldp != String(c.lock_code)) { req->send(403, "text/plain", "BAD OLD"); return; }
      neo.toCharArray(c.lock_code, sizeof(c.lock_code));
      CFG::set(c);
      req->send(200, "text/plain", "OK");
      lastHit = millis();
    }
  );

  // --------- FS LIST (JSON) ----------
  server.on("/api/fslist", HTTP_GET, [](AsyncWebServerRequest* req){
    String js = "[";
    File root = LittleFS.open("/");
    if (!root) { req->send(500, "application/json", "[]"); return; }
    File f = root.openNextFile();
    bool first = true;
    while (f) {
      if (!first) js += ",";
      first = false;
      js += "{\"name\":\"" + String(f.name()) + "\",\"size\":" + String((unsigned)f.size()) + "}";
      f = root.openNextFile();
    }
    js += "]";
    req->send(200, "application/json", js);
    lastHit = millis();
  });

  // --------- Root UI (LittleFS + fallback) ----------
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req){
    SLOGf("[WEB] GET / from %s\n", req->client()->remoteIP().toString().c_str());
    if (!LittleFS.exists("/index.html")) {
      const char* fb =
        "<!doctype html><meta charset=utf-8>"
        "<style>body{font-family:system-ui;padding:16px}pre{background:#eee;padding:8px;border-radius:8px}</style>"
        "<h3>index.html chưa có trên LittleFS</h3>"
        "<p>Vào VSCode → <b>PlatformIO: Upload File System Image</b></p>"
        "<pre>pio run --target uploadfs</pre>"
        "<p>Hoặc đặt file vào thư mục <code>data/index.html</code> rồi uploadfs.</p>";
      req->send(200, "text/html", fb);
      return;
    }
    req->send(LittleFS, "/index.html", "text/html");
    lastHit = millis(); holdPortal = true;
  });

  // (Tuỳ chọn) phục vụ thêm file tĩnh khác nếu bạn có (css/js…)
  server.serveStatic("/", LittleFS, "/");

  // --------- OTA routes ----------
  OTAHTTP_registerRoutes(server);

  // /api/rpm  → trả rpm hiện tại (JSON)
server.on("/api/rpm", HTTP_GET, [](AsyncWebServerRequest* req){
  extern uint16_t RPM_get();          // đã dùng trong /api/calib
  float rpm_raw = (float)RPM_get();
  auto cfg = CFG::get();
  float rpm = rpm_raw * (cfg.rpm_scale > 0 ? cfg.rpm_scale : 1.0f);

  // (tuỳ chọn) clamp để tránh giá trị lố
  if (rpm < 0) rpm = 0;

  // trả JSON rất nhẹ để poll nhanh
  String js = String("{\"rpm\":") + String((int)rpm) + "}";
  req->send(200, "application/json", js);
  lastHit = millis();
});
// === LOCK API ===
server.on("/api/lock_state", HTTP_GET, [](AsyncWebServerRequest* req) {
  const auto c = CFG::get();
  DynamicJsonDocument doc(256);
  doc["locked"]        = LOCK::isLocked();
  doc["lock_enabled"]  = c.lock_enabled;
  doc["lock_cut_sel"]  = (uint8_t)c.lock_cut_sel;
  doc["lock_code_len"] = (uint8_t)strlen(c.lock_code);
  String out; serializeJson(doc, out);
  req->send(200, "application/json", out);
  lastHit = millis();
});

// POST /api/lock_cmd  body: {"cmd":"lock"} | {"cmd":"unlock","pass":"0101"}
server.on("/api/lock_cmd", HTTP_POST, [](AsyncWebServerRequest* req){ 
  SLOGln("[API] POST /api/lock_cmd"); 
}, nullptr, [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t){
  String body((char*)data, len);
  DynamicJsonDocument d(128);
  DeserializationError e = deserializeJson(d, body);
  if (e) { req->send(400, "text/plain", "BAD JSON"); return; }
  String cmd = d["cmd"] | "";
  if (cmd == "lock") {
    LOCK::forceLock();
    req->send(200, "text/plain", "OK");
  } else if (cmd == "unlock") {
    String pass = d["pass"] | "";
    bool ok = LOCK::adminUnlock(pass);   // hàm mới ở lock_guard.*
    req->send(ok?200:403, "text/plain", ok? "OK" : "BAD");
  } else {
    req->send(400, "text/plain", "cmd?");
  }
  lastHit = millis();
});


}

// ===================== Portal lifecycle =====================
void WEB::beginPortal() {
  if (running) return;
  running = true; lastHit = millis(); holdPortal = false;

  // Mount LittleFS
  if (!LittleFS.begin(true)) { // true = format if mount failed (cân nhắc đổi false nếu không muốn format)
    SLOGln("[FS] LittleFS mount FAILED");
  } else {
    SLOGln("[FS] LittleFS mounted");
    listFS(); // in danh sách file để chắc chắn có /index.html
  }

  WiFi.mode(WIFI_AP);
  const auto& ccfg = CFG::get();
  String ssid = String(ccfg.ap_ssid);
  if (ssid.length() == 0) ssid = String("QS-TuLamDienTu-") + String((uint32_t)ESP.getEfuseMac(), HEX).substring(4);
  String pass = String(ccfg.ap_pass);
  if (pass.length() < 8) pass = "12345678";
  WiFi.softAP(ssid.c_str(), pass.c_str());
  SLOGln("=== WEB Portal start ===");
  SLOGf("[AP] SSID: %s\n", ssid.c_str());
  SLOGf("[AP] IP  : %s\n", WiFi.softAPIP().toString().c_str());

  dns.start(53, "*", WiFi.softAPIP());
  handleAPI();
  server.onNotFound([](AsyncWebServerRequest* req) { lastHit = millis(); req->redirect("/"); });
  server.begin();
}

void WEB::loop() {
  if (!running) return;
  dns.processNextRequest();
  if (holdPortal) return; // Giữ AP khi người dùng đang mở UI

  uint16_t tout = CFG::get().ap_timeout_s; // timeout cấu hình trong Web
  if (tout > 0 && (millis() - lastHit) > (uint32_t)tout * 1000UL) {
    server.end(); dns.stop(); WiFi.softAPdisconnect(true);
    running = false;
    SLOGln("[WEB] AP timeout → stop portal");
  }
}
