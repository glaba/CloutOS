#ifndef _PCI_H 
#define _PCI_H

#include "lib.h"
#include "spinlock.h"
#include "irq_defs.h"
#include "list.h"

#define NUM_BASE_ADDRESS_REGS 6

// Struct that represents a single connected PCI function
typedef struct pci_function {
	// Whether or not the function is initialized yet
	uint8_t inited;

	// The physical location in the PCI bus
	uint8_t bus;
	uint8_t slot;
	uint8_t function;

	// The addresses, sizes, and I/O types of the BARs
	uint32_t reg_base[NUM_BASE_ADDRESS_REGS];
	uint32_t reg_size[NUM_BASE_ADDRESS_REGS];
	uint8_t is_memory_space_reg[NUM_BASE_ADDRESS_REGS];

	// The interrupt line used by this function (for now, always IRQ11)
	uint8_t irq;
} pci_function;

// Struct that represents a driver for a PCI function
typedef struct pci_driver {
	// The vendor and device ID
	uint16_t vendor;
	uint16_t device;
	// The function number of the device
	uint8_t function;

	// The name of the driver
	char name[32];

	// Function that is called immediately after the PCI initialization is complete
	// This should perform device specific initialization
	// Returns 0 on success and -1 on failure
	int (*init_device)(pci_function*);

	// Function that is called when a PCI interrupt occurs
	// Returns 0 if the interrupt was handled and -1 if the interrupt was not for this device
	int (*irq_handler)(pci_function*);
} pci_driver;

typedef LIST_ITEM(pci_driver) pci_driver_list_item;
typedef LIST_ITEM(pci_function) pci_function_list_item;

// Registers a PCI driver for some PCI device
// Should be called before enumerate_pci_devices
int register_pci_driver(pci_driver driver);

// Scans the entire PCI bus for devices matching the currently registered drivers
//  and initializes the devices using the drivers if so
void enumerate_pci_devices();

// Reads data from the PCI configuration space for the given PCI function
uint32_t pci_config_read(pci_function *func, uint8_t offset, uint8_t size);
// Writes data to the PCI configuration space for the given PCI function
void pci_config_write(pci_function *func, uint8_t offset, uint8_t size, uint32_t data);
// Interrupt handler for all PCI interrupts
void pci_irq_handler();


#endif /* _PCI_H */
