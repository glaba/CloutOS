#include "e1000.h"
#include "e1000_tx.h"
#include "e1000_misc.h"

// Offsets of E1000 registers in MMIO as well as their associated data
#define ETH_STATUS_REG 0x8
	// The value that the status register should have if correctly configured
	#define CORRECT_ETH_STATUS 0x80080783

pci_driver e1000_driver = {
	.vendor = 0x8086,
	.device = 0x100E,
	.function = 0,
	.init_device = e1000_init_device
};

static pci_function *func;

/*
 * Initializes the E1000
 *
 * INPUTS: the pci_function data structure corresponding to this device
 * OUTPUTS: 0 on success and -1 on failure
 */
int e1000_init_device(pci_function *func_) {
	// Store the pci_function pointer
	func = func_;

	// Pointer into memory mapped I/O for E1000
	volatile uint8_t *eth_mmio_base = (volatile uint8_t*)func->reg_base[0];

	// Check that the status register has the correct expected value
	if (GET_32(eth_mmio_base, ETH_STATUS_REG) != CORRECT_ETH_STATUS) {
		ETH_DEBUG("   E1000 status register has incorrect value of 0x%x\n", GET_32(eth_mmio_base, ETH_STATUS_REG));
		goto eth_init_failed;
	}

	// Initialize transmission
	e1000_init_tx(eth_mmio_base);

	ETH_DEBUG("Successfully initialized E1000\n");
	return 0;

eth_init_failed:
	ETH_DEBUG("Failed to initialize E1000\n");
	return -1;
}

/*
 * Transmits the provided data in a transmit descriptor
 *
 * INPUTS: buf, size: the data to send and the size of the buffer in bytes
 * OUTPUTS: 0 on success and -1 on failure (transmit buffer full or buffer too large)
 */
int e1000_transmit(uint8_t* buf, uint16_t size) {
	if (size > ETH_MAX_PACKET_SIZE)
		return -1;

	struct tx_descriptor desc;

	create_tx_descriptor(buf, size, &desc);
	return add_tx_descriptor((volatile uint8_t*)func->reg_base[0], &desc);
}
