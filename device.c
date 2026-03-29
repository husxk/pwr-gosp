#include <stdlib.h>

#include "device.h"
#include "tcp.h"
#include "debug.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "pico/cyw43_arch.h"

int
init_device(device_ctx_t** ctx)
{
  *ctx = malloc(sizeof(device_ctx_t));

  if(*ctx == NULL)
  {
    return -1;
  }

  memset(*ctx, 0, sizeof(device_ctx_t));

  dht22_init();

  return 0;
}

void
dht22_work(device_ctx_t* ctx, absolute_time_t now)
{
  if(ctx->dht_timeout > now)
    return;

  if(dht22_transact(&ctx->dht_data) == false)
  {
    log("DHT22 transaction failed!\n");
    ctx->dht_data_valid = false;
  }
  else
  {
    ctx->dht_data_valid = true;

    log("DHT22 RESULTS: humidity: %.2f, temperature: %.2f\n",
        ctx->dht_data.humidity, ctx->dht_data.temperature);
  }

  ctx->dht_timeout = make_timeout_time_ms(DHT_TRANSACT_TIMEOUT_MS);
  return;
}
