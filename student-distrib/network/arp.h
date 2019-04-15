#ifndef _ARP_H
#define _ARP_H

#include "../lib.h"

// Uncomment ARP_DEBUG_ENABLE to enable debugging
// #define ARP_DEBUG_ENABLE
#ifdef ARP_DEBUG_ENABLE
	#define ARP_DEBUG(f, ...) printf(f, ##__VA_ARGS__)
#else
	#define ARP_DEBUG(f, ...) // Nothing
#endif

// The number of entries in the ARP table
#define ARP_TABLE_SIZE 2048

// The size of an ARP packet in bytes
#define ARP_PACKET_SIZE 28
// The size in bytes of a IPv4 address
#define IPV4_ADDR_SIZE 4
// The size in bytes of a MAC address
#define MAC_ADDR_SIZE 6

// ARP operation values
#define ARP_REQUEST 1
#define ARP_REPLY   2

// The expected values for Ethernet and IPv4 in the hardware and protocol type fields
#define ETHERNET_HARDWARE_TYPE 0x1
#define IPV4_PROTOCOL_TYPE 0x4

// A struct representing an ARP packet (which we can do since each packet is a fixed size)
// Notice that all fields greater than 1 byte in size need to converted from big endian to little endian
//  since Ethernet packets are big endian by default and x86 is little endian
struct arp_packet {
	// The type of hardware being used (Ethernet), should be set to 1
	uint16_t hardware_type;
	// The protocol being used (IPv4), should be set to 4
	uint16_t protocol_type;
	// The length of a hardware address (MAC), should be set to 6
	uint8_t hw_addr_len;
	// The length of a protocol address (IPv4), should be set to 4
	uint8_t protocol_addr_len;
	// The operation corresponding to this packet, request or reply (1 or 2)
	uint16_t operation;
	// The MAC address of the sender
	uint8_t sender_hw_addr[MAC_ADDR_SIZE];
	// The IPv4 address of the sender
	uint8_t sender_protocol_addr[IPV4_ADDR_SIZE];
	// The MAC address of the target
	uint8_t target_hw_addr[MAC_ADDR_SIZE];
	// The IPv4 address of the target
	uint8_t target_protocol_addr[IPV4_ADDR_SIZE];
} __attribute__((packed));

typedef struct arp_packet arp_packet;

typedef struct arp_table_entry {
	// Whether or not the entry is present
	uint8_t present;
	// The time at which the entry was added
	uint32_t time_added;
	// The IP and MAC addresses that are paired together
	uint8_t ip_addr[IPV4_ADDR_SIZE];
	uint8_t mac_addr[MAC_ADDR_SIZE];
	// The VLAN number (-1 if no VLAN is used)
	int32_t vlan;
} arp_table_entry;

// Receives an ARP packet, performs appropriate actions, and replies if necessary
int receive_arp_packet(uint8_t *buffer, uint32_t length, int32_t vlan);

#endif
