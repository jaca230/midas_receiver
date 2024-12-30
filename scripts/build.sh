#!/bin/bash

# Function to display help
display_help() {
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  --overwrite   Overwrite the build directory if it exists"
    echo "  --help        Display this help message"
    echo "  --install     Perform a full installation, including copying libraries and headers"
    exit 0
}

# Parse command-line arguments
overwrite=false
install=false
for arg in "$@"; do
    case $arg in
        --overwrite)
        overwrite=true
        shift
        ;;
        --install)
        install=true
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

# If --install flag is set, perform additional installation steps (copy libraries and headers)
if $install; then
    echo "Performing full installation..."

    # Default install location for libraries and headers (adjust if needed)
    LIB_DIR=$(realpath "$script_directory/../lib")
    INCLUDE_DIR=$(realpath "$script_directory/../include")

    # Check if the lib directory exists and copy the libraries
    if [ -d "$LIB_DIR" ]; then
        # Copy all .a files and .so files to /usr/lib64, printing each one
        for lib in "$LIB_DIR"/*.a; do
            if [ -e "$lib" ]; then
                echo "Copying library: $(basename "$lib")"
                sudo cp "$lib" /usr/lib64/
            fi
        done

        for lib in "$LIB_DIR"/*.so; do
            if [ -e "$lib" ]; then
                echo "Copying library: $(basename "$lib")"
                sudo cp "$lib" /usr/lib64/
            fi
        done

        # Optionally run ldconfig to update library cache
        sudo ldconfig

        echo "Libraries successfully copied to /usr/lib64."
    else
        echo "Library directory not found: $LIB_DIR"
    fi

    # Check if the include directory exists and copy the headers
    if [ -d "$INCLUDE_DIR" ]; then
        # Copy all header files to /usr/include, converting names to lowercase
        for header in "$INCLUDE_DIR"/*.h; do
            if [ -e "$header" ]; then
                lower_header=$(echo "$header" | tr '[:upper:]' '[:lower:]')
                echo "Copying header: $(basename "$lower_header")"
                sudo cp "$header" "/usr/include/$(basename "$lower_header")"
            fi
        done

        echo "Headers successfully copied to /usr/include."
    else
        echo "Include directory not found: $INCLUDE_DIR"
    fi
fi

echo "Installation complete."
