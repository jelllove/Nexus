cmake_minimum_required(VERSION 3.21)
project(Nexus VERSION 1.0.8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Sql
    Network
    WebEngineWidgets
    WebChannel
)

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
    src/platform/GlobalHotkey.cpp
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
    src/platform/GlobalHotkey.h
)

# Qt resource file
set(RESOURCES
    resources/nexus.qrc
)

# Create executable
if(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})
elseif(APPLE)
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SOURCES} ${HEADERS} ${RESOURCES})
else()
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(UpdateServiceTest
        tests/UpdateServiceTest.cpp
        src/services/UpdateService.cpp
        src/services/UpdateService.h
    )
    target_include_directories(UpdateServiceTest PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(UpdateServiceTest PRIVATE Qt6::Core Qt6::Network Qt6::Test)
    add_test(NAME UpdateServiceTest COMMAND UpdateServiceTest
        -o "${CMAKE_CURRENT_BINARY_DIR}/UpdateServiceTest.txt,txt")
    set(UPDATE_UI_TEST_SOURCES ${SOURCES})
    list(REMOVE_ITEM UPDATE_UI_TEST_SOURCES src/main.cpp)
    add_executable(UpdateUiTest tests/UpdateUiTest.cpp ${UPDATE_UI_TEST_SOURCES} ${HEADERS} ${RESOURCES})
    target_include_directories(UpdateUiTest PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(UpdateUiTest PRIVATE Qt6::Widgets Qt6::Sql Qt6::Network
        Qt6::WebEngineWidgets Qt6::WebChannel Qt6::Test)
    set_target_properties(UpdateServiceTest UpdateUiTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    if(WIN32)
        target_link_libraries(UpdateUiTest PRIVATE user32)
    endif()
    add_test(NAME UpdateUiTest COMMAND UpdateUiTest
        -o "${CMAKE_CURRENT_BINARY_DIR}/UpdateUiTest.txt,txt")
    set_tests_properties(UpdateUiTest PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=$<TARGET_FILE_DIR:Qt6::QOffscreenIntegrationPlugin>/.."
        TIMEOUT 60)
    add_executable(SubtaskMoveTest tests/SubtaskMoveTest.cpp ${UPDATE_UI_TEST_SOURCES} ${HEADERS} ${RESOURCES})
    target_include_directories(SubtaskMoveTest PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(SubtaskMoveTest PRIVATE Qt6::Widgets Qt6::Sql Qt6::Network
        Qt6::WebEngineWidgets Qt6::WebChannel Qt6::Test)
    set_target_properties(SubtaskMoveTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    if(WIN32)
        target_link_libraries(SubtaskMoveTest PRIVATE user32)
    endif()
    add_test(NAME SubtaskMoveTest COMMAND SubtaskMoveTest
        -o "${CMAKE_CURRENT_BINARY_DIR}/SubtaskMoveTest.txt,txt")
    set_tests_properties(SubtaskMoveTest PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=$<TARGET_FILE_DIR:Qt6::QOffscreenIntegrationPlugin>/.."
        TIMEOUT 60)
endif()

# Link Qt6 libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
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
    # Link Win32 API for global hotkey
    target_link_libraries(${PROJECT_NAME} PRIVATE user32)

    # Set application icon
    target_sources(${PROJECT_NAME} PRIVATE resources/nexus.rc)

    # Ensure QtWebEngineProcess is found at runtime
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.nexus.app"
        MACOSX_BUNDLE_BUNDLE_NAME "Nexus"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    )
endif()
