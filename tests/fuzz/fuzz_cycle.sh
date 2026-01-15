#!/bin/bash
# fuzz_cycle.sh - Cycle through all fuzz targets
# Usage: ./fuzz_cycle.sh [DURATION] [INSTANCE_NAME] [MODE]
#   DURATION: Time in seconds per target (default: 600)
#   INSTANCE_NAME: Unique name for this fuzzer instance (e.g., fuzzer01)
#   MODE: -M (Master) or -S (Slave) (default: -S, or "default" for single mode)

DURATION=${1:-600}
INSTANCE_NAME=${2:-"default"}
MODE_FLAG=${3:-"-S"} # Default to slave mode if parallel, or single if not specified in AFL_FLAGS

# Targets associated with their corpus and output directories
TARGETS=("fuzz-http" "fuzz-json" "fuzz-lps" "fuzz-sdk-config" "fuzz-semantic")

# Trap Ctrl+C (SIGINT) to ensure we exit the loop
trap "echo '>> Caught SIGINT. Exiting cycle...'; exit 130" INT TERM


echo ">> Starting fuzzing cycle. Ctrl+C to stop."
echo ">> Instance: $INSTANCE_NAME ($MODE_FLAG)"
echo ">> Time per target: $DURATION seconds"

# Export AFL parallel flags if not default
if [ "$INSTANCE_NAME" != "default" ]; then
    AFL_FLAGS="$MODE_FLAG $INSTANCE_NAME"
fi

while true; do
  for target in "${TARGETS[@]}"; do
    echo "======================================================================"
    echo ">> Running execution for target: $target"
    echo ">> Instance: $INSTANCE_NAME" 
    echo ">> Time: $(date)"
    echo "======================================================================"
    
    export AFL_NO_UI=1
    export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1
    
    # We pass the flags via env var AFL_IMPORT_FLAGS (custom convention)
    # or rely on the Makefile to pick up AFL_FLAGS if we modify it.
    # Actually, simpler: we modify the Makefile to respect AFL_EXTRA_ARGS
    
    timeout -s INT "$DURATION" make "$target" AFL_EXTRA_ARGS="$AFL_FLAGS"
    
    EXIT_CODE=$?
    
    if [ $EXIT_CODE -eq 124 ]; then
      echo ">> Cycle complete for $target (Timed out as expected)."
    elif [ $EXIT_CODE -eq 130 ]; then
      echo ">> Cycle Aborted by User."
      exit 0
    else
      echo ">> Warning: $target exited with code $EXIT_CODE"
    fi
    
    sleep 2
  done
done
