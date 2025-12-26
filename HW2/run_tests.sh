#!/bin/bash

TEST_DIR="./tests"
passed=0
failed=0
failed_tests=()

echo "Starting tests from $TEST_DIR..."
echo "------------------------------------------------"

for cmd_file in "$TEST_DIR"/*.command; do

    test_name=$(basename "$cmd_file" .command)
    expected_file="$TEST_DIR/$test_name.out"

    if [ ! -f "$expected_file" ]; then
        echo "⚠️  [SKIP] $test_name: No expected output found."
        continue
    fi

    # Run command
    command_line=$(cat "$cmd_file" | tr -d '\r')
    eval "$command_line" > "temp.YoursOut" 2>&1

    # --- ROBUST COMPARISON ---
    # 1. Strip \r from both files
    # 2. Ignore all whitespace (-w)
    # 3. Ignore blank lines (-B)
    diff -w -B <(tr -d '\r' < "temp.YoursOut") <(tr -d '\r' < "$expected_file") > /dev/null

    if [ $? -eq 0 ]; then
        echo "✅ [PASS] $test_name"
        ((passed++))
    else
        echo "❌ [FAIL] $test_name"
        echo "   Expected: [$(cat "$expected_file")]"
        echo "   Got:      [$(cat "temp.YoursOut")]"
        echo "   -------------------------"
        ((failed++))
        failed_tests+=("$test_name")
    fi

done

rm -f "temp.YoursOut"

echo ""
echo "Summary: $passed Passed, $failed Failed."

if [ ${#failed_tests[@]} -ne 0 ]; then
    echo "List of failed tests:"
    for name in "${failed_tests[@]}"; do
        echo " - $name"
    done
    exit 1
fi