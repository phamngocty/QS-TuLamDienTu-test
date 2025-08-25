#include "config_store.h"
#include <Preferences.h>
#include <ArduinoJson.h>

static Preferences prefs;
static QSConfig g_cfg;

void CFG::begin(){
  prefs.begin("qs", false);

  // ==== Load các khóa cũ (giữ nguyên phần bạn đã có) ====
  g_cfg.mode              = (Mode)prefs.getUChar("mode", (uint8_t)g_cfg.mode);
  g_cfg.rpm_source        = (RpmSource)prefs.getUChar("rsrc", (uint8_t)g_cfg.rpm_source);
  g_cfg.ppr               = prefs.getFloat("ppr", g_cfg.ppr);
  g_cfg.rpm_min           = prefs.getUShort("rpmmin", g_cfg.rpm_min);
  g_cfg.manual_kill_ms    = prefs.getUShort("mkill", g_cfg.manual_kill_ms);
  g_cfg.debounce_shift_ms = prefs.getUShort("deb", g_cfg.debounce_shift_ms);
  g_cfg.holdoff_ms        = prefs.getUShort("hold", g_cfg.holdoff_ms);
  g_cfg.cut_output        = (CutOutputSel)prefs.getUChar("cout", (uint8_t)g_cfg.cut_output);
  g_cfg.ap_timeout_s      = prefs.getUShort("ap_t", g_cfg.ap_timeout_s);
  g_cfg.rpm_scale         = prefs.getFloat("rpm_s", g_cfg.rpm_scale);

  // ==== Load Wi-Fi AP config ====
  {
    String s = prefs.getString("ap_ssid", String(g_cfg.ap_ssid));
    String p = prefs.getString("ap_pass", String(g_cfg.ap_pass));
    s.toCharArray(g_cfg.ap_ssid, sizeof(g_cfg.ap_ssid));
    p.toCharArray(g_cfg.ap_pass, sizeof(g_cfg.ap_pass));
  }

  // ==== Load Auto Map (giữ nguyên) ====
  for (uint8_t i=0;i<g_cfg.map_count;i++){
    char key[8];
    snprintf(key, sizeof(key), "m%dl", i); g_cfg.map[i].rpm_lo = prefs.getUShort(key, g_cfg.map[i].rpm_lo);
    snprintf(key, sizeof(key), "m%dh", i); g_cfg.map[i].rpm_hi = prefs.getUShort(key, g_cfg.map[i].rpm_hi);
    snprintf(key, sizeof(key), "m%dt", i); g_cfg.map[i].cut_ms  = prefs.getUShort(key, g_cfg.map[i].cut_ms);
  }

  // ==== Load Backfire (KHÓA MỚI bf_*) ====
  g_cfg.bf_enable        = prefs.getUChar ("bfE",    g_cfg.bf_enable);
  g_cfg.bf_ign_only      = prefs.getUChar ("bfIGN",  g_cfg.bf_ign_only);
  g_cfg.bf_mode          = prefs.getUChar ("bfM",    g_cfg.bf_mode);
  g_cfg.bf_rpm_min       = prefs.getUShort("bfRmin", g_cfg.bf_rpm_min);
  g_cfg.bf_rpm_max       = prefs.getUShort("bfRmax", g_cfg.bf_rpm_max);
  g_cfg.bf_warmup_s      = prefs.getUShort("bfWarm", g_cfg.bf_warmup_s);
  g_cfg.bf_decel_thresh  = prefs.getUShort("bfDth",  g_cfg.bf_decel_thresh);
  g_cfg.bf_window_ms     = prefs.getUShort("bfWin",  g_cfg.bf_window_ms);
  g_cfg.bf_burst_count   = prefs.getUChar ("bfCnt",  g_cfg.bf_burst_count);
  g_cfg.bf_burst_on      = prefs.getUShort("bfOn",   g_cfg.bf_burst_on);
  g_cfg.bf_burst_off     = prefs.getUShort("bfOff",  g_cfg.bf_burst_off);
  g_cfg.bf_refractory_ms = prefs.getUShort("bfRef",  g_cfg.bf_refractory_ms);

  // ==== Load Lock config (mới) ====
  g_cfg.lock_enabled       = prefs.getBool ("lk_en",  g_cfg.lock_enabled);
  g_cfg.lock_cut_sel       = (CutOutputSel)prefs.getUChar("lk_cut", (uint8_t)g_cfg.lock_cut_sel);
  {
    String lc = prefs.getString("lk_code", String(g_cfg.lock_code));
    lc.toCharArray(g_cfg.lock_code, sizeof(g_cfg.lock_code));
  }
  g_cfg.lock_short_ms_max  = prefs.getUShort("lk_smax", g_cfg.lock_short_ms_max);
  g_cfg.lock_long_ms_min   = prefs.getUShort("lk_lmin", g_cfg.lock_long_ms_min);
  g_cfg.lock_gap_ms        = prefs.getUShort("lk_gap",  g_cfg.lock_gap_ms);
  g_cfg.lock_timeout_s     = prefs.getUShort("lk_tout", g_cfg.lock_timeout_s);
  g_cfg.lock_max_retries   = prefs.getUChar ("lk_maxr", g_cfg.lock_max_retries);
}

const QSConfig& CFG::get(){ return g_cfg; }


void CFG::set(const QSConfig &c){
  g_cfg = c;

  // ==== Save các khóa cũ (giữ nguyên) ====
  prefs.putUChar ("mode", (uint8_t)c.mode);
  prefs.putUChar ("rsrc", (uint8_t)c.rpm_source);
  prefs.putFloat ("ppr",  c.ppr);
  prefs.putUShort("rpmmin", c.rpm_min);
  prefs.putUShort("mkill",  c.manual_kill_ms);
  prefs.putUShort("deb",    c.debounce_shift_ms);
  prefs.putUShort("hold",   c.holdoff_ms);
  prefs.putUChar ("cout", (uint8_t)c.cut_output);
  prefs.putUShort("ap_t",  c.ap_timeout_s);
  prefs.putFloat ("rpm_s", c.rpm_scale);

  // Wi-Fi AP config
  prefs.putString("ap_ssid", String(c.ap_ssid));
  prefs.putString("ap_pass", String(c.ap_pass));

  for (uint8_t i=0;i<c.map_count;i++){
    char key[8];
    snprintf(key, sizeof(key), "m%dl", i); prefs.putUShort(key, c.map[i].rpm_lo);
    snprintf(key, sizeof(key), "m%dh", i); prefs.putUShort(key, c.map[i].rpm_hi);
    snprintf(key, sizeof(key), "m%dt", i); prefs.putUShort(key, c.map[i].cut_ms);
  }

  // ==== Save Backfire (bf_*) ====
  prefs.putUChar ("bfE",    c.bf_enable);
  prefs.putUChar ("bfIGN",  c.bf_ign_only);
  prefs.putUChar ("bfM",    c.bf_mode);
  prefs.putUShort("bfRmin", c.bf_rpm_min);
  prefs.putUShort("bfRmax", c.bf_rpm_max);
  prefs.putUShort("bfWarm", c.bf_warmup_s);
  prefs.putUShort("bfDth",  c.bf_decel_thresh);
  prefs.putUShort("bfWin",  c.bf_window_ms);
  prefs.putUChar ("bfCnt",  c.bf_burst_count);
  prefs.putUShort("bfOn",   c.bf_burst_on);
  prefs.putUShort("bfOff",  c.bf_burst_off);
  prefs.putUShort("bfRef",  c.bf_refractory_ms);

  // Lock config
  prefs.putBool ("lk_en",   c.lock_enabled);
  prefs.putUChar("lk_cut",  (uint8_t)c.lock_cut_sel);
  prefs.putString("lk_code", String(c.lock_code));
  prefs.putUShort("lk_smax", c.lock_short_ms_max);
  prefs.putUShort("lk_lmin", c.lock_long_ms_min);
  prefs.putUShort("lk_gap",  c.lock_gap_ms);
  prefs.putUShort("lk_tout", c.lock_timeout_s);
  prefs.putUChar ("lk_maxr", c.lock_max_retries);
}

bool CFG::exportJSON(String &out){
  StaticJsonDocument<1536> d;

  // ==== Xuất khóa cũ (giữ nguyên) ====
  d["mode"]               = (uint8_t)g_cfg.mode;
  d["rpm_source"]         = (uint8_t)g_cfg.rpm_source;
  d["ppr"]                = g_cfg.ppr;
  d["rpm_min"]            = g_cfg.rpm_min;
  d["manual_kill_ms"]     = g_cfg.manual_kill_ms;
  d["debounce_shift_ms"]  = g_cfg.debounce_shift_ms;
  d["holdoff_ms"]         = g_cfg.holdoff_ms;
  d["cut_output"]         = (uint8_t)g_cfg.cut_output;
  d["ap_timeout_s"]       = g_cfg.ap_timeout_s;
  d["rpm_scale"]          = g_cfg.rpm_scale;

  // Optionally export masked Wi-Fi info
  d["ap_ssid"]            = String(g_cfg.ap_ssid);
  d["ap_pass_len"]        = (uint8_t)strlen(g_cfg.ap_pass);

  // ==== Xuất Backfire (bf_*) ====
  d["bf_enable"]          = g_cfg.bf_enable;
  d["bf_ign_only"]        = g_cfg.bf_ign_only;
  d["bf_mode"]            = g_cfg.bf_mode;
  d["bf_rpm_min"]         = g_cfg.bf_rpm_min;
  d["bf_rpm_max"]         = g_cfg.bf_rpm_max;
  d["bf_warmup_s"]        = g_cfg.bf_warmup_s;
  d["bf_decel_thresh"]    = g_cfg.bf_decel_thresh;
  d["bf_window_ms"]       = g_cfg.bf_window_ms;
  d["bf_burst_count"]     = g_cfg.bf_burst_count;
  d["bf_burst_on"]        = g_cfg.bf_burst_on;
  d["bf_burst_off"]       = g_cfg.bf_burst_off;
  d["bf_refractory_ms"]   = g_cfg.bf_refractory_ms;

  // ==== Export Lock config ====
  d["lock_enabled"]        = g_cfg.lock_enabled;
  d["lock_cut_sel"]        = (uint8_t)g_cfg.lock_cut_sel;
  d["lock_short_ms_max"]   = g_cfg.lock_short_ms_max;
  d["lock_long_ms_min"]    = g_cfg.lock_long_ms_min;
  d["lock_gap_ms"]         = g_cfg.lock_gap_ms;
  d["lock_timeout_s"]      = g_cfg.lock_timeout_s;
  d["lock_max_retries"]    = g_cfg.lock_max_retries;

  // ==== Map ====
  JsonArray m = d.createNestedArray("map");
  for (uint8_t i=0;i<g_cfg.map_count;i++){
    JsonObject o = m.createNestedObject();
    o["lo"] = g_cfg.map[i].rpm_lo;
    o["hi"] = g_cfg.map[i].rpm_hi;
    o["t"]  = g_cfg.map[i].cut_ms;
  }

  serializeJsonPretty(d, out);
  return true;
}

bool CFG::importJSON(const String &in){
  StaticJsonDocument<2048> d;
  auto err = deserializeJson(d, in);
  if (err) return false;

  QSConfig c = g_cfg; // bắt đầu từ cấu hình hiện tại

  // ==== Nhận khóa cũ (giữ nguyên) ====
  if (d.containsKey("mode"))               c.mode = (Mode)(uint8_t)d["mode"].as<uint8_t>();
  if (d.containsKey("rpm_source"))         c.rpm_source = (RpmSource)(uint8_t)d["rpm_source"].as<uint8_t>();
  if (d.containsKey("ppr"))                c.ppr = d["ppr"].as<float>();
  if (d.containsKey("rpm_min"))            c.rpm_min = d["rpm_min"].as<uint16_t>();
  if (d.containsKey("manual_kill_ms"))     c.manual_kill_ms = d["manual_kill_ms"].as<uint16_t>();
  if (d.containsKey("debounce_shift_ms"))  c.debounce_shift_ms = d["debounce_shift_ms"].as<uint16_t>();
  if (d.containsKey("holdoff_ms"))         c.holdoff_ms = d["holdoff_ms"].as<uint16_t>();
  if (d.containsKey("cut_output"))         c.cut_output = (CutOutputSel)(uint8_t)d["cut_output"].as<uint8_t>();
  if (d.containsKey("ap_timeout_s"))       c.ap_timeout_s = d["ap_timeout_s"].as<uint16_t>();
  if (d.containsKey("rpm_scale"))          c.rpm_scale = d["rpm_scale"].as<float>();

  // ==== Wi-Fi AP config ====
  if (d.containsKey("ap_ssid")) {
    String s = d["ap_ssid"].as<String>();
    s.toCharArray(c.ap_ssid, sizeof(c.ap_ssid));
  }
  if (d.containsKey("ap_pass")) {
    String p = d["ap_pass"].as<String>();
    p.toCharArray(c.ap_pass, sizeof(c.ap_pass));
  }

  // ==== Nhận Backfire (bf_*) ====
  if (d.containsKey("bf_enable"))          c.bf_enable        = d["bf_enable"].as<uint8_t>();
  if (d.containsKey("bf_ign_only"))        c.bf_ign_only      = d["bf_ign_only"].as<uint8_t>();
  if (d.containsKey("bf_mode"))            c.bf_mode          = d["bf_mode"].as<uint8_t>();
  if (d.containsKey("bf_rpm_min"))         c.bf_rpm_min       = d["bf_rpm_min"].as<uint16_t>();
  if (d.containsKey("bf_rpm_max"))         c.bf_rpm_max       = d["bf_rpm_max"].as<uint16_t>();
  if (d.containsKey("bf_warmup_s"))        c.bf_warmup_s      = d["bf_warmup_s"].as<uint16_t>();
  if (d.containsKey("bf_decel_thresh"))    c.bf_decel_thresh  = d["bf_decel_thresh"].as<uint16_t>();
  if (d.containsKey("bf_window_ms"))       c.bf_window_ms     = d["bf_window_ms"].as<uint16_t>();
  if (d.containsKey("bf_burst_count"))     c.bf_burst_count   = d["bf_burst_count"].as<uint8_t>();
  if (d.containsKey("bf_burst_on"))        c.bf_burst_on      = d["bf_burst_on"].as<uint16_t>();
  if (d.containsKey("bf_burst_off"))       c.bf_burst_off     = d["bf_burst_off"].as<uint16_t>();
  if (d.containsKey("bf_refractory_ms"))   c.bf_refractory_ms = d["bf_refractory_ms"].as<uint16_t>();

  // ==== Lock config ====
  if (d.containsKey("lock_enabled"))        c.lock_enabled       = d["lock_enabled"].as<bool>();
  if (d.containsKey("lock_cut_sel")) {
    // allow string ("ign"/"inj") or number
    if (d["lock_cut_sel"].is<const char*>()) {
      String s = String((const char*)d["lock_cut_sel"]);
      s.toLowerCase();
      c.lock_cut_sel = (s == "inj") ? CutOutputSel::INJ : CutOutputSel::IGN;
    } else {
      c.lock_cut_sel = (CutOutputSel)(uint8_t)d["lock_cut_sel"].as<uint8_t>();
    }
  }
  if (d.containsKey("lock_code")) {
    String s = d["lock_code"].as<String>();
    s.toCharArray(c.lock_code, sizeof(c.lock_code));
  }
  if (d.containsKey("lock_short_ms_max"))  c.lock_short_ms_max  = d["lock_short_ms_max"].as<uint16_t>();
  if (d.containsKey("lock_long_ms_min"))   c.lock_long_ms_min   = d["lock_long_ms_min"].as<uint16_t>();
  if (d.containsKey("lock_gap_ms"))        c.lock_gap_ms        = d["lock_gap_ms"].as<uint16_t>();
  if (d.containsKey("lock_timeout_s"))     c.lock_timeout_s     = d["lock_timeout_s"].as<uint16_t>();
  if (d.containsKey("lock_max_retries"))   c.lock_max_retries   = d["lock_max_retries"].as<uint8_t>();

  // ==== Map ====
  if (d.containsKey("map")){
    JsonArray m = d["map"].as<JsonArray>();
    uint8_t idx=0;
    for (JsonObject o : m){
      if (idx>=4) break;
      c.map[idx].rpm_lo = o["lo"];
      c.map[idx].rpm_hi = o["hi"];
      c.map[idx].cut_ms = o["t"];
      idx++;
    }
    c.map_count = (uint8_t)min<size_t>(m.size(), 4);
  }

  CFG::set(c);
  return true;
}
