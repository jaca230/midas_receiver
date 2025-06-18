#!/bin/bash

# Get the absolute path of the script directory
SCRIPT_DIR=$(dirname "$(realpath "$0")")
BASE_DIR=$(realpath "$SCRIPT_DIR/..")
EXECUTABLE="$BASE_DIR/build/receiver_lib_test"

# Variables for options
PRELOAD_LIBS=""
DEBUG=false
VALGRIND=false
EXE_ARGS=()

# Help message
show_help() {
    echo "Usage: ./run.sh [OPTIONS] [-- <args>]"
    echo
    echo "Options:"
    echo "  -h, --help           Display this help message"
    echo "  -d, --debug          Run with gdb for debugging"
    echo "  -v, --valgrind       Run with valgrind for memory analysis"
    echo "  --preload <libs>     Comma-separated list of library paths to LD_PRELOAD"
    echo
    echo "Arguments after '--' will be passed to the executable."
    echo "Example: ./run.sh --preload /usr/lib/libfoo.so,/usr/lib/libbar.so -- -c config.json"
}

# Parse arguments
while [[ "$#" -gt 0 ]]; do
    case "$1" in
        -h|--help) show_help; exit 0 ;;
        -d|--debug) DEBUG=true; shift ;;
        -v|--valgrind) VALGRIND=true; shift ;;
        --preload)
            if [[ -n "$2" && "$2" != -* ]]; then
                # Convert comma-separated list to colon-separated for LD_PRELOAD
                PRELOAD_LIBS="${2//,/':' }"
                shift 2
            else
                echo "[run.sh, ERROR] --preload requires a comma-separated list of paths"
                exit 1
            fi
            ;;
        --) shift; EXE_ARGS+=("$@"); break ;;
        *) echo "[run.sh, ERROR] Unknown option: $1"; show_help; exit 1 ;;
    esac
done

# Check executable presence
if [ ! -f "$EXECUTABLE" ]; then
    echo "[run.sh, ERROR] Executable not found at: $EXECUTABLE"
    echo "Try running './build.sh' first."
    exit 1
fi

# Export LD_PRELOAD if any preload libs specified
if [[ -n "$PRELOAD_LIBS" ]]; then
    export LD_PRELOAD="$PRELOAD_LIBS${LD_PRELOAD:+:$LD_PRELOAD}"
fi

# Run accordingly
if [ "$DEBUG" = true ]; then
    echo "[run.sh, INFO] Running with gdb..."
    gdb --args "$EXECUTABLE" "${EXE_ARGS[@]}"
elif [ "$VALGRIND" = true ]; then
    echo "[run.sh, INFO] Running with valgrind..."
    valgrind --leak-check=full --track-origins=yes "$EXECUTABLE" "${EXE_ARGS[@]}"
else
    echo "[run.sh, INFO] Running analysis_pipeline..."
    "$EXECUTABLE" "${EXE_ARGS[@]}"
fi
