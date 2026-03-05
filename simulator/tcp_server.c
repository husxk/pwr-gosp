// tcp_server_sim.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdbool.h>

#include "../protocol.h"

#define BACKLOG 1

static void send_nack(int client_fd)
{
    char nack[NACK_PACKET_LENGTH];
    uint16_t len = htons(NACK_PACKET_LENGTH);
    memcpy(&nack[COMMON_PACKET_LENGTH_POS], &len, COMMON_PACKET_LENGTH_SIZE);
    nack[COMMON_PACKET_TYPE_POS] = NACK_TYPE;

    send(client_fd, nack, NACK_PACKET_LENGTH, 0);
}

static bool create_reply_dht22(char *buf, int buf_size, int *packet_size_out)
{
    if (buf_size < REPLY_DHT22_PACKET_LENGTH)
        return false;

    bool dht_data_valid = true;

    if (rand() % 100 < 5)                    // 5% chance for invalid return
        dht_data_valid = false;

    int packet_size = REPLY_DHT22_PACKET_LENGTH;

    if (dht_data_valid)
    {
        uint16_t packet_len = htons(REPLY_DHT22_PACKET_LENGTH);
        memcpy(&buf[COMMON_PACKET_LENGTH_POS], &packet_len, COMMON_PACKET_LENGTH_SIZE);

        buf[COMMON_PACKET_TYPE_POS] = REPLY_DHT22_TYPE;
        buf[REPLY_DHT22_SUCCESS_POS] = REPLY_DHT22_SUCCESS;

        int temp = (rand() % 4000 + 1500);  // 15.00°C to 55.00°C
        int hum  = (rand() % 8000 + 2000);  // 20.00% to 100.00%

        uint32_t net_temp = htonl(temp);
        uint32_t net_hum  = htonl(hum);

        memcpy(&buf[REPLY_DHT22_TEMPERATURE_POS], &net_temp, REPLY_DHT22_TEMPERATURE_SIZE);
        memcpy(&buf[REPLY_DHT22_HUMIDITY_POS], &net_hum, REPLY_DHT22_HUMIDITY_SIZE);

        fprintf(stderr, "Sending: temp=%u hum=%u\n", temp, hum);
    }
    else
    {
        uint16_t packet_len = htons(REPLY_DHT22_PACKET_LENGTH_NO_SUCCESS);
        memcpy(&buf[COMMON_PACKET_LENGTH_POS], &packet_len, COMMON_PACKET_LENGTH_SIZE);

        buf[COMMON_PACKET_TYPE_POS] = REPLY_DHT22_TYPE;
        buf[REPLY_DHT22_SUCCESS_POS] = REPLY_DHT22_NO_SUCCESS;

        packet_size = REPLY_DHT22_PACKET_LENGTH_NO_SUCCESS;
    }

    *packet_size_out = packet_size;
    return true;
}

static void packet_handler(int client_fd, const uint8_t *buffer, int total_len)
{
    if (total_len < COMMON_PACKET_LENGTH)
    {
        send_nack(client_fd);
        fprintf(stderr, "Server: Packet too short.\n");
        return;
    }

    uint16_t packet_len;
    memcpy(&packet_len, &buffer[COMMON_PACKET_LENGTH_POS], COMMON_PACKET_LENGTH_SIZE);
    packet_len = ntohs(packet_len);

    const uint8_t packet_type = buffer[COMMON_PACKET_TYPE_POS];

    switch (packet_type)
    {
        case GET_DHT22_TYPE:
            if (packet_len != GET_DHT22_PACKET_LENGTH)
            {
                send_nack(client_fd);
                fprintf(stderr, "Server: Invalid GET_DHT22_TYPE packet length.\n");
                return;
            }

            {
                char response[REPLY_DHT22_PACKET_LENGTH];
                int size = 0;

                if (!create_reply_dht22(response, sizeof(response), &size))
                {
                    fprintf(stderr, "Server: Failed to build DHT22 reply packet.\n");
                    return;
                }

                send(client_fd, response, size, 0);
            }
            break;

        default:
            send_nack(client_fd);
            fprintf(stderr, "Server: Unknown packet type: 0x%02X\n", packet_type);
            break;
    }
}

static void handle_client(int client_fd)
{
    uint8_t buffer[1024];

    while (1)
    {
        int n = recv(client_fd, buffer, sizeof(buffer), 0);
        if (n <= 0)
        {
            printf("Server: Client disconnected.\n");
            break;
        }

        packet_handler(client_fd, buffer, n);
    }

    close(client_fd);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <PORT>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    srand(time(NULL)); // seed RNG

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, BACKLOG) < 0)
    {
        perror("Listen failed");
        close(server_fd);
        return 1;
    }

    printf("Server listening on port %d\n", port);

    while (1)
    {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0)
        {
            perror("Accept failed");
            continue;
        }

        printf("Client connected.\n");
        handle_client(client_fd);
    }

    close(server_fd);
    return 0;
}

