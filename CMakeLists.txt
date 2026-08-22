cmake_minimum_required(VERSION 3.10)
project(vds-miner VERSION 1.1.6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wno-unused-parameter -O3 -ffast-math)
add_compile_definitions(CL_TARGET_OPENCL_VERSION=120)

file(READ "${CMAKE_SOURCE_DIR}/src/kernels/equihash96_5.cl" VDS_KERNEL_CL)
configure_file(
  "${CMAKE_SOURCE_DIR}/src/kernel_embed.h.in"
  "${CMAKE_BINARY_DIR}/generated/kernel_embed.h"
  @ONLY
)
if(NOT WIN32)
  add_compile_options(-pthread)
endif()

find_package(OpenCL REQUIRED)
find_package(Threads REQUIRED)

set(SRCS
  src/crypto.cpp
  src/equihash.cpp
  src/opencl_solver.cpp
  src/stratum.cpp
  src/api.cpp
  src/main.cpp
)

add_executable(vds-miner ${SRCS})
target_include_directories(vds-miner PRIVATE
  src
  "${CMAKE_BINARY_DIR}/generated"
  "${CMAKE_SOURCE_DIR}/third_party/OpenCL/include"
)
target_link_libraries(vds-miner PRIVATE Threads::Threads)

if(OpenCL_FOUND)
  target_link_libraries(vds-miner PRIVATE OpenCL::OpenCL)
  target_compile_definitions(vds-miner PRIVATE VDS_HAVE_OPENCL=1)
  message(STATUS "OpenCL: ${OpenCL_LIBRARIES}")
endif()

install(TARGETS vds-miner RUNTIME DESTINATION bin)
install(FILES src/kernels/equihash96_5.cl DESTINATION share/vds-miner)
