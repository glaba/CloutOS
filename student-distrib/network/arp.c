#include "arp.h"
#include "ethernet.h"
#include "eth_device.h"
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
		if (arp_table[i].state != ARP_TABLE_ENTRY_EMPTY && time - arp_table[i].time_added > ARP_TIMEOUT) {
			ARP_DEBUG("ARP entry for IP %d.%d.%d.%d expired\n",
				arp_table[i].ip_addr[0], arp_table[i].ip_addr[1], arp_table[i].ip_addr[2], arp_table[i].ip_addr[3]);

			arp_table[i].state = ARP_TABLE_ENTRY_EMPTY;

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
		arp_table[i].state = ARP_TABLE_ENTRY_EMPTY;
	}

	// Set the callback to occur every ARP_TIMEOUT seconds
	// Notice that this means that entries might ACTUALLY be flushed anywhere from
	//  ARP_TIMEOUT seconds to 2*ARP_TIMEOUT seconds, though this is not a big deal
	timer_callback_id = register_periodic_callback((int)(PIT_FREQUENCY * ARP_TIMEOUT), flush_arp_entries);
}

/*
 * Fills in the fields of an ARP packet that are common to both requests and replies
 *
 * INPUTS: target_ip_addr: the IP address of the recipient of the ARP packet
 *         id: the ID of the Ethernet device to use
 * OUTPUTS: -1 on failure and 0 on success
            packet: an ARP packet that is filled in with default values
 */
int fill_arp_packet_fields(uint8_t target_ip_addr[IPV4_ADDR_SIZE], uint32_t id, arp_packet *packet) {
	// Fill out the packet, flipping from little endian to big endian for the 16 bit fields
	packet->hardware_type = flip_endian16(ETHERNET_HARDWARE_TYPE);
	packet->protocol_type = flip_endian16(IPV4_PROTOCOL_TYPE);
	packet->hw_addr_len = MAC_ADDR_SIZE;
	packet->protocol_addr_len = IPV4_ADDR_SIZE;

	if (get_mac_addr(id, packet->sender_hw_addr) != 0) {
		ARP_DEBUG("Could not get our MAC address for the given Ethernet device\n");
		return -1;
	}

	if (get_ip_addr(id, packet->sender_protocol_addr) != 0) {
		ARP_DEBUG("Could not get our IP address for the given Ethernet device\n");
		return -1;
	}

	int i;
	for (i = 0; i < IPV4_ADDR_SIZE; i++) {
		packet->target_protocol_addr[i] = target_ip_addr[i];
	}

	return 0;
}

/*
 * Sends the ARP request with the specified fields
 *
 * INPUTS: target_ip_addr: the IP address of the target
 *         id: the ID of the Ethernet device to use
 * OUTPUTS: -1 if a packet was not / could not be sent, and 0 if a packet was sent
 */
int send_arp_request(uint8_t target_ip_addr[IPV4_ADDR_SIZE], uint32_t id) {
	// Mask interrupts so that arp_table is not modified
	cli();

	// Check if an entry for this IP already exists
	// If it is present, we do nothing since it will be overwritten when the reply is received
	// If it is waiting, we return, since a request has already been sent
	// If it does not already exist, we create a new entry that is in the waiting state
	int i, j;
	int entry_exists = 0;
	for (i = 0; i < ARP_TABLE_SIZE; i++) {
		// Check if the address is the same for this entry
		int addr_same = 1;
		for (j = 0; j < IPV4_ADDR_SIZE; j++) {
			if (target_ip_addr[j] != arp_table[i].ip_addr[j])
				addr_same = 0;
		}

		if (addr_same && arp_table[i].id == id) {
			// Check if the entry is already existing
			if (arp_table[i].state != ARP_TABLE_ENTRY_EMPTY)
				entry_exists = 1;

			// If it is waiting, return -1, since we are still waiting on a response
			if (arp_table[i].state == ARP_TABLE_ENTRY_WAITING) {
				sti();
				return -1;
			}
		}
	}

	// If the entry doesn't exist, we need to create a new waiting entry
	if (!entry_exists) {
		int entry_inserted = 0;

		// Find an open spot in the table first
		for (i = 0; i < ARP_TABLE_SIZE; i++) {
			if (arp_table[i].state == ARP_TABLE_ENTRY_EMPTY) {
				// Let's put the entry here
				entry_inserted = 1;
				arp_table[i].state = ARP_TABLE_ENTRY_WAITING;
				arp_table[i].id = id;
				for (j = 0; j < IPV4_ADDR_SIZE; j++)
					arp_table[i].ip_addr[j] = target_ip_addr[j];

				// Break out of the loop because we inserted an entry
				break;
			}
		}

		if (!entry_inserted) {
			sti();
			return -1;
		}
	}

	// Re-enable interrupts since we are not using arp_table anymore
	sti();

	uint8_t dest_mac_addr[MAC_ADDR_SIZE] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	arp_packet packet;

	// This is a request packet, the rest of the fields are default values
	packet.operation = flip_endian16(ARP_REQUEST);
	if (fill_arp_packet_fields(target_ip_addr, id, &packet) != 0)
		return -1;

	return send_eth_packet(dest_mac_addr, ET_ARP, (void*)&packet, ARP_PACKET_SIZE, id);
}

/*
 * Sends an ARP reply to the provided target IP / MAC address
 *
 * INPUTS: target_ip_addr: the IP address of the target
 *         target_mac_addr: the MAC address of the target
 *         id: the ID of the Ethernet device to use
 * OUTPUTS: -1 if a packet was not / could not be sent, and 0 if a packet was sent
 */
int send_arp_reply(uint8_t target_ip_addr[IPV4_ADDR_SIZE], uint8_t target_mac_addr[MAC_ADDR_SIZE], uint32_t id) {
	arp_packet packet;

	// This is a reply packet, the rest of the fields are default values
	packet.operation = flip_endian16(ARP_REPLY);
	if (fill_arp_packet_fields(target_ip_addr, id, &packet) != 0)
		return -1;

	// For a reply, also fill out the target MAC address
	int i;
	for (i = 0; i < MAC_ADDR_SIZE; i++)
		packet.target_hw_addr[i] = target_mac_addr[i];

	// Simply send the packet
	return send_eth_packet(target_mac_addr, ET_ARP, (void*)&packet, ARP_PACKET_SIZE, id);
}

/*
 * Looks up the ARP entry for the given IP address, if it exists, and returns the corresponding MAC address
 *
 * INPUTS: ip_addr: the IP to look up the ARP entry for
 *         id: the Ethernet device to use
 * OUTPUTS: 0 if the entry was found, 1 if we are waiting for a response, and 2 if an entry was not found
 *          If it returns 1, the corresponding MAC address will be placed in mac_addr
 */
int get_arp_entry(uint8_t ip_addr[IPV4_ADDR_SIZE], uint8_t mac_addr[MAC_ADDR_SIZE], uint32_t id) {
	int i, j;
	
	// Check if it is the broadcast IP, return FF:FF:FF:FF:FF:FF if so
	int is_broadcast = 1;
	for (i = 0; i < IPV4_ADDR_SIZE; i++) {
		if (ip_addr[i] != 0xFF)
			is_broadcast = 0;
	}

	if (is_broadcast) {
		for (i = 0; i < MAC_ADDR_SIZE; i++)
			mac_addr[i] = 0xFF;
		return ARP_TABLE_ENTRY_PRESENT;
	}

	// Look through the entire table for the entry
	for (i = 0; i < ARP_TABLE_SIZE; i++) {
		if (arp_table[i].state != ARP_TABLE_ENTRY_EMPTY && arp_table[i].id == id) {
			// Check if the IP address matches
			int matches = 1;
			for (j = 0; j < IPV4_ADDR_SIZE; j++) {
				if (arp_table[i].ip_addr[j] != ip_addr[j])
					matches = 0;
			}

			// If the IP address matches
			if (matches) {
				// If the entry is present, copy the MAC address
				if (arp_table[i].state == ARP_TABLE_ENTRY_PRESENT) {
					for (j = 0; j < MAC_ADDR_SIZE; j++)
						mac_addr[j] = arp_table[i].mac_addr[j];
				}

				// Return the current state of the entry, waiting or present
				return arp_table[i].state;
			}
		}
	}

	// If we got to here, the entry was not found
	return ARP_TABLE_ENTRY_EMPTY;
}

/*
 * Receives an ARP packet, performs appropriate actions, and replies if necessary
 * INPUTS: buffer: a buffer containing the packet data
 *         length: the length of the data (just for validation)
 *         vlan: the VLAN number if this was received from a VLAN packet, or -1 otherwise
 *         id: the ID of the Ethernet device this packet originated from
 * SIDE EFFECTS: updates the ARP table
 * OUTPUTS: -1 if the packet was malformed / could not be understood and 0 otherwise
 */
int receive_arp_packet(uint8_t *buffer, uint32_t length, int32_t vlan, uint32_t id) {
	// Make sure the packet is the right size
	if (length < ARP_PACKET_SIZE) {
		ARP_DEBUG("ARP packet is malformed -- too short\n");
		return -1;
	}

	arp_packet *packet = (arp_packet*)buffer;

	// Convert all the fields greater than 1 byte to little endian
	packet->hardware_type = flip_endian16(packet->hardware_type);
	packet->protocol_type = flip_endian16(packet->protocol_type);
	packet->operation = flip_endian16(packet->operation);

	// We should return our own MAC address if they are requesting our own IP
	if (packet->operation == ARP_REQUEST) {
		// Get our IP address
		uint8_t our_ip[IPV4_ADDR_SIZE];
		get_ip_addr(id, our_ip);

		// Check if the IP they are requesting is our IP
		int i, same_ip;
		for (i = 0, same_ip = 1; i < IPV4_ADDR_SIZE; i++)
			same_ip &= packet->target_protocol_addr[i] == our_ip[i];
	
		// If they are requesting our MAC address, send an ARP response packet
		if (same_ip) {
			ARP_DEBUG("Responding to ARP request from %d.%d.%d.%d\n",
				packet->sender_protocol_addr[0], packet->sender_protocol_addr[1],
				packet->sender_protocol_addr[2], packet->sender_protocol_addr[3]);
			send_arp_reply(packet->sender_protocol_addr, packet->sender_hw_addr, id);
		}
	}

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
		if (arp_table[i].state != ARP_TABLE_ENTRY_EMPTY && arp_table[i].time_added < oldest_time) {
			oldest_index = i;
			oldest_time = arp_table[i].time_added;
		}

		// Check if the current entry is an old entry or waiting for the same IP address on the same Ethernet device
		int addr_same = 0;
		if (arp_table[i].state != ARP_TABLE_ENTRY_EMPTY && arp_table[i].id == id) {
			addr_same = 1;
			for (j = 0; j < IPV4_ADDR_SIZE; j++) {
				if (arp_table[i].ip_addr[j] != packet->sender_protocol_addr[j])
					addr_same = 0;
			}
		}

		// If this entry is either free or if it is an old entry or waiting for the same IP, overwrite it
		if (arp_table[i].state == ARP_TABLE_ENTRY_EMPTY || addr_same) {
			// Decrement the total number of open entries if we're filling an open entry in
			if (arp_table[i].state == ARP_TABLE_ENTRY_EMPTY)
				num_open_arp_entries--;

			// Fill in the fields for the current entry
			arp_table[i].state = ARP_TABLE_ENTRY_PRESENT;
			arp_table[i].vlan = vlan;
			arp_table[i].time_added = sys_time;
			arp_table[i].id = id;

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
	arp_table[oldest_index].state = ARP_TABLE_ENTRY_PRESENT;
	arp_table[oldest_index].vlan = vlan;
	arp_table[oldest_index].time_added = sys_time;
	arp_table[oldest_index].id = id;

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
