#ifndef AX25_TX_H
#define AX25_TX_H

// Specifications
#define INFO_MAX_SIZE        256  // Max number of bytes for Info field. (256 is the default value).
#define AX25_FRAME_MAX_SIZE  276  // Max number of bytes for an AX25 UI-frame. (1+14+1+1+256+2+1).
#define TX_DELAY             300  // Number of flags to be sent before the frame.
                                  // Ex : delay = 250 ms hence TX_DELAY = 0,25 * 9600 / 8 = 300 flags.
#define TX_TAIL              100  // Idem but for the tail of the frame.

// Tx Modes
#define TX_OFF         0x00  // Off mode (Tx off).
#define TX_DELAY_FLAG  0x01  // Synchronization mode (sending flags).
#define TX_DATA_MODE   0x02  // Data transmission mode.
#define TX_TAIL_MODE   0x03  // Tail mode (send flags after the frame).

void AX25_prepareUIFrame(char *buffer, char *info, unsigned int length_info_field);
void AX25_txInitCfg(void);
void AX25_txBit(char bit);
char AX25_checkBitStuffing(void);
char AX25_prepareNextBitToSend(char *buffer);

#endif /* AX25_TX_H */
