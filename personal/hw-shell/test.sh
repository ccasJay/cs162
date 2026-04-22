#!/bin/bash

set -u

fail() {
    echo "FAIL: $1"
    exit 1
}

assert_ge_ms() {
    local actual_ms="$1"
    local expected_ms="$2"
    local msg="$3"
    if (( actual_ms < expected_ms )); then
        fail "$msg (expected >= ${expected_ms}ms, got ${actual_ms}ms)"
    fi
}

assert_lt_ms() {
    local actual_ms="$1"
    local expected_ms="$2"
    local msg="$3"
    if (( actual_ms >= expected_ms )); then
        fail "$msg (expected < ${expected_ms}ms, got ${actual_ms}ms)"
    fi
}

assert_contains() {
    local haystack="$1"
    local needle="$2"
    local msg="$3"
    if [[ "$haystack" != *"$needle"* ]]; then
        fail "$msg"
    fi
}

echo "Building shell"
make clean >/dev/null 2>&1 || fail "make clean failed"
make >/dev/null 2>&1 || fail "make failed"

echo "Testing built-in command: pwd"
SHELL_PWD=$(echo "pwd" | ./shell)
if [[ "$SHELL_PWD" != "$PWD" ]]; then
    printf "%s\n" "Expected: ${PWD}" "Received: ${SHELL_PWD}"
    fail "built-in command pwd failed"
fi
echo "Passed: built-in command"

echo "Testing basic external command"
SHELL_ECHO=$(echo "/bin/echo hello" | ./shell)
if [[ "$SHELL_ECHO" != "hello" ]]; then
    printf "%s\n" "Expected: hello" "Received: ${SHELL_ECHO}"
    fail "basic external command failed"
fi
echo "Passed: basic external command"

echo "Testing background launch does not block"
start_ns=$(date +%s%N)
bg_output=$(printf "/bin/sleep 1 > /dev/null &\n/bin/echo done\n" | ./shell)
end_ns=$(date +%s%N)
elapsed_ms=$(( (end_ns - start_ns) / 1000000 ))
assert_lt_ms "$elapsed_ms" 700 "background command blocked the shell"
assert_contains "$bg_output" "done" "shell did not continue after background launch"
echo "Passed: background launch"

echo "Testing wait built-in blocks for background jobs"
start_ns=$(date +%s%N)
wait_output=$(printf "/bin/sleep 1 &\nwait\n/bin/echo afterwait\n" | ./shell)
end_ns=$(date +%s%N)
elapsed_ms=$(( (end_ns - start_ns) / 1000000 ))
assert_ge_ms "$elapsed_ms" 900 "wait did not block for background job"
assert_contains "$wait_output" "afterwait" "wait command flow failed"
echo "Passed: wait built-in"

echo "Testing fg without pid uses most recent process"
start_ns=$(date +%s%N)
fg_output=$(printf "/bin/sleep 1 &\nfg\n/bin/echo afterfg\n" | ./shell)
end_ns=$(date +%s%N)
elapsed_ms=$(( (end_ns - start_ns) / 1000000 ))
assert_ge_ms "$elapsed_ms" 900 "fg did not bring the job to foreground"
assert_contains "$fg_output" "afterfg" "fg command flow failed"
echo "Passed: fg built-in"

echo "Testing bg resumes a stopped process"
set +e
bg_resume_output=$(timeout 4s ./shell <<'EOF'
/bin/sh -c 'kill -STOP $$; /bin/sleep 1' &
/bin/sleep 0.1
bg
wait
/bin/echo afterbg
EOF
)
bg_resume_status=$?
set -e
if (( bg_resume_status == 124 )); then
    fail "bg test timed out; stopped job was likely not resumed"
fi
if (( bg_resume_status != 0 )); then
    fail "bg test command returned non-zero status ${bg_resume_status}"
fi
assert_contains "$bg_resume_output" "afterbg" "bg did not resume stopped process"
echo "Passed: bg built-in"

echo "All tests passed"

