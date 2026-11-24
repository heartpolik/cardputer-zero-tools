#include "calc_app.h"
#include "runtime.h"

#ifndef CALC_DEV_ASSETS
#define CALC_DEV_ASSETS ""
#endif

int main() {
    cz::Runtime rt("cz-calc", USE_DESKTOP ? CALC_DEV_ASSETS : "");
    if (!rt.init()) return 1;

    int code = 0;
    {
        calc_ui::CalcApp app(rt);
        rt.set_key_handler([&app](const cz::KeyEvent& ev) { return app.on_key(ev); });
        code = rt.run();
        rt.set_key_handler(nullptr);
    }
    return code;
}
