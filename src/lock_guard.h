#pragma once
#include <Arduino.h>
#include "config.h"

namespace LOCK {
  void begin();
  void reset();                 // xóa buffer đang nhập
  void tick();                  // gọi mỗi vòng lặp, đọc SHIFT_NPN
  bool isLocked();              // true nếu đang khóa
  bool justUnlocked();          // true duy nhất 1 lần ngay sau khi mở
  void forceLock();             // ép về trạng thái khóa (nếu cần)
  bool adminUnlock(const String& pass); // so pass với CFG::get().lock_code, mở khóa

}
