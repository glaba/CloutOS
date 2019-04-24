#include "dhcp.h"
#include "eth_device.h"
#include "udp.h"
#include "../endian.h"
#include "../kheap.h"

// Uncomment DHCP_DEBUG_ENABLE to enable debugging
// #define DHCP_DEBUG_ENABLE
#ifdef DHCP_DEBUG_ENABLE
	#define DHCP_DEBUG(f, ...) printf(f, ##__VA_ARGS__)
#else
	#define DHCP_DEBUG(f, ...) // Nothing
#endif

/*
 * Fills out the default fields of the provided dhcp_packet
 *
 * INPUTS: id: the ID of the Ethernet interface being used
 * OUTPUTS: sets the operation, hw_type, hw_len, hops, transaction_id, 
 *          seconds, and flags fields of the given dhcp_packet
 */
void fill_default_dhcp_fields(dhcp_packet *packet, uint32_t id) {
	// Fill in default fields of the packet
	packet->operation = DHCP_OPCODE_CLIENT;
	packet->hw_type = DHCP_HW_TYPE;
	packet->hw_len = DHCP_HW_LEN;
	packet->hops = DHCP_HOPS;
	packet->transaction_id = flip_endian32(DHCP_TRANSACTION_ID);
	packet->seconds = flip_endian16(DHCP_SECONDS);
	packet->flags = flip_endian16(DHCP_FLAGS);
	packet->magic_cookie = flip_endian32(DHCP_MAGIC_COOKIE);

	// Copy the hardware address (MAC in this case, since we're using Ethernet)
	get_mac_addr(id, packet->client_hw_addr);	

	// Fill in the rest of the client_hw_addr field with zeroes (other than MAC address)
	int i;
	for (i = MAC_ADDR_SIZE; i < CLIENT_HW_ADDR_SIZE; i++)
		packet->client_hw_addr[i] = 0;

	// Fill in the reserved bytes with zeroes
	for (i = 0; i < DHCP_RESERVED_BYTES; i++) {
		packet->reserved[i] = 0;
	}
}

/*
 * Sends out a DHCP discover packet over the specified Ethernet interface
 *
 * INPUTS: id: the ID of the Ethernet device to use
 * OUTPUTS: -1 on failure (which includes already having an assigned address) and 0 on success
 * SIDE EFFECTS: sends a DHCP packet over UDP port 68
 */
int send_dhcp_discover_packet(uint32_t id) {
	// Get the eth_device for this ID
	eth_device *device = get_eth_device(id);

	// Check that we are in the correct state
	if (device->dhcp_state != DHCP_STATE_UNINITIALIZED) {
		DHCP_DEBUG("Cannot send discover packet due to being in incorrect state\n");
		return -1;
	}

	// Allocate space for the packet
	uint8_t *data = kmalloc(sizeof(dhcp_packet) + DHCP_DISCOVER_OPTIONS_SIZE);
	dhcp_packet *packet = (dhcp_packet*)data;

	fill_default_dhcp_fields(packet, id);
	
	// Fill in the IP addresses all to 0, since we know nothing about the local network topology yet
	int i;
	for (i = 0; i < IPV4_ADDR_SIZE; i++) {
		packet->client_ip_addr[i] = 0;
		packet->your_ip_addr[i] = 0;
		packet->server_ip_addr[i] = 0;
		packet->relay_ip_addr[i] = 0;
	}

	// Fill in the options field with the message type, parameter request list, and the END tag byte
	uint8_t *options = data + sizeof(dhcp_packet);
	/*** Message type ***/
	options[DHCP_DISCOVER_MESSAGE_TYPE_OFFSET] = DHCP_OPT_MESSAGE_TYPE;
	options[DHCP_DISCOVER_MESSAGE_TYPE_OFFSET + 1] = DHCP_OPT_MESSAGE_TYPE_LEN;
	options[DHCP_DISCOVER_MESSAGE_TYPE_OFFSET + 2] = DHCP_DISCOVER;
	/*** Parameter request list ***/
	options[DHCP_DISCOVER_PARAMETER_REQUEST_LIST_OFFSET] = DHCP_OPT_PARAMETER_REQUEST_LIST;
	options[DHCP_DISCOVER_PARAMETER_REQUEST_LIST_OFFSET + 1] = DHCP_DISCOVER_PARAMETER_REQUEST_LIST_LENGTH;
	// Request the subnet mask and the IP address of the router along with the ACK
	options[DHCP_DISCOVER_PARAMETER_REQUEST_LIST_OFFSET + 2] = DHCP_OPT_SUBNET_MASK;
	options[DHCP_DISCOVER_PARAMETER_REQUEST_LIST_OFFSET + 3] = DHCP_OPT_ROUTER;
	/*** End option ***/
	options[DHCP_DISCOVER_END_OFFSET] = DHCP_OPT_END;

	// Send the packet over UDP with the destination IP of 255.255.255.255
	uint8_t broadcast_ip[IPV4_ADDR_SIZE] = {0xFF, 0xFF, 0xFF, 0xFF};

	// Temporarily mask interrupts so that we can update the state without getting a response
	cli();
	int32_t retval = send_udp_packet((void*)data, sizeof(dhcp_packet) + DHCP_DISCOVER_OPTIONS_SIZE, 
		DHCP_CLIENT_UDP_PORT, broadcast_ip, DHCP_SERVER_UDP_PORT, id);

	// Transition to the next state if it succeeded
	if (retval == 0) {
		DHCP_DEBUG("Successfully sent discover packet, transitioning to state SELECTING\n");
		device->dhcp_state = DHCP_STATE_SELECTING;
	} else {
		DHCP_DEBUG("Failed to send discover packet\n");
	}

	// Free the allocated memory
	kfree(data);
	sti();
	return retval;
}

/*
 * Takes a DHCP packet and a list of options for a DHCPACK packet and performs the appropriate actions
 *
 * INPUTS: packet: a struct containing the DHCP packet data
 *         head: a linked list of options that were attached to the DHCP packet
 *         id: the ID of the Ethernet interface that this came on
 * OUTPUTS: -1 on failure and 0 on success
 */
int receive_dhcp_ack(dhcp_packet *packet, dhcp_option_list_item *head, uint32_t id) {
	// Get the eth_device for this ID
	eth_device *device = get_eth_device(id);

	// Make sure we are in the right state
	if (device->dhcp_state != DHCP_STATE_REQUESTING) {
		DHCP_DEBUG("Cannot respond to DHCP ack due to being in the wrong state\n");
		return -1;
	}

	// Copy in our new address
	int i;
	for (i = 0; i < IPV4_ADDR_SIZE; i++) {
		device->ip_addr[i] = packet->your_ip_addr[i];
	}

	// Copy in the router address, and the subnet mask by iterating through all the options 
	//  and stopping on the ones that we are interested in
	dhcp_option_list_item *cur;
	uint8_t router_set = 0;
	uint8_t subnet_mask_set = 0;
	for (cur = head; cur != NULL; cur = cur->next) {
		// Copy over the subnet mask if that's what this option contains
		if (cur->data.tag == DHCP_OPT_SUBNET_MASK) {
			for (i = 0; i < IPV4_ADDR_SIZE; i++) {
				device->subnet_mask[i] = cur->data.data[i];
			}
			subnet_mask_set = 1;
		}

		// Copy over the router IP address if that's what this option contains
		if (cur->data.tag == DHCP_OPT_ROUTER) {
			for (i = 0; i < IPV4_ADDR_SIZE; i++) {
				device->router_ip_addr[i] = cur->data.data[i];
			}
			router_set = 1;
		}
	}

	// If we didn't get one of the two, there is something wrong with the DHCP server and we give up
	if (!router_set || !subnet_mask_set) {
		DHCP_DEBUG("Received malformed ACK, transitioning back to UNINITIALIZED state\n");
		device->dhcp_state = DHCP_STATE_UNINITIALIZED;
		return -1;
	}

	// Advance to the bound state
	DHCP_DEBUG("Received valid ACK, transitioning to BOUND state\n");
	device->dhcp_state = DHCP_STATE_BOUND;

	return 0;
}

/*
 * Takes a DHCP packet and a list of options for a DHCPOFFER packet and responds appropriately
 *
 * INPUTS: packet: a struct containing the DHCP packet data
 *         head: a linked list of options that were attached to the DHCP packet
 *         id: the ID of the Ethernet interface that this came on
 * OUTPUTS: -1 on failure and 0 on success
 */
int receive_dhcp_offer(dhcp_packet *packet, dhcp_option_list_item *head, uint32_t id) {
	// Get the eth_device for this ID
	eth_device *device = get_eth_device(id);

	// Make sure we are in the right state
	if (device->dhcp_state != DHCP_STATE_SELECTING) {
		DHCP_DEBUG("Cannot respond to DHCP offer due to being in the wrong state\n");
		return -1;
	}

	DHCP_DEBUG("Received valid DHCP offer, sending DHCP request\n");

	// Accept the offer immediately
	// Allocate space for the DHCPREQUEST packet we will send to respond to this offer
	uint8_t *data = kmalloc(sizeof(dhcp_packet) + DHCP_REQUEST_OPTIONS_SIZE);
	dhcp_packet *resp_packet = (dhcp_packet*)data;

	fill_default_dhcp_fields(resp_packet, id);

	// Set all the IPs to be zero except for the server IP address, which we are selecting
	int i;
	for (i = 0; i < IPV4_ADDR_SIZE; i++) {
		resp_packet->client_ip_addr[i] = 0;
		resp_packet->your_ip_addr[i] = 0;
		resp_packet->server_ip_addr[i] = packet->server_ip_addr[i];
		resp_packet->relay_ip_addr[i] = 0;
	}

	// Fill in the options, which are just the message type, server identifier, and end option
	uint8_t *options = data + sizeof(dhcp_packet);
	/*** Message type ***/
	options[DHCP_REQUEST_MESSAGE_TYPE_OFFSET] = DHCP_OPT_MESSAGE_TYPE;
	options[DHCP_REQUEST_MESSAGE_TYPE_OFFSET + 1] = DHCP_OPT_MESSAGE_TYPE_LEN;
	options[DHCP_REQUEST_MESSAGE_TYPE_OFFSET + 2] = DHCP_REQUEST;
	/*** Server identifier ***/
	options[DHCP_REQUEST_SERVER_IDENTIFIER_OFFSET] = DHCP_OPT_SERVER_IDENTIFIER;
	options[DHCP_REQUEST_SERVER_IDENTIFIER_OFFSET + 1] = DHCP_OPT_SERVER_IDENTIFIER_LEN;
	// Fill the next 4 bytes with the IP address of the chosen server
	for (i = 0; i < IPV4_ADDR_SIZE; i++)
		options[DHCP_REQUEST_SERVER_IDENTIFIER_OFFSET + 2 + i] = packet->server_ip_addr[i];
	/*** End option ***/
	options[DHCP_REQUEST_END_OFFSET] = DHCP_OPT_END;

	// Send the packet over UDP with a destination IP of 255.255.255.255
	uint8_t broadcast_ip[IPV4_ADDR_SIZE] = {0xFF, 0xFF, 0xFF, 0xFF};
	int32_t retval = send_udp_packet((void*)data, sizeof(dhcp_packet) + DHCP_REQUEST_OPTIONS_SIZE, 
		DHCP_CLIENT_UDP_PORT, broadcast_ip, DHCP_SERVER_UDP_PORT, id);

	// Transition to the next state if we succeeded (and transition back to uninit if we failed)
	if (retval == 0) {
		DHCP_DEBUG("Successfully sent request packet, transitioning to REQUESTING state\n");
		device->dhcp_state = DHCP_STATE_REQUESTING;
	} else {
		DHCP_DEBUG("Failed to send request packet, transitioning to UNINITIALIZED state\n");
		device->dhcp_state = DHCP_STATE_UNINITIALIZED;
	}

	// Free the allocated memory
	kfree(data);
	return retval;
}

/*
 * Receives a DHCP packet and performs appropriate actions (depends on the current state of the interaction)
 *
 * INPUTS: buffer: the buffer containing the contents of the purported DHCP packet
 *         length: the length of the DHCP packet
 *         id: the ID of the Ethernet device this came from
 * OUTPUTS: -1 if the packet was malformed / unexpected and 0 otherwise
 */
int receive_dhcp_packet(uint8_t *buffer, uint32_t length, uint32_t id) {
	// Get the eth_device for this ID
	eth_device *device = get_eth_device(id);

	// Check that the length is long enough to contain an entire DHCP packet
	if (length < sizeof(dhcp_packet)) {
		DHCP_DEBUG("Packet size not large enough to contain DHCP packet, must be malformed\n");
		return -1;
	}

	dhcp_packet *packet = (dhcp_packet*)buffer;

	// Validate the fields we care about
	if (packet->operation != DHCP_OPCODE_SERVER ||
		packet->hw_type != DHCP_HW_TYPE ||
		packet->hw_len != DHCP_HW_LEN ||
		packet->transaction_id != flip_endian32(DHCP_TRANSACTION_ID) ||
		packet->magic_cookie != flip_endian32(DHCP_MAGIC_COOKIE)) {
		DHCP_DEBUG("Fields of received DHCP packet do not match expected values\n");
		return -1;
	}

	// Check the options field for the message type
	// We only handle DHCP offers and DHCP acks
	uint8_t *options = buffer + sizeof(dhcp_packet);
	uint32_t options_length = length - sizeof(dhcp_packet);

	// Iterate through all the options and add them to a linked list
	int i = 0;
	// Initialize the head as the END option
	dhcp_option_list_item *head = kmalloc(sizeof(dhcp_option_list_item));
	head->data.tag = DHCP_OPT_END;
	head->data.length = 0;
	head->data.data = NULL;
	head->next = NULL;

	while (i < options_length) {
		if (options[i] == DHCP_OPT_END)
			break;

		// First, check that the fields are valid 
		// Check that the length byte exists, and if a buffer of that length fits in the total size
		if (i + 1 >= options_length || i + 1 + options[i + 1] >= options_length) {
			// Free the entire linked list
			dhcp_option_list_item *cur;
			for (cur = head; cur != NULL; cur = cur->next)
				kfree(cur);
			// Return -1 because the packet was malformed
			DHCP_DEBUG("DHCP option fields go beyond length of packet\n");
			return -1;
		}

		// If the fields are valid, create a new linked list node and add it to the list
		dhcp_option_list_item *cur = kmalloc(sizeof(dhcp_option_list_item));
		cur->data.tag = options[i];
		cur->data.length = options[i + 1];
		cur->data.data = options + i + 2;
		cur->next = head;
		head = cur;

		// Increment the index by the correct amount (2 bytes for tag and length, and length bytes for the data)
		i += 2 + options[i + 1];
	}

	// Get the message type from the options list
	int16_t message_type = -1;
	dhcp_option_list_item *cur;
	for (cur = head; cur != NULL; cur = cur->next) {
		// If the current option's tag is the message type tag
		if (cur->data.tag == DHCP_OPT_MESSAGE_TYPE) {
			// Copy over the message type and break
			message_type = (int16_t)(*cur->data.data);
			break;
		}
	}

	// Check if there was a message type field
	if (message_type == -1) {
		DHCP_DEBUG("No message type field attached to DHCP packet\n");
		return -1;
	}

	// Finally, invoke specific behavior depending on the message type
	int retval;

	if (message_type == DHCP_OFFER)
		retval = receive_dhcp_offer(packet, head, id);

	if (message_type == DHCP_ACK)
		retval = receive_dhcp_ack(packet, head, id);

	// In this case, we must restart the entire process since the server didn't accept for some reason
	if (message_type == DHCP_NAK) {
		device->dhcp_state = DHCP_STATE_UNINITIALIZED;
		retval = send_dhcp_discover_packet(id);
	}

	// Free the linked list
	dhcp_option_list_item *next;
	for (cur = head; cur != NULL; cur = next) {
		next = cur->next;
		kfree(cur);
	}

	return retval;
}
