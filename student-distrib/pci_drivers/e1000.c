#include "e1000.h"
#include "e1000_tx.h"
#include "e1000_rx.h"
#include "e1000_misc.h"

// Offsets of E1000 registers in MMIO as well as their associated data
#define ETH_STATUS_REG 0x8
	// The value that the status register should have if correctly configured
	#define CORRECT_ETH_STATUS 0x80080783

pci_driver e1000_driver = {
	.vendor = 0x8086,
	.device = 0x100E,
	.function = 0,
	.name = "E1000 Ethernet controller",
	.init_device = e1000_init_device,
	.irq_handler = e1000_irq_handler
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
		E1000_DEBUG("   E1000 status register has incorrect value of 0x%x\n", GET_32(eth_mmio_base, ETH_STATUS_REG));
		goto eth_init_failed;
	}

	// Initialize transmission
	if (e1000_init_tx(eth_mmio_base) != 0) {
		E1000_DEBUG("   Initializing transmission failed\n");
		goto eth_init_failed;
	}

	// Initialize reception
	if (e1000_init_rx(eth_mmio_base) != 0) {
		E1000_DEBUG("   Initializing reception failed (likely due to full heap)\n");
		goto eth_init_failed;
	}

	// Enable interrupts for packet reception (and transmission complete later)
	GET_32(eth_mmio_base, ETH_INTERRUPT_MASK_SET) = ETH_IMS_RXT0 | ETH_IMS_RXDMT0 | ETH_IMS_TXDW;

	E1000_DEBUG("Successfully initialized E1000\n");

	return 0;

eth_init_failed:
	E1000_DEBUG("Failed to initialize E1000\n");
	return -1;
}

/*
 * Interrupt handler for the E1000 
 * INPUTS: func: the pci_function struct for this PCI device
 * OUTPUTS: 0 if this interrupt was for the E1000 and non-zero otherwise
 */
inline int e1000_irq_handler(pci_function *func) {
	volatile uint8_t *eth_mmio_base = (volatile uint8_t*)func->reg_base[0];

	uint32_t interrupt_cause = GET_32(eth_mmio_base, ETH_INT_CAUSE_REGISTER);

	// Will return 0 if either is 0, and by short circuit evaluation, tx_handler will only be called if
	//  rx_handler returned -1
	return e1000_rx_irq_handler(eth_mmio_base, interrupt_cause) &&
	       e1000_tx_irq_handler(eth_mmio_base, interrupt_cause);
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

	if (create_tx_descriptor(buf, size, &desc) == 0)
		return add_tx_descriptor((volatile uint8_t*)func->reg_base[0], &desc);

	return -1;
}


