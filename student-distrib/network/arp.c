#include "arp.h"
#include "../endian.h"
#include "../list.h"
#include "../pit.h"

// The ARP table, which maps from IP addresses to MAC addresses
arp_table_entry arp_table[ARP_TABLE_SIZE];

// The ID returned by register_periodic_callback for flush_arp_entries
uint32_t timer_callback_id;

// The number of open entries in the arp_table
uint32_t num_open_arp_entries = ARP_TABLE_SIZE;

/*
 * Flushes all ARP entries that have been in the table for more than ARP_TIMEOUT seconds
 *
 * INPUTS: time: the current system time
 * SIDE EFFECTS: removes some entries from arp_table
 */
void flush_arp_entries(double time) {
	int i;
	for (i = 0; i < ARP_TABLE_SIZE; i++) {
		// Remove the entry if enough time has passed
		if (arp_table[i].present && time - arp_table[i].time_added > ARP_TIMEOUT) {
			ARP_DEBUG("ARP entry for IP %d.%d.%d.%d expired\n",
				arp_table[i].ip_addr[0], arp_table[i].ip_addr[1], arp_table[i].ip_addr[2], arp_table[i].ip_addr[3]);

			arp_table[i].present = 0;

			// Increment the number of open entries
			num_open_arp_entries++;
		}
	}
}

/*
 * Initialize the ARP table to be empty and setup periodic calls to flush_arp_entries
 */
void init_arp() {
	int i;
	for (i = 0; i < ARP_TABLE_SIZE; i++) {
		arp_table[i].present = 0;
	}

	// Set the callback to occur every ARP_TIMEOUT seconds
	// Notice that this means that entries might ACTUALLY be flushed anywhere from 
	//  ARP_TIMEOUT seconds to 2*ARP_TIMEOUT seconds, though this is not a big deal
	timer_callback_id = register_periodic_callback((int)(PIT_FREQUENCY * ARP_TIMEOUT), flush_arp_entries);
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
	// Unfortunately, we don't actually have an IP yet....
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

	// Try either filling in an empty spot or replacing an old entry for the same IP
	// Also keep track of the oldest item in the table in case the table is full and we need 
	//  to select an entry to replace
	int inserted_entry = 0;
	double oldest_time = 2 * sys_time;
	int oldest_index = -1;
	int i, j;
	for (i = 0; i < ARP_TABLE_SIZE; i++) {
		// Keep track of the oldest item in the table
		if (arp_table[i].present && arp_table[i].time_added < oldest_time) {
			oldest_index = i;
			oldest_time = arp_table[i].time_added;
		}

		// Check if the current entry is an old entry for the same IP address
		int addr_same = 0;
		if (arp_table[i].present) {
			addr_same = 1;
			for (j = 0; j < IPV4_ADDR_SIZE; j++) {
				if (arp_table[i].ip_addr[j] != packet->sender_protocol_addr[j])
					addr_same = 0;
			}
		}

		// If this entry is either free or if it is an old entry for the same IP, overwrite it
		if (!arp_table[i].present || addr_same) {
			// Decrement the total number of open entries if we're filling an open entry in
			if (!arp_table[i].present)
				num_open_arp_entries--;

			// Fill in the fields for the current entry
			arp_table[i].present = 1;
			arp_table[i].vlan = vlan;
			arp_table[i].time_added = sys_time;

			// Copy over the IPv4 and MAC addresses from the packet
			for (j = 0; j < IPV4_ADDR_SIZE; j++)
				arp_table[i].ip_addr[j] = packet->sender_protocol_addr[j];
			for (j = 0; j < MAC_ADDR_SIZE; j++)
				arp_table[i].mac_addr[j] = packet->sender_hw_addr[j];

			inserted_entry = 1;
			ARP_DEBUG("Inserted new entry into ARP table:\n");
			ARP_DEBUG("    IP %d.%d.%d.%d -> MAC %x:%x:%x:%x:%x:%x\n",
				arp_table[i].ip_addr[0], arp_table[i].ip_addr[1], arp_table[i].ip_addr[2], arp_table[i].ip_addr[3],
				arp_table[i].mac_addr[0], arp_table[i].mac_addr[1], arp_table[i].mac_addr[2], 
				arp_table[i].mac_addr[3], arp_table[i].mac_addr[4], arp_table[i].mac_addr[5]);

			// We successfully updated the table, so return success
			return 0;
		}
	}

	ARP_DEBUG("No free entries in ARP table, replacing entry for %d.%d.%d.%d with\n", 
		arp_table[oldest_index].ip_addr[0], arp_table[oldest_index].ip_addr[1], 
		arp_table[oldest_index].ip_addr[2], arp_table[oldest_index].ip_addr[3]);

	// If we were not able to insert the entry into the table, we need to replace the oldest
	//  entry with this new one
	arp_table[oldest_index].present = 1;
	arp_table[oldest_index].vlan = vlan;
	arp_table[oldest_index].time_added = sys_time;

	for (j = 0; j < IPV4_ADDR_SIZE; j++)
		arp_table[oldest_index].ip_addr[j] = packet->sender_protocol_addr[j];
	for (j = 0; j < MAC_ADDR_SIZE; j++)
		arp_table[oldest_index].mac_addr[j] = packet->sender_hw_addr[j];

	ARP_DEBUG("    IP %d.%d.%d.%d -> MAC %x:%x:%x:%x:%x:%x\n",
		arp_table[oldest_index].ip_addr[0], arp_table[oldest_index].ip_addr[1], 
		arp_table[oldest_index].ip_addr[2], arp_table[oldest_index].ip_addr[3],
		arp_table[oldest_index].mac_addr[0], arp_table[oldest_index].mac_addr[1], arp_table[oldest_index].mac_addr[2], 
		arp_table[oldest_index].mac_addr[3], arp_table[oldest_index].mac_addr[4], arp_table[oldest_index].mac_addr[5]);

	return 0;
}
