#pragma once
#include "config.h"

namespace CFG {
  void begin();
  const QSConfig& get();        // << đổi từ QSConfig get();
  void set(const QSConfig &c);
  bool exportJSON(String &out);
  bool importJSON(const String &in);
  // convenience: set only Wi-Fi credentials
  inline void setWifi(const char* ssid, const char* pass){
    auto c = get();
    if (ssid) strncpy(c.ap_ssid, ssid, sizeof(c.ap_ssid));
    if (pass) strncpy(c.ap_pass, pass, sizeof(c.ap_pass));
    c.ap_ssid[sizeof(c.ap_ssid)-1] = '\0';
    c.ap_pass[sizeof(c.ap_pass)-1] = '\0';
    set(c);
  }
}
