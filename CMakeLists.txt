cmake_minimum_required(VERSION 3.15)
project(SkyDecoder VERSION 1.0.0 LANGUAGES CXX)

# C++ Configuration
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compilation options
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -pedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find dependencies
find_package(PkgConfig REQUIRED)

# TinyXML2 - try multiple methods
find_package(tinyxml2 QUIET)
if(NOT tinyxml2_FOUND)
    message(STATUS "TinyXML2 not found via CMake, trying pkg-config")
    pkg_check_modules(TINYXML2 tinyxml2)
    if(NOT TINYXML2_FOUND)
        message(STATUS "TinyXML2 not found via pkg-config, trying manual detection")
        find_path(TINYXML2_INCLUDE_DIR tinyxml2.h)
        find_library(TINYXML2_LIBRARY tinyxml2)
        if(TINYXML2_INCLUDE_DIR AND TINYXML2_LIBRARY)
            set(TINYXML2_FOUND TRUE)
            set(TINYXML2_INCLUDE_DIRS ${TINYXML2_INCLUDE_DIR})
            set(TINYXML2_LIBRARIES ${TINYXML2_LIBRARY})
        endif()
    endif()
endif()

if(NOT TINYXML2_FOUND AND NOT tinyxml2_FOUND)
    message(FATAL_ERROR "TinyXML2 not found. Please install: sudo apt install libtinyxml2-dev")
endif()

# Threads
find_package(Threads REQUIRED)

# Check that source directories exist
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src")
    message(FATAL_ERROR "Source directory 'src' not found")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include")
    message(FATAL_ERROR "Include directory 'include' not found")
endif()

# Library sources
set(SKYDECODER_SOURCES
    src/asterix_decoder.cpp
    src/xml_parser.cpp
    src/field_parser.cpp
    src/utils.cpp
)

set(SKYDECODER_HEADERS
    include/skydecoder/asterix_decoder.h
    include/skydecoder/asterix_types.h
    include/skydecoder/xml_parser.h
    include/skydecoder/field_parser.h
    include/skydecoder/utils.h
)

# Check that all source files exist
foreach(source_file ${SKYDECODER_SOURCES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source_file}")
        message(WARNING "Source file ${source_file} not found")
    endif()
endforeach()

# Create the library
add_library(skydecoder ${SKYDECODER_SOURCES})

# Include directories
target_include_directories(skydecoder
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link TinyXML2
if(tinyxml2_FOUND)
    target_link_libraries(skydecoder PUBLIC tinyxml2::tinyxml2)
else()
    target_include_directories(skydecoder PRIVATE ${TINYXML2_INCLUDE_DIRS})
    target_link_libraries(skydecoder PUBLIC ${TINYXML2_LIBRARIES})
endif()

# Link Threads
target_link_libraries(skydecoder PRIVATE Threads::Threads)

# Library properties
set_target_properties(skydecoder PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# Compilation definitions
target_compile_definitions(skydecoder PRIVATE
    $<$<CONFIG:Debug>:SKYDECODER_DEBUG>
)

# ============================================================================
# Example executable (optional)
# ============================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/decode_asterix.cpp")
    add_executable(decode_asterix examples/decode_asterix.cpp)
    target_link_libraries(decode_asterix skydecoder)
else()
    message(STATUS "Example file not found, skipping decode_asterix executable")
endif()

# ============================================================================
# Tests (optional)
# ============================================================================
option(BUILD_TESTS "Build tests" OFF)

if(BUILD_TESTS)
    find_package(GTest QUIET)
    if(GTest_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        enable_testing()
        
        file(GLOB TEST_SOURCES "tests/test_*.cpp")
        if(TEST_SOURCES)
            add_executable(test_skydecoder ${TEST_SOURCES})
            target_link_libraries(test_skydecoder skydecoder GTest::GTest GTest::Main)
            add_test(NAME SkyDecoderTests COMMAND test_skydecoder)
        endif()
    else()
        message(STATUS "Google Test not found or no tests directory, skipping tests")
    endif()
endif()

# ============================================================================
# Basic installation
# ============================================================================
include(GNUInstallDirs)

install(TARGETS skydecoder
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/skydecoder
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install data if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
    install(DIRECTORY data/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/skydecoder
    )
endif()

# ============================================================================
# Informational messages
# ============================================================================
message(STATUS "")
message(STATUS "SkyDecoder Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Source dir: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "  Binary dir: ${CMAKE_CURRENT_BINARY_DIR}")
if(tinyxml2_FOUND)
    message(STATUS "  TinyXML2: Found (CMake)")
elseif(TINYXML2_FOUND)
    message(STATUS "  TinyXML2: Found (pkg-config/manual)")
endif()
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "")