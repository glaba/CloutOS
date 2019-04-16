#ifndef _E1000_RX_H
#define _E1000_RX_H

#include "../types.h"

// Receive-related register offsets
#define ETH_RX_RECEIVE_ADDR_LO    0x5400
#define ETH_RX_RECEIVE_ADDR_HI    0x5404
// Fields of ETH_RX_RECEIVE_ADDR_HI
	// Set this bit for the Ethernet controller to receive packets for the MAC address in RECEIVE_ADDR
	#define ETH_RX_RECEIVE_ADDR_VALID 0x80000000
// Allows imperfect matching of packets that might have desired MAC addresses
#define ETH_RX_MULTICAST_TABLE_ARR_START 0x5200
#define ETH_RX_MULTICAST_TABLE_ARR_END   0x53FC
// Register where we set the length of a timer (in increments of 1.024 us) that is reset whenever a packet arrives,
//  where the interrupt fires only when the timer counts down to zero
#define ETH_RX_DELAY_TIMER_REGISTER      0x2820
// Register that serves the same function as the Delay Timer Register, but with an absolute timer
#define ETH_RX_ABSOLUTE_DELAY_TIMER      0x282C
// Contains the address and size of the rx_desc buffer
#define ETH_RX_DESCRIPTOR_BASE_ADDR_L 0x2800 // RDBAL
#define ETH_RX_DESCRIPTOR_BASE_ADDR_H 0x2804 // RDBAH
#define ETH_RX_DESCRIPTOR_BUF_LEN     0x2808 // RDLEN
#define ETH_RX_DESCRIPTOR_HEAD        0x2810 // RDH 
#define ETH_RX_DESCRIPTOR_TAIL        0x2818 // RDT
#define ETH_RX_CONTROL                0x0100 // RCTL
// Fields of ETH_RX_CONTROL
	// Enable reception of packets
	#define ETH_RCTL_ENABLE                (0x1 << 1)
	// Enables reception of packets up to 16384 bytes long (will be split into multiple descriptors)
	#define ETH_RCTL_LONG_PACKET_ENABLE    (0x1 << 5)
	// Fires an interrupt whenever the number of free descriptors is 1/2 the number of descriptors (RDLEN)
	#define ETH_RCTL_MIN_THRESHOLD_SIZE    (0x00 << 8)
	// Accepts broadcast Ethernet packets
	#define ETH_RCTL_BROADCAST_ACCEPT_MODE (0x1 << 15)
	// When RCTL.BSEX is set to 0, this value of BSIZE sets receive packet buffer size to 2048 
	// Update RX_DESCRIPTOR_PACKET_BUFFER_SIZE and RCTL.BSEX whenever this is updated
	#define ETH_RCTL_RECEIVE_BUF_SIZE      (0x00 << 16) // RCTL.BSIZE
	// Determines whether or not buffer size extension is enabled (larger packet buffer sizes allowed)
	#define ETH_RCTL_BUF_SIZE_EXT          (0x0 << 25) // RCTL.BSEX
// Fields of status field of receive descriptor
	// Whether or not this descriptor is the end of a packet (we expect this to always be there)
	#define ETH_STATUS_END_OF_PACKET 0x2
	// Whether or not this descriptor has been filled with data to read 
	#define ETH_STATUS_DESC_DONE     0x1

// The size of a receive descriptor in bytes
#define RX_DESCRIPTOR_SIZE 16
// The boundary that the descriptor buffer must be aligned to
#define RX_DESCRIPTOR_BUFFER_ALIGNMENT 128
// The size of the rx_descriptor buffer
#define RX_DESCRIPTOR_BUFFER_SIZE 128
// The size of the packet buffer within each descriptor (RCTL.BSIZE and RCTL.BSEX must be updated with this)
#define RX_DESCRIPTOR_PACKET_BUFFER_SIZE 2048

struct rx_descriptor {
	// Pointer to buffer containing data
	uint8_t *buf_addr;
	// Length of buffer in bytes
	uint16_t length;
	// The 16-bit ones complement checksum of the packet which comes in the first descriptor of the packet
	uint16_t checksum;
	// Status field containing the status of this descriptor
	uint8_t status;
	// Error field which is only valid when EOP and DD are set in the status field
	uint8_t errors;
	// The rest of the data in the descriptor is reserved
};

// Initializes transmission using the E1000 network card
int e1000_init_rx(volatile uint8_t *eth_mmio_base);
// Interrupt handler for reception
inline int e1000_rx_irq_handler(volatile uint8_t *eth_mmio_base, uint32_t interrupt_cause, int (*receive)(uint8_t*, uint32_t));

#endif
