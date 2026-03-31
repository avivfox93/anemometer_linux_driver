#!/bin/bash
#
# Anemometer Driver Integration Tests
#

set -e

DRIVER_NAME="anemometer"
SYSFS_DIR="/sys/class/anemometer"
CHRDEV="/dev/anemometer"
CONFIGFS_DIR="/sys/kernel/config/anemometer"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

test_count=0
pass_count=0
fail_count=0

run_test() {
    local name="$1"
    local cmd="$2"
    test_count=$((test_count + 1))
    
    echo -n "Test: $name... "
    if eval "$cmd" > /dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        pass_count=$((pass_count + 1))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        fail_count=$((fail_count + 1))
        return 1
    fi
}

echo "========================================"
echo "Anemometer Driver Integration Tests"
echo "========================================"
echo ""

# Test 1: Check if driver is loaded
run_test "Driver loaded" "lsmod | grep -q $DRIVER_NAME"

# Test 2: Check sysfs class exists
run_test "Sysfs class exists" "[ -d $SYSFS_DIR ]"

# Test 3: Check char device exists
run_test "Char device exists" "[ -c $CHRDEV ]"

# Test 4: Create sensor via char device
echo ""
echo "Creating test sensor 'test1' on GPIO 23..."
run_test "Create sensor via chardev" "echo 'add name=test1 gpio=23 window=5' > $CHRDEV"

# Test 5: Check sensor appears in sysfs
run_test "Sensor appears in sysfs" "[ -d $SYSFS_DIR/test1 ]"

# Test 6: Read wind speed
run_test "Read wind speed" "cat $SYSFS_DIR/test1/wind_speed_ms"

# Test 7: Read frequency
run_test "Read frequency" "cat $SYSFS_DIR/test1/frequency_hz"

# Test 8: Read calibration
run_test "Read calibration" "cat $SYSFS_DIR/test1/slope"

# Test 9: Modify calibration
run_test "Modify calibration" "echo '50 1000' > $SYSFS_DIR/test1/slope"

# Test 10: Modify window size
run_test "Modify window size" "echo 10 > $SYSFS_DIR/test1/window_size"

# Test 11: Check raw pulses
run_test "Read raw pulses" "cat $SYSFS_DIR/test1/raw_pulses"

# Test 12: Delete sensor
run_test "Delete sensor via chardev" "echo 'del test1' > $CHRDEV"

# Test 13: Verify sensor removed
run_test "Sensor removed from sysfs" "[ ! -d $SYSFS_DIR/test1 ]"

# Test 14: List command (if sensors exist)
if [ -d "$CONFIGFS_DIR" ]; then
    echo ""
    echo "ConfigFS tests..."
    run_test "ConfigFS directory exists" "[ -d $CONFIGFS_DIR ]"
    
    # Create via ConfigFS
    run_test "Create sensor via ConfigFS" "mkdir $CONFIGFS_DIR/test2"
    run_test "Set GPIO via ConfigFS" "echo 24 > $CONFIGFS_DIR/test2/gpio"
    run_test "Enable sensor via ConfigFS" "echo 1 > $CONFIGFS_DIR/test2/enabled"
    run_test "Sensor appears in sysfs" "[ -d $SYSFS_DIR/test2 ]"
    run_test "Delete sensor via ConfigFS" "rmdir $CONFIGFS_DIR/test2"
    run_test "Sensor removed from sysfs" "[ ! -d $SYSFS_DIR/test2 ]"
fi

echo ""
echo "========================================"
echo "Test Summary"
echo "========================================"
echo "Total: $test_count"
echo -e "${GREEN}Passed: $pass_count${NC}"
echo -e "${RED}Failed: $fail_count${NC}"
echo ""

if [ $fail_count -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi
