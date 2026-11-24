# cz-ohmputer: electronics calculator (markings, colour bands, formula tools).

set(OHM_VERSION 0.1.0)

# Pure C++ core (no LVGL): SI values, E-series, markings, tools.
add_library(ohm_core STATIC
    core/si.cpp
    core/eseries.cpp
    core/marking.cpp
    core/tools.cpp
    core/session.cpp
)
target_include_directories(ohm_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/core")
target_link_libraries(ohm_core PUBLIC czcore)

add_executable(cz-ohmputer
    src/main.cpp
    src/ohm_app.cpp
    src/mark_page.cpp
    src/bands_page.cpp
    src/tools_page.cpp
    src/resistor_view.cpp
)
target_link_libraries(cz-ohmputer PRIVATE czkit ohm_core)
target_compile_definitions(cz-ohmputer PRIVATE OHM_DEV_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")

if(USE_DESKTOP)
    add_executable(ohm_core_tests tests/core_tests.cpp)
    target_link_libraries(ohm_core_tests PRIVATE ohm_core)
    add_test(NAME ohm_core COMMAND ohm_core_tests)
endif()

include("${PROJECT_SOURCE_DIR}/cmake/cz-package.cmake")
cz_package_app(
    TARGET       cz-ohmputer
    DISPLAY_NAME "OhmPuter"
    VERSION      ${OHM_VERSION}
    SUMMARY      "Electronics calculator: markings, colour bands, Ohm, dividers"
    ICON         assets/images/cz-ohmputer.png
    ASSETS_DIR   assets
)
include(CPack)
