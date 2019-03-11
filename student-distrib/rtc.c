#include "rtc.h"
#include "lib.h"
#include "i8259.h"
#include "rtc.h"



/*
 * NMI_enable()
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: Enables the non-maskable interrupt
 */
void NMI_enable() {
    outb(inb(RTC_ADDRESS_PORT) & NMI_ENABLE_MASK, RTC_ADDRESS_PORT);
}
/*
 * NMI_disable()
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: Disables the non-maskable interrupt
 */
void NMI_disable() {
    outb(inb(RTC_ADDRESS_PORT) | NMI_DISABLE_MASK, RTC_ADDRESS_PORT);
}

/*
 * init_rtc()
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: Initializes the RTC
 */
void init_rtc() {
    /* Disables NMI to prevent undefined behavior */
    NMI_disable();

    /* Enables interrupt generation in RTC */
    outb(REGISTER_A, RTC_ADDRESS_PORT);
    outb(RTC_INTERRUPT_ENABLE_CMD, RTC_DATA_PORT);

    /* Turns on IR8 periodic interrupt */
    outb(REGISTER_B, RTC_ADDRESS_PORT);
    char prev_data = inb(RTC_DATA_PORT);
    outb(REGISTER_B, RTC_ADDRESS_PORT);
    outb(prev_data | 0x40, RTC_DATA_PORT);

    /* Changing the interrupt rate */
    outb(REGISTER_A, RTC_ADDRESS_PORT);
    prev_data = inb(RTC_DATA_PORT);
    outb(REGISTER_A, RTC_ADDRESS_PORT);
    outb((prev_data & 0xF0) | _2HZ_, RTC_DATA_PORT);

    /* Enables NMI again */
    NMI_enable();

    enable_irq(RTC_IRQ);
}

/*
 * rtc_handler()
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: Handler for the RTC
 */
void rtc_handler() {

	cli();
	/* Select register C */
	outb(REGISTER_C, RTC_ADDRESS_PORT);
	/* Throw away contents they don't matter :(*/
	inb(RTC_DATA_PORT);
	/* Send EOI */
	send_eoi(RTC_IRQ);

    /* For checkpoint 1 called on RTC interrupt */
    test_interrupts();

	sti();
}


