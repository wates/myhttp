cmake_minimum_required(VERSION 2.6)

project(myhttp)

if(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS "-Wall -Wno-unused-parameter -fno-strict-aliasing -O3")
    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -g -std=c++11 ${CMAKE_CXX_FLAGS}")
    add_definitions(-DGCC)
endif()

add_subdirectory(libuv)

include_directories(.
libuv/include
mysql-connector-c-6.1.6-win32/include #for windows
)

find_library(MYSQL NAMES mysqlclient libmysql)

add_executable(myhttp
  http-parser/http_parser.c
  http-parser/http_parser.h
  main.cc
)

target_link_libraries(myhttp libuv ${MYSQL})
