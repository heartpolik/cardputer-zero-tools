# cz-calc: engineering calculator with graphs.

set(CALC_VERSION 0.1.0)

# Pure C++ core (no LVGL): expression language, formatting, session, plot math.
add_library(calc_core STATIC
    core/expr.cpp
    core/format.cpp
    core/engine.cpp
    core/line_editor.cpp
    core/plot_math.cpp
)
target_include_directories(calc_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/core")

add_executable(cz-calc
    src/main.cpp
    src/calc_app.cpp
    src/calc_page.cpp
    src/functions_page.cpp
    src/graph_page.cpp
    src/style.cpp
)
target_link_libraries(cz-calc PRIVATE czkit calc_core)
target_compile_definitions(cz-calc PRIVATE CALC_DEV_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")

if(USE_DESKTOP)
    add_executable(calc_core_tests tests/core_tests.cpp)
    target_link_libraries(calc_core_tests PRIVATE calc_core)
    add_test(NAME calc_core COMMAND calc_core_tests)
endif()

include("${PROJECT_SOURCE_DIR}/cmake/cz-package.cmake")
cz_package_app(
    TARGET       cz-calc
    DISPLAY_NAME "Calc"
    VERSION      ${CALC_VERSION}
    SUMMARY      "Engineering calculator with function graphs"
    ICON         assets/images/cz-calc.png
    ASSETS_DIR   assets
)
include(CPack)
