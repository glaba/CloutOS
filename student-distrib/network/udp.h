#ifndef _UDP_H
#define _UDP_H

#include "network_misc.h"
#include "../types.h"

// UDP ports to use for DHCP packets
#define DHCP_CLIENT_UDP_PORT 68
#define DHCP_SERVER_UDP_PORT 67

// Sends the UDP packet using the given ports and the specified Ethernet interface
int send_udp_packet(void *data, uint16_t length, uint16_t src_port, uint8_t dest_ip[IPV4_ADDR_SIZE], 
                    uint16_t dest_port, uint32_t id);
// Receives a UDP packet and forwards it to the appropriate location
int receive_udp_packet(uint8_t *buffer, uint32_t length, int32_t vlan, uint32_t id);

#endif
