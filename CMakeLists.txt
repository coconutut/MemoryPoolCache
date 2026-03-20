cmake_minimum_required(VERSION 3.10)
project(MemoryPoolCache LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(main main.cpp
               SpanList/SpanList.cpp
               PageCache/PageCache.cpp
               CentralCache/CentralCache.cpp
               ThreadCache/ThreadCache.cpp)

target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)