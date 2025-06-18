cmake_minimum_required(VERSION 3.14)

# Only define project if not already defined (to avoid conflicts as a submodule)
if(NOT PROJECT_NAME)
  project(MidasReceiver VERSION 0.1 LANGUAGES CXX)
endif()

# Option to build test executable; parent project can disable this
option(BUILD_MIDAS_RECEIVER_TEST "Build the receiver_lib_test executable" ON)

# Check MIDASSYS environment variable
if(NOT DEFINED ENV{MIDASSYS})
  message(FATAL_ERROR
    "Environment variable MIDASSYS not set.\n"
    "Please source your MIDAS environment or run the detect_environment.sh script:\n"
    "  ./scripts/environment/detect_environment.sh\n"
    "  source ./scripts/environment/.env"
  )
endif()

# Gather source files except main.cpp
file(GLOB_RECURSE MIDAS_SRC CONFIGURE_DEPENDS src/*.cpp)
list(FILTER MIDAS_SRC EXCLUDE REGEX ".*/main.cpp$")

# Find external dependencies
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
find_library(RT_LIBRARY rt REQUIRED)

# Create static library
add_library(midas_receiver STATIC ${MIDAS_SRC})

# Set C++17 standard on target
target_compile_features(midas_receiver PUBLIC cxx_std_17)

# Include directories with build/install interface
target_include_directories(midas_receiver PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  $ENV{MIDASSYS}/include
  $ENV{MIDASSYS}/mxml
  $ENV{MIDASSYS}/midasio
)

# Link libraries, with RT_LIBRARY after midas libs for proper linkage
target_link_libraries(midas_receiver PUBLIC
  ZLIB::ZLIB
  Threads::Threads
  $ENV{MIDASSYS}/lib/libmfe.a
  $ENV{MIDASSYS}/lib/libmidas.a
  ${RT_LIBRARY}
)

# Optional test executable
if(BUILD_MIDAS_RECEIVER_TEST)
  add_executable(receiver_lib_test main.cpp)

  target_link_libraries(receiver_lib_test PRIVATE
    midas_receiver
    ZLIB::ZLIB
    Threads::Threads
    ${RT_LIBRARY}
  )
endif()

# Install rules
include(GNUInstallDirs)

install(TARGETS midas_receiver
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(BUILD_MIDAS_RECEIVER_TEST)
  install(TARGETS receiver_lib_test
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()
