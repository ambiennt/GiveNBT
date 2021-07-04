#pragma once

#include "Enchant.h"

enum class EnchantResultType: char {
  Fail_0 = 0x0,
  Conflict = 0x1,
  Increment = 0x2,
  Enchant = 0x3
};

struct EnchantResult {
  EnchantResultType result;
  size_t EnchantIdx;
  int level;
};
