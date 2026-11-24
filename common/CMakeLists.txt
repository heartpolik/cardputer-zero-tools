# czkit: shared runtime for cardputer-zero-tools apps (derived from the
# official CardputerZero Template, MIT, M5Stack Technology CO LTD).

file(GLOB_RECURSE CZKIT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_library(czkit STATIC ${CZKIT_SOURCES})

target_include_directories(czkit
    PUBLIC
        ${LVGL_INCLUDE_DIR}
        ${PLATFORM_INCLUDE_DIR}
        "${CMAKE_CURRENT_SOURCE_DIR}/src/app"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/logger"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/reactive"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/view"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets"
)

target_compile_definitions(czkit
    PUBLIC
        APP_KEY_INPUT_DEVICE="${APP_KEY_INPUT_DEVICE}"
        APP_FRAMEBUFFER_DEVICE="${APP_FRAMEBUFFER_DEVICE}"
        APP_DRM_DEVICE="${APP_DRM_DEVICE}"
        APP_DRM_CONNECTOR_ID=${APP_DRM_CONNECTOR_ID}
        APP_COMMON_ASSETS_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

target_link_libraries(czkit
    PUBLIC
        lvgl
        fmt::fmt
        ${PLATFORM_LIBS}
)

if(USE_DESKTOP)
    target_link_libraries(czkit PRIVATE PNG::PNG)
    if(TARGET SDL2::SDL2)
        target_link_libraries(czkit PRIVATE SDL2::SDL2)
    else()
        target_include_directories(czkit PRIVATE ${SDL2_INCLUDE_DIRS})
        target_link_libraries(czkit PRIVATE ${SDL2_LIBRARIES})
    endif()

    add_executable(czkit_key_tests
        tests/key_decoder_tests.cpp
        src/platform/key_decoder.cpp
    )
    target_include_directories(czkit_key_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/platform")
    add_test(NAME czkit_key_decoder COMMAND czkit_key_tests)
endif()

# Assets every app installs next to its own (icon fonts, click sound).
set(CZKIT_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets" PARENT_SCOPE)
