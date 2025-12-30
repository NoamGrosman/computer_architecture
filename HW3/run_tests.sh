#!/bin/bash

# Test runner script for HW3 dflow_calc
# Runs all tests in the tests/ folder and reports pass/fail status

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Counters
PASSED=0
FAILED=0
TOTAL=0

# Arrays to store failed tests
declare -a FAILED_TESTS

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Check if dflow_calc exists, if not try to build it
if [ ! -f "./dflow_calc" ]; then
    echo -e "${YELLOW}dflow_calc not found, attempting to build...${NC}"
    make
    if [ $? -ne 0 ]; then
        echo -e "${RED}Build failed! Please fix compilation errors first.${NC}"
        exit 1
    fi
    echo ""
fi

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}       Running HW3 Tests               ${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Get list of unique test numbers from .command files
TEST_NUMS=$(ls tests/*.command 2>/dev/null | sed 's/.*test\([0-9]*\)\.command/\1/' | sort -n | uniq)

if [ -z "$TEST_NUMS" ]; then
    echo -e "${RED}No test files found in tests/ directory!${NC}"
    exit 1
fi

for NUM in $TEST_NUMS; do
    TEST_NAME="test${NUM}"
    COMMAND_FILE="tests/${TEST_NAME}.command"
    EXPECTED_FILE="tests/${TEST_NAME}.out"
    
    # Check if required files exist
    if [ ! -f "$COMMAND_FILE" ]; then
        continue
    fi
    
    if [ ! -f "$EXPECTED_FILE" ]; then
        echo -e "${YELLOW}[SKIP] ${TEST_NAME}: Missing expected output file${NC}"
        continue
    fi
    
    TOTAL=$((TOTAL + 1))
    
    # Read and execute the command from .command file
    COMMAND=$(cat "$COMMAND_FILE" | tr -d '\r')  # Remove Windows line endings if present
    
    # Run the test and capture output
    OUTPUT=$(eval "$COMMAND" 2>&1)
    
    # Compare output with expected
    EXPECTED=$(cat "$EXPECTED_FILE" | tr -d '\r')
    
    if [ "$OUTPUT" = "$EXPECTED" ]; then
        echo -e "${GREEN}[PASS]${NC} ${TEST_NAME}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}[FAIL]${NC} ${TEST_NAME}"
        FAILED=$((FAILED + 1))
        FAILED_TESTS+=("$TEST_NAME")
        
        # Optionally save the actual output for debugging
        echo "$OUTPUT" > "tests/${TEST_NAME}.YoursOut"
    fi
done

# Print summary
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}            Summary                    ${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "Total tests: ${TOTAL}"
echo -e "${GREEN}Passed: ${PASSED}${NC}"
echo -e "${RED}Failed: ${FAILED}${NC}"

if [ ${#FAILED_TESTS[@]} -gt 0 ]; then
    echo ""
    echo -e "${RED}Failed tests:${NC}"
    for TEST in "${FAILED_TESTS[@]}"; do
        echo "  - $TEST"
    done
    echo ""
    echo -e "${YELLOW}Tip: Check tests/<testname>.YoursOut for your output vs tests/<testname>.out for expected${NC}"
fi

# Calculate percentage
if [ $TOTAL -gt 0 ]; then
    PERCENT=$((PASSED * 100 / TOTAL))
    echo ""
    echo -e "${BLUE}Score: ${PERCENT}%${NC}"
fi

# Exit with failure if any tests failed
if [ $FAILED -gt 0 ]; then
    exit 1
else
    echo ""
    echo -e "${GREEN}All tests passed! 🎉${NC}"
    exit 0
fi
