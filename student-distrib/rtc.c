#include "rtc.h"
#include "lib.h"
#include "i8259.h"
#include "rtc.h"


/*User set interrupt*/
volatile int interrupt = 0;
/*See if initialization has been called
 *    0 - no
 *    1 - yes
 */
static int init = 0;
/*
 * NMI_enable()
 * Enables non-maskable interrupts
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
 * Disable non-maskable interrupts
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
 * Initialies the RTC registers and enables the interrupt
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

    set_freq(_2HZ_);


    /* Enables NMI again */
    NMI_enable();
    /*init keeps track of whether init_rtc() was
      called or not*/
    init = 1;
    enable_irq(RTC_IRQ);
}

/*
 * set_freq()
 * Sets the frequency to what input says
 *
 * INPUTS: int32_t f: frequency RTC is
 *                    supposed to be set
 *                    to
 * OUTPUTS: -1 for FAIL, otherwise 0
 * SIDE EFFECTS: sets freq
 */
int32_t set_freq(int32_t f) {
    /* Use freq to store real freq byte to write */
    int32_t freq;
    /*Use switch to determine which freqency to
      set freq to.*/
    switch(f) {
        case 2: freq = _2HZ_;break;
        case 4: freq = _4HZ_;break;
        case 8: freq = _8HZ_;break;
        case 16: freq = _16HZ_;break;
        case 32: freq = _32HZ_;break;
        case 64: freq = _64HZ_;break;
        case 128: freq = _128HZ_;break;
        case 256: freq = _256HZ_;break;
        case 512: freq = _512HZ_;break;
        case 1024: freq = _1024HZ_;break;
        default: return -1;/*no need for break*/
    }

    /* Changing the interrupt rate */
    char prev_data;
    outb(REGISTER_A, RTC_ADDRESS_PORT);
    prev_data = inb(RTC_DATA_PORT);
    outb(REGISTER_A, RTC_ADDRESS_PORT);
    outb((prev_data & 0xF0) | freq, RTC_DATA_PORT);
    /*return 4 bytes written*/
    return 4;
}

/*
 * rtc_handler()
 * Handler which gets called every time RTC sends an interrupt
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: Handler for the RTC
 */
void rtc_handler() {
	cli();
	/* Select register C */
	outb(REGISTER_C, RTC_ADDRESS_PORT);
	/* Throw away contents they don't matter :( */
	inb(RTC_DATA_PORT);
	/* Send EOI */
	send_eoi(RTC_IRQ);
    /*set user interrupt to 0*/
    interrupt = 0;

    /*This is checkpoint 2, so we'll ignore this*/
    /* For checkpoint 1 called on RTC interrupt */
    //test_interrupts();

	sti();
}

/*
 * rtc_open()
 * Does necessary stuff to initialize/start rtc
 *
 * INPUTS: none
 * OUTPUTS: 0 for pass
 * SIDE EFFECTS: Initializes RTC if not initialized
 *               and sets freq to 2 HZ
 */
int32_t rtc_open() {
    /*if not initialize, call init_rtc()*/
    if(!init)
      init_rtc();
    /*clear interrupts so nothing stops from setting freq*/
    cli();
    /*set freq to 2 HZ*/
      set_freq(2);
    /*enable interrupts again*/
    sti();
    /*works correctly, so return 0*/
    return 0;
}

/*
 * rtc_close()
 * Does necessary stuff to close rtc
 *
 * INPUTS: none
 * OUTPUTS: 0 for pass
 * SIDE EFFECTS: N/A
 */
int32_t rtc_close() {

    /*nothing to do, so return 0*/
    return 0;
}

/*
 * rtc_read()
 * Waits for an RTC interrupt
 *
 * INPUTS: int32_t fd = file descriptor
 *         void* buf = buffer that's useless for now
 *         int32_t bytes = number of bytes in buf
 * OUTPUTS: 0 for pass
 * SIDE EFFECTS: Wait's for RTC interrupt
 */
int32_t rtc_read(int32_t fd, void* buf, int32_t bytes) {
    /*use user controlled flag for interrupt*/
    interrupt = 1;
    /*enable all interrupts, not necessarily needed*/
    sti();
    /*while loops causes program to poll until interrupt is 0,
      or when rtc_handler() is called and sets interrupt to 0*/
    while (interrupt == 1) {}
    /*everything done correctly, so return 0*/
    return 0;
}

/*
 * rtc_write()
 * Sets frequency to what's specified in buf
 *
 * INPUTS: int32_t fd = file descriptor
 *         const void* buf = buffer that contains frequency
 *         int32_t bytes = number of bytes in buf (should be 4)
 * OUTPUTS: 0 for pass
 * SIDE EFFECTS: Wait's for RTC interrupt
 */
int32_t rtc_write(int32_t fd, const void* buf, int32_t bytes) {
    if(buf == NULL || bytes != 4)
        return -1;
        /*SIDE EFFECT: buf could have 4 bytes, but should
          nevertheless return -1 b/c that's bad coding*/
    /*read in buffer and cast it to freq*/
    int32_t freq;
    freq = *(int32_t *) buf;
    /*clear interrupts for setting freq*/
    cli();
    /*call set_freq to set frequency of RTC*/
    int32_t ret = set_freq(freq);
    /*enables interrupts again*/
    sti();
    /*return number of bytes written or -1 if bad input*/
    return ret;
}
