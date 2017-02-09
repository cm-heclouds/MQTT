# MQTT SDK minimum development versions
set(MIN_VER_CMAKE 2.6)


cmake_minimum_required(VERSION "${MIN_VER_CMAKE}" FATAL_ERROR)

# must go before the project command
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Configs" FORCE)
if(DEFINED CMAKE_BUILD_TYPE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${CMAKE_CONFIGURATION_TYPES})
endif()

project(MqttSDK CXX C)

if(MSVC)
  set(CMAKE_USE_RELATIVE_PATHS ON CACHE INTERNAL "" FORCE)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  
  if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} /Wall")
	set(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} /Wall")
  else()
    set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb ")
    set(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} -O0 -Wall -g -ggdb")
  endif()
else()
  if(MSVC)
    set(CMAKE_CXX_FALGS_RELEASE "$ENV{CXXFLAGS} /Wall")
	set(CMAKE_C_FLAGS_RELEASE "$ENV{CFLAGS} /Wall")
  else()
    set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
    set(CMAKE_C_FLAGS_RELEASE "$ENV{CFLAGS} -O3 -Wall")
  endif()
endif()

set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin" CACHE PATH "Output directory for applications")
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")

#option(BUILD_PYTHON_SDK "Build the Python SDK." OFF)
#option(BUILD_JAVA_SDK "Build the Java SDK." OFF)

include_directories(${CMAKE_SOURCE_DIR})
add_subdirectory(src)
#add_subdirectory(swig)
add_subdirectory(sample)
