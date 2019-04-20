#ifndef _UDP_H
#define _UDP_H

#include "network_misc.h"
#include "../types.h"

// Sends the UDP packet using the given ports and the specified Ethernet interface
int send_udp_packet(void *data, uint16_t length, uint16_t src_port, uint8_t dest_ip[IPV4_ADDR_SIZE], 
                    uint16_t dest_port, uint32_t id);

#endif
