cmake_minimum_required(VERSION 3.16)
project(vds-miner VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_definitions(CL_TARGET_OPENCL_VERSION=120)
if(NOT WIN32)
  add_compile_options(-pthread)
endif()

find_package(OpenCL)
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
target_include_directories(vds-miner PRIVATE src)
target_link_libraries(vds-miner PRIVATE Threads::Threads)

if(OpenCL_FOUND)
  target_link_libraries(vds-miner PRIVATE OpenCL::OpenCL)
  target_compile_definitions(vds-miner PRIVATE VDS_HAVE_OPENCL=1)
  message(STATUS "OpenCL: ${OpenCL_LIBRARIES}")
else()
  message(WARNING "OpenCL not found — GPU mining disabled at link time")
endif()

install(TARGETS vds-miner RUNTIME DESTINATION bin)
install(FILES src/kernels/equihash96_5.cl DESTINATION share/vds-miner)
