#pragma once
#include <Arduino.h>

// ===== Defaults & Limits =====
// Modes
enum class Mode : uint8_t { MANUAL = 0, AUTO = 1 };
enum class RpmSource : uint8_t { COIL = 0, INJECTOR = 1 };
enum class CutOutputSel : uint8_t { IGN = 0, INJ = 1 };

struct AutoBand { uint16_t rpm_lo; uint16_t rpm_hi; uint16_t cut_ms; };
struct BackfireCfg {
  bool     enabled        = false;   // đã có
  uint16_t rpm_min        = 5000;    // đã có
  uint16_t extra_ms       = 15;      // đã có
  bool     force_ign      = true;    // MỚI: ép cắt IGN khi backfire
  uint8_t  skip_sparks    = 0;       // MỚI: số tia lửa bỏ qua sau cắt (0..2)
};

struct QSConfig {
  Mode mode = Mode::AUTO;
  RpmSource rpm_source = RpmSource::COIL;
  float ppr = 1.0f;                 // 0.5 / 1 / 2 selectable
  uint16_t rpm_min = 2500;          // Below this no cut
  uint16_t manual_kill_ms = 70;     // Manual cut time
  uint16_t debounce_shift_ms = 15;  // Shift sensor debounce
  uint16_t holdoff_ms = 200;        // Lockout after cut
  CutOutputSel cut_output = CutOutputSel::IGN; // default output
  // Backfire (relay-simple): OFF by default per user request
    BackfireCfg backfire;
  bool backfire_enabled = false;
  uint16_t backfire_extra_ms = 15;  // Extra cut when backfire enabled
  uint16_t backfire_min_rpm = 4500; // Only above this rpm
  
  // ==== Backfire (mới) ====
  uint8_t  bf_enable        = 0;     // 0/1
  uint8_t  bf_ign_only      = 1;     // 0/1
  uint8_t  bf_mode          = 3;     // 1=SHIFT,2=OVERRUN,3=both
  uint16_t bf_rpm_min       = 4500;
  uint16_t bf_rpm_max       = 9000;
  uint16_t bf_warmup_s      = 120;
  uint16_t bf_decel_thresh  = 3000;  // rpm/s
  uint16_t bf_window_ms     = 250;
  uint8_t  bf_burst_count   = 3;
  uint16_t bf_burst_on      = 25;    // ms
  uint16_t bf_burst_off     = 75;    // ms
  uint16_t bf_refractory_ms = 1500;  // ms
  
  // AP timeout
  uint16_t ap_timeout_s = 120;      // 0 = never auto close
  // Calibration
  float rpm_scale = 1.0f;           // rpm_display = rpm_raw * rpm_scale
  // Auto map (up to 6 bands typical)
  AutoBand map[7] = {
    {3000, 4000, 75},
    {4000, 5000, 64},
    {5000, 6500, 56},
    {6500, 8000, 48},
    {8000, 9500, 44},
    {9500, 11500, 40},
    {11500, 14000, 36}
  };

  
  uint8_t map_count = 4;
  // Wi-Fi AP config
  char     ap_ssid[33]  = "";        // empty -> auto SSID
  char     ap_pass[65]  = "12345678"; // min 8 chars for AP
    // ===== Vehicle Lock =====
  bool     lock_enabled    = false;
  CutOutputSel lock_cut_sel = CutOutputSel::IGN; // cắt IGN khi đang khóa (hoặc INJ)
  char     lock_code[9]    = "1001"; // chuỗi 0/1, tối đa 8 ký tự + null
  uint16_t lock_short_ms_max = 300;  // < 300ms = nhấn "ngắn" (bit 0)
  uint16_t lock_long_ms_min  = 600;  // >= 600ms = nhấn "dài" (bit 1)
  uint16_t lock_gap_ms       = 400;  // khoảng nghỉ tối đa giữa hai nhịp
  uint16_t lock_timeout_s    = 30;   // thời gian cho phép nhập, hết giờ -> giữ khóa
  uint8_t  lock_max_retries  = 5;    // số lần sai tối đa, vượt -> giữ khóa (có thể cần tắt mở lại)

};

// Limits / safety
static constexpr uint16_t CUT_MS_MAX = 150; // hard cap
static constexpr uint16_t CUT_MS_MIN = 20;
