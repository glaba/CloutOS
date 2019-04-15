/* i8259.c - Functions to interact with the 8259 interrupt controller
 * vim:ts=4 noexpandtab
 */

#include "i8259.h"
#include "lib.h"

#define SLAVE_PIN_IRQ 2

/* Interrupt masks to determine which interrupts are enabled and disabled */
uint8_t master_mask; /* IRQs 0-7  */
uint8_t slave_mask;  /* IRQs 8-15 */

/* 
 * Initialize the 8259 PIC
 * 
 * INPUTS: none
 * SIDE EFFECTS: initializes the PIC
 */
void i8259_init(void) {
    /* Start with masking all interrupts */
    master_mask = 0xFF;
    slave_mask = 0xFF;

    /* Sends the 4 ICWs to master and slave */

    /* ICW 1 */
    outb(ICW1, MASTER_8259_PORT);
    outb(ICW1, SLAVE_8259_PORT);

    /* ICW 2 */
    outb(ICW2_MASTER, MASTER_8259_PORT + 1);
    outb(ICW2_SLAVE, SLAVE_8259_PORT + 1);

    /* ICW 3 */
    outb(ICW3_MASTER, MASTER_8259_PORT + 1);
    outb(ICW3_SLAVE, SLAVE_8259_PORT + 1);

    /* ICW 4 */
    outb(ICW4, MASTER_8259_PORT + 1);
    outb(ICW4, SLAVE_8259_PORT + 1);

    /* Mask all the interrupts */
    outb(master_mask, MASTER_8259_PORT + 1);
    outb(slave_mask, SLAVE_8259_PORT + 1);    

    /* Enable slave pin on master */
    enable_irq(SLAVE_PIN_IRQ);
}

/* 
 * Enable (unmask) the specified IRQ 
 *
 * INPUTS: irq_num: the interrupt number to unmask
 * SIDE EFFECTS: unmasks the interrupt requested
 */
void enable_irq(uint32_t irq_num) {
    if (irq_num < 8) {
        /* Unmasks irq pin on master PIC */
        master_mask &= ~(1 << irq_num);
        outb(master_mask, MASTER_8259_PORT + 1);
    }
    else {
        /* Unmasks irq pin on slave PIC */
        slave_mask &= ~(1 << (irq_num - 8));
        outb(slave_mask, SLAVE_8259_PORT + 1);
    }
}

/* 
 * Disable (mask) the specified IRQ 
 *
 * INPUTS: irq_num: the interrupt number to mask
 * SIDE EFFECTS: masks the interrupt requested
 */
void disable_irq(uint32_t irq_num) {
    if (irq_num < 8){
        /* Masks irq pin on master PIC */
        master_mask |= (1 << irq_num);
        outb(master_mask, MASTER_8259_PORT + 1);
    }
    else{
        /* Masks irq pin on slave PIC */
        slave_mask |= (1 << (irq_num - 8));
        outb(slave_mask, SLAVE_8259_PORT + 1);
    }
}

/* 
 * Send end-of-interrupt signal for the specified IRQ 
 *
 * INPUTS: irq_num: the interrupt number for which to send EOI
 * SIDE EFFECTS: sends the EOI signal
 */
void send_eoi(uint32_t irq_num) {
    if (irq_num >= 8){
        /* Sends EOI to slave IRQ (0 to 7) subtracts 8*/
        outb((irq_num - 8) | EOI, SLAVE_8259_PORT);
        /* Sends EOI to master IRQ pin 2 where slave 
         * is connected  */
        outb(2 | EOI, MASTER_8259_PORT);
    }
    else
        /* send EOI to master PIC */
        outb(irq_num | EOI, MASTER_8259_PORT);
}
