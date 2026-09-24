cmake_minimum_required(VERSION 3.15)
project(5G_reorder VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(Threads REQUIRED)

add_executable(pdcp_server src/server.cpp)
target_link_libraries(pdcp_server PRIVATE Threads::Threads)

add_executable(pdcp_traffic_gen src/traffic_gen.cpp)
target_link_libraries(pdcp_traffic_gen PRIVATE Threads::Threads)
