#ifndef _E1000_H
#define _E1000_H

#include "../pci.h"

// The maximum size of an Ethernet packet, in bytes
#define ETH_MAX_PACKET_SIZE 1518

// Initializes the registers of the E1000 and sets up transmit / receive buffers
int e1000_init_device(pci_function *func);
// 
int e1000_transmit(uint8_t* buf, uint16_t size);

extern pci_driver e1000_driver;

#endif
