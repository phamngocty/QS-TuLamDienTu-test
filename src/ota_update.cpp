#include "ota_update.h"
#include <Update.h>
#include <LittleFS.h>
#include <FS.h>

static uint32_t s_reboot_delay_ms = 1200;

void OTAHTTP_setRebootDelayMs(uint32_t ms){ s_reboot_delay_ms = ms; }

// Gửi JSON ngắn gọn
static void sendJSON(AsyncWebServerRequest* req, int code, const String& msg){
  String out = "{\"ok\":"; out += (code==200?"true":"false");
  out += ",\"msg\":\""; out += msg; out += "\"}";
  req->send(code, "application/json", out);
}

// Trang OTA tối giản (phòng khi bạn chưa sửa index.html)
static const char* OTA_PAGE = R"HTML(
<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>OTA</title>
<style>body{font-family:sans-serif;margin:16px} fieldset{margin:12px 0} input[type=file]{width:100%}</style></head>
<body>
<h2>OTA Update</h2>
<fieldset><legend>Firmware (.bin)</legend>
<form id="f_fw" method="POST" action="/api/ota/firmware" enctype="multipart/form-data">
<input type="file" name="update" required><br><br><button type="submit">Upload Firmware</button>
</form></fieldset>
<fieldset><legend>Filesystem image LittleFS (.bin)</legend>
<form id="f_fs" method="POST" action="/api/ota/fsimage" enctype="multipart/form-data">
<input type="file" name="update" required><br><br><button type="submit">Upload FS Image</button>
</form></fieldset>
<fieldset><legend>Upload 1 file (ví dụ /index.html)</legend>
<form id="f_one" method="POST" action="/api/upload?path=/index.html" enctype="multipart/form-data">
<input type="file" name="file" required><br><br><button type="submit">Upload /index.html</button>
</form></fieldset>
<script>
// giữ AP không tắt trong khi đang mở trang
fetch('/api/wifi_hold?on=1').catch(()=>{});
window.addEventListener('beforeunload',()=>{navigator.sendBeacon('/api/wifi_hold?on=0')});
</script>
</body></html>
)HTML";

void OTAHTTP_registerRoutes(AsyncWebServer& server){
  // Trang OTA đơn giản
  server.on("/ota", HTTP_GET, [](AsyncWebServerRequest* req){
    req->send(200, "text/html", OTA_PAGE);
  });

  // ===== Helpers for backups =====
  auto ensureBackupDir = [](){ LittleFS.mkdir("/backup"); };
  auto fileSizeOr0 = [](const char* p)->size_t{ File f = LittleFS.open(p, "r"); size_t s = f ? (size_t)f.size() : 0; if (f) f.close(); return s; };
  auto copyFile = [](const char* src, const char* dst){
    File fi = LittleFS.open(src, "r"); if (!fi) return false;
    File fo = LittleFS.open(dst, "w"); if (!fo){ fi.close(); return false; }
    uint8_t buf[1024]; size_t n;
    while ((n = fi.read(buf, sizeof(buf))) > 0){ if (fo.write(buf, n) != n){ fi.close(); fo.close(); return false; } }
    fi.close(); fo.close(); return true;
  };

  // ===== OTA FIRMWARE (.bin) =====
  server.on("/api/ota/firmware", HTTP_POST,
    // onRequest
    [](AsyncWebServerRequest* req){
      // Kết thúc: nếu không lỗi → reboot
      if (Update.hasError()){
        sendJSON(req, 500, "FW update failed");
      } else {
        sendJSON(req, 200, "FW update ok, rebooting...");
        req->client()->close(true);
        delay(s_reboot_delay_ms);
        ESP.restart();
      }
    },
    // onUpload
    [](AsyncWebServerRequest* req, const String& filename, size_t index,
       uint8_t *data, size_t len, bool final){
      static size_t total = 0; if (!index) total = 0;
      static File sBackup;
      if (!index){
        ensureBackupDir();
        if (sBackup) sBackup.close();
        sBackup = LittleFS.open("/backup/last_fw.bin", "w");
        // Bắt đầu update firmware
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)){
          Update.printError(Serial);
        }
      }
      if (len){
        total += len;
        if (sBackup) sBackup.write(data, len);
        if (Update.write(data, len) != len){
          Update.printError(Serial);
        }
      }
      if (final){
        if (sBackup) sBackup.close();
        if (!Update.end(true)){
          Update.printError(Serial);
        }
        // First-time create original backup if not present (best-effort)
        if (!LittleFS.exists("/backup/orig_fw.bin")) {
          copyFile("/backup/last_fw.bin", "/backup/orig_fw.bin");
        }
      }
    });

  // ===== OTA FS IMAGE (LittleFS .bin) =====
  // Build từ PlatformIO: "Build Filesystem Image" -> .pio/build/<env>/littlefs.bin
  server.on("/api/ota/fsimage", HTTP_POST,
    [](AsyncWebServerRequest* req){
      if (Update.hasError()){
        sendJSON(req, 500, "FS image update failed");
      } else {
        sendJSON(req, 200, "FS image ok, rebooting...");
        req->client()->close(true);
        delay(s_reboot_delay_ms);
        ESP.restart();
      }
    },
    [](AsyncWebServerRequest* req, const String& filename, size_t index,
       uint8_t *data, size_t len, bool final){
      static size_t total = 0; if (!index) total = 0;
      static File sBackup;
      if (!index){
        ensureBackupDir();
        if (sBackup) sBackup.close();
        sBackup = LittleFS.open("/backup/last_fs.bin", "w");
        // Với LittleFS trên ESP32, dùng U_SPIFFS cho phân vùng DATA (LittleFS)
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)){
          Update.printError(Serial);
        }
      }
      if (len){
        total += len;
        if (sBackup) sBackup.write(data, len);
        if (Update.write(data, len) != len){
          Update.printError(Serial);
        }
      }
      if (final){
        if (sBackup) sBackup.close();
        if (!Update.end(true)){
          Update.printError(Serial);
        }
        if (!LittleFS.exists("/backup/orig_fs.bin")) {
          copyFile("/backup/last_fs.bin", "/backup/orig_fs.bin");
        }
      }
    });

  // ===== Backups: list and restore =====
  server.on("/api/backup/list", HTTP_GET, [&](AsyncWebServerRequest* req){
    String out = "{";
    out += "\"last_fw\":"; out += String(fileSizeOr0("/backup/last_fw.bin")); out += ",";
    out += "\"orig_fw\":"; out += String(fileSizeOr0("/backup/orig_fw.bin")); out += ",";
    out += "\"last_fs\":"; out += String(fileSizeOr0("/backup/last_fs.bin")); out += ",";
    out += "\"orig_fs\":"; out += String(fileSizeOr0("/backup/orig_fs.bin"));
    out += "}";
    req->send(200, "application/json", out);
  });

  // GET /api/ota/restore?what=fw|fs&which=last|orig
  server.on("/api/ota/restore", HTTP_GET, [&](AsyncWebServerRequest* req){
    String what = req->getParam("what") ? req->getParam("what")->value() : "";
    String which= req->getParam("which")? req->getParam("which")->value(): "last";
    ensureBackupDir();
    String path = String("/backup/") + (which=="orig"? (what=="fs"?"orig_fs.bin":"orig_fw.bin") : (what=="fs"?"last_fs.bin":"last_fw.bin"));
    File f = LittleFS.open(path, "r");
    if (!f){ sendJSON(req, 404, "no backup"); return; }
    bool ok = true;
    if (what == "fw"){
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) ok = false;
      const size_t bufSz = 1024; uint8_t buf[bufSz]; size_t n;
      while (ok && (n=f.read(buf, bufSz))>0){ if (Update.write(buf, n) != n) ok=false; }
      if (ok) ok = Update.end(true);
    } else if (what == "fs"){
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) ok = false;
      const size_t bufSz = 1024; uint8_t buf[bufSz]; size_t n;
      while (ok && (n=f.read(buf, bufSz))>0){ if (Update.write(buf, n) != n) ok=false; }
      if (ok) ok = Update.end(true);
    } else {
      ok = false;
    }
    f.close();
    if (!ok){ sendJSON(req, 500, "restore fail"); return; }
    sendJSON(req, 200, "restore ok; rebooting");
    req->client()->close(true); delay(s_reboot_delay_ms); ESP.restart();
  });

  // ===== Upload 1 file vào LittleFS (ví dụ /index.html) =====
  server.on("/api/upload", HTTP_POST,
    [](AsyncWebServerRequest* req){
      // nếu tới đây mà không có onUpload nghĩa là lỗi
      sendJSON(req, 200, "Upload done");
    },
    [](AsyncWebServerRequest* req, const String& filename, size_t index,
       uint8_t *data, size_t len, bool final){
      // đường dẫn đích qua query ?path=/index.html
      static File f;
      if (!index){
        if (!req->hasParam("path", true)){
          req->send(400, "application/json", "{\"ok\":false,\"msg\":\"missing path\"}");
          return;
        }
        String path = req->getParam("path", true)->value();
        if (!path.startsWith("/")) path = "/" + path;
        // tạo thư mục nếu cần (đơn giản hoá: chỉ cho file gốc)
        if (LittleFS.exists(path)) LittleFS.remove(path);
        f = LittleFS.open(path, "w");
        if (!f){
          req->send(500, "application/json", "{\"ok\":false,\"msg\":\"open fail\"}");
          return;
        }
      }
      if (len && f){
        f.write(data, len);
      }
      if (final && f){
        f.close();
      }
    });
}
