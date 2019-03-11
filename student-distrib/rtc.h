#ifndef RTC_H
#define RTC

/* Pin RTC is connected to on IRQ */
#define RTC_IRQ 8
/* Address port allows you to specify index/register number */
#define RTC_ADDRESS_PORT 0x70
/* Data port allows you to read/write to byte in CMOS RAM */
#define RTC_DATA_PORT 0x71
/* Command to turn on interrupts */
#define RTC_INTERRUPT_ENABLE_CMD 0x20
/* Following are addresses of registers in CMOS RAM */
#define REGISTER_A	0x0A
#define	REGISTER_B	0x0B
#define	REGISTER_C	0x0C
#define	REGISTER_D	0x0D
/* Following are masks to enable and disable NMI when
   initializing RTC */
#define NMI_ENABLE_MASK 0x7F
#define NMI_DISABLE_MASK 0x80
/* Following are various interrupt rates */
#define _2HZ_ 0x0F
#define _4HZ_ 0x0E
#define _8HZ_ 0x0D
#define _16HZ_ 0x0C
#define _32HZ_ 0x0B
#define _64HZ_ 0x0A
#define _128HZ_ 0x09
#define _256HZ_ 0x08
#define _512HZ_ 0x07
#define _1025HZ_ 0x06


void init_rtc();
void rtc_handler();
void NMI_enable();
void NMI_disable();

#endif

