#ifndef _ETH_DEVICE_H
#define _ETH_DEVICE_H

#include "../types.h"

typedef struct eth_device {
	// A name associated with the device
	uint8_t *name;
	// Function to transmit an Ethernet packet using the device
	// Takes pointer to start of the buffer and the length of the buffer
	int (*transmit)(uint8_t*, uint16_t);	
	// Function that the driver calls when a packet is received
	// Takes pointer to start of the buffer and the length of the buffer
	int (*receive)(uint8_t*, uint32_t);
} eth_device;

// Registers an Ethernet device
int register_eth_dev(eth_device *dev);
// Removes the Ethernet device with the specified ID from the list
void unregister_eth_dev(uint32_t id);

// Transmits a packet on the Ethernet device of given ID
int eth_transmit(uint8_t *buffer, uint16_t size, unsigned int id);

#endif
