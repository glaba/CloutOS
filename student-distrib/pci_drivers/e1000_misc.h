#ifndef _E1000_MISC_H
#define _E1000_MISC_H

// Uncomment E1000_DEBUG_ENABLE to enable debugging
#define E1000_DEBUG_ENABLE
#ifdef E1000_DEBUG_ENABLE
	#define E1000_DEBUG(f, ...) printf(f, ##__VA_ARGS__)
#else
	#define E1000_DEBUG(f, ...) // Nothing
#endif

// Accesses a 32-bit value in a uint8_t array starting at the given offset
#define GET_32(arr, offset) *((uint32_t*)((arr) + (offset)))
// Accesses a 16-bit value in a uint8_t array starting at the given offset
#define GET_16(arr, offset) *((uint16_t*)((arr) + (offset)))
// Accesses a 8-bit value in a uint8_t array starting at the given offset
#define GET_8(arr, offset) (arr)[offset]

// The maximum size of an Ethernet packet, in bytes (including VLAN tagged frames)
#define ETH_MAX_PACKET_SIZE 1522
// Offset registers in MMIO that apply to both tx and rx
#define ETH_INTERRUPT_MASK_SET 0xD0
// Bit fields that we will (possibly) enable for the interrupt mask
	#define ETH_IMS_RXT0   0x80  // Interrupts when receiver timer expires
	#define ETH_IMS_RXDMT0 0x10  // Interrupts when min number of rx descriptors are available 
	#define ETH_IMS_TXDW   0x01  // Interrupts when transmit descriptor is processed
#define ETH_DEVICE_STATUS_REG  0x8
// Register that contains the cause of the interrupt with the same fields as IMS
#define ETH_INT_CAUSE_REGISTER 0xC0

#endif
