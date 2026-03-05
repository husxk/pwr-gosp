#pragma once

/* common packet */
#define COMMON_PACKET_LENGTH_POS 0x0
#define COMMON_PACKET_TYPE_POS   0x2

#define COMMON_PACKET_LENGTH_SIZE 0x2
#define COMMON_PACKET_TYPE_SIZE   0x1

#define COMMON_PACKET_LENGTH   (COMMON_PACKET_LENGTH_SIZE + \
                                COMMON_PACKET_TYPE_SIZE     )

/* NACK packet type */

#define NACK_TYPE               0x01
#define NACK_PACKET_LENGTH      COMMON_PACKET_LENGTH

/* GET_DHT22 packet type */

#define GET_DHT22_TYPE          0x60
#define GET_DHT22_PACKET_LENGTH COMMON_PACKET_LENGTH

/* REPLY_DHT22 packet type */

#define REPLY_DHT22_TYPE             0x61

#define REPLY_DHT22_SUCCESS_SIZE     0x1
#define REPLY_DHT22_TEMPERATURE_SIZE 0x4
#define REPLY_DHT22_HUMIDITY_SIZE    0x4

#define REPLY_DHT22_SUCCESS_POS     (COMMON_PACKET_TYPE_POS      + 1)
#define REPLY_DHT22_TEMPERATURE_POS (REPLY_DHT22_SUCCESS_POS     +  \
                                     REPLY_DHT22_SUCCESS_SIZE       )

#define REPLY_DHT22_HUMIDITY_POS    (REPLY_DHT22_TEMPERATURE_POS +  \
                                     REPLY_DHT22_TEMPERATURE_SIZE)

#define REPLY_DHT22_PACKET_LENGTH   (COMMON_PACKET_LENGTH         + \
                                     REPLY_DHT22_SUCCESS_SIZE     + \
                                     REPLY_DHT22_TEMPERATURE_SIZE + \
                                     REPLY_DHT22_HUMIDITY_SIZE      )

#define REPLY_DHT22_PACKET_LENGTH_NO_SUCCESS (COMMON_PACKET_LENGTH + \
                                              REPLY_DHT22_SUCCESS_SIZE)

#define REPLY_DHT22_NO_SUCCESS 0x0
#define REPLY_DHT22_SUCCESS    0x1


#define DHT22_READ_SUCCESS     0x1
#define DHT22_READ_FAILURE     0x0


