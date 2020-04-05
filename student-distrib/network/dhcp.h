#ifndef _DHCP_H
#define _DHCP_H

#include "network_misc.h"
#include "../types.h"
#include "../list.h"

// States of a DHCP client
#define DHCP_STATE_UNINITIALIZED 0
#define DHCP_STATE_SELECTING     1
#define DHCP_STATE_REQUESTING    2
#define DHCP_STATE_BOUND         3

// Sizes of fields in dhcp_packet
#define CLIENT_HW_ADDR_SIZE 16
#define DHCP_RESERVED_BYTES 192

// Size of the options field for DHCPDISCOVER assuming we are sending 
//  - the DHCP message type option (53) -- 3 bytes
//  - the parameter request list asking for: subnet mask, router (55) -- 4 bytes
//  - END option -- 1 byte
#define DHCP_DISCOVER_OPTIONS_SIZE 8
	// Offsets of each option in the list
	#define DHCP_DISCOVER_MESSAGE_TYPE_OFFSET 0
	#define DHCP_DISCOVER_PARAMETER_REQUEST_LIST_OFFSET 3
	#define DHCP_DISCOVER_END_OFFSET 7
	// Values for the parameter request list
	#define DHCP_DISCOVER_PARAMETER_REQUEST_LIST_LENGTH 2
// Size of the options field for DHCPREQUEST assuming we are sending
//  - the DHCP message type option (53) -- 3 bytes
//  - the server identifier which indicates the server we have chosen to go with (54) -- 6 bytes
//  - END option -- 1 byte
#define DHCP_REQUEST_OPTIONS_SIZE 10
	// Offsets of each option in the list
	#define DHCP_REQUEST_MESSAGE_TYPE_OFFSET 0
	#define DHCP_REQUEST_SERVER_IDENTIFIER_OFFSET 3
	#define DHCP_REQUEST_END_OFFSET 9

// Default values for the fields of DHCP packets that we send
#define DHCP_OPCODE_CLIENT 0x1 // Indicates that this packet is from a client to a server
#define DHCP_HW_TYPE 0x1 // Ethernet 
#define DHCP_HW_LEN MAC_ADDR_SIZE
#define DHCP_HOPS 0 // We start initially at 0 hops
#define DHCP_TRANSACTION_ID 0xDEADBEEF // This should be a random new value each time, but for our purposes, it can be fixed
#define DHCP_SECONDS 0x0 // Not used
#define DHCP_FLAGS 0x0 // Indicates that the server's response should be unicast and not broadcast
#define DHCP_MAGIC_COOKIE 0x63825363 // Distinguishes a DHCP packet from a BOOTP packet

// Default values for the fields of DHCP packets that we receive that are different from sending (for validation)
#define DHCP_OPCODE_SERVER 0x2

// Options for a DHCP packet
#define DHCP_OPT_SUBNET_MASK 1
#define DHCP_OPT_ROUTER 3
#define DHCP_OPT_MESSAGE_TYPE 53 
#define DHCP_OPT_MESSAGE_TYPE_LEN 1
	// DHCP message types
	#define DHCP_DISCOVER 1
	#define DHCP_OFFER    2
	#define DHCP_REQUEST  3
	#define DHCP_DECLINE  4
	#define DHCP_ACK      5
	#define DHCP_NAK      6
	#define DHCP_RELEASE  7
	#define DHCP_INFORM   8
#define DHCP_OPT_PARAMETER_REQUEST_LIST 55
#define DHCP_OPT_SERVER_IDENTIFIER 54
#define DHCP_OPT_SERVER_IDENTIFIER_LEN 4
#define DHCP_OPT_END 255

// Structure representing the contents of a DHCP packet, without the options field at the end
struct dhcp_packet {
	uint8_t operation;
	uint8_t hw_type;
	uint8_t hw_len;
	uint8_t hops;
	uint32_t transaction_id;
	uint16_t seconds;
	uint16_t flags;
	uint8_t client_ip_addr[IPV4_ADDR_SIZE];
	uint8_t your_ip_addr[IPV4_ADDR_SIZE];
	uint8_t server_ip_addr[IPV4_ADDR_SIZE];
	uint8_t relay_ip_addr[IPV4_ADDR_SIZE];
	uint8_t client_hw_addr[CLIENT_HW_ADDR_SIZE];
	uint8_t reserved[DHCP_RESERVED_BYTES];
	uint32_t magic_cookie;
} __attribute__((packed));

typedef struct dhcp_packet dhcp_packet;

// Struct which represents an option in a DHCP packet
typedef struct dhcp_option {
	uint8_t tag;
	uint8_t length;
	uint8_t *data;
} dhcp_option;

typedef LIST_ITEM(dhcp_option, dhcp_option_list_item) dhcp_option_list_item;

// Sends out a DHCP discover packet over the specified Ethernet interface
int send_dhcp_discover_packet(uint32_t id);
// Receives a DHCP packet and performs appropriate actions (depends on the current state of the interaction)
int receive_dhcp_packet(uint8_t *buffer, uint8_t src_mac_addr[MAC_ADDR_SIZE], uint32_t length, uint32_t id);

#endif
