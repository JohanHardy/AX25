/*--------------------------------------------------------------------------*
 * OUFTI-1 OBC Embedded software
 * 2008-2009, University of Liege
 *--------------------------------------------------------------------------*
 * AX25_Tx.c
 * Main functions to transmit AX.25 UI frames. Included : bit stuffing, NRZI
 * encoding, scrambling, sync pattern and tail of the frame. The implementa-
 * tion is compatible with G3RUH modem 9600 baud FSK.
 * 
 *
 * Johan Hardy
 *--------------------------------------------------------------------------*/

#include "AX25_Tx.h"
#include "AX25_CRC.h"

/*--------------------------------------------------------------------------*
 * Declaration of the global variables for Tx.
 *--------------------------------------------------------------------------*/
unsigned char bitToSend;
unsigned char lastBit;
unsigned char txMode;
unsigned long shiftRegister;
unsigned char bitSetCounter;
unsigned int nbFlagToSend; 
unsigned int byteCounter; 
unsigned char bitCounter;
unsigned int lengthFrame;

/*--------------------------------------------------------------------------*
* ADF7021 configuration registers (see Excel file) - Freq = 434MHz
*--------------------------------------------------------------------------*/
char ADFR0[4] = { 0b11000000 , 0b11011111 , 0b01110011 , 0b00000010 };
char ADFR1[4] = { 0b10010001 , 0b01010000 , 0b01000111 , 0b00000000 };
char ADFR2[4] = { 0b10000010 , 0b00110000 , 0b11000000 , 0b00000001 };
char ADFR3[4] = { 0b10010011 , 0b01001000 , 0b10111100 , 0b00101001 };
char ADFR8[4] = { 0b00011000 , 0b00000001 , 0b00000000 , 0b00000000 };

/*--------------------------------------------------------------------------*
 * AX.25 TX header : < Address | Control |  PID   > 
 *                   < 14bytes |  1byte  | 1byte  >
 *
 *   - Address : Destination = ON4ULG, SSID = 0x60
 *   - Address : Source = OUFTI1, SSID = 0x61
 *
 *  NB : Characters coded in standard ASCII 7-bits with LSB (HDLC extension
 *      bit) set to 0. Except for the last SSID (means that's the last byte).
 *
 *   - Control : 0x03 (type of frame : UI frame)
 *   - PID     : 0xF0 (no layer 3 protocol implemented)
 *--------------------------------------------------------------------------*/
static const unsigned char AX25_TxHeader[16] = { 
  'O' << 1, 'N' << 1, '4' << 1, 'U' << 1, 'L' << 1, 'G' << 1, 0x60, 
  'O' << 1, 'U' << 1, 'F' << 1, 'T' << 1, 'I' << 1, '1' << 1, 0x61, 
  0x03, 0xF0 
};

/*--------------------------------------------------------------------------*
 * Preparation of the frame (Layer 2). Assembling the info field with the 
 * header, the two flags and the FCS in the buffer.
 *
 * PARAMETERS:
 * *buffer            pointer of the buffer.
 * *info              pointer of the info field to transmit.
 * lengthInfoField    length of the info field (in bytes).
 *--------------------------------------------------------------------------*/
void AX25_prepareUIFrame(char *buffer, char *info, unsigned int lengthInfoField) {
  int i;

  // Check the size of *info.
  if(lengthInfoField > INFO_MAX_SIZE) {
    lengthInfoField = INFO_MAX_SIZE;
  }

  // Set the length of the frame to lengthInfoField + 20 bytes (address + 
  // control + PID + FCS + 2 flags).
  lengthFrame = (unsigned int) (lengthInfoField + 20);

  // Put flags at the right place in the buffer.
  buffer[0] = buffer[lengthFrame-1] = 0x7E;

  // Add the header in the buffer.
  for(i=1; i < (lengthFrame - lengthInfoField - 3); i++) {
    buffer[i] = AX25_TxHeader[i-1];
  }

  // Add the info field in the buffer.
  for(i=17; i < (lengthFrame - 3); i++) {
    buffer[i] = *info;
    info++;
  }

  // Calculation and insertion of the FCS in the buffer.
  AX25_putCRC(buffer, lengthFrame);
}

/*--------------------------------------------------------------------------*
 * Initialization of the transmission. The routine switches txMode from 
 * TX_OFF to TX_DELAY_FLAG and resets the main variables.
 *--------------------------------------------------------------------------*/
void AX25_txInitCfg(void) {
  txMode = TX_DELAY_FLAG;
  nbFlagToSend = TX_DELAY;
  bitToSend = 0;
  lastBit = 0;
  shiftRegister = 0;
  bitSetCounter = 0;
  byteCounter = 0; 
  bitCounter = 0;
  lengthFrame = 0;
}

/*--------------------------------------------------------------------------*
 * NRZI encoding and scrambling operations. The next bit to send from the
 * buffer is first NRZI encoded and then scrambled. The polynom of the
 * scrambler is S(x) = x17 + x12 + 1. The result of the NRZI and scrambling
 * operations is bitToSend (input of the modulator).
 *
 * PARAMETERS:
 * bit              next bit from the buffer to send.
 *--------------------------------------------------------------------------*/
void AX25_txBit(char bit){
  volatile char firstXorScrambler;

  // First XOR of the scrambler : bit17 XOR bit12.
  firstXorScrambler = ((shiftRegister >> 11) & 1) ^ ((shiftRegister >> 16) & 1);

  // NRZI encoding : if bit is a 0 -> make a transistion.
  //                 if bit is a 1 -> no transition.
  if(!bit) {
    if(lastBit == 0) {
      bitToSend = 1 ^ firstXorScrambler;
      firstXorScrambler ^= 0;
      lastBit = 1;  // Refresh the last bit sent.
    }
    else {
      bitToSend = 0 ^ firstXorScrambler;
      firstXorScrambler ^= 1;
      lastBit = 0;  // Refresh the last bit sent.
    }
    bitSetCounter = 0;  // If bit=0 reset bitSetCounter for bit stuffing.
    shiftRegister <<= 1;  // Shift the register of the scrambler to the left.
    if(firstXorScrambler) shiftRegister |= 1;  // Scrambling operation.
  }
  else {
    if(lastBit == 0) {
      bitToSend = 0 ^ firstXorScrambler;
      firstXorScrambler ^= 1;
      lastBit = 0;  // Refresh the last bit sent.
    }
    else {
      bitToSend = 1 ^ firstXorScrambler;
      firstXorScrambler ^= 0;
      lastBit = 1;  // Refresh the last bit sent.
    }
    bitSetCounter++;  // If bit=1 increment bitSetCounter for bit stuffing.
    shiftRegister <<= 1;  // Shift the register of the scrambler to the left.
    if(firstXorScrambler) shiftRegister |= 1;  // Scrambling operation.
  }
}   

/*--------------------------------------------------------------------------*
 * This function checks if we must insert a 0 (bit stuffing).
 *
 * RETURN:
 * 1       if there is a stuffed bit.
 * 0       if not.
 *--------------------------------------------------------------------------*/    
char AX25_checkBitStuffing(void) {
  volatile char firstXorScrambler;

  if(bitSetCounter >= 5) {  // If we have 5 ones -> stuff a 0.
    // First XOR of the scrambler : bit17 XOR bit12.
    firstXorScrambler = ((shiftRegister >> 11) & 1) ^ ((shiftRegister >> 16) & 1);
    
    // NRZI encoding : inertion of a 0 -> make a transistion.
    if(lastBit == 0) {
      bitToSend = 1 ^ firstXorScrambler;
      firstXorScrambler ^= 0;
      lastBit = 1;  // Refresh the last bit sent.
    }
    else {
      bitToSend = 0 ^ firstXorScrambler;
      firstXorScrambler ^= 1;
      lastBit = 0;  // Refresh the last bit sent.
    }
    bitSetCounter = 0;  // A 0 is inserted so reset the counter.
    shiftRegister <<= 1;  // Shift the register of the scrambler to the left.
    if(firstXorScrambler) shiftRegister |= 1;  // Scrambling operation.
    return 1;
  }
  return 0;
}

/*--------------------------------------------------------------------------*
 * AX25_prepareNextBitToSend is the main function. AX25_prepareNextBitToSend
 * prepares the next bit to send to the modulator. This function includes
 * bit stuffing, NRZI encoding and scrambling operations. First, the flags 
 * are sent for delaying and synchronizing the TNC. Then the data are sent.
 * Finally, flags are sent for delimiting the frame.
 *
 * PARAMETERS:
 * *buffer    pointer of the buffer.        
 *
 * RETURN:
 * 1       if there are still bits to send.
 * 0       when all bits are sent.
 *--------------------------------------------------------------------------*/   
char AX25_prepareNextBitToSend(char *buffer) {
  if(txMode != TX_DATA_MODE) {  // If txMode is not TX_DATA_MODE.
    AX25_txBit(buffer[byteCounter] & 1);  // Fetch the bit and encode it.
    bitCounter++;
    buffer[byteCounter] >>= 1;  // Prepare the next bit of the buffer.
    if(bitCounter > 7) {
      bitCounter = 0;  // A byte is sent. Reset the bitCounter.
      nbFlagToSend--;
      if(!nbFlagToSend) {  // Check if there are still flags to send.
        if(txMode == TX_DELAY_FLAG) {
          txMode = TX_DATA_MODE;  // The flags are sent : go to TX_DATA_MODE.
          byteCounter++;  // Next byte to send.
          return 1;
        }
        else {
          txMode = TX_OFF;  // Frame is sent : go to TX_OFF.
          return 0;
        }
      }
      else buffer[byteCounter] = 0x7E;  // Reload flag in the buffer.
    }
    return 1;
  }
  else {
    if(AX25_checkBitStuffing()) return 1;
    AX25_txBit(buffer[byteCounter] & 1);  // Fetch the bit and encode it.
    bitCounter++;
    buffer[byteCounter] >>= 1;  // Prepare the next bit of the buffer.
    if(bitCounter > 7) {
      bitCounter = 0;  // A byte is sent. Reset the bitCounter.
      byteCounter++;  // Next byte to send.
      if(byteCounter == lengthFrame-1) {  // Check if all the data are sent.
        txMode = TX_TAIL_MODE;  // If so : go to TX_TAIL_MODE.
        nbFlagToSend = TX_TAIL;
      }
    }
    return 1;
  }
}
