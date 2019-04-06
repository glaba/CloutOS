#ifndef _E1000_H
#define _E1000_H

#include "../pci.h"

// Offset registers in MMIO that apply to both tx and rx
#define ETH_INTERRUPT_MASK_SET 0xD0
// Bit fields that we will (possibly) enable for the interrupt mask
	#define ETH_IMS_RXT0   0x80  // Interrupts when receiver timer expires
	#define ETH_IMS_RXDMT0 0x10  // Interrupts when min number of rx descriptors are available 
	#define ETH_IMS_TXDW   0x01  // Interrupts when transmit descriptor is processed
#define ETH_DEVICE_STATUS_REG  0x8

// Initializes the registers of the E1000 and sets up transmit / receive buffers
int e1000_init_device(pci_function *func);
// The interrupt handler for the E1000
int e1000_irq_handler(pci_function *func);
// Copies and transmits the provided buffer of given size using the E1000
int e1000_transmit(uint8_t* buf, uint16_t size);

extern pci_driver e1000_driver;

#endif
