#include "e1000_rx.h"
#include "e1000_misc.h"
#include "../pci.h"
#include "../kheap.h"

// The descriptor buffer that will be shared between the ethernet controller and the kernel
static volatile uint8_t rx_desc_buf[RX_DESCRIPTOR_BUFFER_SIZE * RX_DESCRIPTOR_SIZE] __attribute__((aligned (RX_DESCRIPTOR_BUFFER_ALIGNMENT)));

/*
 * Serializes a rx_descriptor struct into the 16-byte packed format that the E1000 controller needs
 *
 * INPUTS: desc: the receive descriptor to be serialized
 * OUTPUTS: serialized: the serialized value of the rx_descriptor as bytes
 */
void serialize_rx_descriptor(struct rx_descriptor *desc, volatile uint8_t serialized[RX_DESCRIPTOR_SIZE]) {
	// These offsets are directly obtained from the documentation, and their meaning from context
	//  is obvious enough that they don't merit their own constants
	GET_32(serialized, 0)  = (uint32_t)desc->buf_addr; // Little endian, so smaller half comes first
	GET_32(serialized, 4)  = 0; // Upper 32 bits of 64-bit address are 0 obviously
	GET_16(serialized, 8)  = desc->length;
	GET_16(serialized, 10) = desc->checksum; // Doesn't really make sense to set this but whatever
	GET_8 (serialized, 12) = desc->status;
	GET_8 (serialized, 13) = desc->errors;
	// Bytes 14-15 are reserved
}

/*
 * Deserializes a sequence of bytes into a rx_descriptor struct
 *
 * INPUTS: serialized: a rx_descriptor serialized in the format the E1000 wants in bytes
 * OUTPUTS: desc: a receive descriptor that will contain the data from serialized
 */
void deserialize_rx_descriptor(volatile uint8_t serialized[RX_DESCRIPTOR_SIZE], struct rx_descriptor *desc) {
	desc->buf_addr = (uint8_t*)GET_32(serialized, 0);
	desc->length   = GET_16(serialized, 8);
	desc->checksum = GET_16(serialized, 10);
	desc->status   = GET_8(serialized, 12);
	desc->errors   = GET_8(serialized, 13);
}

/*
 * Initializes transmission using the E1000 network card
 *
 * INPUTS: eth_mmio_base: the start of the region of memory mapped region for the ethernet controller
 * SIDE EFFECTS: initializes reception of ethernet packets
 * RETURNS: -1 if it fails and 0 on success
 */
int e1000_init_rx(volatile uint8_t *eth_mmio_base) {
	// Enable receiving packets for the MAC address loaded into the Ethernet controller by default
	GET_32(eth_mmio_base, ETH_RX_RECEIVE_ADDR_HI) |= ETH_RX_RECEIVE_ADDR_VALID;

	// The multicast table array allows this Ethernet controller to filter packets
	//  imperfectly in order to match more MAC addresses
	// We are only going to use one MAC address, so this is cleared
	int i;
	for (i = ETH_RX_MULTICAST_TABLE_ARR_START; i <= ETH_RX_MULTICAST_TABLE_ARR_END; i++) {
		GET_8(eth_mmio_base, i) = 0;
	}

	// Disable absolute timer
	GET_16(eth_mmio_base, ETH_RX_ABSOLUTE_DELAY_TIMER) = 0;

	// Set the timer delay to 0 to generate an interrupt for each packet (could be changed)
	GET_16(eth_mmio_base, ETH_RX_DELAY_TIMER_REGISTER) = 0;

	// Give the Ethernet controller the address of the descriptor buffer
	GET_32(eth_mmio_base, ETH_RX_DESCRIPTOR_BASE_ADDR_L) = (uint32_t)(&rx_desc_buf);
	GET_32(eth_mmio_base, ETH_RX_DESCRIPTOR_BASE_ADDR_H) = 0; // Obviously 0 on a 32-bit machine

	// Set the length of the buffer IN BYTES
	GET_32(eth_mmio_base, ETH_RX_DESCRIPTOR_BUF_LEN) = RX_DESCRIPTOR_BUFFER_SIZE * RX_DESCRIPTOR_SIZE;

	// Fill in rx_desc_buf with empty buffers of the desired size
	struct rx_descriptor desc;
	for (i = 0; i < RX_DESCRIPTOR_BUFFER_SIZE; i++) {
		// Allocate memory for the buffer on the heap, since we only have 4MB of static memory
		//  in the main kernel page that will be filled up with more kernel code 
		// Note that these allocations will already take up 5% of the entire 4MB heap
		desc.buf_addr = kmalloc(RX_DESCRIPTOR_PACKET_BUFFER_SIZE);
		if (desc.buf_addr == NULL)
			return -1;

		// Write the descriptor to rx_desc_buf
		serialize_rx_descriptor(&desc, rx_desc_buf + i * RX_DESCRIPTOR_SIZE);
	}

	// Set the head and tail registers
	GET_32(eth_mmio_base, ETH_RX_DESCRIPTOR_HEAD) = 0;
	GET_32(eth_mmio_base, ETH_RX_DESCRIPTOR_TAIL) = RX_DESCRIPTOR_BUFFER_SIZE;

	// Program the receive control register with the desired configuration values
	GET_32(eth_mmio_base, ETH_RX_CONTROL) = 
		ETH_RCTL_ENABLE | ETH_RCTL_LONG_PACKET_ENABLE | ETH_RCTL_MIN_THRESHOLD_SIZE | 
		ETH_RCTL_BROADCAST_ACCEPT_MODE | ETH_RCTL_RECEIVE_BUF_SIZE | ETH_RCTL_BUF_SIZE_EXT;

	return 0;
}

static int cur_descriptor = 0;

/*
 * Interrupt handler for packet receive-related interrupts
 * INPUTS: eth_mmio_base: pointer to the start of memory-mapped I/O for the E1000
 *         interrupt_cause: the contents of the Interrupt Cause Read register
 * OUTPUTS: 0 if the interrupt was handled and -1 if not
 */
inline int e1000_rx_irq_handler(volatile uint8_t *eth_mmio_base, uint32_t interrupt_cause) {
	// Make sure this interrupt is for ethernet frame reception
	if (!(interrupt_cause & ETH_IMS_RXT0 || interrupt_cause & ETH_IMS_RXDMT0))
		return -1;

	struct rx_descriptor cur;
	while (1) {
		// Get the rx_descriptor at the current position
		deserialize_rx_descriptor(rx_desc_buf + RX_DESCRIPTOR_SIZE * cur_descriptor, &cur);
		
		// Check if it has been filled out
		if (cur.status & ETH_STATUS_DESC_DONE) {
			// Make sure it is one entire packet, we do not support packets split between frames
			if ((cur.status & ETH_STATUS_END_OF_PACKET) == 0) {
				E1000_DEBUG("Received incomplete packet split between frames, ignoring...\n");
				return 0;
			}

			// For the moment we don't actually do anything with the data, so let's just print it
			E1000_DEBUG("Received packet stored at 0x%x of length 0x%x\n", (uint32_t)cur.buf_addr, (uint32_t)cur.length);
			E1000_DEBUG("Contents: ");
			int i;
			for (i = 0; i < cur.length; i++) {
				E1000_DEBUG("%x", cur.buf_addr[i]);
			}
			E1000_DEBUG("\n");

			// Write back with desc_done not set
			cur.status &= ~ETH_STATUS_DESC_DONE;
			serialize_rx_descriptor(&cur, rx_desc_buf + RX_DESCRIPTOR_SIZE * cur_descriptor);

			// Update cur descriptor index
			cur_descriptor = (cur_descriptor + 1) % RX_DESCRIPTOR_BUFFER_SIZE;
		} else {
			break;
		}
	}

	// Write back cur_descriptor - 1 to E1000
	GET_32(eth_mmio_base, ETH_RX_DESCRIPTOR_TAIL) = (cur_descriptor + RX_DESCRIPTOR_BUFFER_SIZE - 1) % RX_DESCRIPTOR_BUFFER_SIZE;

	return 0;
}
