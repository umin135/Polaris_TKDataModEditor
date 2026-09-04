#pragma once
#include "extract/T7Moveset.h"
#include "moveset/data/MotbinData.h"
#include <string>

// Convert extracted T7 moveset into MotbinData ready for SaveMotbin.
// On success, out.rawBytes is a synthetic 0x318 header and blocks are filled.
// optionalWarn receives a short conversion summary (unmapped ids, etc.).
bool ConvertT7ToMotbin(const T7::Moveset& src,
                       uint32_t t7FighterId,
                       MotbinData& out,
                       std::string& errorMsg,
                       std::string* optionalWarn = nullptr);
