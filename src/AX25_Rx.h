#ifndef AX25_RX_H
#define AX25_RX_H

// Specifications
#define AX25_FRAME_MAX_SIZE  276  // Max number of bytes for an AX25 UI-frame. (1+14+1+1+256+2+1).

// Rx Modes
#define RX_OFF               0x00  // Receiver is off.
#define RX_FIRST_FLAG_MODE   0x01  // First flag detection mode.
#define RX_FLAGS_MODE        0x02  // Ignoring sync flags.
#define RX_DATA_MODE         0x03  // Data decoder mode.

void AX25_rxInitCfg(void);
char AX25_rxBit(char bit);
char AX25_checkBitStuffing(char bit);
char AX25_analyzeNextBit(char *buffer, char rxBit);

#endif /* AX25_RX_H */
