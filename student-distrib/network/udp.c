#include "udp.h"
#include "ethernet.h"
#include "eth_device.h"
#include "arp.h"
#include "dhcp.h"
#include "../endian.h"
#include "../kheap.h"
#include "../list.h"
#include "../processes.h"

// The size of a UDP/IP header, assuming there are no IP options
#define IP_HEADER_SIZE 20
#define UDP_HEADER_SIZE 8

// The maximum size of an IP packet
#define IP_PACKET_MAX_SIZE 65535

// Default values for the IP header
#define IP_HEADER_VERSION      4 // Indicates IPv4
#define IP_HEADER_IHL          5 // Indicates a 20 byte IP header
#define IP_HEADER_DSCP         0 // We don't use this field
#define IP_HEADER_ECN          0 // We don't use this field
#define IP_HEADER_TTL          64 // A maximum of 64 hops
#define IP_HEADER_UDP_PROTOCOL 0x11 // Indicates that we are using UDP

// Offsets in the IP header + shift/mask after shifting for fields less than 1 byte
#define VERSION_OFFSET 0
#define VERSION_SHIFT  4
#define VERSION_MASK   0xF0
#define IHL_OFFSET 0
#define IHL_SHIFT  0
#define IHL_MASK   0xF
#define DSCP_OFFSET 1
#define DSCP_SHIFT  2
#define DSCP_MASK   0xFC
#define ECN_OFFSET 1
#define ECN_SHIFT  0
#define ECN_MASK   0x3
#define TOTAL_LENGTH_OFFSET 2
#define FRAGMENT_ID_OFFSET 4
#define FLAGS_OFFSET 6
#define FLAGS_SHIFT  5
#define FLAGS_MASK   0xE0
	// Flag fields
	#define FLAG_DONT_FRAGMENT  0x2
	#define FLAG_MORE_FRAGMENTS 0x1 
// The 13-bit fragment offset is split into a high 5-bit portion and a low 8-bit portion
#define FRAGMENT_OFFSET_HI_OFFSET 6
#define FRAGMENT_OFFSET_HI_SHIFT  0
#define FRAGMENT_OFFSET_HI_MASK   0x1F
#define FRAGMENT_OFFSET_LO_OFFSET 7
#define TTL_OFFSET 8
#define PROTOCOL_OFFSET 9
#define HEADER_CHECKSUM_OFFSET 10
#define SOURCE_IP_OFFSET 12
#define DEST_IP_OFFSET 16

// 2-byte offsets in the UDP header
#define SRC_PORT_OFFSET 0
#define DEST_PORT_OFFSET 1
#define UDP_LENGTH_OFFSET 2
#define UDP_CHECKSUM_OFFSET 3

/*
 * Fills the IP header with default fields and fields based on the given values
 *
 * INPUTS: data_length: the length of the data in the IP packet in bytes
 *         fragment_id: the ID that uniquely identifies members of the same fragment
 *         fragment_offset: the offset, in 8-byte increments, of the current fragment
 *         more_fragments: set if this is not the last fragment in a fragmented packet, unset for unfragmented packets
 *         src_ip dest_ip: the source and destination IP addresses
 * OUTPUTS: returns -1 if the provided data is invalid and 0 on success
 * SIDE EFFECTS: fills the IP header in the provided buffer, which must be at least 20 bytes long
 */
int fill_ip_header(uint16_t data_length, uint16_t fragment_id, uint16_t fragment_offset, uint8_t more_fragments,
                    uint8_t src_ip[IPV4_ADDR_SIZE], uint8_t dest_ip[IPV4_ADDR_SIZE], uint8_t *buffer) {
	int i;

	// Zero out the entire buffer first
	for (i = 0; i < IP_HEADER_SIZE; i++) {
		buffer[i] = 0;
	}

	buffer[VERSION_OFFSET] |= (IP_HEADER_VERSION << VERSION_SHIFT) & VERSION_MASK;
	buffer[IHL_OFFSET] |= (IP_HEADER_IHL << IHL_SHIFT) & IHL_MASK;
	buffer[DSCP_OFFSET] |= (IP_HEADER_DSCP << DSCP_SHIFT) & DSCP_MASK;
	buffer[ECN_OFFSET] |= (IP_HEADER_ECN << ECN_SHIFT) & ECN_MASK;

	// Check that the total length is valid
	if ((uint32_t)data_length + IP_HEADER_SIZE > IP_PACKET_MAX_SIZE)
		return -1;

	// Cast to uint16_t to set 16-bit fields and remember to flip from little endian to big endian
	*(uint16_t*)(buffer + TOTAL_LENGTH_OFFSET) = flip_endian16(IP_HEADER_SIZE + data_length);
	*(uint16_t*)(buffer + FRAGMENT_ID_OFFSET) = flip_endian16(fragment_id);

	// The only flag we set is more_fragments, if desired, which is also the least significant bit of the flags
	buffer[FLAGS_OFFSET] |= (!!more_fragments << FLAGS_SHIFT) & FLAGS_MASK;

	// Get the high byte of fragment_offset
	buffer[FRAGMENT_OFFSET_HI_OFFSET] |= 
		((fragment_offset & 0xFF00) << FRAGMENT_OFFSET_HI_SHIFT) & FRAGMENT_OFFSET_HI_MASK;
	// Get the low byte of fragment offset
	buffer[FRAGMENT_OFFSET_LO_OFFSET] |= fragment_offset & 0x00FF;

	buffer[TTL_OFFSET] |= IP_HEADER_TTL;
	buffer[PROTOCOL_OFFSET] |= IP_HEADER_UDP_PROTOCOL;

	// Copy all 4 bytes of the IP addresses
	for (i = 0; i < IPV4_ADDR_SIZE; i++) {
		buffer[SOURCE_IP_OFFSET + i] = src_ip[i];
		buffer[DEST_IP_OFFSET + i] = dest_ip[i];
	}

	// Finally, compute the checksum of the header, which is the "16-bit one's complement of the one's complement
	//  sum of all 16-bit words in the header"
	// We can do one's commplement addition by taking regular sums, but adding 1 whenever there is a carry
	// This is equivalent to doing 32-bit addition and adding the total amount left over in the upper 16 bits
	uint32_t sum = 0;
	for (i = 0; i < IP_HEADER_SIZE / 2; i++) {
		// Remember to flip from big endian to little endian
		sum += (uint32_t)flip_endian16(*(uint16_t*)(buffer + 2 * i));
	}

	uint16_t ones_complement_sum = (sum & 0xFFFF) + (sum >> 16);

	// Finally, set the checksum to be the one's complement of the one's complement sum, in big endian
	*(uint16_t*)(buffer + HEADER_CHECKSUM_OFFSET) = ~flip_endian16(ones_complement_sum); 

	return 0;
}

/*
 * Writes data over UDP
 *
 * INPUTS: buf: a buffer of the following format (bytewise): IP address (4 bytes), Src Port (2 bytes)
 *              Dest port (2 bytes), data (rest)
 *         bytes: total size of the buffer
 */
int32_t udp_write(int32_t fd, const void *buf, int32_t bytes) {
	if (bytes < 8)
		return -1;

	uint8_t dest_ip[IPV4_ADDR_SIZE];
	uint16_t src_port, dest_port;

	int i;
	for (i = 0; i < 4; i++)
		dest_ip[i] = *(uint8_t*)(buf + i);
	src_port = *(uint16_t*)(buf + 4);
	dest_port = *(uint16_t*)(buf + 6);

	return send_udp_packet(buf + 8, bytes - 8, src_port, dest_ip, dest_port, 1);
}

/*
 * Sends the UDP packet using the given ports and the specified Ethernet interface
 *
 * INPUTS: data: a pointer to the start of the data
 *         length: the length of the data
 *         src_port, dest_port: the ports to use for the UDP transmission
 *         id: the ID of the Ethernet interface to use
 * RETURNS: -1 if the parameters are invalid / sending otherwise failed and 0 otherwise
 */
int send_udp_packet(void *data, uint16_t length, uint16_t src_port, uint8_t dest_ip[IPV4_ADDR_SIZE], 
                    uint16_t dest_port, uint32_t id) {

	void *packet = kmalloc(length + IP_HEADER_SIZE + UDP_HEADER_SIZE);
	if (packet == NULL)
		return -1;

	// Get our IP address
	uint8_t src_ip[IPV4_ADDR_SIZE];
	get_ip_addr(id, src_ip);

	// Fill in the IP header
	if (fill_ip_header(UDP_HEADER_SIZE + length, 0, 0, 0, src_ip, dest_ip, packet) != 0) {
		kfree(packet);
		return -1;
	}

	// Fill in the UDP header
	uint16_t *udp_header = (uint16_t*)(packet + IP_HEADER_SIZE);
	udp_header[SRC_PORT_OFFSET] = flip_endian16(src_port);
	udp_header[DEST_PORT_OFFSET] = flip_endian16(dest_port);
	udp_header[UDP_LENGTH_OFFSET] = flip_endian16(UDP_HEADER_SIZE + length);
	udp_header[UDP_CHECKSUM_OFFSET] = 0; // We don't compute the UDP checksum

	// Copy over the actual data
	int i;
	for (i = 0; i < length; i++) {
		*(uint8_t*)(packet + IP_HEADER_SIZE + UDP_HEADER_SIZE + i) = *(uint8_t*)(data + i);
	}

	// The destination MAC address this packet will be addressed to
	uint8_t dest_mac_addr[MAC_ADDR_SIZE];

	// First, check the IP address with the subnet mask
	// If it is equal to our own IP address under the subnet mask, we have to send an ARP request
	eth_device *device = get_eth_device(id);
	int outsider = 0;
	for (i = 0; i < IPV4_ADDR_SIZE; i++) {
		if ((device->ip_addr[i] & device->subnet_mask[i]) != (dest_ip[i] & device->subnet_mask[i])) {
			outsider = 1;
			break;
		}
	}

	if (!outsider) {
		// If there is no ARP entry, send an ARP request
		if (get_arp_entry(dest_ip, dest_mac_addr, id) == ARP_TABLE_ENTRY_EMPTY)
			send_arp_request(dest_ip, id);

		// Loop while we are waiting
		while (get_arp_entry(dest_ip, dest_mac_addr, id) == ARP_TABLE_ENTRY_WAITING);

		// Check that the ARP entry is present -- if it is not, this means that we did not get an ARP response
		//  so we should fail
		if (get_arp_entry(dest_ip, dest_mac_addr, id) != ARP_TABLE_ENTRY_PRESENT) {
			printf("Did not receive ARP response, UDP packet cannot be sent\n");

			kfree(packet);
			return -1;
		}
	} else {
		// Copy in the router's MAC address
		for (i = 0; i < MAC_ADDR_SIZE; i++) {
			dest_mac_addr[i] = device->router_mac_addr[i];
		}
	}

	// Transmit the packet and store whether or not it succeeded
	int retval = send_eth_packet(dest_mac_addr, ET_IPV4, packet, length + IP_HEADER_SIZE + UDP_HEADER_SIZE, id);

	// Free data allocated and return success / failure
	kfree(packet);
	return retval;
}

typedef struct received_udp_packet {
	int length;
	char buffer[3000];
} received_udp_packet;

int32_t udp_read(int32_t fd, void *buf, int32_t bytes) {
	spin_lock_irqsave(pcb_spin_lock);

	pcb_t *pcb = get_pcb();
	int32_t pid = pcb->pid;

	// Set aside a buffer and store it in the PCB
	pcb->blocking_call.type = BLOCKING_CALL_UDP_READ;
	pcb->blocking_call.data = (uint32_t)kmalloc(sizeof(received_udp_packet));

	// Put the process to sleep while we wait for a response
	process_sleep(pid);

	spin_lock_irqsave(pcb_spin_lock);

	// Now that the process has woken up, copy the data into the buffer
	int i;
	received_udp_packet *packet = (received_udp_packet*)get_pcb()->blocking_call.data;
	for (i = 0; i < packet->length && i < bytes; i++) {
		*(uint8_t*)(buf + i) = packet->buffer[i];
	}

	// Free the memory allocated
	kfree(pcb->blocking_call.data);

	spin_unlock_irqsave(pcb_spin_lock);
	return i;
}

/*
 * Receives a UDP packet and forwards it to the appropriate location
 * INPUTS: buffer: a buffer containing the packet data
 *         src_mac_addr: the source mac address
 *         length: the length of the data (just for validation)
 *         vlan: the VLAN number if this was received from a VLAN packet, or -1 otherwise
 *         id: the ID of the Ethernet device this packet originated from
 * OUTPUTS: -1 if the packet was malformed / could not be understood and 0 otherwise
 */
int receive_udp_packet(uint8_t *buffer, uint8_t src_mac_addr[MAC_ADDR_SIZE], uint32_t length, int32_t vlan, uint32_t id) {
	// Extract the fields we care about from the IP header
	// Make sure that the protocol is UDP
	if (buffer[PROTOCOL_OFFSET] != IP_HEADER_UDP_PROTOCOL)
		return -1;

	// We don't support fragmented packets, which is indicated in a flag
	uint8_t flags = (buffer[FLAGS_OFFSET] & FLAGS_MASK) >> FLAGS_SHIFT;
	if (flags & FLAG_MORE_FRAGMENTS)
		return -1;

	// Copy over the source IP address
	uint8_t src_ip_addr[IPV4_ADDR_SIZE];
	int i;
	for (i = 0; i < IPV4_ADDR_SIZE; i++)
		src_ip_addr[i] = buffer[SOURCE_IP_OFFSET + i];

	// Check if the length is large enough to at least contain the UDP header + IP header
	if (length < IP_HEADER_SIZE + UDP_HEADER_SIZE)
		return -1;

	uint16_t *udp_header = (uint16_t*)(buffer + IP_HEADER_SIZE);

	// Extract the fields from the UDP header
	uint16_t dest_port = flip_endian16(udp_header[DEST_PORT_OFFSET]);
	// Subtract the header size since the length field includes the size of the header
	uint16_t udp_data_length = flip_endian16(udp_header[UDP_LENGTH_OFFSET]) - UDP_HEADER_SIZE;

	// Make sure the udp_data_length is valid
	// We don't check for strict equality since packets can be padded
	if (udp_data_length + UDP_HEADER_SIZE > length)
		return -1;

	// Check if any of the ports are special purpose ports
	switch (dest_port) {
		case DHCP_CLIENT_UDP_PORT:
			// Forward the packet to DHCP processing code
			return receive_dhcp_packet(buffer + IP_HEADER_SIZE + UDP_HEADER_SIZE, src_mac_addr, udp_data_length, id);
		default:
			// Go through all the PCBs looking for something that is waiting on a UDP read
			for (i = 0; i < pcbs.length; i++) {
				if (pcbs.data[i].pid >= 0 && pcbs.data[i].state == PROCESS_SLEEPING &&
					pcbs.data[i].blocking_call.type == BLOCKING_CALL_UDP_READ) {

					// Copy the data into the buffer and wake up the process
					received_udp_packet *packet = (received_udp_packet*)pcbs.data[i].blocking_call.data;
					packet->length = udp_data_length;

					uint8_t *syscall_buffer = (uint8_t*)packet->buffer;
					int j;
					for (j = 0; j < udp_data_length; j++) {
						syscall_buffer[j] = buffer[IP_HEADER_SIZE + UDP_HEADER_SIZE + j];
					}
					process_wake(i);
				}
			}

			// We have nothing to do with this packet
			// Let's just print it
			UDP_DEBUG("Received on UDP port %d from %d.%d.%d.%d: ", dest_port,
				src_ip_addr[0], src_ip_addr[1], src_ip_addr[2], src_ip_addr[3]);
			for (i = 0; i < udp_data_length; i++) {
				UDP_DEBUG("%c", buffer[IP_HEADER_SIZE + UDP_HEADER_SIZE + i]);
			}
			UDP_DEBUG("\n");
			return 0;
	}
}
