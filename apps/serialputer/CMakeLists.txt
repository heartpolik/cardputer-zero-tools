# cz-serialputer: serial terminal, Modbus RTU master and bus sniffer.

set(SER_VERSION 0.1.0)

# Pure C++ core (no LVGL): framing, capture views, Modbus, the demo bus.
add_library(ser_core STATIC
    core/serial_format.cpp
    core/capture.cpp
    core/modbus.cpp
    core/sim_bus.cpp
    core/session.cpp
)
target_include_directories(ser_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/core")
target_link_libraries(ser_core PUBLIC czcore)

find_package(Threads REQUIRED)

add_executable(cz-serialputer
    src/main.cpp
    src/serial_port.cpp
    src/engine.cpp
    src/serial_app.cpp
    src/term_page.cpp
    src/modbus_page.cpp
    src/port_page.cpp
)
target_link_libraries(cz-serialputer PRIVATE czkit ser_core Threads::Threads)
target_compile_definitions(cz-serialputer PRIVATE SER_DEV_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")

if(USE_DESKTOP)
    add_executable(ser_core_tests tests/core_tests.cpp)
    target_link_libraries(ser_core_tests PRIVATE ser_core)
    add_test(NAME ser_core COMMAND ser_core_tests)
endif()

include("${PROJECT_SOURCE_DIR}/cmake/cz-package.cmake")
cz_package_app(
    TARGET       cz-serialputer
    DISPLAY_NAME "SerialPuter"
    VERSION      ${SER_VERSION}
    SUMMARY      "Serial terminal, Modbus RTU master and bus sniffer"
    ICON         assets/images/cz-serialputer.png
    ASSETS_DIR   assets
)
include(CPack)
