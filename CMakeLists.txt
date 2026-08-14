cmake_minimum_required(VERSION 3.21)
project(Nexus VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Enable CTest
include(CTest)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Sql
    Network
)

find_package(Qt6 QUIET COMPONENTS
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
    src/ui/TaskCardDelegate.cpp
    src/ui/EditorPane.cpp
    src/ui/SearchBar.cpp
    src/ui/SettingsDialog.cpp
    src/editor/EditorBridge.cpp
    src/services/AIService.cpp
    src/services/ImageManager.cpp
    src/services/UpdateService.cpp
    src/platform/GlobalHotkey.cpp
)

set(HEADERS
    src/app/MainWindow.h
    src/db/DatabaseManager.h
    src/models/EditorTarget.h
    src/models/Product.h
    src/models/Task.h
    src/models/ProductListModel.h
    src/models/SearchResult.h
    src/models/TaskListModel.h
    src/ui/ProductPane.h
    src/ui/TaskPane.h
    src/ui/TaskCardDelegate.h
    src/ui/EditorPane.h
    src/ui/SearchBar.h
    src/ui/SettingsDialog.h
    src/editor/EditorBridge.h
    src/services/AIService.h
    src/services/ImageManager.h
    src/services/UpdateService.h
    src/platform/GlobalHotkey.h
)

# Qt resource file
set(RESOURCES
    resources/nexus.qrc
)

if(Qt6WebEngineWidgets_FOUND AND Qt6WebChannel_FOUND)
    # Create executable
    if(WIN32)
        add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})
    elseif(APPLE)
        add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SOURCES} ${HEADERS} ${RESOURCES})
    else()
        add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})
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

    # Pass version to C++ code
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        APP_VERSION="${PROJECT_VERSION}"
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
else()
    message(WARNING "Qt WebEngine components not found; skipping Nexus application target.")
endif()

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(NexusTests
        tests/SubtaskFeatureTests.cpp
        src/db/DatabaseManager.cpp
        src/models/TaskListModel.cpp
        src/ui/TaskPane.cpp
        src/ui/TaskCardDelegate.cpp
    )
    target_include_directories(NexusTests PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(NexusTests PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Sql
        Qt6::Test
    )

    add_test(NAME NexusTests COMMAND NexusTests)
    set_tests_properties(NexusTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_PREFIX_PATH}/bin"
    )
endif()
