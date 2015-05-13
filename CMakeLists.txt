cmake_minimum_required(VERSION 2.8)
project(service_queue)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

set(Boost_USE_STATIC_LIBS        ON) # only find static libs
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME    OFF)

find_package(Boost 1.57.0 COMPONENTS thread system log filesystem REQUIRED)
find_package(ZeroMQ REQUIRED)
find_package(JSONCpp REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -fpermissive -lboost_system -lboost_thread -lboost_filesystem")

set(SOURCE_FILES main.cpp main.hpp zmq.hpp)

include_directories(${JsonCpp_INCLUDE_DIR})
add_executable(service_queue ${SOURCE_FILES} broker.cpp broker.hpp)
target_link_libraries(service_queue ${Boost_LIBRARIES})
target_link_libraries(service_queue ${ZeroMQ_LIBRARY})
target_link_libraries(service_queue ${JsonCpp_LIBRARY})

add_custom_command(TARGET service_queue PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/distfiles $<TARGET_FILE_DIR:service_queue>)
