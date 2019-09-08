#include "tpm2net.h"

// buffers for receiving the TPM2 header, the data we directly read into the led buffer
uint8_t data[TPM2NET_HEADER_SIZE]; 
uint8_t *ledBuffer;

uint16_t expectedPacketSize = 0;

// An UDP instance to let us receive packets over UDP
UDP udp;

void setupTpm2Net(uint8_t *_ledBuffer, uint16_t numLeds) {
    expectedPacketSize = TPM2NET_HEADER_SIZE + numLeds * BPP;
    ledBuffer = _ledBuffer;
    udp.begin(TPM2NET_LISTENING_PORT);
}

bool loopTpm2Net() {
    // Checks for the presence of a UDP packet, and reports the buffer size
    if (udp.parsePacket() < expectedPacketSize)
        return false;
    
    // read the packet into packetBufffer
    uint16_t size = udp.read(data, TPM2NET_HEADER_SIZE);
    if (size < TPM2NET_HEADER_SIZE)
        return false;
    
    /*------------------*/
    /*-- Header check --*/
    /*------------------*/
    // Block Start Byte
    if (data[0] == TPM2NET_HEADER_IDENT) {
        // Block Type: Command Packet
        if (data[1] == TPM2NET_CMD_COMMAND)
            return false; // Don't care

        // Block Type: Frame Data Packet - that's what we want
        if (data[1] == TPM2NET_CMD_DATAFRAME) {
            // Calculate frame size
            uint16_t frameSize = data[2];
            frameSize = (frameSize << 8) + data[3];

            // Use packetNumber to calculate offset
            char packetNumber = data[4];
            char totalPackets = data[5];

            udp.read(ledBuffer, expectedPacketSize - TPM2NET_HEADER_SIZE);
            return true;
        }
    }
}