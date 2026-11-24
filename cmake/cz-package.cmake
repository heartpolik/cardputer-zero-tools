# Install layout + Debian package for one cardputer-zero-tools app.
#
#   cz_package_app(
#       TARGET       cz-calc            # executable target, also the package/binary name
#       DISPLAY_NAME "Calc"             # name shown in APPLaunch
#       VERSION      0.1.0
#       SUMMARY      "Engineering calculator with graphs"
#       ICON         assets/images/cz-calc.png
#       ASSETS_DIR   assets             # copied to /usr/share/<TARGET>/
#   )

include(GNUInstallDirs)

set(CZ_MAINTAINER "heartpolik <heartpolik@users.noreply.github.com>" CACHE STRING "Debian package maintainer")
set(CZ_DEBIAN_REVISION "1" CACHE STRING "Debian package revision")

function(cz_package_app)
    cmake_parse_arguments(APP "" "TARGET;DISPLAY_NAME;VERSION;SUMMARY;ICON;ASSETS_DIR" "" ${ARGN})

    set(APP_SHARE "${CMAKE_INSTALL_DATADIR}/${APP_TARGET}")
    get_filename_component(APP_ICON_NAME "${APP_ICON}" NAME)
    set(APP_ICON_INSTALLED "${APP_TARGET}.png")

    install(TARGETS ${APP_TARGET} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    # app assets + shared czkit assets (icon fonts, click sound); not the simulator shell images
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${APP_ASSETS_DIR}/"
        DESTINATION "${APP_SHARE}"
        PATTERN ".DS_Store" EXCLUDE)
    install(DIRECTORY "${CZKIT_ASSETS_DIR}/fonts" "${CZKIT_ASSETS_DIR}/audio"
        DESTINATION "${APP_SHARE}"
        PATTERN ".DS_Store" EXCLUDE)

    # APPLaunch registration: desktop entry + icon (Icon= is relative to /usr/share/APPLaunch)
    set(APP_EXEC "/usr/bin/${APP_TARGET}")
    set(APP_DESKTOP_ICON "share/images/${APP_ICON_INSTALLED}")
    configure_file("${PROJECT_SOURCE_DIR}/cmake/templates/app.desktop.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/${APP_TARGET}.desktop" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${APP_TARGET}.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/APPLaunch/applications")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${APP_ICON}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/APPLaunch/share/images"
        RENAME "${APP_ICON_INSTALLED}")

    set(APP_DOC "${CMAKE_INSTALL_DATADIR}/doc/${APP_TARGET}")
    install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION "${APP_DOC}")
    install(FILES "${CZKIT_ASSETS_DIR}/fonts/LICENSE.txt"
        DESTINATION "${APP_DOC}"
        RENAME "third-party-assets-license.txt")

    set(CPACK_GENERATOR "DEB" PARENT_SCOPE)
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr" PARENT_SCOPE)
    set(CPACK_OUTPUT_FILE_PREFIX "${PROJECT_SOURCE_DIR}/dist" PARENT_SCOPE)
    set(CPACK_PACKAGE_NAME "${APP_TARGET}" PARENT_SCOPE)
    set(CPACK_PACKAGE_VERSION "${APP_VERSION}" PARENT_SCOPE)
    set(CPACK_PACKAGE_CONTACT "${CZ_MAINTAINER}" PARENT_SCOPE)
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${APP_SUMMARY}" PARENT_SCOPE)
    set(CPACK_PACKAGE_FILE_NAME "${APP_TARGET}_${APP_VERSION}-${CZ_DEBIAN_REVISION}_arm64" PARENT_SCOPE)
    set(CPACK_DEBIAN_PACKAGE_NAME "${APP_TARGET}" PARENT_SCOPE)
    set(CPACK_DEBIAN_PACKAGE_VERSION "${APP_VERSION}" PARENT_SCOPE)
    set(CPACK_DEBIAN_PACKAGE_RELEASE "${CZ_DEBIAN_REVISION}" PARENT_SCOPE)
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64" PARENT_SCOPE)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CZ_MAINTAINER}" PARENT_SCOPE)
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils" PARENT_SCOPE)
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional" PARENT_SCOPE)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON PARENT_SCOPE)
    set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE PARENT_SCOPE)
endfunction()
