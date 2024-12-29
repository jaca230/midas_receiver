# MIDAS Receiver

## Description

This project provides a C++ library and test applications for receiving and processing events from a MIDAS experiment. The receiver connects to the experiment server, requests events, and processes them based on predefined configurations.

## Project Structure

- **bin/**: Contains compiled binaries and test executables.
- **build/**: The build directory where CMake generates build files.
- **config/**: Configuration files for the receiver (e.g., `config.json`).
- **include/**: Header files for the receiver library.
- **lib/**: Compiled static library (`libmidas_receiver_lib.a`).
- **scripts/**: Build and environment setup scripts.
- **source/**: The source code for the receiver and test applications.
- **CMakeLists.txt**: The main CMake build configuration file.

## Dependencies

- **CMake**: For building the project.
- **MIDAS**: The backend system that handles event processing.
- **C++14 or higher**: For compiling the code.

## Building the Project

To build the project, run the following commands:

```bash
# Create and navigate to a build directory
mkdir build && cd build

# Run CMake to configure the project
cmake ..

# Build the project
make
```

This will generate the necessary binaries in the `bin/` directory.

## Sample Receiver

Once the project is built, you can run an example midas consumer/receiver using the compiled binaries. The receiver will attempt to connect to the specified experiment server and start processing events. It operates on its own thread and does not use the generated library methods.

```bash
# Running the test binary
./bin/midas_receiver_test
```

### Configuration

The receiver's behavior can be customized using the `config/config.json` file. Ensure that the experiment server and event parameters are correctly set in this configuration file.

## Testing

The project includes test executables for verifying the functionality of the receiver library. To run the tests:

```bash
# Run the receiver test binary
./bin/receiver_lib_test
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

```
