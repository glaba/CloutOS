#ifndef _E1000_H
#define _E1000_H

#include "../pci.h"
#include "../network/eth_device.h"

// Initializes the interface the driver exposes to the OS
int e1000_init_eth(eth_device *device);
// Initializes the registers of the E1000 and sets up transmit / receive buffers
int e1000_init_device(pci_function *func);
// The interrupt handler for the E1000
inline int e1000_irq_handler(pci_function *func);
// Copies and transmits the provided buffer of given size using the E1000
int e1000_transmit(uint8_t* buf, uint16_t size);

extern pci_driver e1000_driver;
extern eth_device e1000_eth_device;

#endif
