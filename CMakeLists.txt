cmake_minimum_required(VERSION 3.16)
project(HashLinkCLI
    VERSION 1.0.0
    DESCRIPTION "C++17 URL Shortener Using Custom Open-Addressing Hash Tables"
    LANGUAGES CXX
)

# Enforce C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable compiler warnings
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Core library sources (shared between CLI and Tests)
set(CORE_SOURCES
    src/URLRecord.cpp
    src/HashTable.cpp
    src/URLShortener.cpp
    src/FileStorage.cpp
)

# Core static library
add_library(hashlink_core STATIC ${CORE_SOURCES})

# Main Application Executable
add_executable(hashlink src/main.cpp)
target_link_libraries(hashlink PRIVATE hashlink_core)

# Enable Testing
enable_testing()

# Unit Test Executables
add_executable(test_hash_table tests/test_hash_table.cpp)
target_link_libraries(test_hash_table PRIVATE hashlink_core)
add_test(NAME HashTableTests COMMAND test_hash_table)

add_executable(test_url_shortener tests/test_url_shortener.cpp)
target_link_libraries(test_url_shortener PRIVATE hashlink_core)
add_test(NAME URLShortenerTests COMMAND test_url_shortener)
