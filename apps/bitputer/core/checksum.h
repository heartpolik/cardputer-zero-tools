// Byte strings and checksums now live in czcore (common/core/bytes.h).
#pragma once

#include "bytes.h"

namespace bits {
using cz::ByteParse;
using cz::checksums;
using cz::compute;
using cz::CrcSpec;
using cz::parse_bytes;
using cz::wire_bytes;
}  // namespace bits
