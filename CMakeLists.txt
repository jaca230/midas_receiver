cmake_minimum_required(VERSION 3.0)

project(midas_receiver)

# Check environment variables
if (NOT DEFINED ENV{MIDASSYS})
    message(STATUS "MIDASSYS environment variable not defined.")

    # Check if environment_variables.txt exists
    if(EXISTS "${CMAKE_SOURCE_DIR}/environment_variables.txt")
        message(STATUS "Found environment_variables.txt. Checking its contents.")

        # Read the contents of environment_variables.txt
        file(READ "${CMAKE_SOURCE_DIR}/environment_variables.txt" ENV_VARS_CONTENTS)

        # Check if MIDASSYS is defined in the environment_variables.txt
        if(NOT ENV_VARS_CONTENTS MATCHES "MIDASSYS=.*")
            message(STATUS "MIDASSYS is not defined in environment_variables.txt. Running detect_environment.sh.")
            execute_process(
                COMMAND bash "${CMAKE_SOURCE_DIR}/detect_environment.sh"
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                RESULT_VARIABLE DETECT_ENV_RESULT
            )
            if (DETECT_ENV_RESULT EQUAL 0)
                message(STATUS "Environment detection completed.")
            else()
                message(SEND_ERROR "Failed to detect environment. Please check detect_environment.sh.")
            endif()
        endif()
    else()
        message(STATUS "environment_variables.txt not found. Running detect_environment.sh.")
        execute_process(
            COMMAND bash "${CMAKE_SOURCE_DIR}/detect_environment.sh"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE DETECT_ENV_RESULT
        )
        if (DETECT_ENV_RESULT EQUAL 0)
            message(STATUS "Environment detection completed.")
        else()
            message(SEND_ERROR "Failed to detect environment. Please check detect_environment.sh.")
        endif()
    endif()

    # Run setup_environment.sh if needed (optional, can be removed if not required)
    message(STATUS "Running setup_environment.sh.")
    execute_process(
        COMMAND bash "${CMAKE_SOURCE_DIR}/setup_environment.sh"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE SETUP_ENV_RESULT
    )
    if (SETUP_ENV_RESULT EQUAL 0)
        message(STATUS "Environment setup completed.")
    else()
        message(SEND_ERROR "Failed to set up environment. Please check setup_environment.sh.")
    endif()

    # After setup, check if MIDASSYS is still not defined
    if (NOT DEFINED ENV{MIDASSYS})
        message(SEND_ERROR "MIDASSYS environment variable is still not defined after running setup.")
    endif()
endif()

# Create the bin directory if it doesn't exist
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# Create the lib directory if it doesn't exist
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")

# Create a library from all the source files in source
file(GLOB_RECURSE MIDAS_SOURCE_FILES "${CMAKE_SOURCE_DIR}/source/*.cpp")

# If no source files are found, send an error
if(NOT MIDAS_SOURCE_FILES)
    message(SEND_ERROR "No source files found in source directory.")
endif()

# Create the library from the source files
add_library(midas_receiver_lib STATIC ${MIDAS_SOURCE_FILES})

# Define libraries for the "midas_receiver" target
set(MIDAS_LIBS
   # Midas libs
   $ENV{MIDASSYS}/lib/libmfe.a
   $ENV{MIDASSYS}/lib/libmidas.a
)

# Add necessary include directories
target_include_directories(midas_receiver_lib PRIVATE
   # Include Midas directories
   $ENV{MIDASSYS}/include
   $ENV{MIDASSYS}/mxml
   $ENV{MIDASSYS}/midasio

   # Include your own project's include directory
   ${CMAKE_SOURCE_DIR}/include
)

# Set the C++ standard for the library
set_property(TARGET midas_receiver_lib PROPERTY CXX_STANDARD 17)

# Link the library with MIDAS libraries
target_link_libraries(midas_receiver_lib PRIVATE ${MIDAS_LIBS})

# Now create the midas_receiver_test executable
add_executable(midas_receiver_test source/receiver_test.cpp)

# Link the midas_receiver_test executable with the created library and MIDAS libraries
target_link_libraries(midas_receiver_test PRIVATE midas_receiver_lib ${MIDAS_LIBS})

# Ensure midas_receiver_test also has access to the include directory
target_include_directories(midas_receiver_test PRIVATE
   # Include Midas directories
   $ENV{MIDASSYS}/include
   $ENV{MIDASSYS}/mxml
   $ENV{MIDASSYS}/midasio

   # Include your own project's include directory
   ${CMAKE_SOURCE_DIR}/include
)

# Now create the receiver_lib_test executable
add_executable(receiver_lib_test source/main.cpp)

# Link the receiver_lib_test executable with the created library and MIDAS libraries
target_link_libraries(receiver_lib_test PRIVATE midas_receiver_lib ${MIDAS_LIBS})

# Ensure receiver_lib_test also has access to the include directory
target_include_directories(receiver_lib_test PRIVATE
   # Include Midas directories
   $ENV{MIDASSYS}/include
   $ENV{MIDASSYS}/mxml
   $ENV{MIDASSYS}/midasio

   # Include your own project's include directory
   ${CMAKE_SOURCE_DIR}/include
)

# Set the installation directory for the receiver_test executable
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}") # Install to the parent directory

# Install the midas_receiver_test executable
install(TARGETS midas_receiver_test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

# Install the receiver_lib_test executable
install(TARGETS receiver_lib_test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

# Install the midas_receiver_lib library
install(TARGETS midas_receiver_lib DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
