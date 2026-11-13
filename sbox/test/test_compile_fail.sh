#!/bin/bash
# Run a compiler invocation and expect it to fail with a given message.
# The first argument is the expected message and the remaining arguments are
# the compiler invocation. Returns 0 if compilation fails with an error
# containing the message, and 1 otherwise.

expected="$1"
shift

output=$("$@" 2>&1)
if [ $? -eq 0 ]; then
    echo "FAIL: compilation succeeded but should have failed"
    exit 1
fi
if ! grep -qF -- "$expected" <<< "$output"; then
    echo "FAIL: compilation failed without the expected error: $expected"
    echo "$output"
    exit 1
fi
exit 0
