cmake_minimum_required(VERSION 3.0.0)
project(constexpr VERSION 0.1.0)

SET(CLANG_CPP_VERSION_FLAGS "-std=c++2a")
SET(CLANG_MISC_FLAGS "")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CLANG_CPP_VERSION_FLAGS} ${CLANG_MISC_FLAGS}")


include(CTest)
enable_testing()

add_executable(example main.cpp tasks.cpp)

set(RANGE_V3 "/Users/gpa/libs/ranges/range-v3/include")
target_include_directories(example PRIVATE ${RANGE_V3})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
