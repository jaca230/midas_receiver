cmake_minimum_required(VERSION 3.0)

project(midas_receiver)

# Check MIDASSYS environment variable
if(NOT DEFINED ENV{MIDASSYS})
  message(FATAL_ERROR
    "Environment variable MIDASSYS not set.\n"
    "Please source your MIDAS environment or run the detect_environment.sh script:\n"
    "  ./scripts/environment/detect_environment.sh\n"
    "  source ./scripts/environment/.env"
  )
endif()

# Create local bin and lib folders (for build outputs)
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")

# Set output directories for build artifacts
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)       # for executables
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)       # for shared libs
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)       # for static libs (.a)

# Glob source files (adjust folder name if needed)
file(GLOB_RECURSE MIDAS_SOURCE_FILES "${CMAKE_SOURCE_DIR}/src/*.cpp")

if(NOT MIDAS_SOURCE_FILES)
    message(FATAL_ERROR "No source files found in src directory.")
endif()

add_library(midas_receiver_lib STATIC ${MIDAS_SOURCE_FILES})

set(MIDAS_LIBS
   $ENV{MIDASSYS}/lib/libmfe.a
   $ENV{MIDASSYS}/lib/libmidas.a
)

find_package(ZLIB REQUIRED)

target_include_directories(midas_receiver_lib PRIVATE
   $ENV{MIDASSYS}/include
   $ENV{MIDASSYS}/mxml
   $ENV{MIDASSYS}/midasio
   ${CMAKE_SOURCE_DIR}/include
)

set_property(TARGET midas_receiver_lib PROPERTY CXX_STANDARD 17)

target_link_libraries(midas_receiver_lib PRIVATE ${MIDAS_LIBS} ZLIB::ZLIB)

add_executable(midas_receiver_test src/receiver_test.cpp)
target_link_libraries(midas_receiver_test PRIVATE midas_receiver_lib ${MIDAS_LIBS} ZLIB::ZLIB pthread rt util)
target_include_directories(midas_receiver_test PRIVATE
   $ENV{MIDASSYS}/include
   $ENV{MIDASSYS}/mxml
   $ENV{MIDASSYS}/midasio
   ${CMAKE_SOURCE_DIR}/include
)

add_executable(receiver_lib_test src/main.cpp)
target_link_libraries(receiver_lib_test PRIVATE midas_receiver_lib ${MIDAS_LIBS} ZLIB::ZLIB pthread rt util)
target_include_directories(receiver_lib_test PRIVATE
   $ENV{MIDASSYS}/include
   $ENV{MIDASSYS}/mxml
   $ENV{MIDASSYS}/midasio
   ${CMAKE_SOURCE_DIR}/include
)

# Set default install prefix if none specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Install path prefix." FORCE)
endif()

# Install targets to global system locations (based on CMAKE_INSTALL_PREFIX)
install(TARGETS midas_receiver_test DESTINATION bin)
install(TARGETS receiver_lib_test DESTINATION bin)
install(TARGETS midas_receiver_lib DESTINATION lib)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include)
