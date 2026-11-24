#include "ohm_app.h"
#include "runtime.h"

#ifndef OHM_DEV_ASSETS
#define OHM_DEV_ASSETS ""
#endif

int main() {
    cz::Runtime rt("cz-ohmputer", USE_DESKTOP ? OHM_DEV_ASSETS : "");
    if (!rt.init()) return 1;

    int code = 0;
    {
        ohm_ui::OhmApp app(rt);
        rt.set_key_handler([&app](const cz::KeyEvent& ev) { return app.on_key(ev); });
        code = rt.run();
        rt.set_key_handler(nullptr);
    }
    return code;
}
