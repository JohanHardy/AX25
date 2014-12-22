/*--------------------------------------------------------------------------*
 * OUFTI-1 OBC Embedded software
 * 2008-2009, University of Liege
 *--------------------------------------------------------------------------*
 * AX25_Rx.c
 * Main functions to receive AX.25 UI frames. Included : bit stuffing, NRZI
 * decoding and descrambling. The implementation is compatible with G3RUH 
 * modem 9600 baud FSK.
 * 
 *
 * Johan Hardy
 *--------------------------------------------------------------------------*/

#include "AX25_Rx.h"

/*--------------------------------------------------------------------------*
 * Declaration of the global variables for Rx.
 *--------------------------------------------------------------------------*/
unsigned char rxMode;
unsigned char bitSetCounter;
unsigned char lastBit;
unsigned long shiftRegister;
unsigned int byteCounter; 
unsigned char bitCounter;
unsigned char checkBitStuff;

/*--------------------------------------------------------------------------*
* ADF7021 configuration registers (see Excel file) - Freq = 434MHz
*--------------------------------------------------------------------------*/
char ADFR0[4] = { 0b01110000 , 0b10111100 , 0b01110011 , 0b00001010 };
char ADFR1[4] = { 0b00010001 , 0b01010000 , 0b01000111 , 0b00000000 };
char ADFR3[4] = { 0b11010011 , 0b00110000 , 0b10111000 , 0b00101001 };
char ADFR4[4] = { 0b10010100 , 0b00001010 , 0b11010011 , 0b10000000 };  // Invert Data !!
char ADFR5[4] = { 0b10110101 , 0b00011011 , 0b00000000 , 0b00000000 };
char ADFR6[4] = { 0b00000110 , 0b00000000 , 0b00000000 , 0b00000000 };
char ADFR8[4] = { 0b11011000 , 0b00000111 , 0b00000000 , 0b00000000 };

/*--------------------------------------------------------------------------*
 * Initialization of the reception. The routine switches rxMode from 
 * RX_OFF to RX_FIRST_FLAG_MODE and resets the main variables.
 *--------------------------------------------------------------------------*/
void AX25_rxInitCfg(void) {
  rxMode = RX_FIRST_FLAG_MODE;
  bitSetCounter = 0;
  lastBit = 1;
  byteCounter = 0;
  bitCounter = 0;
  checkBitStuff = 0;
}

/*--------------------------------------------------------------------------*
 * NRZI decoding and descrambling operations. The next bit to decode from 
 * the demodulator is first descrambled and then NRZI decoded. The polynom 
 * of the descrambler is D(x) = x17 + x12 + 1. The result of the NRZI decod-
 * ing and descrambling operations is returned.
 *
 * PARAMETER:
 * bit          the bit which has to be decoded.
 *
 * RETURN: 
 * char         the decoded bit.
 *--------------------------------------------------------------------------*/
char AX25_rxBit(char bit) {
  volatile char nextBit;

  // Descramble the bit.
  nextBit = bit ^ (((shiftRegister >> 11) & 1) ^ ((shiftRegister >> 16) & 1));

  // NRZI decoding.
  if(nextBit == lastBit) {  // If so we have a 1.
    lastBit = nextBit;  // Refresh the lastBit received.
    shiftRegister <<= 1;  // Shift the register to the left.
    if(bit) shiftRegister |= 1;  // Update the input of the descrambler.
    return 0x01;  // Return the decoded bit.
  }
  else {  // Otherwise we have a 0.
    lastBit = nextBit;  // Refresh the lastBit received.
    shiftRegister <<= 1;  // Shift the register to the left.
    if(bit) shiftRegister |= 1;  // Update the input of the descrambler.
    return 0x00;  // Return the decoded bit.
  }
}

/*--------------------------------------------------------------------------*
 * This function checks if we have a stuffed bit after 5 ones. If the rxBit 
 * is a 0, it is a stuffed bit. In the other case, we have 6 ones (in other 
 * words, we have a flag).
 *
 * PARAMETER:
 * bit           the bit which has to be analyzed.
 *
 * RETURNS: 
 * 1             if we have a stuffed bit.
 * 0             if we have a flag.
 *--------------------------------------------------------------------------*/
char AX25_checkBitStuffing(char bit) {
  if(AX25_rxBit(bit) == 0x00) {  // Check if the rxBit is a 0.
    checkBitStuff = 0;  // Clear the bit stuffing indicator.
    bitSetCounter = 0;  // Clear the bit set counter.
    return 1;  // If so, it is a stuffed bit.
  }
  else return 0;  // If not, we have a flag.
}

/*--------------------------------------------------------------------------*
 * AX25_analyzeNextBit is the main function. AX25_analyzeNextBit analyzes 
 * the next bit from the demodulator. This function includes bit stuffing, 
 * NRZI decoding and descrambling operations. First, the very first flag 
 * is detected. Then the next flags (for sync) are ignored. Finally, the 
 * data is decoded until we meet a flag.
 *
 * PARAMETERS:
 * *buffer         pointer of the buffer to stock the decoded AX.25 frame.
 * rxBit           the bit from the demodulator.
 *
 * RETURNS: 
 * 1             if the reception is still running.
 * 0             if we have a end-flag and the reception is over.
 *--------------------------------------------------------------------------*/
char AX25_analyzeNextBit(char *buffer, char rxBit) {
  if(rxMode == RX_FIRST_FLAG_MODE) {
    buffer[byteCounter] >>= 1;  // Shift to the right.
    if(AX25_rxBit(rxBit)) buffer[byteCounter] |= 0x80; // Stock the bit.
    if(buffer[byteCounter] == 0x7E) {  // Check if we have a flag.
      rxMode = RX_FLAGS_MODE;  // If so, go to ignoring flags mode.
      byteCounter++;
    }
    return 1;
  }

  if(rxMode == RX_FLAGS_MODE) {
    buffer[byteCounter] >>= 1;  // Shift to the right.
    if(AX25_rxBit(rxBit)) buffer[byteCounter] |= 0x80;  // Stock the bit.
    bitCounter++;  // Increment the bit counter.
    if(bitCounter > 7) {  // Check if we have a byte.
      bitCounter = 0;
      if(buffer[byteCounter] != 0x7E) {  // If the byte is not a flag.
        rxMode = RX_DATA_MODE;  // go to Rx Data mode. 
        byteCounter++;
      }
    }
    return 1;
  }

  if(rxMode == RX_DATA_MODE) {
    if(checkBitStuff) {
      if(AX25_checkBitStuffing(rxBit)) return 1;  // Stuffed bit detected.
      else {  // If it is not a stuffed bit it is a flag.
        buffer[byteCounter] = 0x7E; 
        rxMode = RX_OFF;  // End of the frame go to off mode.
        return 0;
      }
    }
    if(AX25_rxBit(rxBit)) {  // Decode the next bit.
      buffer[byteCounter] >>= 1;   // Shift to the right.
      buffer[byteCounter] |= 0x80;  // Stock the bit.
      bitSetCounter++;  // Count up every 1.
      if(bitSetCounter >= 5) checkBitStuff = 1;  // Check the next bit
                                                 // for bit stuffing.
    }
    else {  // We have a 0.
      buffer[byteCounter] >>= 1;   // Shift to the right.
      bitSetCounter = 0;  // Clear bit set counter.
    }
    bitCounter++;  // Increment the bit counter.
    if(bitCounter > 7) {  // Check if we have a byte.
      bitCounter = 0;
      byteCounter++;
    }
    return 1;
  }
}
