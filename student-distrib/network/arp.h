#ifndef _ARP_H
#define _ARP_H

#include "../lib.h"
#include "network_misc.h"

// Uncomment ARP_DEBUG_ENABLE to enable debugging
#define ARP_DEBUG_ENABLE
#ifdef ARP_DEBUG_ENABLE
	#define ARP_DEBUG(f, ...) printf(f, ##__VA_ARGS__)
#else
	#define ARP_DEBUG(f, ...) // Nothing
#endif

// The number of entries in the ARP table
#define ARP_TABLE_SIZE 64
// The amount of time an entry in the ARP table stays valid in seconds
#define ARP_TIMEOUT 10.0

// The size of an ARP packet in bytes
#define ARP_PACKET_SIZE 28

// ARP operation values
#define ARP_REQUEST 1
#define ARP_REPLY   2

// The expected values for Ethernet and IPv4 in the hardware and protocol type fields
#define ETHERNET_HARDWARE_TYPE 0x1
#define IPV4_PROTOCOL_TYPE 0x800

// Values of the arp_table_entry's state field
#define ARP_TABLE_ENTRY_PRESENT 0 // The entry exists and is valid
#define ARP_TABLE_ENTRY_WAITING 1 // The entry exists but is not yet valid, and is waiting for a response
#define ARP_TABLE_ENTRY_EMPTY   2 // There is no entry

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
	uint8_t state;
	// The time at which the entry was added
	double time_added;
	// The IP and MAC addresses that are paired together
	uint8_t ip_addr[IPV4_ADDR_SIZE];
	uint8_t mac_addr[MAC_ADDR_SIZE];
	// The VLAN number (-1 if no VLAN is used)
	int32_t vlan;
	// The ID of the Ethernet device that this entry is for
	uint32_t id;
	// A linked list of UDP packets
} arp_table_entry;

// Looks up the ARP entry for the given IP address, if it exists, and gets the MAC address
// If it doesn't exist, it performs an ARP lookup
int get_arp_entry(uint8_t ip_addr[IPV4_ADDR_SIZE], uint8_t mac_addr[MAC_ADDR_SIZE], uint32_t id);
// Sends the ARP request for the given target ip on the given Ethernet device
int send_arp_request(uint8_t target_ip_addr[IPV4_ADDR_SIZE], uint32_t id);
// Receives an ARP packet, performs appropriate actions, and replies if necessary
int receive_arp_packet(uint8_t *buffer, uint32_t length, int32_t vlan, uint32_t id);
// Initialize the ARP table to be empty and setup periodic calls to flush_arp_entries
void init_arp();

#endif
