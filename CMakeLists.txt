cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
#------------------------------------------------------------------------------------------
# Determine if HazardSystem is built as a standalone project or included by other projects
set(HAZARDSYSTEM_STANDALONE_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(HAZARDSYSTEM_STANDALONE_PROJECT ON)
endif()
#------------------------------------------------------------------------------------------
# Get the name of the folder and use it as the project name
get_filename_component(PARENT_DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
string(REPLACE " " "_" PARENT_DIR_NAME ${PARENT_DIR_NAME})
project(${PARENT_DIR_NAME} VERSION 1.0 DESCRIPTION "A Circularbuffer function library" LANGUAGES CXX)
#------------------------------------------------------------------------------------------
# Set the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#------------------------------------------------------------------------------------------
# Print out the build type
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
#------------------------------------------------------------------------------------------
if(HAZARDSYSTEM_STANDALONE_PROJECT)
    if(MSVC)
        # MSVC specific warning flags
        add_library(common_options INTERFACE)
        target_compile_options(common_options INTERFACE /W4)  # Equivalent to -Wall in GCC/Clang
    else()
        # GCC/Clang specific warning flags
        add_library(common_options INTERFACE)
        target_compile_options(common_options INTERFACE -Wall -Wextra -Wpedantic -pedantic-errors -Werror=format-security -fasynchronous-unwind-tables)
    endif()
endif()
#------------------------------------------------------------------------------------------
# Set the runtime output directory only if this is a standalone project
if(HAZARDSYSTEM_STANDALONE_PROJECT)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    # Optionally, to set these directories only for specific build configurations (Debug, Release, etc.),
    # you can use the following approach:
    # set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
    # set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
    # set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib)
    # set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib)
    # set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib)
    # set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib)
    # Diagnostic output
    message(STATUS "ARCHIVE_OUTPUT_DIRECTORY: ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
    message(STATUS "LIBRARY_OUTPUT_DIRECTORY: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    message(STATUS "RUNTIME_OUTPUT_DIRECTORY: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()
#------------------------------------------------------------------------------------------
# Offer switches for the type of the library and optional builds
option(BUILD_HAZARDSYSTEM_SHARED_LIBS "Build using shared libraries" ON)
option(BUILD_HAZARDSYSTEM_EXAMPLE "Build HazardSystem example" ${HAZARDSYSTEM_STANDALONE_PROJECT})
option(BUILD_HAZARDSYSTEM_TESTS "Build HazardSystem tests" ${HAZARDSYSTEM_STANDALONE_PROJECT})
option(BUILD_HAZARDSYSTEM_BENCHMARK "Build HazardSystem benchmarks" ${HAZARDSYSTEM_STANDALONE_PROJECT})
set(HAS_TBB OFF CACHE BOOL "Set to TRUE if TBB is found, otherwise FALSE")
#------------------------------------------------------------------------------------------
# Force colored output
option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." OFF)
if(${FORCE_COLORED_OUTPUT})
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        add_compile_options(-fdiagnostics-color=always)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        add_compile_options(-fcolor-diagnostics)
    endif()
endif()
#------------------------------------------------------------------------------------------
# Define the source and include directories
set(HAZARDSYSTEM_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(HAZARDSYSTEM_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
#------------------------------------------------------------------------------------------
set(HAZARDSYSTEM_SOURCES
    # ${HAZARDSYSTEM_SOURCE_DIR}/atomic_unique_ptr.cpp
    ${HAZARDSYSTEM_SOURCE_DIR}/Hasher.cpp
    ${HAZARDSYSTEM_SOURCE_DIR}/HashTable.cpp
    ${HAZARDSYSTEM_SOURCE_DIR}/HashMultiTable.cpp
    ${HAZARDSYSTEM_SOURCE_DIR}/HashSet.cpp
    ${HAZARDSYSTEM_SOURCE_DIR}/BitmaskTable.cpp
    ${HAZARDSYSTEM_SOURCE_DIR}/RetireSet.cpp
    ${HAZARDSYSTEM_SOURCE_DIR}/HazardPointer.cpp
    ${HAZARDSYSTEM_SOURCE_DIR}/HazardPointerManager.cpp
    ${HAZARDSYSTEM_SOURCE_DIR}/ThreadRegistry.cpp
    ${HAZARDSYSTEM_SOURCE_DIR}/ProtectedPointer.cpp
    ${HAZARDSYSTEM_SOURCE_DIR}/HazardThreadManager.cpp
    
)
#------------------------------------------------------------------------------------------
# Check if the library should be built as a shared library
if(BUILD_HAZARDSYSTEM_SHARED_LIBS)
    add_library(${PROJECT_NAME} SHARED ${HAZARDSYSTEM_SOURCES})
    # If needed, set properties specific to shared libraries here
else()
    add_library(${PROJECT_NAME} STATIC ${HAZARDSYSTEM_SOURCES})
    # If needed, set properties specific to static libraries here
endif()
#------------------------------------------------------------------------------------------
target_include_directories(${PROJECT_NAME} PUBLIC 
    $<BUILD_INTERFACE:${HAZARDSYSTEM_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
)
#------------------------------------------------------------------------------------------
# Alias for HazardSystem library
add_library(HazardSystem::hazardsystem ALIAS ${PROJECT_NAME})
#------------------------------------------------------------------------------------------
# Set HAZARDSYSTEM_LIBRARIES variable
if(HAZARDSYSTEM_STANDALONE_PROJECT)
    set(HAZARDSYSTEM_LIBRARIES HazardSystem::hazardsystem)
else()
    set(HAZARDSYSTEM_LIBRARIES HazardSystem::hazardsystem PARENT_SCOPE)
endif()
message(STATUS "HAZARDSYSTEM_LIBRARIES: ${HAZARDSYSTEM_LIBRARIES}")
#------------------------------------------------------------------------------------------
# Building examples
if(BUILD_HAZARDSYSTEM_EXAMPLE)
    set(HAZARDSYSTEM_EXAMPLE_NAME "${PROJECT_NAME}_example")
    add_executable(${HAZARDSYSTEM_EXAMPLE_NAME} example/main.cpp)
    target_link_libraries(${HAZARDSYSTEM_EXAMPLE_NAME} PRIVATE ${PROJECT_NAME})
endif()
#------------------------------------------------------------------------------------------
# Compiler-specific flags
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /MD")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /MDd")
    target_compile_options(${PROJECT_NAME} PRIVATE 
        "$<$<CONFIG:Release>:/O2>" 
        "$<$<CONFIG:Debug>:/Od>" 
        "$<$<CONFIG:Debug>:/Zi>"
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(${PROJECT_NAME} PRIVATE /DEBUG)
    endif()

    set_target_properties(${PROJECT_NAME} PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS TRUE
    )
else()
    # For non-MSVC compilers
    target_compile_options(${PROJECT_NAME} PRIVATE 
        "$<$<CONFIG:Release>:-O3>"
    )
    # Conditionally include AddressSanitizer flags only if standalone and in Debug mode
    # if(HAZARDSYSTEM_STANDALONE_PROJECT AND "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang|GNU")
    #     set(SANITIZE_FLAGS "$<$<CONFIG:Debug>:-fsanitize=address>")
    #     target_compile_options(${PROJECT_NAME} PRIVATE 
    #         "$<$<CONFIG:Debug>:-g>"
    #         "$<$<CONFIG:Debug>:-O0>"
    #         ${SANITIZE_FLAGS}
    #     )
    #     target_link_options(${PROJECT_NAME} PRIVATE ${SANITIZE_FLAGS})
    # endif()
endif()
#------------------------------------------------------------------------------------------
# Enable testing and add the test directory if this is a standalone project
if(BUILD_HAZARDSYSTEM_TESTS AND HAZARDSYSTEM_STANDALONE_PROJECT)
    enable_testing()
    add_subdirectory(test)
endif()
#------------------------------------------------------------------------------------------
# Enable benchmarks and add the benchmark directory if this is a standalone project
if(BUILD_HAZARDSYSTEM_BENCHMARK AND HAZARDSYSTEM_STANDALONE_PROJECT)
    add_subdirectory(benchmark)
endif()
#------------------------------------------------------------------------------------------
# Export the library targets (only if it's a standalone project)
if(HAZARDSYSTEM_STANDALONE_PROJECT)
    include(GNUInstallDirs)
    install(TARGETS ${PROJECT_NAME} EXPORT CircularBufferTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY ${HAZARDSYSTEM_INCLUDE_DIR}/${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(EXPORT CircularBufferTargets
        FILE CircularBufferTargets.cmake
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    )
endif()
#------------------------------------------------------------------------------------------