#pragma once
#include "common.hpp"
#include <cstdarg>

namespace lacz::log {

// Append-only diagnostic log. Written to %APPDATA%\LaczDLC\lacz.log. Safe to
// call from any thread; each call opens/appends/closes so a crash still leaves
// the last line on disk.
void write(const char* fmt, ...);

} // namespace lacz::log