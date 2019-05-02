#include <stdint.h>

#include "ece391support.h"
#include "ece391syscall.h"

char their_buf[1024];
char our_buf[1024];

char our_packet[2000];

int main ()
{
    while (1) {
        ece391_fdputs(1, "Us: ");
        int our_len = ece391_read(0, our_buf, 1024);
        our_packet[0] = 192;
        our_packet[1] = 168;
        our_packet[2] = 10;
        our_packet[3] = 4;
        *(uint16_t*)(our_packet + 4) = 80;
        *(uint16_t*)(our_packet + 6) = 2280;
        // Copy the message into our_packet
        int i;
        for (i = 0; i < our_len; i++) {
            our_packet[8 + i] = our_buf[i];
        }
        // Send the packet
        ece391_write(3, our_packet, our_len + 8);

        // Read their message and print it out
        ece391_fdputs(1, "Them: ");
        int their_len = ece391_read(3, their_buf, 1024);
        ece391_write(1, their_buf, their_len);
        ece391_write(1, "\n", 1);
    }

    return 0;
}

