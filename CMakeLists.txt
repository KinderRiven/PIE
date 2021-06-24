cmake_minimum_required(VERSION 2.8)

project(PIE)

set (CMAKE_CXX_FLAGS "-O3 -std=c++17 -mrtm")
set (CMAKE_CXX_FALGS ${CMAKE_CXX_FLAGS} "-DSTRINGKEY")
set(SRC_BASE ${PROJECT_SOURCE_DIR})

include_directories(
    ${SRC_BASE}/include
    ${SRC_BASE}/util
    ${SRC_BASE}/src
    ${SRC_BASE}/src/include
)

set(SRC_ALLOCATOR
)

set(SRC_INDEX
    ${SRC_BASE}/src/index/CCEH/CCEH_MSB.cc
    ${SRC_BASE}/src/index/example/example_index.cc
)

set(SRC_UTILS
)

set(SRC_SCHEME
    ${SRC_BASE}/src/scheme.cc
    ${SRC_BASE}/src/status.cc
    ${SRC_BASE}/src/scheme/single/single_scheme.cc
)

set(LIBS
)

link_directories(
)

add_library(${PROJECT_NAME} STATIC ${SRC_INDEX} ${SRC_ALLOCATOR} ${SRC_UTILS} ${SRC_SCHEME})
target_link_libraries(${PROJECT_NAME} ${LIBS})
