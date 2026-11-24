#include "bit_app.h"
#include "runtime.h"

#ifndef BIT_DEV_ASSETS
#define BIT_DEV_ASSETS ""
#endif

int main() {
    cz::Runtime rt("cz-bitputer", USE_DESKTOP ? BIT_DEV_ASSETS : "");
    if (!rt.init()) return 1;

    int code = 0;
    {
        bit_ui::BitApp app(rt);
        rt.set_key_handler([&app](const cz::KeyEvent& ev) { return app.on_key(ev); });
        code = rt.run();
        rt.set_key_handler(nullptr);
    }
    return code;
}
