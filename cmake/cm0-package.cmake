#
# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
#
# SPDX-License-Identifier: MIT
# Packaging and install layout for CardputerZero Debian builds.

include(GNUInstallDirs)
set(APP_DOC_DIR "${CMAKE_INSTALL_DATAROOTDIR}/doc/cardputerzero-mesh")

set(APP_DISPLAY_NAME "Cardputer Zero Mesh" CACHE STRING "Human-readable application name used by launchers and package filename")
set(APP_DEBIAN_REVISION "rev1" CACHE STRING "Debian package revision/vendor suffix")
set(APP_DEBIAN_ARCHITECTURE "arm64" CACHE STRING "Debian package architecture")
# Preserve the explicit project maintainer; template identities fail Store checks.
set(APP_MAINTAINER "Jose Di Lenola <Prophet60191@users.noreply.github.com>" CACHE STRING "Debian package maintainer")
set(APP_PACKAGE_DESCRIPTION "Meshtastic client for the M5Stack CardputerZero" CACHE STRING "Debian package summary")
# Off by default: the app launches from the APPLaunch grid as the logged-in
# user. The optional service runs as APP_SERVICE_USER (never root) if enabled.
set(APP_INSTALL_SYSTEMD_SERVICE OFF CACHE BOOL "Install a systemd service file for embedded deployments")
set(APP_SERVICE_USER "" CACHE STRING "Unprivileged user the optional systemd service runs as (required if the service is enabled)")

set(APP_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/package")
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/templates/app.desktop.in"
    "${APP_GENERATED_DIR}/${APP_NAME}.desktop"
    @ONLY
)

if(APP_INSTALL_SYSTEMD_SERVICE)
    if(APP_SERVICE_USER STREQUAL "")
        message(FATAL_ERROR "APP_INSTALL_SYSTEMD_SERVICE is ON but APP_SERVICE_USER is unset; "
                            "set -DAPP_SERVICE_USER=<user> so the service does not run as root.")
    endif()
    configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/templates/app.service.in"
        "${APP_GENERATED_DIR}/${PROJECT_NAME}.service"
        @ONLY
    )
endif()

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/config/cardputerzero-mesh.conf"
    DESTINATION "/etc"
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/${APP_NAME}/audio"
    PATTERN ".DS_Store" EXCLUDE
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/${APP_NAME}/fonts"
    PATTERN ".DS_Store" EXCLUDE
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/images/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/${APP_NAME}/images"
    PATTERN ".DS_Store" EXCLUDE
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/images/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/APPLaunch/share/images"
    FILES_MATCHING
    PATTERN "mesh.png"
)

install(FILES "${APP_GENERATED_DIR}/${APP_NAME}.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/APPLaunch/applications"
)

if(APP_INSTALL_SYSTEMD_SERVICE)
    install(FILES "${APP_GENERATED_DIR}/${PROJECT_NAME}.service"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/systemd/system"
    )
endif()

# Ship the Markdown guides and screenshots for offline reference. Generated PDFs
# are release downloads, not device runtime files.
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/docs" DESTINATION "${APP_DOC_DIR}"
    PATTERN "*.pdf" EXCLUDE
    PATTERN ".DS_Store" EXCLUDE
)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md" DESTINATION "${APP_DOC_DIR}")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    DESTINATION "${APP_DOC_DIR}"
)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    "${CMAKE_CURRENT_SOURCE_DIR}/NOTICE"
    "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    DESTINATION "${APP_DOC_DIR}"
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/licenses/" DESTINATION "${APP_DOC_DIR}/licenses")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/supervisor/LICENSE.GPL" DESTINATION "${APP_DOC_DIR}")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/LICENSE.txt"
    DESTINATION "${APP_DOC_DIR}"
    RENAME "third-party-assets-license.txt"
)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/dist" CACHE PATH "Package output directory")
set(CPACK_PACKAGE_NAME "${APP_DISPLAY_NAME}")
set(CPACK_PACKAGE_VENDOR "${APP_MAINTAINER}")
set(CPACK_PACKAGE_CONTACT "${APP_MAINTAINER}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${APP_PACKAGE_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "cardputerzero-mesh_${PROJECT_VERSION}-${APP_DEBIAN_REVISION}_${APP_DEBIAN_ARCHITECTURE}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

string(TOLOWER "${APP_DISPLAY_NAME}" APP_DEBIAN_PACKAGE_NAME)
string(REGEX REPLACE "[^a-z0-9+.-]" "-" APP_DEBIAN_PACKAGE_NAME "${APP_DEBIAN_PACKAGE_NAME}")
set(CPACK_DEBIAN_PACKAGE_NAME "cardputerzero-mesh")
set(CPACK_DEBIAN_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_DEBIAN_PACKAGE_RELEASE "${APP_DEBIAN_REVISION}")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${APP_DEBIAN_ARCHITECTURE}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${APP_MAINTAINER}")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
# Derive native library dependencies from the built ELF, including SONAMEs.
# Hardcoded libjpeg/libfmt package names were wrong across Debian/Ubuntu/BSPs.
# preinst uses Python before this package is unpacked; ordinary Depends is too late.
set(CPACK_DEBIAN_PACKAGE_PREDEPENDS "python3 (>= 3.13), python3 (<< 3.14)")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "python3-venv, python3-pil, libzbar0t64, fonts-noto-cjk, nftables, adduser, systemd")
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "rpicam-apps-core, python3-opencv, tailscale")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS "${CM0_LIBRARY_HINTS}")
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/templates/conffiles.in"
    "${APP_GENERATED_DIR}/conffiles"
    @ONLY
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/supervisor/"
    DESTINATION "lib/cardputerzero-mesh"
    FILES_MATCHING PATTERN "*.py" PATTERN "*.yaml" PATTERN "LICENSE.GPL")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/app-builder.json" DESTINATION "lib/cardputerzero-mesh")
# Native releases are complete offline runtimes. Desktop/build-only work may
# omit this path, but a thin Debian installer must never be produced silently.
set(MESH_RUNTIME_BUNDLE "" CACHE PATH "Prepared, checksummed arm64 runtime directory")
if(MESH_RUNTIME_BUNDLE)
    if(NOT EXISTS "${MESH_RUNTIME_BUNDLE}/manifest.json")
        message(FATAL_ERROR "Runtime manifest missing; use scripts/build-package.py")
    endif()
    install(DIRECTORY "${MESH_RUNTIME_BUNDLE}/" DESTINATION "lib/cardputerzero-mesh/runtime"
        USE_SOURCE_PERMISSIONS)
elseif(NOT USE_DESKTOP)
    install(CODE "message(FATAL_ERROR \"Release runtime missing: use scripts/build-package.py or set MESH_RUNTIME_BUNDLE\")")
endif()

install(PROGRAMS supervisor/firewall-setup.sh packaging/run-supervisor
    DESTINATION "lib/cardputerzero-mesh")
install(PROGRAMS packaging/cardputerzero-mesh-launch DESTINATION "bin")
install(FILES packaging/cardputerzero-mesh-firewall.service DESTINATION "lib/systemd/system")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/packaging/stop-user-service" "${APP_GENERATED_DIR}/preinst" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/packaging/stop-user-service" "${APP_GENERATED_DIR}/prerm" COPYONLY)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${APP_GENERATED_DIR}/conffiles;${APP_GENERATED_DIR}/preinst;${APP_GENERATED_DIR}/prerm;${CMAKE_CURRENT_SOURCE_DIR}/packaging/postinst;${CMAKE_CURRENT_SOURCE_DIR}/packaging/postrm")

include(CPack)
