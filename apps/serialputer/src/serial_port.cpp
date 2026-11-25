#include "serial_port.h"

#include "session.h"
#include "sim_bus.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

#include <fcntl.h>
#include <poll.h>
#include <sys/file.h>
#include <termios.h>
#include <unistd.h>

namespace ser_ui {
namespace fs = std::filesystem;

uint64_t now_us() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}

namespace {

std::string sys_error(const std::string& what) { return what + ": " + std::strerror(errno); }

// kernel console devices from the command line ("console=serial0,115200 console=tty1")
std::vector<std::string> console_devices() {
    std::vector<std::string> out;
    std::ifstream in("/proc/cmdline");
    std::string tok;
    while (in >> tok) {
        if (tok.rfind("console=", 0) != 0) continue;
        std::string dev = tok.substr(8);
        dev = dev.substr(0, dev.find(','));
        out.push_back(dev);
    }
    return out;
}

bool is_console(const std::string& path, const std::string& target) {
    const auto name = fs::path(path).filename().string();
    const auto tname = fs::path(target).filename().string();
    for (const auto& c : console_devices()) {
        std::error_code ec;
        const auto real = fs::canonical("/dev/" + c, ec);
        const std::string rname = ec ? c : real.filename().string();
        if (c == name || rname == name || (!tname.empty() && rname == tname)) return true;
    }
    return false;
}

#if defined(__linux__)
speed_t speed_for(int baud, bool& ok) {
    ok = true;
    switch (baud) {
        case 1200: return B1200;
        case 2400: return B2400;
        case 4800: return B4800;
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 921600: return B921600;
        default: ok = false; return B9600;
    }
}
#else
speed_t speed_for(int baud, bool& ok) {
    ok = baud > 0;
    return static_cast<speed_t>(baud);
}
#endif

// ------------------------------------------------------------ tty

class TtyPort : public Port {
public:
    TtyPort(int fd) : fd_(fd) { reader_ = std::thread([this] { read_loop(); }); }
    ~TtyPort() override {
        stop_ = true;
        if (reader_.joinable()) reader_.join();
        flock(fd_, LOCK_UN);
        ::close(fd_);
    }

    bool set_format(const ser::SerialFormat& f, std::string& error) override {
        termios t{};
        if (tcgetattr(fd_, &t) != 0) {
            error = sys_error("tcgetattr");
            return false;
        }
        cfmakeraw(&t);
        t.c_cflag |= CLOCAL | CREAD;
        t.c_cflag &= ~static_cast<tcflag_t>(CSIZE | PARENB | PARODD | CSTOPB);
#ifdef CRTSCTS
        t.c_cflag &= ~static_cast<tcflag_t>(CRTSCTS);
#endif
        t.c_iflag &= ~static_cast<tcflag_t>(IXON | IXOFF | IXANY);
        switch (f.data_bits) {
            case 5: t.c_cflag |= CS5; break;
            case 6: t.c_cflag |= CS6; break;
            case 7: t.c_cflag |= CS7; break;
            default: t.c_cflag |= CS8; break;
        }
        if (f.parity == 'E') t.c_cflag |= PARENB;
        if (f.parity == 'O') t.c_cflag |= PARENB | PARODD;
        if (f.stop_bits == 2) t.c_cflag |= CSTOPB;
        t.c_cc[VMIN] = 0;
        t.c_cc[VTIME] = 0;
        bool ok = false;
        const speed_t sp = speed_for(f.baud, ok);
        if (!ok) {
            error = "unsupported baud " + std::to_string(f.baud);
            return false;
        }
        cfsetispeed(&t, sp);
        cfsetospeed(&t, sp);
        if (tcsetattr(fd_, TCSANOW, &t) != 0) {
            error = errno == EINVAL ? "this UART can't do " + ser::frame_name(f) + " (Pi mini UART: 8N1/7N1 only)"
                                    : sys_error("tcsetattr");
            return false;
        }
        // drivers may drop what they cannot do (the Pi mini UART has no parity
        // and one stop bit only): read back and refuse a different frame
        termios got{};
        const tcflag_t mask = CSIZE | PARENB | PARODD | CSTOPB;
        if (tcgetattr(fd_, &got) == 0 && (got.c_cflag & mask) != (t.c_cflag & mask)) {
            error = "this UART can't do " + ser::frame_name(f) + " (Pi mini UART: 8N1/7N1 only)";
            return false;
        }
        tcflush(fd_, TCIOFLUSH);
        return true;
    }

    bool write(const ser::Bytes& b, std::string& error) override {
        size_t done = 0;
        const uint64_t deadline = now_us() + 2000000;
        while (done < b.size()) {
            const ssize_t n = ::write(fd_, b.data() + done, b.size() - done);
            if (n > 0) {
                done += static_cast<size_t>(n);
                continue;
            }
            if (n < 0 && errno != EAGAIN && errno != EINTR) {
                error = sys_error("write");
                return false;
            }
            if (now_us() > deadline) {
                error = "write timed out";
                return false;
            }
            pollfd p{fd_, POLLOUT, 0};
            ::poll(&p, 1, 20);
        }
        return true;
    }

    void drain(std::vector<Chunk>& out) override {
        std::lock_guard<std::mutex> lock(mu_);
        while (!queue_.empty()) {
            out.push_back(std::move(queue_.front()));
            queue_.pop_front();
        }
    }

    std::string failure() const override {
        std::lock_guard<std::mutex> lock(mu_);
        return failure_;
    }

private:
    void read_loop() {
        uint8_t buf[512];
        while (!stop_) {
            pollfd p{fd_, POLLIN, 0};
            const int r = ::poll(&p, 1, 50);
            if (r < 0) {
                if (errno == EINTR) continue;
                fail(sys_error("poll"));
                return;
            }
            if (r == 0) continue;
            if (p.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                fail("port closed (device removed?)");
                return;
            }
            const ssize_t n = ::read(fd_, buf, sizeof buf);
            if (n > 0) {
                Chunk c{now_us(), ser::Bytes(buf, buf + n)};
                std::lock_guard<std::mutex> lock(mu_);
                queue_.push_back(std::move(c));
                // a reader that is not drained must not eat all memory
                while (queue_.size() > 4096) queue_.pop_front();
            } else if (n < 0 && errno != EAGAIN && errno != EINTR) {
                fail(sys_error("read"));
                return;
            }
        }
    }

    void fail(const std::string& why) {
        std::lock_guard<std::mutex> lock(mu_);
        failure_ = why;
    }

    int fd_;
    std::thread reader_;
    std::atomic<bool> stop_{false};
    mutable std::mutex mu_;
    std::deque<Chunk> queue_;
    std::string failure_;
};

// ------------------------------------------------------------ simulated bus

class SimPort : public Port {
public:
    explicit SimPort(const ser::SerialFormat& f) : fmt_(f) {}

    bool set_format(const ser::SerialFormat& f, std::string&) override {
        fmt_ = f;
        return true;
    }

    bool write(const ser::Bytes& b, std::string&) override {
        const uint64_t now = now_us();
        const uint64_t ch = ser::char_time_us(fmt_);
        const ser::Bytes reply = bus_.respond(b, now);
        if (reply.empty()) return true;
        // the request goes out, the device thinks ~6 ms, then answers
        const uint64_t at = now + ch * b.size() + 6000 + ch * reply.size();
        pending_.push_back({at, reply});
        return true;
    }

    void drain(std::vector<Chunk>& out) override {
        const uint64_t now = now_us();
        while (!pending_.empty() && pending_.front().t_us <= now) {
            out.push_back(std::move(pending_.front()));
            pending_.pop_front();
        }
    }

private:
    ser::SerialFormat fmt_;
    ser::SimBus bus_;
    std::deque<Chunk> pending_;
};

}  // namespace

PortInfo describe_port(const std::string& path) {
    PortInfo info;
    info.path = path;
    if (path == ser::kSimPort) return info;
    std::error_code ec;
    if (fs::is_symlink(path, ec)) {
        const auto real = fs::canonical(path, ec);
        if (!ec) info.target = real.string();
    }
    info.console = is_console(path, info.target);
    info.accessible = ::access(path.c_str(), R_OK | W_OK) == 0;
    return info;
}

std::vector<PortInfo> list_ports() {
    std::vector<PortInfo> out;
    out.push_back(describe_port(ser::kSimPort));
    std::vector<std::string> paths;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator("/dev", ec)) {
        const std::string name = e.path().filename().string();
#if defined(__APPLE__)
        const bool want = name.rfind("cu.", 0) == 0;
#else
        bool want = name.rfind("ttyAMA", 0) == 0 || name.rfind("ttyUSB", 0) == 0 || name.rfind("ttyACM", 0) == 0 ||
                    name == "serial0" || name == "serial1";
        // ttyS* exist as placeholders on most PCs: keep only ones backed by a device
        if (name.rfind("ttyS", 0) == 0) {
            want = fs::exists("/sys/class/tty/" + name + "/device", ec) && name.size() <= 5;
        }
#endif
        if (want) paths.push_back(e.path().string());
    }
    std::sort(paths.begin(), paths.end(), [](const std::string& a, const std::string& b) {
        // serial0/1 aliases first, then by name
        const bool sa = a.find("/serial") != std::string::npos, sb = b.find("/serial") != std::string::npos;
        if (sa != sb) return sa;
        return a < b;
    });
    for (const auto& p : paths) out.push_back(describe_port(p));
    return out;
}

std::unique_ptr<Port> open_port(const std::string& path, const ser::SerialFormat& fmt, std::string& error) {
    if (path == ser::kSimPort) return std::make_unique<SimPort>(fmt);
    const int fd = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        error = errno == EACCES ? "no permission (user not in dialout?)" : sys_error("open");
        return nullptr;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        ::close(fd);
        error = "port is in use by another program";
        return nullptr;
    }
    auto port = std::make_unique<TtyPort>(fd);
    if (!port->set_format(fmt, error)) return nullptr;
    return port;
}

}  // namespace ser_ui
