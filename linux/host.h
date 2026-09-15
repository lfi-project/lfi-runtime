#pragma once

#include "linux.h"

#include <stdio.h>

int
host_err(int err);

ssize_t
host_getrandom(void *buf, size_t size, unsigned int flags);
