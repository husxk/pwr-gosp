#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "device.h"

#define TCP_PORT     5555

bool
tcp_is_conn_active(device_ctx_t*);

void
tcp_work(device_ctx_t*);
