/*--------------------------------------------------------------------------*
 * OUFTI-1 OBC Embedded software
 * 2008-2009, University of Liege
 *--------------------------------------------------------------------------*
 * AX25_CRC.c
 * FCS operations of the AX.25 frame.
 * 
 *
 * Johan Hardy
 *--------------------------------------------------------------------------*/

#include "AX25_CRC.h"

/*--------------------------------------------------------------------------*
 * AX.25 FCS calculation (bitwise method).
 * CRC-16-CCITT G(x) = x16 + x12 + x5 + 1.
 * Polynom = 0x1021.
 *
 * PARAMETERS:
 * *buffer       pointer of the frame buffer.
 * size_frame    length of the frame (in bytes).
 *
 * RETURN:
 * the CRC with a final XORed operation.
 *--------------------------------------------------------------------------*/
unsigned short AX25_computeCRC(char *buffer, unsigned short size_frame) {
  unsigned int i, j;
  unsigned short shiftRegister, outBit;
  char byte;

  // The last flag and the 2 bytes for FCS are removed.
  size_frame = size_frame - 3;

  // Initialization of the Shift Register to 0xFFFF
  shiftRegister = 0xFFFF;

  for(i=1 ; i<size_frame; i++) {  // The first flag is not calculated so i=1.
    byte = buffer[i];

    for(j=0; j<8; j++) {
      outBit = shiftRegister & 0x0001;
      shiftRegister >>= 0x01;  // Shift the register to the right.

      if(outBit != (byte & 0x01)) {
        shiftRegister ^= 0x8408;  // Mirrored polynom.
        byte >>= 0x01;
        continue;
      }
      byte >>= 0x01;
    }
  }
  return shiftRegister ^ 0xFFFF;  // Final XOR.
}

/*--------------------------------------------------------------------------*
 * AX.25 FCS positioning.
 * Put the FCS in the right place in the frame. The FCS is sent MSB first
 * so we prepare the 15th bit of the CRC to be sent first.
 *
 * PARAMETERS:
 * *frame        pointer of the frame buffer.
 * size_frame    length of the frame (in bytes).
 *--------------------------------------------------------------------------*/
void AX25_putCRC(char *frame, unsigned short size_frame) {
  unsigned short crc;

  // FCS calculation.
  crc = AX25_computeCRC(frame, size_frame); 

  // Put the FCS in the right place with the 15th bit to be sent first.
  frame[size_frame - 3] = (crc & 0xff);
  frame[size_frame - 2] = ((crc >> 8) & 0xff);
}


