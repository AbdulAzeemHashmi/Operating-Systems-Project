#!/bin/bash

# ─────────────────────────────────────
# VARIABLES
# ─────────────────────────────────────
INPUT_DIR=""
OUTPUT_DIR=""
NUM_THREADS="4"
QUEUE_SIZE="10"
DISPATCHER_PID=""
START_TIME=""

# ─────────────────────────────────────
# FUNCTION 1: Print Help
# ─────────────────────────────────────
print_help(){
    echo "========================================"
    echo "   Web Clickstream Pipeline"
    echo "========================================"
    echo "Usage: ./run.sh -i input_dir -o output_dir -n threads"
    echo ""
    echo "Options:"
    echo "  -i  Input directory  (required)"
    echo "  -o  Output directory (required)"
    echo "  -n  Number of threads (default: 4)"
    echo "  -c  Clean build files"
    echo "  -h  Show this help"
    echo ""
    echo "Example:"
    echo "  ./run.sh -i data/ -o output/ -n 4"
    echo "========================================"
}

# ─────────────────────────────────────
# FUNCTION 2: Check Dependencies
# ─────────────────────────────────────
check_deps(){
    echo "[*] Checking dependencies..."

    # Check gcc installed
    if ! command -v gcc &> /dev/null; then
        echo "[ERROR] gcc not found! Install with: sudo apt install gcc"
        exit 1
    fi
    echo "[OK] gcc found!"

    # Check make installed
    if ! command -v make &> /dev/null; then
        echo "[ERROR] make not found! Install with: sudo apt install make"
        exit 1
    fi
    echo "[OK] make found!"

    # Check input directory exists
    if [ -z "$INPUT_DIR" ]; then
        echo "[ERROR] Input directory not specified! Use -i flag"
        exit 1
    fi

    if [ ! -d "$INPUT_DIR" ]; then
        echo "[ERROR] Input directory '$INPUT_DIR' does not exist!"
        exit 1
    fi

    # Check at least one csv file exists
    CSV_COUNT=$(ls "$INPUT_DIR"/*.csv 2>/dev/null | wc -l)
    if [ "$CSV_COUNT" -eq 0 ]; then
        echo "[ERROR] No CSV files found in '$INPUT_DIR'!"
        exit 1
    fi
    echo "[OK] Found $CSV_COUNT CSV file(s) in $INPUT_DIR"
}

# ─────────────────────────────────────
# FUNCTION 3: Build Project
# ─────────────────────────────────────
build_project(){
    echo "[*] Building project..."

    make clean > /dev/null 2>&1

    if ! make; then
        echo "[ERROR] Build failed! Fix errors above and try again."
        exit 1
    fi
    echo "[OK] Build successful!"
}

# ─────────────────────────────────────
# FUNCTION 4: Print Summary
# ─────────────────────────────────────
print_summary(){
    END_TIME=$(date +%s)
    # arithmetic expansion!
    RUNTIME=$(( END_TIME - START_TIME ))

    echo ""
    echo "========================================"
    echo "         PIPELINE SUMMARY"
    echo "========================================"
    echo "Total Runtime: ${RUNTIME} seconds"
    echo "Threads Used:  $NUM_THREADS"
    echo "Input Dir:     $INPUT_DIR"
    echo "Output Dir:    $OUTPUT_DIR"

    if [ -f "$OUTPUT_DIR/report.csv" ]; then
        RECORDS=$(( $(wc -l < "$OUTPUT_DIR/report.csv") - 1 ))
        echo "Users Processed: $RECORDS"
        echo ""
        echo "Report Files:"
        # for loop iterating over output files
        for f in "$OUTPUT_DIR"/report.txt "$OUTPUT_DIR"/report.csv; do
            if [ -f "$f" ]; then
                echo "  → $f ($(wc -c < "$f") bytes)"
            else
                echo "  → $f [MISSING]"
            fi
        done
    else
        echo "[WARNING] report.csv not found!"
    fi	
    echo "========================================"
}

# ─────────────────────────────────────
# CLEANUP FUNCTION
# Runs on EXIT/INT/TERM automatically!
# ─────────────────────────────────────
cleanup(){
    echo ""
    echo "[*] Cleaning up..."

    # Kill dispatcher if still running
    if [ -n "$DISPATCHER_PID" ]; then
        kill "$DISPATCHER_PID" 2>/dev/null
    fi

    # Remove pid file
    rm -f dispatcher.pid

    echo "[OK] Cleanup done!"
}

# ─────────────────────────────────────
# INSTALL TRAP
# Runs cleanup on exit or Ctrl+C!
# ─────────────────────────────────────
trap cleanup EXIT INT TERM

# ─────────────────────────────────────
# PARSE ARGUMENTS using getopts
# ─────────────────────────────────────
while getopts "i:o:n:ch" opt; do
    case $opt in
        i) INPUT_DIR="$OPTARG" ;;
        o) OUTPUT_DIR="$OPTARG" ;;
        n) NUM_THREADS="$OPTARG" ;;
        c)
            echo "[*] Cleaning build files..."
            make clean
            echo "[OK] Done!"
            exit 0
            ;;
        h)
            print_help
            exit 0
            ;;
        *)
            echo "[ERROR] Unknown option!"
            print_help
            exit 1
            ;;
    esac
done

# ─────────────────────────────────────
# SET DEFAULT OUTPUT DIR
# ─────────────────────────────────────
if [ -z "$OUTPUT_DIR" ]; then
    OUTPUT_DIR="output/"
fi

# ─────────────────────────────────────
# RUN ALL STEPS
# ─────────────────────────────────────

# Step 1: Check everything
check_deps

# Step 2: Build
build_project

# Step 3: Create needed folders
mkdir -p logs
mkdir -p "$OUTPUT_DIR"
echo "[OK] Folders ready!"

# Step 4: Record start time
START_TIME=$(date +%s)

# Step 5: Launch dispatcher
echo "[*] Starting pipeline..."
echo "[*] Input:   $INPUT_DIR"
echo "[*] Output:  $OUTPUT_DIR"
echo "[*] Threads: $NUM_THREADS"
echo ""

./dispatcher "$INPUT_DIR" "$OUTPUT_DIR" "$NUM_THREADS" "$QUEUE_SIZE" &
DISPATCHER_PID=$!

# Save PID to file
echo "$DISPATCHER_PID" > dispatcher.pid
echo "[*] Dispatcher PID: $DISPATCHER_PID"

# Step 6: Wait for dispatcher to finish
wait "$DISPATCHER_PID"
EXIT_STATUS=$?

echo "[*] Pipeline finished with status: $EXIT_STATUS"

# Step 7: Print summary
print_summary

exit $EXIT_STATUS
