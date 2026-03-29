#pragma once

#include "pico/stdlib.h"

#include "dht22.h"

#define TCP_BUF_SIZE 64

typedef struct
{
  struct tcp_pcb* server_pcb;
  struct tcp_pcb* client_pcb;
  uint8_t         recv_buffer[TCP_BUF_SIZE];
  uint16_t        recv_len;
} tcp_ctx_t;

typedef struct
{
  tcp_ctx_t       tcp;
  absolute_time_t dht_timeout;
  dht22_data_t    dht_data;
  bool            dht_data_valid;
} device_ctx_t;

int
init_device(device_ctx_t**);

void
dht22_work(device_ctx_t*, absolute_time_t);

void
tcp_work(device_ctx_t*);
