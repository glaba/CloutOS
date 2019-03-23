#include "e1000_tx.h"
#include "e1000_misc.h"
#include "../pci.h"

// The descriptor buffer that will be shared between the ethernet controller and the kernel
static volatile uint8_t tx_desc_buf[DESCRIPTOR_BUFFER_SIZE * TX_DESCRIPTOR_SIZE] __attribute__((aligned (DESCRIPTOR_BUFFER_ALIGNMENT)));

/*
 * Initializes transmission using the E1000 network card
 *
 * INPUTS: eth_mmio_base: a pointer to the base of the memory mapped I/O region with the E1000
 * SIDE EFFECTS: initializes transmission with the E1000 and connects it to the transmit descriptor buffer
 */
void e1000_init_tx(volatile uint8_t *eth_mmio_base) {
	// Fill tx_desc_buf with all zeroes 
	int i;
	for (i = 0; i < DESCRIPTOR_BUFFER_SIZE * TX_DESCRIPTOR_SIZE; i++)
		tx_desc_buf[i] = 0;

	// Set base address of descriptor array (64-bit address)
	GET_32(eth_mmio_base, ETH_TRANSMIT_DESCRIPTOR_BASE_ADDR_H) = 0;
	GET_32(eth_mmio_base, ETH_TRANSMIT_DESCRIPTOR_BASE_ADDR_L) = (uint32_t)tx_desc_buf;
	
	// Set the size of the descriptor array 
	GET_32(eth_mmio_base, ETH_TRANSMIT_DESCRIPTOR_BUF_LEN) = DESCRIPTOR_BUFFER_SIZE;

	// Start the head and tail to point at 0 (meaning there is nothing in the buffer now)
	GET_32(eth_mmio_base, ETH_TRANSMIT_DESCRIPTOR_HEAD) = 0;
	GET_32(eth_mmio_base, ETH_TRANSMIT_DESCRIPTOR_TAIL) = 0;	

	GET_32(eth_mmio_base, ETH_TRANSMIT_IPG) = 10;

	// Set the control fields to the values recommended in the Intel documentation
	GET_32(eth_mmio_base, ETH_TRANSMIT_CONTROL) = 
		ETH_TCTL_ENABLE | ETH_TCTL_PAD_SHORT_PACKETS | ETH_TCTL_COLLISION_THRESH | ETH_TCTL_COLLISION_DIST;
}

/*
 * Serializes a tx_descriptor struct into the 16-byte packed format that the E1000 controller needs
 *
 * INPUTS: desc: the transmission descriptor to be serialized
 * OUTPUTS: serialized: the serialized value of the tx_descriptor as bytes
 */
void serialize_tx_descriptor(struct tx_descriptor *desc, volatile uint8_t serialized[TX_DESCRIPTOR_SIZE]) {
	// These offsets are directly obtained from the documentation, and their meaning from context
	//  is obvious enough that they don't merit their own constants
	GET_32(serialized, 0) = (uint32_t)desc->buf_addr; // Little endian, so smaller half comes first
	GET_32(serialized, 4) = 0; // Upper 32 bits of 64-bit address are 0 obviously
	GET_16(serialized, 8) = desc->length;
	GET_8 (serialized, 10) = desc->cso;
	GET_8 (serialized, 11) = desc->cmd;
	GET_8 (serialized, 12) = desc->status & 0xF;
	GET_8 (serialized, 13) = desc->css;
	GET_16(serialized, 14) = desc->special;
}

/*
 * Deserializes a sequence of bytes into a tx_descriptor struct
 *
 * INPUTS: serialized: a tx_descriptor serialized in the format the E1000 wants in bytes
 * OUTPUTS: desc: a transmission descriptor that will contain the data from serialized
 */
void deserialize_tx_descriptor(volatile uint8_t serialized[TX_DESCRIPTOR_SIZE], struct tx_descriptor *desc) {
	desc->buf_addr = (uint8_t*)GET_32(serialized, 0);
	desc->length   = GET_16(serialized, 8);
	desc->cso      = GET_8 (serialized, 10);
	desc->cmd      = GET_8 (serialized, 11);
	desc->status   = GET_8 (serialized, 12) & 0xF;
	desc->css      = GET_8 (serialized, 13);
	desc->special  = GET_16(serialized, 14);
}

/*
 * Attempts to add a given descriptor to the transmission descriptor buffer (if not full)
 *
 * INPUTS: eth_mmio_base: a pointer to the start of the MMIO for the E1000
 *         desc: the descriptor to add to the buffer
 * OUTPUTS: 0 if successful and -1 if the buffer was full
 * SIDE EFFECTS: modifies the transmittion descriptor buffer and notifies the E1000
 */
int add_tx_descriptor(volatile uint8_t *eth_mmio_base, struct tx_descriptor *desc) {
	// The current value of the tail "pointer" (it's really an index into tx_desc_buf)
	uint32_t cur_tail = GET_32(eth_mmio_base, ETH_TRANSMIT_DESCRIPTOR_TAIL);

	// First, check if the buffer is full
	// We accomplish this by checking the DD bit in the status register for
	//  the tail element in the buffer
	struct tx_descriptor next;
	deserialize_tx_descriptor(tx_desc_buf + cur_tail * TX_DESCRIPTOR_SIZE, &next);

	// If the next element in the buffer was previously filled by us
	//  and it is not done, then the buffer is full
	if ((next.cmd & TX_DESC_CMD_REPORT_STATUS) &&
		!(next.status & TX_DESC_STATUS_DESCRIPTOR_DONE)) {

		ETH_DEBUG("Transmit buffer full, could not send packet\n");
		return -1;
	}

	// Serialize the transmit descriptor into the buffer
	serialize_tx_descriptor(desc, tx_desc_buf + cur_tail * TX_DESCRIPTOR_SIZE);

	// Update the tail pointer
	GET_32(eth_mmio_base, ETH_TRANSMIT_DESCRIPTOR_TAIL) = (cur_tail + 1) % DESCRIPTOR_BUFFER_SIZE;

	// Return success
	return 0;
}

/*
 * Fills in the tx_descriptor field for a given input packet 
 *
 * INPUTS: buf, size: the input packet as well as the size, in bytes
 * OUTPUTS: desc: the generated transmit descriptor 
 */
void create_tx_descriptor(uint8_t *buf, uint16_t size, struct tx_descriptor *desc) {
	desc->buf_addr = buf;
	desc->length = size;
	desc->cso = 0;
	desc->cmd = TX_DESC_CMD_REPORT_STATUS | TX_DESC_CMD_REPORT_PACKET_SENT | TX_DESC_CMD_IDE | TX_DESC_CMD_END_OF_PACKET;
	desc->status = 0;
	desc->css = 0;
	desc->special = 0;
}
