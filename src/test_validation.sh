#!/bin/bash

# Allow passing number of runs as first argument; default to 10
# 使用方式是 ./test_validation.sh times    其中times是运行次数，默认是10
NUM_RUNS=${1:-10}

# Exit on any error
set -e

echo "🔍 Running validation with $NUM_RUNS repetitions per configuration..."

# Directories
BIN_DIR="."
IMAGE_DIR="../images"
REF_BIN="./histogram"  # reference serial implementation

# List of test binaries (must exist in src/)
TEST_BINS=("histo-private" "histo-lockfree" "histo-lock1" "histo-lock2")

# Check that reference binary exists
if [ ! -x "$REF_BIN" ]; then
    echo "❌ Error: Reference binary '$REF_BIN' not found or not executable."
    exit 1
fi

# Find all .ppm files
PPM_FILES=($(find "$IMAGE_DIR" -type f -name "*.ppm" | sort))
if [ ${#PPM_FILES[@]} -eq 0 ]; then
    echo "❌ Error: No .ppm files found in $IMAGE_DIR"
    exit 1
fi

echo "Found ${#PPM_FILES[@]} image(s): ${PPM_FILES[@]##*/}"

# For each image
for ppm in "${PPM_FILES[@]}"; do
    img_name=$(basename "$ppm")
    echo -e "\n🧪 Testing on image: $img_name"

    # Generate reference output (threads=1)
    ref_out="/tmp/ref_${img_name}.txt"
    $REF_BIN "$ppm" "$ref_out" 1

    # Test each binary
    for bin in "${TEST_BINS[@]}"; do
        if [ ! -x "$BIN_DIR/$bin" ]; then
            echo "⚠️  Skipping $bin: not found or not executable."
            continue
        fi

        echo "  → Testing $bin..."

        for threads in 1 2 4 8; do
            echo -n "    Threads=$threads: "
            for run in $(seq 1 $NUM_RUNS); do
                test_out="/tmp/test_${bin}_${threads}_${run}.txt"
                ./$bin "$ppm" "$test_out" $threads

                # Compare with reference
                if ! diff -w "$ref_out" "$test_out" > /dev/null; then
                    echo "❌ MISMATCH on run $run (threads=$threads)!"
                    echo "    Reference: $ref_out"
                    echo "    Test output: $test_out"
                    rm -f "$ref_out" "$test_out"
                    exit 1
                fi
                rm -f "$test_out"
            done
            echo "✅ OK ($NUM_RUNS/$NUM_RUNS runs match)"
        done
    done

    rm -f "$ref_out"
done

echo -e "\n🎉 All tests passed! All outputs match the serial reference."