#include "ethernet.h"
#include "arp.h"
#include "udp.h"
#include "eth_device.h"
#include "../endian.h"
#include "../kheap.h"

/*
 * Receives an Ethernet packet, performs appropriate actions, and replies if necessary
 * Currently it implements the IPv4 and ARP protocols, and UDP on top of that
 * INPUTS: buffer: a buffer containing the packet data
 *         length: the length of the buffer (actual length could be <= 64 if length = 64)
 *         id: the ID of the Ethernet device that received this packet
 * OUTPUTS: 0 if successfully processed and -1 if the packet could not be handled
 */
int receive_eth_packet(uint8_t *buffer, uint32_t length, uint32_t id) {
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
	if (ether_type[1] == (ET_VLAN & 0xFF) && ether_type[0] == ((ET_VLAN >> 8) & 0xFF)) {
		// Change the payload address and size if it is a VLAN packet
		payload = buffer + VLAN_PAYLOAD_OFFSET;
		payload_size = length - VLAN_PAYLOAD_OFFSET - CRC_SIZE;
		if (payload_size < 0) goto malformed_packet;

		// Store the VLAN ID in vlan, which is the bottom 12 bits of bytes 14 and 15 (in big endian order)
		vlan = (int32_t)(flip_endian16(*(uint16_t*)(buffer + PCP_DEI_VID_OFFSET)) & 0xFFF);

		// Store the actual ether_type in ether_type
		for (i = VLAN_ETHER_TYPE_OFFSET; i < VLAN_ETHER_TYPE_OFFSET + ETHER_TYPE_SIZE; i++) {
			if (i >= length) goto malformed_packet;
			ether_type[i - VLAN_ETHER_TYPE_OFFSET] = buffer[i];
		}
	}

	if (ether_type[1] == (ET_ARP & 0xFF) && ether_type[0] == ((ET_ARP >> 8) & 0xFF)) {
		// Forward this to receive_arp_packet
		if (receive_arp_packet(payload, payload_size, vlan, id) != 0)
			goto malformed_packet;
	}

	if (ether_type[1] == (ET_IPV4 & 0xFF) && ether_type[0] == ((ET_IPV4 >> 8) & 0xFF)) {
		// If it is an IP packet, we will assume that it is UDP, since we don't implement any other
		//  protocol on top of IPv4
		if (receive_udp_packet(payload, payload_size, vlan, id) != 0)
			goto malformed_packet;
	}

	return 0;

malformed_packet:
	ETH_DEBUG("Malformed packet!\n");
	return -1;
}

/*
 * Generates an Ethernet packet with the specified fields and transmits it
 *
 * INPUTS: dest_mac_addr: the destination MAC address of the packet
 *         ether_type: the EtherType value of the packet (corresponding to ARP, IPv4, etc)
 *         payload: a pointer to a buffer containing the payload of the packet
 *         payload_size: the size of the payload buffer
 *         id: the ID of the Ethernet device to use
 * OUTPUTS: -1 on failure and 0 on success
 * SIDE EFFECTS: transmits an Ethernet packet
 */
int send_eth_packet(uint8_t dest_mac_addr[MAC_ADDR_SIZE], uint16_t ether_type, void *payload, uint32_t payload_size, uint32_t id) {
	uint8_t *packet = kmalloc(2 * MAC_ADDR_SIZE + ETHER_TYPE_SIZE + payload_size);
	int i;

	if (packet == NULL)
		return -1;

	// Copy in the destination MAC address
	for (i = DST_MAC_ADDR_OFFSET; i < DST_MAC_ADDR_OFFSET + MAC_ADDR_SIZE; i++)
		packet[i] = dest_mac_addr[i - DST_MAC_ADDR_OFFSET];

	// Copy in the source MAC address
	if (get_mac_addr(id, packet + SRC_MAC_ADDR_OFFSET) != 0) {
		kfree(packet);
		return -1;
	}

	// Copy in the EtherType
	packet[ETHER_TYPE_OFFSET] = (ether_type >> 8) & 0xFF;
	packet[ETHER_TYPE_OFFSET + 1] = ether_type & 0xFF;

	// Copy in the payload
	for (i = PAYLOAD_OFFSET; i < PAYLOAD_OFFSET + payload_size; i++) {
		packet[i] = ((uint8_t*)payload)[i - PAYLOAD_OFFSET];
	}

	// Transmit the packet
	eth_transmit(packet, 2 * MAC_ADDR_SIZE + ETHER_TYPE_SIZE + payload_size, id);

	// Free the memory allocated for the packet
	kfree(packet);

	return 0;
}
