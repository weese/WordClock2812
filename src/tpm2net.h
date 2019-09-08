#include "application.h"

//define some tpm constants
#define TPM2NET_LISTENING_PORT 65506

// set down if arduino with lesser UDP_PACKET_SIZE + PIXEL_DATA bytes of RAM is used
// keep track of your arduino board ram !!!
#define UDP_PACKET_SIZE   1600
#define PIXEL_DATA        (2048*3) /// 2048 PIXEL!

// Define some tpm2.net constants
#define TPM2NET_LISTENING_PORT  65506
#define TPM2NET_HEADER_SIZE     6
#define TPM2NET_HEADER_IDENT    0x9C
#define TPM2NET_CMD_DATAFRAME   0xDA
#define TPM2NET_CMD_COMMAND     0xC0
#define TPM2NET_CMD_ANSWER      0xAC
#define TPM2NET_FOOTER_IDENT    0x36
#define TPM2NET_PACKET_TIMEOUT  0xFA    // 1/4 of a second

// 3 bytes per pixel or 24bit (RGB)
#define BPP                     3

void setupTpm2Net(uint8_t *ledBuffer, uint16_t numLeds);
bool loopTpm2Net();
