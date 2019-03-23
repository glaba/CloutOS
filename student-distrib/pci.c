#include "pci.h"
#include "paging.h"

// Uncomment PCI_DEBUG_ENABLE to enable debugging
#define PCI_DEBUG_ENABLE
#ifdef PCI_DEBUG_ENABLE
	#define PCI_DEBUG(f, ...) printf(f, ##__VA_ARGS__)
#else
	#define PCI_DEBUG(f, ...) // Nothing
#endif

// The port numbers for the configuration space address and data registers
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

// Addresses of PCI configuration space registers / related information
#define PCI_VENDOR_ID_REGISTER      0x0 
#define PCI_DEVICE_ID_REGISTER      0x2
#define PCI_COMMAND_REGISTER        0x4
#define PCI_INTERRUPT_LINE_REGISTER 0x3C
#define PCI_BAR_BASE                0x10
	// The number of PCI BAR registers
	#define PCI_BAR_LEN          6
	// Masks for BAR registers
	#define PCI_BAR_IS_IO_SPACE_MASK 0x1
	#define PCI_BAR_IO_SPACE_BASE_ADDR_MASK        0xFFFFFFFC
	#define PCI_BAR_MEMORY_SPACE_BASE_ADDR_MASK    0xFFFFFFF0
	#define PCI_BAR_MEMORY_SPACE_TYPE_MASK         0x00000006
	#define PCI_BAR_MEMORY_SPACE_PREFETCHABLE_MASK 0x00000008
	// Value of Type field of memory space BAR that indicates a 32-bit wide base register
	#define PCI_BAR_32_BIT_REG_TYPE 0x0
#define PCI_HEADER_TYPE        0xE
	// The mask for the PCI Header Type register that ignores whether or not the device is multifunction
	#define PCI_HEADER_TYPE_MASK 0x7F
	// Value of header type after masking that indicates that the device is a regular endpoint (not a bridge)
	#define PCI_HEADER_TYPE_GENERAL_DEVICE 0x0

// Bit fields that can be set in the PCI command register
#define PCI_ALLOW_BUS_MASTER           0x4
#define PCI_ENABLE_MEMORY_SPACE_ACCESS 0x2
#define PCI_ENABLE_IO_SPACE_ACCESS     0x1
#define PCI_DISABLE_INTERRUPTS         0x200

// Maximum number of buses, slots, and functions 
#define NUM_BUSES     256
#define NUM_SLOTS     256
#define NUM_FUNCTIONS 8

// This is how it will be for now, until we figure out dynamically loading drivers
#define NUM_DRIVERS 8

// Spinlock that prevents multiple initialization of PCI devices
static struct spinlock_t pci_spin_lock = SPIN_LOCK_UNLOCKED;

// Array of drivers
pci_driver pci_drivers[NUM_DRIVERS];
// Array of PCI function data corresponding to the drivers in pci_drivers
pci_function pci_functions[NUM_DRIVERS];
// Number of drivers loaded
int num_loaded_drivers = 0;

// Function prototypes for functions not exposed in the header file
int initialize_pci_function(pci_driver *driver, pci_function *func, uint8_t bus, uint8_t slot, uint8_t function);
uint32_t pci_config_get_addr(int8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint32_t _pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t size);
void _pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t size, uint32_t data);

/*
 * Registers a PCI device driver which will be applied when enumerate_pci_devices is called
 */
int register_pci_driver(pci_driver driver) {
	// Check the driver for validity
	if (driver.init_device == NULL) {
		return -1;
	}

	// Lock before we use pci_drivers, pci_functions and num_loaded_drivers
	spin_lock(&pci_spin_lock);
	
	// If there is enough room to register a driver
	if (num_loaded_drivers < NUM_DRIVERS) {
		// Add an item into the drivers array
		pci_drivers[num_loaded_drivers] = driver;
		// Set the device as uninitialized
		pci_functions[num_loaded_drivers].inited = 0;
		num_loaded_drivers++;

		spin_unlock(&pci_spin_lock);

		PCI_DEBUG("Registered driver for VendorID 0x%x, DeviceID 0x%x and Function %d\n", 
			driver.vendor, driver.device, driver.function);

		// Return success since we were able to add the driver
		return 0;
	}

	spin_unlock(&pci_spin_lock);

	// Return failure if we cannot add the driver
	return -1;
}

/*
 * Undoes any initialization performed by initialize_pci_function 
 *
 * INPUTS: func: the PCI function for which to undo initialization steps
 */
void uninitialize_pci_function(pci_function *func) {
	// Undo written configuration and disable interrupts
	pci_config_write(func, PCI_COMMAND_REGISTER, 2, PCI_DISABLE_INTERRUPTS);

	// Remove memory mapped I/O from kernel page directory
	int i;
	for (i = 0; i < PCI_BAR_LEN; i++) {
		if (func->reg_base[i] != 0)
			unmap_region_from_kernel((void*)func->reg_base[i], func->reg_size[i]);
	}

	func->inited = 0;
}

/*
 * Initializes the memory mapped I/O and interrupts for the provided PCI function,
 *  assuming that the device has not already been initialized
 *
 * This function does not work in general for all cases or all devices, and is much less
 *  thorough than the corresponding function in the Linux kernel (but is also much simpler)
 *
 * INPUTS: driver: the driver for this PCI function
 *         bus, slot, function: the corresponding values for the PCI device being initialized
 * OUTPUTS: returns 0 if successful and -1 if it failed
 * SIDE EFFECTS: fills in all fields of func
 */
int initialize_pci_function(pci_driver *driver, pci_function *func, uint8_t bus, uint8_t slot, uint8_t function) {
	PCI_DEBUG("Begin generic initialization of PCI device - VendorID: 0x%x, DeviceID: 0x%x\n", driver->vendor, driver->device);

	func->bus = bus;
	func->slot = slot;
	func->function = function;

	// Check the header type to see if it is a regular endpoint (as opposed to a bridge)
	// This is indicated by bits 0 to 6 being 0
	// If it isn't, we can't handle it and return failure
	if ((pci_config_read(func, PCI_HEADER_TYPE, 1) & PCI_HEADER_TYPE_MASK) != PCI_HEADER_TYPE_GENERAL_DEVICE) {
		PCI_DEBUG("   Not a regular PCI endpoint, failing\n");
		goto pci_init_failed;
	}

	// Write correct configuration to the command register
	// This also implicitly enables interrupts
	pci_config_write(func, PCI_COMMAND_REGISTER, 2, 
		PCI_ALLOW_BUS_MASTER | PCI_ENABLE_IO_SPACE_ACCESS | PCI_ENABLE_MEMORY_SPACE_ACCESS);

	PCI_DEBUG("   Wrote values into command register\n");

	// Setup BARs (base address registers)
	uint32_t bar;
	for (bar = 0; bar < PCI_BAR_LEN; bar++) {
		// Initialize the base address and size to 0
		func->reg_base[bar] = 0;
		func->reg_size[bar] = 0;

		// Read the value of the current BAR register
		uint32_t original_value = pci_config_read(func, PCI_BAR_BASE + 4 * bar, 4);

		// If the BAR uses I/O space addressing or memory space addressing
		if (original_value & PCI_BAR_IS_IO_SPACE_MASK) {
			// The BAR uses I/O space addressing
			func->is_memory_space_reg[bar] = 0;

			// Simply copy the value
			func->reg_base[bar] = original_value & PCI_BAR_IO_SPACE_BASE_ADDR_MASK;

			PCI_DEBUG("   For I/O  BAR %d: base_addr=0x%x\n", bar, func->reg_base[bar]);
		} else {
			// The BAR uses memory space addressing
			func->is_memory_space_reg[bar] = 1;

			// We only support base registers that map to 32-bit memory space (obviously)
			uint8_t type = (original_value & PCI_BAR_MEMORY_SPACE_TYPE_MASK) >> 1;
			if (type != PCI_BAR_32_BIT_REG_TYPE) {
				PCI_DEBUG("   BAR %d does not use 32-bit MMIO, failing\n", bar);
				goto pci_init_failed;
			}

			uint32_t base_addr = original_value & PCI_BAR_MEMORY_SPACE_BASE_ADDR_MASK;

			// Write -1 to BAR to get the amount of address space required
			pci_config_write(func, PCI_BAR_BASE + 4 * bar, 4, 0xFFFFFFFF);

			// Take negative of masked value to get size (essentially a magic procedure specified in the documentation)
			uint32_t size = -(pci_config_read(func, PCI_BAR_BASE + 4 * bar, 4) & PCI_BAR_MEMORY_SPACE_BASE_ADDR_MASK);
		
			// Write the original value back to the BAR register
			pci_config_write(func, PCI_BAR_BASE + 4 * bar, 4, original_value);
		
			// If for some reason the base pointer is null but the size is non-zero
			//  there was a misconfiguration at some point and we have failed
			if (base_addr == 0 && size != 0) {
				PCI_DEBUG("   Base and size values are inconsistent, failing\n");
				goto pci_init_failed;
			}

			// If there should be a MMIO region
			if (base_addr != 0) {
				// Page the MMIO region into the kernel page directory
				if (map_region_into_kernel((void*)base_addr, size, 
						PAGE_DISABLE_CACHE | PAGE_READ_WRITE | PAGE_WRITE_THROUGH_CACHE) != 0) {
					
					// If this fails, quit device initialization
					PCI_DEBUG("   Adding MMIO to kernel page directory failed, failing\n");
					goto pci_init_failed;
				}
			}

			// The MMIO region is valid and was successfully mapped in
			func->reg_base[bar] = base_addr;
			func->reg_size[bar] = size;

			PCI_DEBUG("   For MMIO BAR %d: base_addr=0x%x, size=0x%x\n", bar, base_addr, size);
		}
	}

	// Finally, enable interrupts
	pci_config_write(func, PCI_INTERRUPT_LINE_REGISTER, 1, PCI_IRQ);
	func->irq = PCI_IRQ;
	PCI_DEBUG("   Set device to use IRQ%d\n", PCI_IRQ);

	// Device has been inited	
	func->inited = 1;
	PCI_DEBUG("   Successfully inited device\n");
	return 0;

	// Exception handling is a standard use for goto labels in C
pci_init_failed:
	PCI_DEBUG("   Initialization failed\n");
	
	// Undo any initialization steps taken
	uninitialize_pci_function(func);

	// Return failure
	return -1;
}

/*
 * Brute force scans for all connected PCI devices and initializes them using the
 *  existing drivers in pci_drivers if so
 *
 * SIDE EFFECTS: initializes uninitialized PCI devices that have registered drivers
 */
void enumerate_pci_devices() {
	uint16_t vendor, device, function, bus, slot;
	int i;

	// Lock spinlock so that no new drivers can be added while enumeration is occurring
	spin_lock(&pci_spin_lock);

	// Iterate through all possible buses, slots, and functions
	// Basically bruteforce probe all possible devices
	for (bus = 0; bus < NUM_BUSES; bus++) {
		for (slot = 0; slot < NUM_SLOTS; slot++) {
			for (function = 0; function < NUM_FUNCTIONS; function++) {
				// Code copied from osdev.org
				// If the vendor is 0xFFFF that means there is no device there
				if ((vendor = _pci_config_read(bus, slot, function, PCI_VENDOR_ID_REGISTER, 2)) != 0xFFFF) {
					// Get the device code of the device
					device = _pci_config_read(bus, slot, function, PCI_DEVICE_ID_REGISTER, 2);

					PCI_DEBUG("Detected device with VendorID 0x%x and DeviceID 0x%x\n", vendor, device);

					// Check if there is a driver for this device
					for (i = 0; i < NUM_DRIVERS; i++) {
						if (pci_drivers[i].vendor == vendor && 
							pci_drivers[i].device == device && 
							pci_drivers[i].function == function) {

							// Initialize the function's memory mapped I/O and interrupts
							if (initialize_pci_function(&pci_drivers[i], &pci_functions[i], bus, slot, function) == 0) {
								// Use the driver to initialize the device if regular init succeeded
								PCI_DEBUG("Begin driver initialization - VendorID 0x%x, DeviceID 0x%x\n", 
									pci_drivers[i].vendor, pci_drivers[i].device);

								if (pci_drivers[i].init_device(&pci_functions[i]) != 0) {
									PCI_DEBUG("Driver initialization failed\n");
									// If the driver init failed, uninitialize everything
									uninitialize_pci_function(&pci_functions[i]);
								}
							}
						}
					}
				}
			}
		}
	}

	spin_unlock(&pci_spin_lock);
}

uint32_t pci_config_get_addr(int8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
	uint32_t address;
	uint32_t lbus  = (uint32_t)bus;
	uint32_t lslot = (uint32_t)slot;
	uint32_t lfunc = (uint32_t)func;
 
	// Creates a configuration space address of the correct format 
	address = (uint32_t)((lbus << 16) | (lslot << 11) |
			  (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

	return address;
}

/* 
 * Reads a 32-bit value from the PCI Configuration Space
 *
 * INPUTS: bus, slot: the number of the PCI bus and the slot of the device in that bus
 *         func: a specific function in the device (if multiple functions are supported)
 *         offset: the offset into the 256-byte configuration space
 *         size: the number of bytes to read (must range from 1-4, and cannot straddle a 4-byte boundary)
 *               this is in the little endian direction
 * OUTPUTS: the 32-bit value at the specified location in the configuration space 
 *          for the specified device 
 */
uint32_t _pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t size) {
	uint32_t data, i;
	uint32_t mask = 1;
	uint32_t address = pci_config_get_addr(bus, slot, func, (offset / 4) * 4);

	// Write the address to the address register
	outl(address, PCI_CONFIG_ADDRESS);

	// At this point, data will be placed into the data register
	// Read in the data
	data = inl(PCI_CONFIG_DATA);

	// Return only the bits that the caller wanted
	data = data >> ((offset % 4) * 8); // First, shift by the offset from 4-byte alignment
	// Then, create a mask: 0xFF if size = 1, 0xFFFF if size = 2, etc.
	for (i = 0; i < size; i++)
		mask = mask << 8;
	mask -= 1;
	// Mask out the bits of data that we don't want
	data = data & mask; 

	return data;
}

/* 
 * Writes a 32-bit value to the PCI Configuration Space
 *
 * INPUTS: bus, slot: the number of the PCI bus and the slot of the device in that bus
 *         func: a specific function in the device (if multiple functions are supported)
 *         offset: the offset into the 256-byte configuration space
 *         size: the number of bytes to write (must range from 1-4, and cannot straddle a 4-byte boundary)
 *         data: the size-byte value to write
 */
void _pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t size, uint32_t data) {
	uint32_t cur_data, mask;
	uint32_t address = pci_config_get_addr(bus, slot, func, (offset / 4) * 4);

	// Write out the address to the address register
	outl(address, PCI_CONFIG_ADDRESS);
	// Read in the data that is currently stored there
	cur_data = inl(PCI_CONFIG_DATA);

	// Clear out the bits that will be overwritten in cur_data by data
	mask = (1 << (size * 8)) - 1;
	mask = mask << ((offset % 4) * 8);
	mask = ~mask;
	cur_data &= mask;

	// Mask any unwanted bits in data and shift data to the correct position
	data = data & ((1 << (size * 8)) - 1);
	data = data << ((offset % 4) * 8);

	// Fill in the cleared out bits with the correctly masked and shifted data
	cur_data |= data;

	// Output the new value
	outl(address, PCI_CONFIG_ADDRESS);
	outl(cur_data, PCI_CONFIG_DATA);
}

/* 
 * Calls the other pci_config_read with bus, slot, func values from pci_function struct
 */
uint32_t pci_config_read(pci_function *func, uint8_t offset, uint8_t size) {
	return _pci_config_read(func->bus, func->slot, func->function, offset, size);
}

/* 
 * Calls the other pci_config_write with bus, slot, func values from pci_function struct
 */
void pci_config_write(pci_function *func, uint8_t offset, uint8_t size, uint32_t data) {
	_pci_config_write(func->bus, func->slot, func->function, offset, size, data);
}
