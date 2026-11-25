#include "runtime.h"
#include "serial_app.h"

#ifndef SER_DEV_ASSETS
#define SER_DEV_ASSETS ""
#endif

int main() {
    cz::Runtime rt("cz-serialputer", USE_DESKTOP ? SER_DEV_ASSETS : "");
    if (!rt.init()) return 1;

    int code = 0;
    {
        ser_ui::SerialApp app(rt);
        rt.set_key_handler([&app](const cz::KeyEvent& ev) { return app.on_key(ev); });
        code = rt.run();
        rt.set_key_handler(nullptr);
    }
    return code;
}
