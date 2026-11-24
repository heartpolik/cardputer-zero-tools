// Byte strings and the checksums/CRCs common in serial protocols (pure C++).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bits {

struct ByteParse {
    bool ok{false};
    std::vector<uint8_t> bytes;
    bool text{false};  // parsed as a "quoted string"
    std::string error;
    size_t error_pos{0};
};

// "01 03 00 0A", "0x01,0x03", "01030A" or "\"text\\r\\n\"" (escapes \n \r \t \0 \\ \" \xHH)
ByteParse parse_bytes(const std::string& src);

struct CrcSpec {
    const char* name;
    const char* note;  // where you meet it
    int width;         // 8, 16 or 32 (0 = simple sum/xor)
    uint32_t poly, init;
    bool refin, refout;
    uint32_t xorout;
    bool little_endian_on_wire;  // e.g. Modbus sends the low byte first
};

const std::vector<CrcSpec>& checksums();

// SUM8 / XOR8 are width 0 with poly 1 / 2.
uint32_t compute(const CrcSpec& spec, const std::vector<uint8_t>& data);

// the value as bytes in wire order: "37 4B"
std::string wire_bytes(const CrcSpec& spec, uint32_t value);

}  // namespace bits
