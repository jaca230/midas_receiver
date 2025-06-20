cmake_minimum_required(VERSION 3.14)

project(MidasReceiver VERSION 0.1 LANGUAGES CXX)

# Use position-independent code for static/shared libs
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Generate compile_commands.json (for clangd, VSCode, etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Optional build of test executable
option(BUILD_MIDAS_RECEIVER_TEST "Build the receiver_lib_test executable" ON)

# Require MIDASSYS
if(NOT DEFINED ENV{MIDASSYS})
  message(FATAL_ERROR
    "Environment variable MIDASSYS not set.\n"
    "Please source your MIDAS environment or run the detect_environment.sh script:\n"
    "  ./scripts/environment/detect_environment.sh\n"
    "  source ./scripts/environment/.env"
  )
endif()

set(MIDASSYS_INCLUDE_DIRS
  $ENV{MIDASSYS}/include
  $ENV{MIDASSYS}/mxml
  $ENV{MIDASSYS}/midasio
)
set(MIDASSYS_LIB_DIR $ENV{MIDASSYS}/lib)

# Find system dependencies
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
find_library(RT_LIBRARY rt REQUIRED)

# Gather sources
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER SRC_FILES EXCLUDE REGEX ".*/main.cpp$")

# Create library
add_library(midas_receiver STATIC ${SRC_FILES})
add_library(MidasReceiver::midas_receiver ALIAS midas_receiver)

set_target_properties(midas_receiver PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION 0
)

target_compile_features(midas_receiver PUBLIC cxx_std_17)

target_include_directories(midas_receiver PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/midas_receiver>
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${MIDASSYS_INCLUDE_DIRS}
)

target_link_libraries(midas_receiver PUBLIC
  ZLIB::ZLIB
  Threads::Threads
  ${MIDASSYS_LIB_DIR}/libmfe.a
  ${MIDASSYS_LIB_DIR}/libmidas.a
  ${RT_LIBRARY}
)

# Optional test executable
if(BUILD_MIDAS_RECEIVER_TEST)
  add_executable(receiver_lib_test main.cpp)
  target_link_libraries(receiver_lib_test PRIVATE midas_receiver)
endif()

# Install logic
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)

  install(TARGETS midas_receiver
    EXPORT MidasReceiverTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/midas_receiver
  )

  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/midas_receiver
  )

  if(BUILD_MIDAS_RECEIVER_TEST)
    install(TARGETS receiver_lib_test
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
  endif()

  install(EXPORT MidasReceiverTargets
    FILE MidasReceiverTargets.cmake
    NAMESPACE MidasReceiver::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MidasReceiver
  )

  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/MidasReceiverConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/MidasReceiverConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MidasReceiver
  )

  configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/MidasReceiverConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MidasReceiver
  )

  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/MidasReceiverConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MidasReceiver
  )

  export(PACKAGE MidasReceiver)

endif()
