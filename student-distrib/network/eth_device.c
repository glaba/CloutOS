#include "eth_device.h"
#include "../list.h"
#include "../spinlock.h"
#include "../kheap.h"

static struct spinlock_t eth_device_spin_lock = SPIN_LOCK_UNLOCKED;

// Type for a linked list of eth_device with IDs
typedef LIST_ITEM_ID_PTR(eth_device, eth_device_list_item) eth_device_list_item;

// Head of linked list of Ethernet devices
eth_device_list_item *eth_device_list_head = NULL;

/*
 * Registers an Ethernet device
 *
 * INPUTS: dev: a struct containing function pointers to interface with the Ethernet device
 *              *** MUST BE EITHER GLOBAL OR ON THE HEAP ***
 * OUTPUTS: -1 on failure or the positive non-zero ID if the device was successfully registered
 * SIDE EFFECTS: sets the receive function pointer in the provided eth_device, as well as the ID
 */
int register_eth_dev(eth_device *dev) {
	eth_device_list_item *new_item = kmalloc(sizeof(eth_device_list_item));

	// Check for a successful allocation
	if (new_item == NULL)
		return -1;

	// Call the init function and return -1 on failure
	if (dev->init(dev) != 0) 
		return -1;

	// Lock the spinlock since we will be modifying the linked list
	spin_lock(&eth_device_spin_lock);

	// Fill in the data for the new list item
	new_item->data = dev;

	// Insert the list item into the linked list with a unique ID and return it
	dev->id = INSERT_WITH_UNIQUE_ID(eth_device_list_item, eth_device_list_head, new_item);

	// Unlock the spinlock since we are done modifying the linked list
	spin_unlock(&eth_device_spin_lock);

	// Set the receive function pointer
	dev->receive = receive_eth_packet;

	return new_item->id;
}

/*
 * Removes the Ethernet device with the specified ID from the list
 *
 * INPUTS: id: the id assigned to the Ethernet device when it was registered
 * SIDE EFFECTS: removes an item from the list of Ethernet devices
 */
void unregister_eth_dev(uint32_t id) {
	eth_device_list_item *cur, *prev;

	// Lock because we will be modifying the linked list
	spin_lock(&eth_device_spin_lock);

	// Loop through the list looking for the device with the specified id
	for (prev = NULL, cur = eth_device_list_head; 
		 cur != NULL; 
		 prev = cur, cur = cur->next) {

		// If this element has the right ID, remove it from the linked list
		if (cur->id == id) {
			// Check if it's the head or not and update the pointers to remove cur
			if (prev == NULL)
				eth_device_list_head = cur->next;
			else
				prev->next = cur->next;

			// Free the memory allocated for the old linked list node
			kfree(cur);
			break;
		}
	}

	// Unlock after finishing linked list operations
	spin_unlock(&eth_device_spin_lock);
}

/*
 * Transmits a packet on the Ethernet device of given ID
 *
 * INPUTS: buffer: buffer containing the Ethernet packet to transmit
 *         size: the size of the buffer
 *         id: the ID of the Ethernet device to transmit the packet on
 * OUTPUTS: -1 on failure and 0 on success
 */
int eth_transmit(uint8_t *buffer, uint16_t size, uint32_t id) {
	// Lock the spinlock since we will be using the linked list
	spin_lock(&eth_device_spin_lock);

	// Iterate through the linked list looking for the desired Ethernet device
	eth_device_list_item *cur;
	for (cur = eth_device_list_head; cur != NULL; cur = cur->next) {
		if (cur->id == id) {
			// Transmit using this device
			int retval = cur->data->transmit(buffer, size);
			
			spin_unlock(&eth_device_spin_lock);
			return retval;
		}
	}

	// Return -1 since the device was not found
	spin_unlock(&eth_device_spin_lock);
	return -1;
}

/*
 * Gets the MAC address associated with the Ethernet device of given ID
 *
 * INPUTS: id: the ID of the Ethernet device to get the MAC address of
 * OUTPUTS: -1 on failure, 0 on success
 *          Puts the MAC address into the provided array
 */
int get_mac_addr(uint32_t id, uint8_t mac_addr[MAC_ADDR_SIZE]) {
	// Iterate through the linked list looking for the desired Ethernet device
	eth_device_list_item *cur;
	for (cur = eth_device_list_head; cur != NULL; cur = cur->next) {
		if (cur->id == id) {
			// Copy the MAC address
			int i;
			for (i = 0; i < MAC_ADDR_SIZE; i++) {
				mac_addr[i] = cur->data->mac_addr[i];
			}
			return 0;
		}
	}

	return -1;
}

/*
 * Gets the IP address associated with the Ethernet device of given ID
 *
 * INPUTS: id: the ID of the Ethernet device to get the IP address of
 * OUTPUTS: -1 on failure, 0 on success
 *          Puts the IP address into the provided array
 */
int get_ip_addr(uint32_t id, uint8_t ip_addr[IPV4_ADDR_SIZE]) {
	// Iterate through the linked list looking for the desired Ethernet device
	eth_device_list_item *cur;
	for (cur = eth_device_list_head; cur != NULL; cur = cur->next) {
		if (cur->id == id) {
			// Copy the IP address
			int i;
			for (i = 0; i < IPV4_ADDR_SIZE; i++) {
				ip_addr[i] = cur->data->ip_addr[i];
			}
			return 0;
		}
	}

	return -1;
}
