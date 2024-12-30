cmake_minimum_required(VERSION 3.0)

project(midas_receiver)

# Check if the MIDASSYS environment variable is defined
if (NOT DEFINED ENV{MIDASSYS})
    message(STATUS "MIDASSYS environment variable not defined.")

    # Check if environment_variables.txt exists in the specified directory
    if(EXISTS "${CMAKE_SOURCE_DIR}/scripts/environment/environment_variables.txt")
        message(STATUS "Found environment_variables.txt. Checking its contents.")
        
        # Read the contents of the environment_variables.txt
        file(READ "${CMAKE_SOURCE_DIR}/scripts/environment/environment_variables.txt" ENV_VARS_CONTENTS)
        
        # Check if MIDASSYS is defined within the environment_variables.txt
        if(NOT ENV_VARS_CONTENTS MATCHES "MIDASSYS=.*")
            message(STATUS "MIDASSYS is not defined in environment_variables.txt. Running detect_environment.sh.")
            
            # Run the detect_environment.sh script to automatically detect environment
            execute_process(COMMAND bash "${CMAKE_SOURCE_DIR}/scripts/environment/detect_environment.sh"
                            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                            RESULT_VARIABLE DETECT_ENV_RESULT)
            
            if (DETECT_ENV_RESULT EQUAL 0)
                message(STATUS "Environment detection completed.")
            else()
                message(SEND_ERROR "Failed to detect environment. Please check detect_environment.sh.")
            endif()
        endif()
    else()
        message(STATUS "environment_variables.txt not found. Running detect_environment.sh.")
        
        # Run detect_environment.sh if environment_variables.txt does not exist
        execute_process(COMMAND bash "${CMAKE_SOURCE_DIR}/scripts/environment/detect_environment.sh"
                        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                        RESULT_VARIABLE DETECT_ENV_RESULT)
        
        if (DETECT_ENV_RESULT EQUAL 0)
            message(STATUS "Environment detection completed.")
        else()
            message(SEND_ERROR "Failed to detect environment. Please check detect_environment.sh.")
        endif()
    endif()

    # Now attempt to run setup_environment.sh to ensure environment variables are set
    message(STATUS "Running setup_environment.sh.")
    execute_process(COMMAND bash "${CMAKE_SOURCE_DIR}/scripts/environment/setup_environment.sh"
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    RESULT_VARIABLE SETUP_ENV_RESULT)
    
    if (SETUP_ENV_RESULT EQUAL 0)
        message(STATUS "Environment setup completed.")
    else()
        message(SEND_ERROR "Failed to set up environment. Please check setup_environment.sh.")
    endif()

    # Final check if MIDASSYS environment variable is still not defined
    if (NOT DEFINED ENV{MIDASSYS})
        message(SEND_ERROR "MIDASSYS environment variable is still not defined after running setup.")
    endif()
endif()


file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")

file(GLOB_RECURSE MIDAS_SOURCE_FILES "${CMAKE_SOURCE_DIR}/source/*.cpp")

if(NOT MIDAS_SOURCE_FILES)
    message(SEND_ERROR "No source files found in source directory.")
endif()

add_library(midas_receiver_lib STATIC ${MIDAS_SOURCE_FILES})

set(MIDAS_LIBS
   $ENV{MIDASSYS}/lib/libmfe.a
   $ENV{MIDASSYS}/lib/libmidas.a
)

# Find and link zlib
find_package(ZLIB REQUIRED)

target_include_directories(midas_receiver_lib PRIVATE
   $ENV{MIDASSYS}/include
   $ENV{MIDASSYS}/mxml
   $ENV{MIDASSYS}/midasio
   ${CMAKE_SOURCE_DIR}/include
)

set_property(TARGET midas_receiver_lib PROPERTY CXX_STANDARD 17)

# Link MIDAS and Zlib libraries
target_link_libraries(midas_receiver_lib PRIVATE ${MIDAS_LIBS} ZLIB::ZLIB)

add_executable(midas_receiver_test source/receiver_test.cpp)
target_link_libraries(midas_receiver_test PRIVATE midas_receiver_lib ${MIDAS_LIBS} ZLIB::ZLIB)
target_include_directories(midas_receiver_test PRIVATE
   $ENV{MIDASSYS}/include
   $ENV{MIDASSYS}/mxml
   $ENV{MIDASSYS}/midasio
   ${CMAKE_SOURCE_DIR}/include
)

add_executable(receiver_lib_test source/main.cpp)
target_link_libraries(receiver_lib_test PRIVATE midas_receiver_lib ${MIDAS_LIBS} ZLIB::ZLIB)
target_include_directories(receiver_lib_test PRIVATE
   $ENV{MIDASSYS}/include
   $ENV{MIDASSYS}/mxml
   $ENV{MIDASSYS}/midasio
   ${CMAKE_SOURCE_DIR}/include
)

set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}")

install(TARGETS midas_receiver_test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS receiver_lib_test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS midas_receiver_lib DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
