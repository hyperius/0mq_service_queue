cmake_minimum_required(VERSION 2.8)
project(service_queue)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

set(Boost_USE_STATIC_LIBS        ON) # only find static libs
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME    OFF)

find_package(Boost 1.46.0 COMPONENTS thread system log filesystem REQUIRED)
find_package(ZeroMQ REQUIRED)
find_package(JSONCpp REQUIRED)

if (CMAKE_COMPILER_IS_GNUCXX)
   execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)

   if (GCC_VERSION VERSION_GREATER 4.7 OR GCC_VERSION VERSION_EQUAL 4.7)

        message(STATUS "C++11 activated.")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

   elseif (GCC_VERSION VERSION_GREATER 4.3 OR GCC_VERSION VERSION_EQUAL 4.3)

        message(WARNING "C++0x activated. If you get any errors update to a compiler which fully supports C++11")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -fpermissive -lboost_system -lboost_thread -lboost_filesystem -D_GLIBCXX_USE_NANOSLEEP")

   else ()

        message(FATAL_ERROR "C++11 needed. Therefore a gcc compiler with a version higher than 4.3 is needed.")

   endif()

else(CMAKE_COMPILER_IS_GNUCXX)

   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

endif(CMAKE_COMPILER_IS_GNUCXX)


set(SOURCE_FILES main.cpp main.hpp zmq.hpp)

include_directories(${JsonCpp_INCLUDE_DIR})
add_executable(service_queue ${SOURCE_FILES} broker.cpp broker.hpp)
target_link_libraries(service_queue ${Boost_LIBRARIES})
target_link_libraries(service_queue ${ZeroMQ_LIBRARY})
target_link_libraries(service_queue ${JsonCpp_LIBRARY})

add_custom_command(TARGET service_queue PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/distfiles $<TARGET_FILE_DIR:service_queue>)
