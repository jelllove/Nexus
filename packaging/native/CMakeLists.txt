cmake_minimum_required(VERSION 3.21)

file(READ "${CMAKE_CURRENT_LIST_DIR}/../../CMakeLists.txt" NEXUS_PROJECT)
string(REGEX MATCH "project\\(Nexus VERSION ([0-9.]+)" VERSION_MATCH "${NEXUS_PROJECT}")
if(NOT VERSION_MATCH)
    message(FATAL_ERROR "Cannot determine Nexus version")
endif()
project(NexusNative VERSION "${CMAKE_MATCH_1}" LANGUAGES NONE)

set(NEXUS_STAGE "" CACHE PATH "Directory containing the deployed portable Linux runtime")
foreach(required IN ITEMS bin/Nexus bin/nexus-launch bin/qt.conf
        plugins/platforms/libqxcb.so plugins/sqldrivers/libqsqlite.so
        libexec/QtWebEngineProcess)
    if(NOT EXISTS "${NEXUS_STAGE}/${required}")
        message(FATAL_ERROR "Deployed runtime is missing ${required}")
    endif()
endforeach()

install(DIRECTORY "${NEXUS_STAGE}/" DESTINATION opt/nexus USE_SOURCE_PERMISSIONS)
install(PROGRAMS nexus DESTINATION usr/bin)
install(FILES com.nexus.app.desktop DESTINATION usr/share/applications)
install(FILES ../../resources/icons/app-icon.png
    DESTINATION usr/share/icons/hicolor/256x256/apps RENAME nexus.png)
install(FILES ../../LICENSE DESTINATION usr/share/licenses/nexus)

set(CPACK_GENERATOR "DEB;RPM")
set(CPACK_PACKAGE_NAME nexus)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR jelllove)
set(CPACK_PACKAGE_CONTACT "jelllove <jelllove@gmail.com>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/jelllove/Nexus")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Nexus desktop task management")
set(CPACK_PACKAGE_DESCRIPTION "Three-pane task management with a rich editor and local SQLite notes.")
set(CPACK_PACKAGING_INSTALL_PREFIX "/")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages")
set(CPACK_PACKAGE_RELOCATABLE FALSE)
set(CPACK_DEBIAN_FILE_NAME "Nexus-${PROJECT_VERSION}-Linux-x64.deb")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
set(CPACK_DEBIAN_PACKAGE_SECTION utils)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libc6 (>= 2.35), libstdc++6, libgcc-s1, libatomic1, libglib2.0-0, libnss3, libasound2 | libasound2t64, libx11-6, libxext6, libxcomposite1, libxrandr2, libxtst6, libxdamage1, libxfixes3, libgbm1, libfontconfig1, libfreetype6, libdbus-1-3, libgl1, libegl1, libopengl0, libdrm2, libxcb1, libxcb-cursor0, libxcb-icccm4, libxcb-keysyms1, libxcb-image0, libxcb-render-util0, libxcb-randr0, libxcb-xfixes0, libxcb-sync1, libxcb-xkb1, libxcb-shape0, libxkbcommon0, libxkbcommon-x11-0, libssl3 | libssl3t64, zlib1g")

set(CPACK_RPM_FILE_NAME "Nexus-${PROJECT_VERSION}-Linux-x64.rpm")
set(CPACK_RPM_PACKAGE_ARCHITECTURE x86_64)
set(CPACK_RPM_PACKAGE_LICENSE MIT)
set(CPACK_RPM_PACKAGE_AUTOREQPROV OFF)
set(CPACK_RPM_PACKAGE_REQUIRES
    "glibc >= 2.35, libstdc++, libgcc, libatomic, glib2, nss, alsa-lib, libX11, libXext, libXcomposite, libXrandr, libXtst, libXdamage, libXfixes, mesa-libgbm, fontconfig, freetype, dbus-libs, libglvnd-glx, libglvnd-egl, libglvnd-opengl, libdrm, libxcb, libxkbcommon, libxkbcommon-x11, xcb-util-cursor, xcb-util-wm, xcb-util-keysyms, xcb-util-image, xcb-util-renderutil, openssl-libs >= 3, zlib")
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    /opt /usr /usr/bin /usr/share /usr/share/applications /usr/share/icons
    /usr/share/icons/hicolor /usr/share/icons/hicolor/256x256
    /usr/share/icons/hicolor/256x256/apps /usr/share/licenses)
include(CPack)
