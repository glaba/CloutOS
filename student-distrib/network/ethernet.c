#include "ethernet.h"
#include "arp.h"
#include "../endian.h"

/*
 * Receives an Ethernet packet, performs appropriate actions, and replies if necessary
 * Currently it implements the IPv4 and ARP protocols, and UDP on top of that
 * INPUTS: buffer: a buffer containing the packet data
 *         length: the length of the buffer (actual length could be <= 64 if length = 64)
 * OUTPUTS: 0 if successfully processed and -1 if the packet could not be handled
 */
int receive_eth_packet(uint8_t *buffer, uint32_t length) {
	uint8_t dst_mac_addr[MAC_ADDR_SIZE];
	uint8_t src_mac_addr[MAC_ADDR_SIZE];
	uint8_t ether_type[ETHER_TYPE_SIZE];
	uint8_t *payload;
	int32_t payload_size;

	int i;
	// Get the destination MAC address
	for (i = DST_MAC_ADDR_OFFSET; i < DST_MAC_ADDR_OFFSET + MAC_ADDR_SIZE; i++) {
		if (i >= length) goto malformed_packet;
		dst_mac_addr[i - DST_MAC_ADDR_OFFSET] = buffer[i];
	}

	// Get the source MAC address
	for (i = SRC_MAC_ADDR_OFFSET; i < SRC_MAC_ADDR_OFFSET + MAC_ADDR_SIZE; i++) {
		if (i >= length) goto malformed_packet;
		src_mac_addr[i - SRC_MAC_ADDR_OFFSET] = buffer[i];
	}

	// Get the EtherType (or check if it's a VLAN tagged packet)
	for (i = ETHER_TYPE_OFFSET; i < ETHER_TYPE_OFFSET + ETHER_TYPE_SIZE; i++) {
		if (i >= length) goto malformed_packet;
		ether_type[i - ETHER_TYPE_OFFSET] = buffer[i];
	}

	// Set payload to point to the payload in the input buffer and set its (max) size
	payload = buffer + PAYLOAD_OFFSET;
	payload_size = length - PAYLOAD_OFFSET - CRC_SIZE;
	if (payload_size < 0) goto malformed_packet;

	int32_t vlan = -1;

	// Check the EtherType field to see whether or not it is a VLAN tagged packet
	if (ether_type[0] == ET_VLAN_0 && ether_type[1] == ET_VLAN_1) {
		// Change the payload address and size if it is a VLAN packet
		payload = buffer + VLAN_PAYLOAD_OFFSET;
		payload_size = length - VLAN_PAYLOAD_OFFSET - CRC_SIZE;
		if (payload_size < 0) goto malformed_packet;

		// Store the VLAN ID in vlan, which is the bottom 12 bits of bytes 14 and 15 (in big endian order)
		vlan = (int32_t)(b_to_l_endian16(*(uint16_t*)(buffer + PCP_DEI_VID_OFFSET)) & 0xFFF);

		// Store the actual ether_type in ether_type
		for (i = VLAN_ETHER_TYPE_OFFSET; i < VLAN_ETHER_TYPE_OFFSET + ETHER_TYPE_SIZE; i++) {
			if (i >= length) goto malformed_packet;
			ether_type[i - VLAN_ETHER_TYPE_OFFSET] = buffer[i];
		}
	}

	if (ether_type[0] == ET_ARP_0 && ether_type[1] == ET_ARP_1) {
		// Forward this to receive_arp_packet
		if (receive_arp_packet(payload, payload_size, vlan) != 0)
			goto malformed_packet;
	}

	if (ether_type[0] == ET_IPV4_0 && ether_type[1] == ET_IPV4_1) {
		// Nothing for now
		return -1;
	}

	return 0;

malformed_packet:
	ETH_DEBUG("Malformed packet!");
	return -1;
}
