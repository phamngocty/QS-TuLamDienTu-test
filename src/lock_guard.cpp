#include "lock_guard.h"
#include "config_store.h"
#include "trigger_input.h"
#include "cut_output.h"
#include "pins.h"

#include "config.h"
static inline CutLine toCutLine(CutOutputSel s) {
  return (s == CutOutputSel::IGN) ? CutLine::IGN : CutLine::INJ;
}
namespace {
  bool locked = false;
  bool unlocked_pulse = false;

  enum class Stage : uint8_t { IDLE, PRESSING, GAP };
  Stage st = Stage::IDLE;

  uint32_t t_press_start = 0;
  uint32_t t_last_edge = 0;
  String   seq = "";
  uint8_t  retries = 0;
  uint32_t t_start_window = 0;

  QSConfig cfg() { return CFG::get(); }

 // thay cho applyCutWhileLocked()
void applyCutWhileLocked() {
  auto c = CFG::get();
  CUT::set(toCutLine(c.lock_cut_sel), true); // true = cắt
}

// thay cho releaseCut()
void releaseCut() {
  // Tùy bạn muốn nhả cả 2 line hay chỉ line đang dùng.
  CUT::set(CutLine::IGN, false);
  CUT::set(CutLine::INJ, false);
}


  void finishAttempt(bool ok) {
    if (ok) {
      locked = false;
      unlocked_pulse = true;
      releaseCut();
    } else {
      retries++;
      seq = "";
      st = Stage::IDLE;
      t_press_start = 0;
    }
  }

  // Phân loại nhịp thành 0/1 theo threshold
  bool classifyBit(uint32_t dur_ms, char &bitOut) {
    auto c = cfg();
    if (dur_ms < c.lock_short_ms_max) { bitOut = '0'; return true; }
    if (dur_ms >= c.lock_long_ms_min) { bitOut = '1'; return true; }
    return false; // vùng mờ không chấp nhận
  }

  void checkSequenceDone() {
    auto c = cfg();
    // Khi khoảng nghỉ > gap hoặc đủ độ dài mã -> kết thúc và so sánh
    if (seq.length() >= strlen(c.lock_code)) {
      bool ok = (seq.substring(0, strlen(c.lock_code)) == String(c.lock_code));
      finishAttempt(ok);
    }
  }
}

void LOCK::begin() {
  auto c = cfg();
  locked = c.lock_enabled;
  unlocked_pulse = false;
  retries = 0;
  seq = "";
  st = Stage::IDLE;
  t_press_start = 0;
  t_start_window = millis();

  if (locked) applyCutWhileLocked();
}

void LOCK::reset() {
  seq = "";
  st = Stage::IDLE;
  t_press_start = 0;
  t_start_window = millis();
}

void LOCK::forceLock() {
  locked = true;
  unlocked_pulse = false;
  retries = 0;
  reset();
  applyCutWhileLocked();
}

bool LOCK::isLocked() { return locked; }

bool LOCK::justUnlocked() {
  if (unlocked_pulse) { unlocked_pulse = false; return true; }
  return false;
}

void LOCK::tick() {
  auto c = cfg();
  if (!c.lock_enabled) { locked = false; return; }
  if (!locked) return;

  // Timeout & retry limit
  if (c.lock_timeout_s > 0 && (millis() - t_start_window) > (uint32_t)c.lock_timeout_s * 1000UL) {
    // Hết thời gian -> vẫn locked, giữ cut
    applyCutWhileLocked();
    return;
  }
  if (c.lock_max_retries > 0 && retries >= c.lock_max_retries) {
    applyCutWhileLocked();
    return;
  }

  // Đọc cảm biến nhịp (sử dụng API trong trigger_input)
  // Giả định SHIFT_NPN active-low và trigger_input có hàm trả trạng thái theo thời gian thực:
  //   bool TRIG::rawLevel();  // mức tín hiệu hiện tại (true = đang nhấn)
  //   (Nếu chưa có, bạn có thể thêm một hàm getRaw() trong trigger_input.cpp)
  bool pressed = TRIG::rawLevel();

  uint32_t now = millis();
  switch (st) {
    case Stage::IDLE:
      if (pressed) { st = Stage::PRESSING; t_press_start = now; }
      break;

    case Stage::PRESSING:
      if (!pressed) {
        uint32_t dur = now - t_press_start;
        char bit;
        if (classifyBit(dur, bit)) seq += bit;
        else { // nhịp lỗi -> reset chuỗi hiện tại
          seq = "";
        }
        st = Stage::GAP;
        t_last_edge = now;
        checkSequenceDone();
      }
      break;

    case Stage::GAP:
      if (pressed) {
        // Bắt đầu nhịp tiếp theo
        st = Stage::PRESSING;
        t_press_start = now;
      } else {
        if (now - t_last_edge > c.lock_gap_ms) {
          // Kết thúc chuỗi vì nghỉ quá lâu
          if (seq.length() > 0) {
            bool ok = (seq == String(c.lock_code));
            finishAttempt(ok);
          }
          // reset cho lần kế tiếp (nếu sai)
          if (locked) { seq = ""; st = Stage::IDLE; }
        }
      }
      break;
  }

  // Khi đang khóa, đảm bảo cắt
  applyCutWhileLocked();
}

bool LOCK::adminUnlock(const String& pass){
  auto c = CFG::get();
  String code = String(c.lock_code);
  if (pass == code) {
    locked = false;
    unlocked_pulse = true;  // cho UI biết vừa mở
    // nhả cắt ngay khi mở:
    CUT::set(toCutLine(c.lock_cut_sel), false);
    return true;
  }
  return false;
}