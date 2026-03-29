#include "tcp.h"
#include "debug.h"
#include "protocol.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "pico/cyw43_arch.h"


static err_t
tcp_server_write(struct tcp_pcb* tpcb, const uint8_t *data, uint16_t size)
{
  return tcp_write(tpcb, data, size, TCP_WRITE_FLAG_COPY);
}

static void
send_nack(struct tcp_pcb* tpcb)
{
  char nack[NACK_PACKET_LENGTH];
  uint16_t packet_len = htons(NACK_PACKET_LENGTH);
  memcpy(&nack[COMMON_PACKET_LENGTH_POS], &packet_len, COMMON_PACKET_LENGTH_SIZE);

  nack[COMMON_PACKET_TYPE_POS] = NACK_TYPE;

  tcp_server_write(tpcb, nack, NACK_PACKET_LENGTH);
}

static bool
create_reply_dht22(device_ctx_t* ctx, char* buf, int size, int *packet_size_)
{
  if(size < REPLY_DHT22_PACKET_LENGTH)
  {
    DEBUG("TCP: Invalid use of %s - buffer too small.\n", __func__);
    return false;
  }

  int packet_size = REPLY_DHT22_PACKET_LENGTH;

  if(ctx->dht_data_valid)
  {
    uint16_t packet_len = htons(REPLY_DHT22_PACKET_LENGTH);
    memcpy(&buf[COMMON_PACKET_LENGTH_POS], &packet_len, COMMON_PACKET_LENGTH_SIZE);

    buf[COMMON_PACKET_TYPE_POS]   = REPLY_DHT22_TYPE;
    buf[REPLY_DHT22_SUCCESS_POS]  = REPLY_DHT22_SUCCESS;

    int temp = ctx->dht_data.temperature * 100;
    int hum  = ctx->dht_data.humidity    * 100;

    temp = htonl(temp);
    memcpy(&buf[REPLY_DHT22_TEMPERATURE_POS],
                &temp,
                REPLY_DHT22_TEMPERATURE_SIZE);

    hum = htonl(hum);
    memcpy(&buf[REPLY_DHT22_HUMIDITY_POS],
                &hum,
                REPLY_DHT22_HUMIDITY_SIZE);

  }
  else
  {
    uint16_t packet_len = htons(REPLY_DHT22_PACKET_LENGTH_NO_SUCCESS);
    memcpy(&buf[COMMON_PACKET_LENGTH_POS], &packet_len, COMMON_PACKET_LENGTH_SIZE);

    buf[COMMON_PACKET_TYPE_POS]   = REPLY_DHT22_TYPE;
    buf[REPLY_DHT22_SUCCESS_POS]  = REPLY_DHT22_NO_SUCCESS;
    packet_size                   = REPLY_DHT22_PACKET_LENGTH_NO_SUCCESS;
  }

  *packet_size_ = packet_size;

  return true;
}

static void
packet_handler(device_ctx_t* ctx, struct tcp_pcb* tpcb)
{
  const int total_len = ctx->tcp.recv_len;
  uint8_t* buffer     = ctx->tcp.recv_buffer;

  if(total_len < COMMON_PACKET_LENGTH)
  {
    /* Send NACK, as the packet is invalid. */
    /* This case may be an issue, if packet get segmented */
    send_nack(tpcb);
    DEBUG("TCP: packet invalid - too short!\n");
    return;
  }

  unsigned short packet_len;

  memcpy(&packet_len, buffer, COMMON_PACKET_LENGTH_SIZE);
  packet_len = ntohs(packet_len);

  const unsigned int packet_type = buffer[COMMON_PACKET_TYPE_POS];

  switch(packet_type)
  {
    case GET_DHT22_TYPE:
      {
        if(packet_len != GET_DHT22_PACKET_LENGTH)
        {
          /* send NACK as it is invalid packet */
          send_nack(tpcb);
          DEBUG("TCP: packet invalid! invalid length of GET_DHT22_TYPE!\n");
          return;
        }

        char buf[REPLY_DHT22_PACKET_LENGTH];
        int size = 0;

        if(!create_reply_dht22(ctx, buf, sizeof(buf), &size))
        {
          DEBUG("TCP: Fail at creating reply dht22 packet!\n");
          return;
        }

        tcp_server_write(tpcb, buf, size);

      } break;

    default:
      {
        /* We did not recognise packet */
        send_nack(tpcb);
      } break;
  }
}

bool
tcp_is_conn_active(device_ctx_t* ctx)
{
  return !(ctx->tcp.server_pcb == NULL ||
           ctx->tcp.server_pcb->state == CLOSED);
}

static err_t
tcp_server_close(device_ctx_t* ctx)
{
  err_t err = ERR_OK;

  if (ctx->tcp.client_pcb != NULL)
  {
    tcp_arg(ctx->tcp.client_pcb, NULL);
    tcp_recv(ctx->tcp.client_pcb, NULL);
    tcp_err(ctx->tcp.client_pcb, NULL);
    err = tcp_close(ctx->tcp.client_pcb);

    if (err != ERR_OK)
    {
      log("TCP: Close failed %d, calling abort\n", err);
      tcp_abort(ctx->tcp.client_pcb);
      err = ERR_ABRT;
    }

    ctx->tcp.client_pcb = NULL;
  }

  if (ctx->tcp.server_pcb != NULL)
  {
    tcp_arg(ctx->tcp.server_pcb, NULL);
    tcp_close(ctx->tcp.server_pcb);
    ctx->tcp.server_pcb = NULL;
  }

  return err;
}

static void
tcp_server_recv_(device_ctx_t *ctx, struct tcp_pcb* tpcb, struct pbuf* p)
{
  DEBUG("TCP: tcp_server_recv %d\n", p->tot_len);

  if (p->tot_len == 0)
    return;

  /*
   * Receive the buffer
   *
   * TODO: We should probably call pbuf_copy_partial in a loop
   *       to make sure we receive everything.
   */
  const uint16_t buffer_left = TCP_BUF_SIZE;
  ctx->tcp.recv_len =
    pbuf_copy_partial(p, ctx->tcp.recv_buffer,
                      p->tot_len > buffer_left ? buffer_left : p->tot_len, 0);

  tcp_recved(tpcb, p->tot_len);
}

static err_t
tcp_server_recv(void* ctx_, struct tcp_pcb* tpcb, struct pbuf* p, err_t err)
{
  device_ctx_t *ctx = (device_ctx_t*)ctx_;

  if (!p)
  {
    tcp_server_close(ctx);
    return err;
  }

  tcp_server_recv_(ctx, tpcb, p);

  DEBUG("tcp_server_recv:\n");
  hexdump(ctx->tcp.recv_buffer, ctx->tcp.recv_len);

  pbuf_free(p);

  packet_handler(ctx, tpcb);

  return ERR_OK;
}

static void
tcp_server_err(void* ctx_, err_t err)
{
  device_ctx_t *ctx = (device_ctx_t*)ctx_;

  DEBUG("tcp_client_err_fn %d\n", err);
  tcp_server_close(ctx);
}

static err_t
tcp_server_accept(void* ctx_, struct tcp_pcb* client_pcb, err_t err)
{
  device_ctx_t *ctx = (device_ctx_t*)ctx_;

  if (err != ERR_OK || client_pcb == NULL)
  {
    DEBUG("TCP: Failure in accept\n");
    tcp_server_close(ctx);
    return ERR_VAL;
  }

  log("TCP: Client connected\n");

  client_pcb->so_options |= SOF_KEEPALIVE;
  client_pcb->keep_intvl = 4000; /* 4 seconds */

  ctx->tcp.client_pcb = client_pcb;

  tcp_arg(client_pcb, ctx);
  tcp_recv(client_pcb, tcp_server_recv);
  tcp_err(client_pcb, tcp_server_err);

  return ERR_OK;
}

static bool
tcp_server_open(void* ctx_)
{
  device_ctx_t *ctx = (device_ctx_t*)ctx_;

  log("TCP: Starting server at %s on port %u\n",
      ip4addr_ntoa(netif_ip4_addr(netif_list)),
      TCP_PORT);

  struct tcp_pcb* pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
  if (!pcb)
  {
    DEBUG("TCP: Failed to create pcb\n");
    return false;
  }

  err_t err = tcp_bind(pcb, NULL, TCP_PORT);
  if (err)
  {
    DEBUG("TCP: Failed to bind to port %u\n", TCP_PORT);
    return false;
  }

  if(ctx->tcp.server_pcb != NULL)
    tcp_server_close(ctx);

  ctx->tcp.server_pcb = tcp_listen_with_backlog(pcb, 1);
  if (!ctx->tcp.server_pcb)
  {
    if (pcb)
    {
      tcp_close(pcb);
    }

    return false;
  }

  tcp_arg(ctx->tcp.server_pcb, ctx);
  tcp_accept(ctx->tcp.server_pcb, tcp_server_accept);

  return true;
}

static void
tcp_server_reset(device_ctx_t* ctx)
{
  if (tcp_server_open(ctx) == false)
  {
    tcp_server_close(ctx);
  }
}

void
tcp_work(device_ctx_t* ctx)
{
  cyw43_arch_poll();

  // If disconnected, reset and setup listening
  if (tcp_is_conn_active(ctx) == false)
    tcp_server_reset(ctx);
}
