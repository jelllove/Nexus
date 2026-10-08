add_executable(NexusCoreTests
    NexusCoreTests.cpp
    ${PROJECT_SOURCE_DIR}/src/db/DatabaseManager.cpp
    ${PROJECT_SOURCE_DIR}/src/models/ProductListModel.cpp
    ${PROJECT_SOURCE_DIR}/src/models/TaskListModel.cpp
)

target_include_directories(NexusCoreTests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(NexusCoreTests PRIVATE
    Qt6::Test
    Qt6::Core
    Qt6::Concurrent
    Qt6::Gui
    Qt6::Sql
)

if(WIN32)
    add_test(NAME nexus-core COMMAND ${CMAKE_COMMAND} -E env
        "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}"
        $<TARGET_FILE:NexusCoreTests>
        -o -,txt -o nexus-core.junit.xml,junitxml
    )
else()
    add_test(NAME nexus-core COMMAND NexusCoreTests
        -o -,txt -o nexus-core.junit.xml,junitxml
    )
endif()

set_tests_properties(nexus-core PROPERTIES TIMEOUT 60)
