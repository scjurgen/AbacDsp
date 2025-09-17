#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
ORANGE='\033[0;33m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE} Unit Test Coverage Checker${NC}"
echo -e "${BLUE} (Unified *_test.cpp pattern)${NC}"
echo -e "${BLUE}========================================${NC}"

if [ ! -d "./src" ]; then
echo -e "${RED}Error: ./src directory not found${NC}"
exit 1
fi

if [ ! -d "./test" ]; then
echo -e "${RED}Error: ./test directory not found${NC}"
exit 1
fi

DEBUG=false
if [[ "$1" == "-d" || "$1" == "--debug" ]]; then
DEBUG=true
echo -e "${CYAN}DEBUG MODE ENABLED${NC}"
fi

analyze_test_file() {
    local test_file="$1"
    local tests_found=0
    local expects_found=0
    local test_fixtures=0
    local instantiate_found=0

    if [ -f "$test_file" ]; then
        tests_found=$(grep -c "^\s*TEST\s*(" "$test_file" 2>/dev/null)
        if [ -z "$tests_found" ]; then tests_found=0; fi

        test_fixtures=$(grep -c "^\s*TEST_F\s*(" "$test_file" 2>/dev/null)
        if [ -z "$test_fixtures" ]; then test_fixtures=0; fi

        instantiate_found=$(grep -c "^\s*INSTANTIATE_TEST_SUITE_P\s*(" "$test_file" 2>/dev/null)
        if [ -z "$instantiate_found" ]; then instantiate_found=0; fi

        expects_found=$(grep -c "EXPECT_" "$test_file" 2>/dev/null)
        if [ -z "$expects_found" ]; then expects_found=0; fi

        tests_found=$((tests_found + test_fixtures + instantiate_found))
    fi

    echo "$tests_found:$expects_found"
}

echo -e "${BLUE}Scanning files...${NC}"

missing_count=0
total_count=0
missing_tests=()
empty_test_files=()
total_test_count=0
total_expect_count=0

for header in $(find ./src -name "*.h" | sort); do
class_name=$(basename "$header" .h)
total_count=$((total_count + 1))

if [ "$DEBUG" = true ]; then
echo -e "${CYAN}DEBUG: Processing header '$header' -> class '$class_name'${NC}"
fi

test_file_pattern="${class_name}_test.cpp"
found_test_file=$(find ./test -name "$test_file_pattern" 2>/dev/null | head -1)

if [ -n "$found_test_file" ]; then
if [ "$DEBUG" = true ]; then
echo -e "${CYAN}DEBUG: Found test file: $found_test_file${NC}"
fi

test_stats=$(analyze_test_file "$found_test_file")
IFS=':' read -r test_count expect_count <<< "$test_stats"

total_test_count=$((total_test_count + test_count))
total_expect_count=$((total_expect_count + expect_count))

if [ "$test_count" -eq 0 ]; then
echo -e "${ORANGE}⚠ EMPTY TEST:${NC} $class_name (no TEST/TEST_F/INSTANTIATE_TEST_SUITE_P found)"
empty_test_files+=("$class_name:$found_test_file")
else
echo -e "${GREEN}✓ COVERED:${NC} $class_name (${test_count} tests, ${expect_count} expectations)"
fi
else
echo -e "${RED}✗ MISSING TEST:${NC} $class_name"
header_dir=$(dirname "$header" | sed 's|\\./src/includes/||' | sed 's|\\./src/||')
echo -e " ${YELLOW}Expected: ./test/$header_dir/${class_name}_test.cpp${NC}"
echo
missing_count=$((missing_count + 1))
missing_tests+=("$header:$class_name:$header_dir")
fi

done

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE} SUMMARY${NC}"
echo -e "${BLUE}========================================${NC}"

if [ $total_count -gt 0 ]; then
coverage_percent=$(( (total_count - missing_count) * 100 / total_count ))
else
coverage_percent=0
fi

echo -e "Total header files: ${BLUE}$total_count${NC}"
echo -e "Files with tests: ${GREEN}$((total_count - missing_count))${NC}"
echo -e "Missing tests: ${RED}$missing_count${NC}"
echo -e "Empty test files: ${ORANGE}${#empty_test_files[@]}${NC}"
echo -e "Coverage: ${YELLOW}$coverage_percent%${NC}"
echo -e "Total tests found: ${CYAN}$total_test_count${NC}"
echo -e "Total expectations: ${CYAN}$total_expect_count${NC}"

if [ $missing_count -eq 0 ] && [ ${#empty_test_files[@]} -eq 0 ]; then
echo -e "${GREEN}🎉 Perfect! All header files have tests with actual implementations!${NC}"
elif [ $missing_count -eq 0 ]; then
echo -e "${ORANGE}⚠ All files have test files, but some are empty${NC}"
fi

if [ $missing_count -gt 0 ]; then
echo
echo -e "${RED}Missing tests for:${NC}"
for entry in "${missing_tests[@]}"; do
IFS=':' read -r header_file class_name header_dir <<< "$entry"
echo -e "${RED} • $class_name${NC} ($header_file)"
done
echo
echo -e "${YELLOW}💡 Quick commands to create missing test files:${NC}"
for entry in "${missing_tests[@]}"; do
IFS=':' read -r header_file class_name header_dir <<< "$entry"
test_dir="./test/$header_dir"
test_file="$test_dir/${class_name}_test.cpp"
echo -e "${BLUE}mkdir -p $test_dir && touch $test_file${NC}"
done
fi

if [ ${#empty_test_files[@]} -gt 0 ]; then
echo
echo -e "${ORANGE}Empty test files (no TEST/TEST_F/INSTANTIATE_TEST_SUITE_P found):${NC}"
for entry in "${empty_test_files[@]}"; do
IFS=':' read -r class_name test_file <<< "$entry"
echo -e "${ORANGE} • $class_name${NC} ($test_file)"
done
fi

if [ $missing_count -gt 0 ] || [ ${#empty_test_files[@]} -gt 0 ]; then
exit 1
else
exit 0
fi