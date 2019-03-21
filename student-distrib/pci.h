#ifndef _PCI_H 
#define _PCI_H

#include "lib.h"
#include "spinlock.h"
#include "irq_defs.h"

#define NUM_BASE_ADDRESS_REGS 6

typedef struct pci_function {
	uint8_t inited;

	uint8_t bus;
	uint8_t slot;
	uint8_t function;

	uint32_t reg_base[NUM_BASE_ADDRESS_REGS];
	uint32_t reg_size[NUM_BASE_ADDRESS_REGS];
	uint8_t is_memory_space_reg[NUM_BASE_ADDRESS_REGS];

	uint8_t irq;
} pci_function;

typedef struct pci_driver {
	uint16_t vendor;
	uint16_t device;
	uint8_t function;

	void (*init_device)(pci_function*);
} pci_driver;

int register_pci_driver(pci_driver driver);

void enumerate_pci_devices();

uint32_t pci_config_read(pci_function *func, uint8_t offset, uint8_t size);
void pci_config_write(pci_function *func, uint8_t offset, uint8_t size, uint32_t data);

#endif /* _PCI_H */
