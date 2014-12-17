/*
   insert header here

*/
#ifndef _____OpenBCI_32__
#define _____OpenBCI_32__


#include <DSPI.h>
#include <WProgram.h>
#include "Definitions_32.h"

class OpenBCI_32 {
public:
    DSPI0 spi;  // use DSPI library

// BOARD 
    boolean useAccel;
    boolean useAux;
    void initialize(void);  // ADD DAISY USE outputType
    void printAllRegisters(void);   // ADD DAISY USE outputType
    void sendChannelData(byte); // send the current data with sample number
    void startStreaming(void);  // ADD DAISY USE outputType
    void stopStreaming(void);   // ADD DAISY USE outputType
    void writeAuxData(void);

// ADS1299  
    void initialize_ads(void);
    void updateChannelSettings(void);
    void writeChannelSettings(void);
    void writeChannelSettings(char);
    void setChannelsToDefault(void);
    void setChannelsToEMG(void);
    void setChannelsToECG(void);
    void reportDefaultChannelSettings(void);
    void printADSregisters(void);
    void WAKEUP(void);  // get out of low power mode
    void STANDBY(void); // go into low power mode
    void RESET(void);   // set all register values to default
    void START(void);   // start data acquisition
    void STOP(void);    // stop data acquisition
    void RDATAC(void);  // go into read data continuous mode 
    void SDATAC(void);  // get out of read data continuous mode
    void RDATA(void);   // read data one-shot   
    byte RREG(byte);           // read one register
    void RREGS(byte, byte);         // read multiple registers
    void WREG(byte, byte);          // write one register
    void WREGS(byte, byte);         // write multiple registers
    byte ADS_getDeviceID();         
    void printRegisterName(byte);   // used for verbosity
    void printHex(byte);            // used for verbosity
    void updateChannelData(void);   // retrieve data from ADS
    byte xfer(byte);        // SPI Transfer function
    int DRDY, CS, RST; 		// pin numbers for DataRead ChipSelect Reset pins 
    void resetADS(void);    // reset all the ADS1299's settings.  Call however you'd like
    void startADS(void);
    void stopADS(void);
    void activateChannel(int);                  // enable the selected channel
    void deactivateChannel(int);                // disable given channel 1-8(16)
    void configureLeadOffDetection(byte, byte);
    void changeChannelLeadOffDetect(void);
    void configureInternalTestSignal(byte, byte);  
    boolean isDataAvailable(void);
    void ADS_writeChannelData(void);
    void setSRB1(boolean desired_state);
    void ADS_printDeviceID(void);   // add to read LIS3DH dev ID
    int stat;           // used to hold the status register
    byte regData[24];          // array is used to mirror register data
    int channelDataInt[9];    // array used when reading channel data as ints
    byte channelDataRaw[24];    // array to hold raw channel data
    boolean verbosity;      // turn on/off Serial verbosity
    char channelSettings[8][6];  // array to hold current channel settings
    char defaultChannelSettings[6];  // default channel settings
    boolean useInBias[8];        // used to remember if we were included in Bias before channel power down
    boolean useSRB1;             // used to keep track of if we are using SRB1
    boolean useSRB2[8];          // used to remember if we were included in SRB2 before channel power down
    char leadOffSettings[8][2];  // used to control on/off of impedance measure for P and N side of each channel
    short auxData[3];

// LIS3DH
    short axisData[3];
    void initialize_accel(byte);    // initialize 
    void enable_accel(byte);  // start acceleromoeter with default settings
    void disable_accel(void); // stop data acquisition and go into low power mode
    byte LIS3DH_getDeviceID(void);    
    byte LIS3DH_read(byte);     // read a register on LIS3DH
    void LIS3DH_write(byte,byte);   // write a register on LIS3DH
    int LIS3DH_read16(byte);    // read two bytes, used to get axis data
    int getX(void);     
    int getY(void);
    int getZ(void);
    boolean LIS3DH_DataReady(void); // check LIS3DH_DRDY pin
    boolean LIS3DH_DataAvailable(void); // check LIS3DH STATUS_REG2
    void LIS3DH_readAllRegs(void);
    void LIS3DH_writeAxisData(void);
    void LIS3DH_updateAxisData(void);

    void csLow(int);
    void csHigh(int);


private:
// ADS1299
    boolean isRunning;  
// LIS3DH
    int DRDYpinValue;
    int lastDRDYpinValue;

};

#endif
