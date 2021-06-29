cmake_minimum_required(VERSION 2.8)

project(PIE)

set (CMAKE_CXX_FLAGS "-O3 -std=c++17 -mrtm")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCCEH_STRINGKEY")

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
    # EXAMPLE
    ${SRC_BASE}/src/index/example/example_index.cc
    # CCEH
    ${SRC_BASE}/src/index/CCEH/CCEH_MSB.cc
    # RHTREE
    ${SRC_BASE}/src/index/RHTREE/rhtree.cc
    ${SRC_BASE}/src/index/RHTREE/rhtreecity.cc
    ${SRC_BASE}/src/index/RHTREE/rhtreenode.cc
    # WORT
    ${SRC_BASE}/src/index/WORT/wort.cc
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
