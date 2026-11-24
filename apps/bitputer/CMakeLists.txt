# cz-bitputer: programmer's calculator (bases, bit grid, CRCs).

set(BIT_VERSION 0.1.0)

# Pure C++ core (no LVGL): integer expressions, formats, decoders, checksums.
add_library(bit_core STATIC
    core/word.cpp
    core/int_expr.cpp
    core/decode.cpp
    core/checksum.cpp
    core/session.cpp
)
target_include_directories(bit_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/core")
target_link_libraries(bit_core PUBLIC czcore)

add_executable(cz-bitputer
    src/main.cpp
    src/bit_app.cpp
    src/calc_page.cpp
    src/crc_page.cpp
)
target_link_libraries(cz-bitputer PRIVATE czkit bit_core)
target_compile_definitions(cz-bitputer PRIVATE BIT_DEV_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")

if(USE_DESKTOP)
    add_executable(bit_core_tests tests/core_tests.cpp)
    target_link_libraries(bit_core_tests PRIVATE bit_core)
    add_test(NAME bit_core COMMAND bit_core_tests)
endif()

include("${PROJECT_SOURCE_DIR}/cmake/cz-package.cmake")
cz_package_app(
    TARGET       cz-bitputer
    DISPLAY_NAME "BitPuter"
    VERSION      ${BIT_VERSION}
    SUMMARY      "Programmer's calculator: bases, bit fields, CRCs"
    ICON         assets/images/cz-bitputer.png
    ASSETS_DIR   assets
)
include(CPack)
