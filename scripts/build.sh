#!/bin/bash

# Function to display help
display_help() {
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  --overwrite   Overwrite the build directory if it exists"
    echo "  --help        Display this help message"
    exit 0
}

# Parse command-line arguments
overwrite=false
for arg in "$@"; do
    case $arg in
        --overwrite)
        overwrite=true
        shift
        ;;
        --help)
        display_help
        ;;
        *)
        echo "Unknown option: $arg"
        display_help
        ;;
    esac
done

# Get the directory of the script
SOURCE="${BASH_SOURCE[0]}"
while [ -L "$SOURCE" ]; do
    DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
    SOURCE=$(readlink "$SOURCE")
    [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
done
script_directory=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )

# Always source setup_env.sh with --quiet
source "$script_directory/environment/setup_environment.sh" -q

# Build directory path
build_directory=$(realpath "$script_directory/../build")

# Remove the build directory if --overwrite flag is set
if $overwrite && [ -d "$build_directory" ]; then
    rm -rf "$build_directory"
fi

# Create build directory if it doesn't exist
mkdir -p "$build_directory"

# Navigate into build directory
cd "$build_directory"

# Try cmake first, then fallback to cmake3 if cmake isn't available
if command -v cmake &> /dev/null; then
    cmake ..
elif command -v cmake3 &> /dev/null; then
    cmake3 ..
else
    echo "Error: Neither cmake nor cmake3 is installed."
    exit 1
fi

# Run make install with parallel jobs based on the number of processors
make install -j$(nproc)
