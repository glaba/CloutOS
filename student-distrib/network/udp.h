#ifndef _UDP_H
#define _UDP_H

// Uncomment UDP_DEBUG_ENABLE to enable debugging
// #define UDP_DEBUG_ENABLE
#ifdef UDP_DEBUG_ENABLE
	#define UDP_DEBUG(f, ...) printf(f, ##__VA_ARGS__)
#else
	#define UDP_DEBUG(f, ...) // Nothing
#endif

#include "network_misc.h"
#include "../types.h"

// UDP ports to use for DHCP packets
#define DHCP_CLIENT_UDP_PORT 68
#define DHCP_SERVER_UDP_PORT 67

// Sends the UDP packet using the given ports and the specified Ethernet interface
int send_udp_packet(void *data, uint16_t length, uint16_t src_port, uint8_t dest_ip[IPV4_ADDR_SIZE], 
                    uint16_t dest_port, uint32_t id);
// Receives a UDP packet and forwards it to the appropriate location
int receive_udp_packet(uint8_t *buffer, uint8_t src_mac_addr[MAC_ADDR_SIZE], uint32_t length, int32_t vlan, uint32_t id);

int32_t udp_read(int32_t fd, void *buf, int32_t bytes);

int32_t udp_write(int32_t fd, const void *buf, int32_t bytes);

#endif
