#!/bin/bash
# Run the given command and expect it to fail.
# Used for compile-failure tests: passes all arguments as a compiler invocation,
# returns 0 if compilation fails (expected), 1 if it succeeds (unexpected).

"$@" 2>/dev/null
if [ $? -ne 0 ]; then
    exit 0
else
    echo "FAIL: compilation succeeded but should have failed"
    exit 1
fi
