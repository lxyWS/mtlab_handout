#!/bin/bash

# 此脚本运行时有引导，跟着引导即可完成运行
# Default values
DEFAULT_THREADS=1
DEFAULT_RUNS=1
BIN_DIR="."
IMAGE_DIR="../images"

list_binaries() {
    echo "Available binaries in $BIN_DIR:"
    find "$BIN_DIR" -maxdepth 1 -type f -executable -exec basename {} \; | grep -E '^(histogram|histo-)' | sort
}

list_images() {
    echo "Available .ppm images in $IMAGE_DIR:"
    find "$IMAGE_DIR" -name "*.ppm" -exec basename {} \; | sort
}

prompt_with_default() {
    local prompt="$1"
    local default="$2"
    read -p "$prompt (default: $default): " input
    echo "${input:-$default}"
}

# === Parse arguments ===
BINARY="$1"
THREADS="$2"
IMAGE_NAME="$3"
RUNS="$4"

if [ -z "$BINARY" ]; then
    list_binaries
    BINARY=$(prompt_with_default "Enter binary name" "histogram")
fi

if [ -z "$THREADS" ]; then
    THREADS=$(prompt_with_default "Number of threads" "$DEFAULT_THREADS")
fi

if [ -z "$IMAGE_NAME" ]; then
    list_images
    IMAGE_NAME=$(prompt_with_default "Enter PPM image name (e.g., earth.ppm)" "")
fi

if [ -z "$RUNS" ]; then
    RUNS=$(prompt_with_default "Number of runs" "$DEFAULT_RUNS")
fi

BIN_PATH="./$BINARY"
if [ ! -x "$BIN_PATH" ]; then
    echo "❌ Error: Binary '$BIN_PATH' not found or not executable."
    exit 1
fi

IMAGE_PATH="$IMAGE_DIR/$IMAGE_NAME"
if [ ! -f "$IMAGE_PATH" ]; then
    echo "❌ Error: Image '$IMAGE_PATH' not found."
    exit 1
fi

echo -e "\n Running $BINARY with $THREADS threads on $IMAGE_NAME ($RUNS time(s))..."
declare -a TIMES

for i in $(seq 1 $RUNS); do
    OUT_FILE="/tmp/${BINARY}_out_${i}.txt"
    echo -n "Run $i: "
    
    # Run and capture stdout
    OUTPUT=$($BIN_PATH "$IMAGE_PATH" "$OUT_FILE" "$THREADS" 2>&1)
    rm -f "$OUT_FILE"

    # Parse "Time: 1234567 ns"
    if [[ $OUTPUT =~ Time:\ ([0-9]+)\ ns ]]; then
        TIME_NS=${BASH_REMATCH[1]}
        TIMES+=($TIME_NS)  # ✅ Correct variable
        echo "${TIME_NS} ns"
    else
        echo "Failed to parse time."
        TIMES+=(0)
    fi
done

if [ ${#TIMES[@]} -eq 0 ]; then
    echo "❌ No valid timing data collected."
    exit 1
fi

echo -e "\n Raw times (ns): ${TIMES[@]}"

# Compute mean and std dev using awk
if [ "$RUNS" -gt 1 ]; then
    awk -v N="$RUNS" '
    BEGIN {
        sum = 0;
        for (i = 1; i <= N; i++) {
            val = ARGV[i];
            sum += val;
            data[i] = val;
        }
        mean = sum / N;
        sumsq = 0;
        for (i = 1; i <= N; i++) {
            sumsq += (data[i] - mean)^2;
        }
        stddev = sqrt(sumsq / (N - 1));
        printf "Mean: %.2f ns\n", mean;
        printf "Std Dev: %.2f ns\n", stddev;
    }' "${TIMES[@]}"
else
    echo "Mean: ${TIMES[0]} ns"
    echo "Std Dev: 0.00 ns (only one run)"
fi

echo -e "\n✅ Done."