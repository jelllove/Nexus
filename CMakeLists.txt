cmake_minimum_required(VERSION 3.21)
project(Nexus VERSION 1.0.9 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(GNUInstallDirs)
if(WIN32)
    # Qt snapshots deployment paths during discovery; preserve the legacy root layout.
    set(CMAKE_INSTALL_BINDIR ".")
endif()

# Find Qt6 components
find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Concurrent
    Gui
    Widgets
    Sql
    Network
    WebEngineWidgets
    WebChannel
)

add_library(NexusPlatform STATIC
    src/platform/GlobalHotkey.cpp
    src/platform/GlobalHotkey.h
    src/platform/UpdatePackage.cpp
    src/platform/UpdatePackage.h
)
target_include_directories(NexusPlatform PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(NexusPlatform PUBLIC Qt6::Core Qt6::Gui)
if(WIN32)
    target_link_libraries(NexusPlatform PRIVATE user32)
elseif(APPLE)
    find_library(CARBON_FRAMEWORK Carbon REQUIRED)
    target_link_libraries(NexusPlatform PRIVATE ${CARBON_FRAMEWORK})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11 REQUIRED)
    target_compile_definitions(NexusPlatform PRIVATE NEXUS_HAVE_X11)
    target_link_libraries(NexusPlatform PRIVATE X11::X11)
endif()

# Collect source files
set(SOURCES
    src/main.cpp
    src/app/MainWindow.cpp
    src/db/DatabaseManager.cpp
    src/models/ProductListModel.cpp
    src/models/TaskListModel.cpp
    src/ui/ProductPane.cpp
    src/ui/TaskPane.cpp
    src/ui/TaskListView.cpp
    src/ui/TaskCardDelegate.cpp
    src/ui/ExportProgressDialog.cpp
    src/ui/MarkdownPreviewDialog.cpp
    src/ui/EditorPane.cpp
    src/ui/SearchBar.cpp
    src/ui/SettingsDialog.cpp
    src/editor/EditorBridge.cpp
    src/services/AIService.cpp
    src/services/ImageManager.cpp
    src/services/UpdateService.cpp
    src/services/TaskExportService.cpp
    src/platform/InstallerSmoke.cpp
)

set(HEADERS
    src/app/MainWindow.h
    src/db/DatabaseManager.h
    src/models/Product.h
    src/models/Task.h
    src/models/ProductListModel.h
    src/models/TaskListModel.h
    src/ui/ProductPane.h
    src/ui/TaskPane.h
    src/ui/TaskListView.h
    src/ui/TaskCardDelegate.h
    src/ui/ExportProgressDialog.h
    src/ui/MarkdownPreviewDialog.h
    src/ui/EditorPane.h
    src/ui/SearchBar.h
    src/ui/SettingsDialog.h
    src/editor/EditorBridge.h
    src/services/AIService.h
    src/services/ImageManager.h
    src/services/UpdateService.h
    src/services/TaskExportService.h
    src/platform/InstallerSmoke.h
)

# Qt resource file
set(RESOURCES
    resources/nexus.qrc
)

# Create executable
if(WIN32)
    qt_add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})
elseif(APPLE)
    qt_add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SOURCES} ${HEADERS} ${RESOURCES})
else()
    qt_add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    find_package(Python3 3.12 REQUIRED COMPONENTS Interpreter)
    add_test(NAME InstallerVerificationTest
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_installer_verification.py)
    add_test(NAME ReleasePreparationTest
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_release_preparation.py)
    add_test(NAME InstallerSmokeCliTest
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_installer_smoke_cli.py
            $<TARGET_FILE:Nexus>)
    set_tests_properties(InstallerSmokeCliTest PROPERTIES TIMEOUT 60)
    add_executable(UpdateServiceTest
        tests/UpdateServiceTest.cpp
        src/services/UpdateService.cpp
        src/services/UpdateService.h
    )
    target_include_directories(UpdateServiceTest PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(UpdateServiceTest PRIVATE NexusPlatform Qt6::Network Qt6::Test)
    add_test(NAME UpdateServiceTest COMMAND UpdateServiceTest
        -o "${CMAKE_CURRENT_BINARY_DIR}/UpdateServiceTest.txt,txt")
    add_executable(UpdatePackageTest tests/UpdatePackageTest.cpp)
    target_link_libraries(UpdatePackageTest PRIVATE NexusPlatform Qt6::Test)
    set_target_properties(UpdatePackageTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME UpdatePackageTest COMMAND UpdatePackageTest
        -o "${CMAKE_CURRENT_BINARY_DIR}/UpdatePackageTest.txt,txt")
    set(UPDATE_UI_TEST_SOURCES ${SOURCES})
    list(REMOVE_ITEM UPDATE_UI_TEST_SOURCES src/main.cpp)
    add_executable(UpdateUiTest tests/UpdateUiTest.cpp ${UPDATE_UI_TEST_SOURCES} ${HEADERS} ${RESOURCES})
    target_include_directories(UpdateUiTest PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(UpdateUiTest PRIVATE NexusPlatform Qt6::Widgets Qt6::Sql Qt6::Network Qt6::Concurrent
        Qt6::WebEngineWidgets Qt6::WebChannel Qt6::Test)
    set_target_properties(UpdateServiceTest UpdateUiTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME UpdateUiTest COMMAND UpdateUiTest
        -o "${CMAKE_CURRENT_BINARY_DIR}/UpdateUiTest.txt,txt")
    set_tests_properties(UpdateUiTest PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=$<TARGET_FILE_DIR:Qt6::QOffscreenIntegrationPlugin>/.."
        TIMEOUT 60)
    add_executable(SubtaskMoveTest tests/SubtaskMoveTest.cpp ${UPDATE_UI_TEST_SOURCES} ${HEADERS} ${RESOURCES})
    target_include_directories(SubtaskMoveTest PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(SubtaskMoveTest PRIVATE NexusPlatform Qt6::Widgets Qt6::Sql Qt6::Network Qt6::Concurrent
        Qt6::WebEngineWidgets Qt6::WebChannel Qt6::Test)
    set_target_properties(SubtaskMoveTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME SubtaskMoveTest COMMAND SubtaskMoveTest
        -o "${CMAKE_CURRENT_BINARY_DIR}/SubtaskMoveTest.txt,txt")
    set_tests_properties(SubtaskMoveTest PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=$<TARGET_FILE_DIR:Qt6::QOffscreenIntegrationPlugin>/.."
        TIMEOUT 60)
endif()

# Link Qt6 libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    NexusPlatform
    Qt6::Core
    Qt6::Concurrent
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
    Qt6::WebEngineWidgets
    Qt6::WebChannel
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Build metadata
set(GIT_COMMIT_HASH "unknown")
execute_process(
    COMMAND git rev-parse --short=8 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "unknown")
endif()

# Pass version to C++ code
target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    APP_GIT_COMMIT="${GIT_COMMIT_HASH}"
)

# Windows-specific settings
if(WIN32)
    # Set application icon
    target_sources(${PROJECT_NAME} PRIVATE resources/nexus.rc)

    # Ensure QtWebEngineProcess is found at runtime
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

if(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${CMAKE_INSTALL_BINDIR}")
    include(InstallRequiredSystemLibraries)
endif()
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.nexus.app"
        MACOSX_BUNDLE_BUNDLE_NAME "Nexus"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    configure_file(packaging/nexus-launch.in nexus-launch @ONLY)
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/nexus-launch"
        DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES packaging/com.nexus.app.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES resources/icons/app-icon.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
        RENAME nexus.png)
endif()
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
set(NEXUS_DEPLOY_OPTIONS "")
if(WIN32)
    # Nexus does not use positioning device plugins (the legacy build also excludes them).
    set(NEXUS_DEPLOY_OPTIONS DEPLOY_TOOL_OPTIONS --skip-plugin-types position)
endif()
qt_generate_deploy_app_script(
    TARGET ${PROJECT_NAME}
    OUTPUT_SCRIPT nexus_deploy_script
    ${NEXUS_DEPLOY_OPTIONS}
)
install(SCRIPT ${nexus_deploy_script})

if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
    set(NEXUS_PACKAGE_ARCH arm64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|x64)$"
        OR CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
    set(NEXUS_PACKAGE_ARCH x64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(NEXUS_PACKAGE_ARCH arm64)
else()
    set(NEXUS_PACKAGE_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()
if(APPLE)
    if("arm64" IN_LIST CMAKE_OSX_ARCHITECTURES AND "x86_64" IN_LIST CMAKE_OSX_ARCHITECTURES)
        set(NEXUS_PACKAGE_ARCH universal)
    elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
        set(NEXUS_PACKAGE_ARCH x64)
    elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
        set(NEXUS_PACKAGE_ARCH arm64)
    endif()
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_PACKAGE_FILE_NAME "Nexus-${PROJECT_VERSION}-macOS-${NEXUS_PACKAGE_ARCH}")
elseif(WIN32)
    set(CPACK_GENERATOR ZIP)
    set(CPACK_PACKAGE_FILE_NAME "Nexus-${PROJECT_VERSION}-Windows-${NEXUS_PACKAGE_ARCH}")
else()
    set(CPACK_GENERATOR TGZ)
    set(CPACK_PACKAGE_FILE_NAME "Nexus-${PROJECT_VERSION}-Linux-${NEXUS_PACKAGE_ARCH}")
endif()
set(CPACK_PACKAGE_NAME Nexus)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages")
set(CPACK_DMG_VOLUME_NAME Nexus)
include(CPack)
