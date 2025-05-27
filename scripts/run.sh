#!/bin/bash

# Function to display help
display_help() {
    echo "Usage: $0 [options] -- [executable args]"
    echo
    echo "Options:"
    echo "  --help       Display this help message"
    echo "  --gdb        Run receiver_lib_test under gdb debugger"
    echo
    echo "Any arguments after '--' are passed to the executable."
    exit 0
}

# Default options
run_with_gdb=false
exec_args=()

# Parse options until '--'
while [[ $# -gt 0 ]]; do
    case "$1" in
        --help)
            display_help
            ;;
        --gdb)
            run_with_gdb=true
            shift
            ;;
        --)
            shift
            # All remaining args go to executable
            exec_args=("$@")
            break
            ;;
        *)
            echo "Unknown option: $1"
            display_help
            ;;
    esac
done

# Resolve script directory (handles symlinks)
SOURCE="${BASH_SOURCE[0]}"
while [ -L "$SOURCE" ]; do
    DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
    SOURCE=$(readlink "$SOURCE")
    [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
done
script_dir=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
readonly script_dir

echo "Run script directory resolved as: $script_dir"

# Source environment setup script quietly
source "$script_dir/environment/setup_environment.sh" -q
echo "Environment script sourced."

# Path to executable (adjust if needed)
executable="$script_dir/../build/receiver_lib_test"

if [ ! -x "$executable" ]; then
    echo "Error: Executable not found or not executable: $executable"
    exit 1
fi

echo "Running $executable"

if $run_with_gdb; then
    echo "Starting under gdb..."
    gdb --args "$executable" "${exec_args[@]}"
else
    "$executable" "${exec_args[@]}"
fi
