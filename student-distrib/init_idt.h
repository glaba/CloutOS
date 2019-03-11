#ifndef _INIT_IDT_H
#define _INIT_IDT_H

/* Fills the IDT with entries and sets settings
   such as privilege level, and gate type */
void initialize_idt();

#endif
