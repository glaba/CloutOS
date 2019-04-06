#ifndef _E1000_TX_H
#define _E1000_TX_H

#include "../types.h"

// Transmit-related register offsets
#define ETH_TX_DESCRIPTOR_BASE_ADDR_L 0x3800 // TDBAL
#define ETH_TX_DESCRIPTOR_BASE_ADDR_H 0x3804 // TDBAH
#define ETH_TX_DESCRIPTOR_BUF_LEN     0x3808 // TDLEN
#define ETH_TX_DESCRIPTOR_HEAD        0x3810 // TDH 
#define ETH_TX_DESCRIPTOR_TAIL        0x3818 // TDT 
#define ETH_TX_IPG                    0x0410 // TIPG
#define ETH_TX_CONTROL                0x0400 // TCTL 
// Fields of TCTL for our desired configuration
	#define ETH_TCTL_ENABLE            0x2
	#define ETH_TCTL_PAD_SHORT_PACKETS 0x8
	// Bits 11:4, number of attempts at re-transmission prior to giving up (16)
	#define ETH_TCTL_COLLISION_THRESH  (0x10 << 4)
	// Bits 21:12
	#define ETH_TCTL_COLLISION_DIST    (0x40 << 12)

// The size of a transmit descriptor in bytes
#define TX_DESCRIPTOR_SIZE 16
// The boundary that the descriptor buffer must be aligned to
#define TX_DESCRIPTOR_BUFFER_ALIGNMENT 16
// The size of the tx_descriptor buffer
#define TX_DESCRIPTOR_BUFFER_SIZE 128

// Flag in the status field of tx_descriptor that indicates that the 
//  corresponding packet has been dispatched
#define TX_DESC_STATUS_DESCRIPTOR_DONE 0x1 

// Flags for command field of tx_descriptor
// Indicates that the Ethernet controller should set the DD field in status
#define TX_DESC_CMD_REPORT_STATUS      0x8
// Indicates that the DD should only be written after packet has been sent
#define TX_DESC_CMD_REPORT_PACKET_SENT 0x10
// I actually have no idea what this means but it's supposed to be enabled
#define TX_DESC_CMD_IDE                0x80
// Signifies that this descriptor is the end of a packet
#define TX_DESC_CMD_END_OF_PACKET      0x1

struct tx_descriptor {
	// Pointer to buffer containing data
	uint8_t *buf_addr;
	// Length of buffer in bytes (max 16288)
	uint16_t length;
	// TCP specific feature (optional)
	uint8_t cso;
	// Command field (optional, but we use)
	uint8_t cmd;
	// Status field
	uint8_t status;
	// TCP specific feature (optional)
	uint8_t css;
	// Special field
	uint16_t special;
};

// Initializes transmission using the E1000 network card
int e1000_init_tx(volatile uint8_t *eth_mmio_base);
// Attempts to add a given descriptor to the transmission descriptor buffer (if not full)
int add_tx_descriptor(volatile uint8_t *eth_mmio_base, struct tx_descriptor *desc);
// Fills in the tx_descriptor field for a given input packet 
int create_tx_descriptor(uint8_t *buf, uint16_t size, struct tx_descriptor *desc);

#endif
