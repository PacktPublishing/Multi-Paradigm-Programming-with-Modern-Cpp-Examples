cmake_minimum_required(VERSION 3.0.0)
project(constexpr VERSION 0.1.0)

SET(CLANG_CPP_VERSION_FLAGS "-std=c++2a")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CLANG_CPP_VERSION_FLAGS}")

include(CTest)
enable_testing()

add_executable(constexpr main.cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
