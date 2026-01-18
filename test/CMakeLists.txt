# Try Finding Installed Google Test (with gmock)
find_package(GTest QUIET COMPONENTS gtest gtest_main gmock gmock_main)
#------------------------------------------------------------------------------------------
# Use Installed Version or Fetch
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE # Do a shallow clone to speed up the process
    )

    # This line ensures GoogleTest uses the same runtime library
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(googletest)
    add_library(GTest::GTest ALIAS gtest)
    add_library(GTest::Main ALIAS gtest_main)
    add_library(GTest::gmock ALIAS gmock)
    add_library(GTest::gmock_main ALIAS gmock_main)
endif()
#------------------------------------------------------------------------------------------
# Function to setup a test executable
function(create_test_target TARGET_NAME SOURCE_FILE)
    # Correcting the file path according to the directory structure
    add_executable(${TARGET_NAME} ${SOURCE_FILE})
    target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/include)
    
    # Set the output directories for test executables
    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/testbin
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/testlib
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/testlib
    )
    
    # Link the main project library and Google Test
    target_link_libraries(${TARGET_NAME} PRIVATE ${PROJECT_NAME})
    if(TARGET common_options)
        target_link_libraries(${TARGET_NAME} PRIVATE common_options)
    endif()
    if(GTest_FOUND)
        target_link_libraries(${TARGET_NAME} PRIVATE GTest::gmock_main)
    else()
        target_link_libraries(${TARGET_NAME} PRIVATE gmock_main)
    endif()

    # Enable testing and add the test to CTest
    enable_testing()
    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
endfunction()
#------------------------------------------------------------------------------------------
# Add the tests without the 'test/' prefix since we are already in the test directory
create_test_target(${PROJECT_NAME}_atomic_unique_ptr_Test   atomic_unique_ptr_test.cpp)
create_test_target(${PROJECT_NAME}_HashTable_Test               HashTableTest.cpp)
create_test_target(${PROJECT_NAME}_HashSet_Test                 HashSetTest.cpp)
create_test_target(${PROJECT_NAME}_HashMultiTable_Test          HashMultiTableTest.cpp)
create_test_target(${PROJECT_NAME}_RetireSet_Test               RetireSetTest.cpp)
create_test_target(${PROJECT_NAME}_RetireMap_Test               RetireMapTest.cpp)
create_test_target(${PROJECT_NAME}_BitmaskTable_Test            BitmaskTableTest.cpp)
create_test_target(${PROJECT_NAME}_BitmaskTable_Dynamic_Test    BitmaskTableDynamicTest.cpp)
create_test_target(${PROJECT_NAME}_Fixed_Test                   HazardPointerManagerFixedTest.cpp)
create_test_target(${PROJECT_NAME}_Dynamic_Test                 HazardPointerManagerDynamicTest.cpp)
create_test_target(${PROJECT_NAME}_Test                         HazardPointerManagerTest.cpp)
create_test_target(${PROJECT_NAME}_ProtectedPointer_Test        ProtectedPointerTest.cpp)
create_test_target(${PROJECT_NAME}_HazardRegistry_Test          HazardRegistryTest.cpp)
create_test_target(${PROJECT_NAME}_ThreadRegistry_Test          ThreadRegistryTest.cpp)
create_test_target(${PROJECT_NAME}_HazardThreadManager_Test     HazardThreadManagerTest.cpp)
create_test_target(${PROJECT_NAME}_HashSet_Fixed_Test           HashSetFixedTest.cpp)
create_test_target(${PROJECT_NAME}_AvailabilityBitmapTree_Test  AvailabilityBitmapTreeTest.cpp)
# Diagnostic probe (non-gtest): exercises protect/reset loops and logs on failure
create_test_target(${PROJECT_NAME}_HazardPointer_Protect_Probe  HazardPointerProtectProbe.cpp)
#------------------------------------------------------------------------------------------
