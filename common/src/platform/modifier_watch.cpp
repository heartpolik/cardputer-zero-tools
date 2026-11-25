#include "modifier_watch.h"

#if USE_DESKTOP
#include <SDL.h>
#else
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace platform {
namespace {

#if !USE_DESKTOP
constexpr const char* kBus = "/dev/i2c-1";
constexpr int kM5ioe1 = 0x4F;
constexpr uint8_t kRegShift = 0xBD;
constexpr uint8_t kRegFn = 0xBE;
constexpr uint8_t kRegSym = 0xBF;

bool read_reg(int fd, uint8_t reg, uint8_t& value) {
    i2c_smbus_data data{};
    i2c_smbus_ioctl_data args{};
    args.read_write = I2C_SMBUS_READ;
    args.command = reg;
    args.size = I2C_SMBUS_BYTE_DATA;
    args.data = &data;
    if (ioctl(fd, I2C_SMBUS, &args) != 0) return false;
    value = data.byte;
    return true;
}
#endif

}  // namespace

const char* layer_name(KeyLayer layer) {
    switch (layer) {
        case KeyLayer::Shift: return "SHIFT";
        case KeyLayer::Sym: return "SYM";
        case KeyLayer::Fn: return "FN";
        default: return "";
    }
}

LayerState layer_from_registers(uint8_t shift, uint8_t fn, uint8_t sym) {
    if (fn) return {KeyLayer::Fn, fn >= 3};
    if (sym) return {KeyLayer::Sym, sym >= 3};
    if (shift) return {KeyLayer::Shift, shift >= 3};
    return {};
}

ModifierWatch::ModifierWatch() {
#if !USE_DESKTOP
    fd_ = open(kBus, O_RDWR | O_CLOEXEC);
    // the expander has a kernel driver bound: FORCE is needed to address it; only reads follow
    if (fd_ >= 0 && ioctl(fd_, I2C_SLAVE_FORCE, kM5ioe1) != 0) {
        close(fd_);
        fd_ = -1;
    }
#endif
}

ModifierWatch::~ModifierWatch() {
#if !USE_DESKTOP
    if (fd_ >= 0) close(fd_);
#endif
}

LayerState ModifierWatch::read() {
    if (has_forced_) return forced_;
#if USE_DESKTOP
    const SDL_Keymod m = SDL_GetModState();
    return layer_from_registers((m & KMOD_SHIFT) ? 1 : 0, (m & KMOD_CTRL) ? 1 : 0, (m & KMOD_ALT) ? 1 : 0);
#else
    if (fd_ < 0) return {};
    uint8_t shift = 0, fn = 0, sym = 0;
    if (!read_reg(fd_, kRegShift, shift) || !read_reg(fd_, kRegFn, fn) || !read_reg(fd_, kRegSym, sym)) return {};
    return layer_from_registers(shift, fn, sym);
#endif
}

}  // namespace platform
