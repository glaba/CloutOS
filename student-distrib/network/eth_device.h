#ifndef _ETH_DEVICE_H
#define _ETH_DEVICE_H

#include "../types.h"
#include "ethernet.h"
#include "network_misc.h"

typedef struct eth_device {
	// A name associated with the device
	int8_t *name;
	// The ID associated with the device
	uint32_t id;
	// The MAC address of the device
	uint8_t mac_addr[MAC_ADDR_SIZE];
	// Whether or not the ip_addr, subnet_mask and router_ip_addr fields have been inited
	//  0 means not at all, 3 means fully inited, and 1-2 means that DHCP assignment is in progress
	uint8_t dhcp_state;
	// The IP address of the device
	uint8_t ip_addr[IPV4_ADDR_SIZE];
	// The subnet mask
	uint8_t subnet_mask[IPV4_ADDR_SIZE];
	// The IP address of the router
	uint8_t router_ip_addr[IPV4_ADDR_SIZE];
	// Function to initialize the interface the driver exposes to the OS,
	//  including the mac_addr field
	// Set by the device driver and called by the OS
	int (*init)(struct eth_device*);
	// Function to transmit an Ethernet packet using the device
	// Set by the device driver and called by the OS
	// Takes pointer to start of the buffer and the length of the buffer
	int (*transmit)(uint8_t*, uint16_t);	
	// Function that the driver calls when a packet is received
	// Set by the OS and called by the driver
	// Takes pointer to start of the buffer and the length of the buffer
	int (*receive)(uint8_t*, uint32_t, uint32_t);
} eth_device;

// Registers an Ethernet device
int register_eth_dev(eth_device *dev);
// Removes the Ethernet device with the specified ID from the list
void unregister_eth_dev(uint32_t id);

// Transmits a packet on the Ethernet device of given ID
int eth_transmit(uint8_t *buffer, uint16_t size, unsigned int id);

// Gets the MAC address associated with the Ethernet device of given ID
int get_mac_addr(uint32_t id, uint8_t mac_addr[MAC_ADDR_SIZE]);
// Gets the IP address associated with the Ethernet device of given ID
int get_ip_addr(uint32_t id, uint8_t ip_addr[IPV4_ADDR_SIZE]);

// Gets a pointer to the eth_device structure for modification from the ID
eth_device *get_eth_device(uint32_t id);

#endif
