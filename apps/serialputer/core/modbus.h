// Modbus RTU: building requests, reading responses, labelling sniffed frames,
// a request/response master with timeouts and an address scanner (pure C++).
#pragma once

#include "capture.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ser::modbus {

enum : uint8_t {
    kReadCoils = 1,
    kReadDiscrete = 2,
    kReadHolding = 3,
    kReadInput = 4,
    kWriteCoil = 5,
    kWriteRegister = 6,
    kWriteCoils = 15,
    kWriteRegisters = 16,
};

const char* fn_name(uint8_t fc);         // "READ HOLDING"; "" when unknown
const char* exception_name(uint8_t code);  // "ILLEGAL DATA ADDRESS"
bool is_read(uint8_t fc);
bool is_bit_fn(uint8_t fc);  // coils / discrete inputs
uint8_t next_read_fn(uint8_t fc, int dir);  // cycles 1, 2, 3, 4

uint16_t crc16(const uint8_t* data, size_t n);
bool crc_ok(const Bytes& frame);
void append_crc(Bytes& frame);

struct Request {
    uint8_t addr{1};
    uint8_t fc{kReadHolding};
    uint16_t start{0};
    uint16_t count{10};
    std::vector<uint16_t> values;  // writes: 6 uses values[0], 16 all, 5 values[0] != 0
};

Bytes encode(const Request& r);
// most registers / bits one read may ask for
uint16_t max_count(uint8_t fc);

struct Response {
    bool ok{false};
    bool exception{false};
    uint8_t code{0};             // exception code
    std::string error;           // why the frame does not answer the request
    std::vector<uint16_t> regs;  // register reads
    std::vector<bool> bits;      // coil / discrete reads
};

Response decode_response(const Request& req, const Bytes& frame);

// ------------------------------------------------------------ sniffing

struct Description {
    enum class Kind { Request, Response, Exception, Unknown } kind{Kind::Unknown};
    bool crc_ok{false};
    uint8_t addr{0};
    uint8_t fc{0};
    uint16_t start{0};  // first register of a response (from the matching request)
    bool start_known{false};
    std::vector<uint16_t> regs;
    std::string text;  // "#1 READ HOLDING 0..9", "#1 -> 10 regs", "#1 ! ILLEGAL DATA ADDRESS"
};

// Labels frames seen on a bus; remembers requests to place response registers.
class Sniffer {
public:
    Description describe(const Bytes& frame);
    void reset() { pending_.clear(); }

private:
    std::map<uint8_t, Request> pending_;  // by address
};

// Back-to-back RTU frames read as one chunk -> the frames, split where CRCs match.
// Returns the input unchanged when it cannot be split cleanly.
std::vector<Bytes> split_frames(const Bytes& data);

// ------------------------------------------------------------ master

struct Stats {
    uint32_t sent{0}, ok{0}, exceptions{0}, errors{0}, timeouts{0};
    uint32_t last_ms{0};  // last response time
};

enum class Event { None, Ok, Exception, Error, Timeout };

class Master {
public:
    uint32_t timeout_ms{500};

    bool busy() const { return busy_; }
    const Stats& stats() const { return stats_; }
    void reset_stats() { stats_ = {}; }
    const Request& request() const { return req_; }
    const Response& last() const { return last_; }

    // the bytes to send now
    Bytes start(const Request& r, uint64_t now_us);
    // a received frame (frames equal to our own request are echo and ignored)
    Event on_frame(const Bytes& frame, uint64_t now_us);
    Event on_tick(uint64_t now_us);
    void cancel() { busy_ = false; }

private:
    Request req_;
    Bytes sent_;
    Response last_;
    Stats stats_;
    bool busy_{false};
    uint64_t sent_us_{0};
    Bytes partial_;  // a response may arrive split by a pause
};

// Walks addresses first..last with one request each; any answer, exceptions included,
// means a device is there.
class Scanner {
public:
    struct Found {
        uint8_t addr;
        bool exception;
        uint8_t code;
    };

    void start(const Request& probe, uint8_t first, uint8_t last);
    bool running() const { return running_; }
    void stop() { running_ = false; }
    uint8_t current() const { return addr_; }
    uint8_t first() const { return first_; }
    uint8_t last() const { return last_; }
    const std::vector<Found>& found() const { return found_; }
    Request probe() const;
    // result of probing current(); moves on, false when finished
    bool advance(Event ev, const Response& r);

private:
    Request probe_;
    uint8_t first_{1}, last_{247}, addr_{1};
    bool running_{false};
    std::vector<Found> found_;
};

// ------------------------------------------------------------ registers

enum class Interp { U16, I16, Hex, U32, U32Swap, I32, F32, F32Swap };
const char* interp_name(Interp i);  // "U16", "U32 ABCD", "F32 CDAB"
Interp next_interp(Interp i, int dir = 1);
bool is_pair(Interp i);  // takes two registers
// value of regs[i] (and regs[i+1] for pairs); "" if the pair is incomplete
std::string format_value(const std::vector<uint16_t>& regs, size_t i, Interp interp);

}  // namespace ser::modbus
