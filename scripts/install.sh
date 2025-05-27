#!/bin/bash

# Get the absolute path of the script directory
SCRIPT_DIR=$(dirname "$(realpath "$0")")

# Default values
INSTALL_PREFIX="/usr/local"
OVERWRITE=false

# Function to display help
display_help() {
    echo "Usage: $0 [--overwrite|-o] [--prefix|-p <path>]"
    echo
    echo "Options:"
    echo "  --overwrite, -o       Remove the existing build directory before building"
    echo "  --prefix, -p <path>   Set custom installation prefix (default: /usr/local)"
    exit 1
}

# Parse arguments
while [[ "$#" -gt 0 ]]; do
    case "$1" in
        -o|--overwrite)
            OVERWRITE=true
            shift
            ;;
        -p|--prefix)
            if [[ -z "$2" ]]; then
                echo "Error: --prefix requires a path argument."
                exit 1
            fi
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        -h|--help)
            display_help
            ;;
        *)
            echo "Unknown option: $1"
            display_help
            ;;
    esac
done

# Build directory (relative to the script directory)
BUILD_DIR="$SCRIPT_DIR/../build"

# If overwrite flag is set, remove the build directory
if [ "$OVERWRITE" = true ]; then
    echo "Overwrite flag set: Cleaning previous build..."
    rm -rf "$BUILD_DIR"
fi

# Create the build directory if it doesn't exist
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo "Configuring the project with CMake..."
cmake -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" "$SCRIPT_DIR/.."

# Build the project
echo "Building the project..."
make -j"$(nproc)"

# Install the project
echo "Installing the project to $INSTALL_PREFIX..."
sudo make install

# Show the installation locations
echo "Installation finished!"
echo "Headers and libraries are installed in:"
echo "  $INSTALL_PREFIX/include"
echo "  $INSTALL_PREFIX/lib or lib64 (depending on your system)"
