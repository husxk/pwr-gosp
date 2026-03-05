#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <ctype.h>

#include "../protocol.h"

int connect_to_server(int *sockfd, struct sockaddr_in *server_addr,
                      const char* ip, int port)
{
  if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
      perror("Socket creation failed");
      return 1;
  }

  server_addr->sin_family = AF_INET;
  server_addr->sin_port = htons(port);
  
  if (inet_pton(AF_INET, ip, &server_addr->sin_addr) <= 0)
  {
      perror("Invalid address");
      close(*sockfd);
      return 1;
  }

  if (connect(*sockfd, (struct sockaddr*)server_addr, sizeof(*server_addr)) < 0)
  {
      perror("Connection failed");
      close(*sockfd);
      return 1;
  }

  return 0;
}

static void
hexdump(const char* data, size_t len)
{
  int c;
  size_t p = 0;

  char dumpdata[16 * 3];
  char dumpdata2[17];

  memset(dumpdata,  '\0', 16 * 3);
  memset(dumpdata2, '\0', 17);

  const char hexChars[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

  fprintf(stderr, "hexdump len %lu\n", len);

  while(p < len)
  {
    c = data[p];

    const int index  = p % 16;
    const int offset = 3 * index;

    dumpdata[offset]     = hexChars[(c >> 4) & 0x0F];
    dumpdata[offset + 1] = hexChars[c & 0x0F];
    dumpdata[offset + 2] = ' ';
    dumpdata2[index]     = (isprint(c) ? c : '.');

    if ((index == 15) || (p == len - 1))
    {
      dumpdata[16 * 3 - 1] = '\0';
      dumpdata2[16]        = '\0';
      fprintf(stderr, "%-48s   %s\n", dumpdata, dumpdata2);

      memset(dumpdata,  '\0', 16 * 3);
      memset(dumpdata2, '\0', 17);
    }

    p++;
  }
}

static void
send_request(int fd)
{
  char req[GET_DHT22_PACKET_LENGTH];

  uint16_t length = htons(GET_DHT22_PACKET_LENGTH);
//  req[COMMON_PACKET_LENGTH_POS] = htons(GET_DHT22_PACKET_LENGTH);
  memcpy(&req[COMMON_PACKET_LENGTH_POS], &length, sizeof(length));

  req[COMMON_PACKET_TYPE_POS] = GET_DHT22_TYPE;

  send(fd, req, GET_DHT22_PACKET_LENGTH, 0);
}

static void
get_results(int fd, char* buffer, int buffer_size)
{
  const int n = read(fd, buffer, buffer_size);
  printf("Received: %u\n", n);
  hexdump(buffer, n);

  if(n < 2)
  {
    printf("Invalid packet received\n");
  }

  uint16_t length;
  memcpy(&length, &buffer[COMMON_PACKET_LENGTH_POS], COMMON_PACKET_LENGTH_SIZE);
  length = ntohs(length);

  if(length < COMMON_PACKET_LENGTH)
  {
    printf("Invalid packet received, length < 3\n");
    printf("Received: %u\n", length);
    return;
  }

  if (buffer[COMMON_PACKET_TYPE_POS] == NACK_TYPE)
  {
    printf("Got NACK!\n");
    return;
  }

  if (buffer[COMMON_PACKET_TYPE_POS] != REPLY_DHT22_TYPE)
  {
    printf("Invalid packet type received: %02X\n", buffer[COMMON_PACKET_TYPE_POS]);
    return;
  }

  if (length < REPLY_DHT22_PACKET_LENGTH_NO_SUCCESS)
  {
    printf("Incomplete packet type received\n");
    return;
  }

  const bool success = buffer[REPLY_DHT22_SUCCESS_POS];
  if(success == 0)
  {
    printf("Firmware error! DHT22 transaction error!\n");
    return;
  }

  if (length != REPLY_DHT22_PACKET_LENGTH)
  {
    printf("Incomplete packet type received\n");
    return;
  }
 
  uint32_t raw_temp_network;
  memcpy(&raw_temp_network,
         &buffer[REPLY_DHT22_TEMPERATURE_POS],
         REPLY_DHT22_TEMPERATURE_SIZE);

  const uint32_t raw_temp = ntohl(raw_temp_network);
  const float temp = (float) raw_temp / 100;

  uint32_t raw_hum_network;
  memcpy(&raw_hum_network,
         &buffer[REPLY_DHT22_HUMIDITY_POS],
         REPLY_DHT22_HUMIDITY_SIZE);


  const uint32_t raw_hum = ntohl(raw_hum_network);
  const float hum        = (float) raw_hum / 100;

  printf("Read temperature: %f, humidity: %f\n", temp, hum);
  return;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);

    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[1024];

    if (connect_to_server(&sockfd, &server_addr, ip, port) < 0)
    {
      printf("Connect failed!\n");
      return -1;
    }

    printf("Connected to %s:%d\n", ip, port);


    while (true)
    {
      send_request(sockfd);
      get_results(sockfd, buffer, sizeof(buffer));
      sleep(2);
    }
    // Optional: Receive a message
    close(sockfd);
    return 0;
}

