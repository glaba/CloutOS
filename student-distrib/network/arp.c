#include "arp.h"
#include "../endian.h"
#include "../list.h"

// The ARP table, which maps from IP addresses to MAC addresses
arp_table_entry arp_table[ARP_TABLE_SIZE];

/*
 * Initialize the ARP table to be empty
 */
void init_arp() {
	int i;
	for (i = 0; i < ARP_TABLE_SIZE; i++) {
		arp_table[i].present = 0;
	}
}

/*
 * Receives an ARP packet, performs appropriate actions, and replies if necessary
 * INPUTS: buffer: a buffer containing the packet data
 *         length: the length of the data (just for validation)
 *         vlan: the VLAN number if this was received from a VLAN packet, or -1 otherwise
 * SIDE EFFECTS: updates the ARP table
 * OUTPUTS: -1 if the packet was malformed / could not be understood and 0 otherwise
 */
int receive_arp_packet(uint8_t *buffer, uint32_t length, int32_t vlan) {
	// Make sure the packet is the right size
	if (length < ARP_PACKET_SIZE) {
		ARP_DEBUG("ARP packet is malformed -- too short\n");
		return -1;
	}

	arp_packet *packet = (arp_packet*)buffer;

	// Convert all the fields greater than 1 byte to little endian
	packet->hardware_type = b_to_l_endian16(packet->hardware_type);
	packet->protocol_type = b_to_l_endian16(packet->protocol_type);
	packet->operation = b_to_l_endian16(packet->operation);

	// We should return our own MAC address if they are requesting our own IP 
	// Unfortunately, we don't actually have an IP yet.... so for now just return -1
	if (packet->operation == ARP_REQUEST)
		return -1;

	// Make sure the operation is a reply, otherwise we don't know how to handle it
	if (packet->operation != ARP_REPLY) {
		ARP_DEBUG("Unknown ARP operation -- %d -- ignoring packet", packet->operation);
		return -1;
	}

	// If it is a reply, make sure all the fields of the packet are correct as expected
	if (!(packet->hardware_type == ETHERNET_HARDWARE_TYPE &&
		  packet->protocol_type == IPV4_PROTOCOL_TYPE &&
		  packet->hw_addr_len == MAC_ADDR_SIZE &&
		  packet->protocol_addr_len == IPV4_ADDR_SIZE)) {
		ARP_DEBUG("Not using at least one of: Ethernet, IPv4\n");
		return -1;
	}
	
	// Then, store the result in the ARP table in the first empty entry
	int i, j;
	for (i = 0; i < ARP_TABLE_SIZE; i++) {
		if (arp_table[i].present) {
			// Check if there is already an entry for this IP address
			int addr_same = 1;
			for (j = 0; j < IPV4_ADDR_SIZE; j++) {
				if (arp_table[i].ip_addr[j] != packet->sender_protocol_addr[j])
					addr_same = 0;
			}

			// Update if they have the same address
			if (addr_same) {
				for (j = 0; j < MAC_ADDR_SIZE; j++)
					arp_table[i].mac_addr[j] = packet->sender_hw_addr[j];

				// Break out of the loop since we're done now
				break;
			}
		} else {
			// There is an empty spot in the table, so we put our entry here
			arp_table[i].present = 1;
			arp_table[i].vlan = vlan;

			// Copy over the IPv4 and MAC addresses
			for (j = 0; j < IPV4_ADDR_SIZE; j++)
				arp_table[i].ip_addr[j] = packet->sender_protocol_addr[j];
			for (j = 0; j < MAC_ADDR_SIZE; j++)
				arp_table[i].mac_addr[j] = packet->sender_hw_addr[j];

			// Break out of the loop since we filled in the entry
			break;
		}
	}
	return 0;
}