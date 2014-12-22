#ifndef AX25_CRC_H
#define AX25_CRC_H

unsigned short AX25_computeCRC(char *buffer, unsigned short size_frame);
void AX25_putCRC(char *frame, unsigned short size_frame);

#endif /* AX25_CRC_H */
