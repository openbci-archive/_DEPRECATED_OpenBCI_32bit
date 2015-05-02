
/*
    BUILDING OUT THE LIBRARY-TO-RULE-THEM-ALL HERE

*/

#include "OpenBCI_32.h"


// <<<<<<<<<<<<<<<<<<<<<<<<<  BOARD WIDE FUNCTIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

void OpenBCI_32::initialize(){
  pinMode(SD_SS,OUTPUT); digitalWrite(SD_SS,HIGH);  // de-select SDcard if present
  spi.begin();
  spi.setSpeed(4000000);  // try also 8MHz, 10MHz is max for LIS3DH
  spi.setMode(DSPI_MODE0);  // default to SD card mode!
  initialize_ads(); // hard reset ADS, set pin directions 
  initialize_accel(SCALE_4G); // set pin directions, G scale, DRDY interrupt, power down
}

void OpenBCI_32::printAllRegisters(){
  printADSregisters();
  LIS3DH_readAllRegs(); 
}

void OpenBCI_32::startStreaming(){
  if(useAccel){enable_accel(RATE_50HZ);}
  startADS();
} 

void OpenBCI_32::sendChannelData(byte sampleNumber){
  Serial0.write(sampleNumber); // 1 byte
  ADS_writeChannelData();      // 24 bytes
  if(useAux){ 
    writeAuxData();            // 3 16bit shorts
    useAux = false;
  }else{ 
    LIS3DH_writeAxisData();    // 3 16bit shorts
  }
}

void OpenBCI_32::stopStreaming(){
  stopADS();
  if(useAccel){disable_accel();}
}

//SPI communication method
byte OpenBCI_32::xfer(byte _data) 
{
    byte inByte;
    inByte = spi.transfer(_data);
    return inByte;
}

//SPI slave select method
void OpenBCI_32::csLow(int SS)
{
  switch(SS){
    case ADS_SS:
      spi.setMode(DSPI_MODE1); spi.setSpeed(4000000); digitalWrite(ADS_SS, LOW); break;
    case LIS3DH_SS:
      spi.setMode(DSPI_MODE3); spi.setSpeed(4000000); digitalWrite(LIS3DH_SS, LOW); break;
    case SD_SS:
      spi.setMode(DSPI_MODE0); spi.setSpeed(20000000); digitalWrite(SD_SS, LOW); break;
    case DAISY_SS:
      spi.setMode(DSPI_MODE1); spi.setSpeed(4000000); digitalWrite(DAISY_SS, LOW); break;  
    default: break;
  }
}

void OpenBCI_32::csHigh(int SS)
{
  switch(SS){
    case ADS_SS:
      digitalWrite(ADS_SS, HIGH); spi.setSpeed(20000000); break;
    case LIS3DH_SS:
      digitalWrite(LIS3DH_SS, HIGH); spi.setSpeed(20000000); break;
    case SD_SS:
      digitalWrite(SD_SS, HIGH); spi.setSpeed(20000000); break;
    case DAISY_SS:
      digitalWrite(DAISY_SS, HIGH); spi.setSpeed(20000000); break;  
    default:
      break;
  }
  spi.setMode(DSPI_MODE0);  // DEFAULT TO SD MODE!
}

void OpenBCI_32::writeAuxData(){
  for(int i=0; i<3; i++){
    Serial0.write(highByte(auxData[i])); // write 16 bit axis data MSB first
    Serial0.write(lowByte(auxData[i]));  // axisData is array of type short (16bit)
    auxData[i] = 0;   // reset auxData bytes to 0
  }
}

// <<<<<<<<<<<<<<<<<<<<<<<<<  END OF BOARD WIDE FUNCTIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
// *************************************************************************************
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  ADS1299 FUNCTIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>


// void OpenBCI_32::initialize(int _DRDY, int _RST, int _CS){
void OpenBCI_32::initialize_ads(){
    
    pinMode(ADS_SS, OUTPUT); digitalWrite(ADS_SS,HIGH); 
    pinMode(DAISY_SS, OUTPUT); digitalWrite(DAISY_SS,HIGH); 
// recommended power up sequence requiers Tpor (~32mS)	
    delay(50);				
    pinMode(ADS_RST,OUTPUT);  
    digitalWrite(ADS_RST,LOW);
    delayMicroseconds(4);	// toggle reset pin
    digitalWrite(ADS_RST,HIGH);
    delayMicroseconds(20);	// recommended to wait 18 Tclk before using device (~8uS);    
// initalize the  data ready chip select and reset pins:
    pinMode(ADS_DRDY, INPUT);	
    delay(100);
    // Serial0.println("got to first spi xfer");
    resetADS(); // software reset on-board and on-daisy if present
    // DEFAULT CHANNEL SETTINGS FOR ADS
    defaultChannelSettings[POWER_DOWN] = NO;        // on = NO, off = YES
    defaultChannelSettings[GAIN_SET] = ADS_GAIN24;     // Gain setting
    defaultChannelSettings[INPUT_TYPE_SET] = ADSINPUT_NORMAL;// input muxer setting
    defaultChannelSettings[BIAS_SET] = YES;    // add this channel to bias generation
    defaultChannelSettings[SRB2_SET] = YES;       // connect this P side to SRB2
    defaultChannelSettings[SRB1_SET] = NO;        // don't use SRB1
    for(int i=0; i<8; i++){
      for(int j=0; j<6; j++){
        channelSettings[i][j] = defaultChannelSettings[j];
      }
    }
    for(int i=0; i<8; i++){
      useInBias[i] = true;    // keeping track of Bias Generation
      useSRB2[i] = true;
      useSRB1 = false;
    }
    writeChannelSettings();
    // defaultChannelBitField = RREG(CH1SET);
    WREG(CONFIG3,0b11101100); delay(1);  // enable internal reference drive and etc.
    for(int i=0; i<8; i++){  // turn off the impedance measure signal
      leadOffSettings[i][PCHAN] = OFF;
      leadOffSettings[i][NCHAN] = OFF;
    }
    verbosity = false;      // when verbosity is true, there will be Serial feedback
};

void OpenBCI_32::updateChannelData(){
    byte inByte;
    int byteCounter = 0;
    csLow(ADS_SS);				//  open SPI
    for(int i=0; i<3; i++){ 
      inByte = xfer(0x00);		//  read status register (1100 + LOFF_STATP + LOFF_STATN + GPIO[7:4])
      stat = (stat << 8) | inByte;
    }
    for(int i = 0; i<8; i++){
      for(int j=0; j<3; j++){		//  read 24 bits of channel data in 8 3 byte chunks
        inByte = xfer(0x00);
        channelDataRaw[byteCounter] = inByte;  // raw data gets streamed to the radio
        byteCounter++;
        channelDataInt[i] = (channelDataInt[i]<<8) | inByte;
      }
    }
    csHigh(ADS_SS);				//  close SPI
	// need to convert 24bit to 32bit if using the filter
	for(int i=0; i<8; i++){			// convert 3 byte 2's compliment to 4 byte 2's compliment	
	  if(bitRead(channelDataInt[i],23) == 1){	
	    channelDataInt[i] |= 0xFF000000;
	  }else{
	    channelDataInt[i] &= 0x00FFFFFF;
	  }
	}

}

//reset all the ADS1299's settings.  Call however you'd like.  Stops all data acquisition
void OpenBCI_32::resetADS()
{
  RESET();             // send RESET command to default all registers
  SDATAC();            // exit Read Data Continuous mode to communicate with ADS
  delay(100);
  // turn off all channels
  for (int chan=1; chan <= 8; chan++) {
    deactivateChannel(chan);
  } 
};

void OpenBCI_32::setChannelsToDefault(void){
  for(int i=0; i<8; i++){
    for(int j=0; j<6; j++){
      channelSettings[i][j] = defaultChannelSettings[j];
    }
  }
  writeChannelSettings();
  for(int i=0; i<8; i++){  // turn off the impedance measure signal
    leadOffSettings[i][PCHAN] = OFF;
    leadOffSettings[i][NCHAN] = OFF;
  }
  changeChannelLeadOffDetect();
  for(int i=0; i<8; i++){
    channelSettings[i][SRB1_SET] = NO;
  }
  WREG(MISC1,0x00);
}

void OpenBCI_32::setChannelsToEMG(void){
  channelSettings[0][POWER_DOWN] = NO;        // NO = on, YES = off
  channelSettings[0][GAIN_SET] = ADS_GAIN12;     // Gain setting
  channelSettings[0][INPUT_TYPE_SET] = ADSINPUT_NORMAL;// input muxer setting
  channelSettings[0][BIAS_SET] = NO;    // add this channel to bias generation
  channelSettings[0][SRB2_SET] = NO;       // connect this P side to SRB2
  channelSettings[0][SRB1_SET] = NO;        // don't use SRB1
  for(int i=1; i<8; i++){
    for(int j=0; j<6; j++){
      channelSettings[i][j] = channelSettings[0][j];
    }
  }
  writeChannelSettings();
}

void OpenBCI_32::setChannelsToECG(void){
  channelSettings[0][POWER_DOWN] = NO;        // NO = on, YES = off
  channelSettings[0][GAIN_SET] = ADS_GAIN12;     // Gain setting
  channelSettings[0][INPUT_TYPE_SET] = ADSINPUT_NORMAL;// input muxer setting
  channelSettings[0][BIAS_SET] = NO;    // add this channel to bias generation
  channelSettings[0][SRB2_SET] = NO;       // connect this P side to SRB2
  channelSettings[0][SRB1_SET] = NO;        // don't use SRB1
  for(int i=1; i<8; i++){
    for(int j=0; j<6; j++){
      channelSettings[i][j] = channelSettings[0][j];
    }
  }
  writeChannelSettings();
}

void OpenBCI_32::reportDefaultChannelSettings(void){

    Serial0.write(defaultChannelSettings[POWER_DOWN] + '0');        // on = NO, off = YES
    Serial0.write((defaultChannelSettings[GAIN_SET] >> 4) + '0');     // Gain setting
    Serial0.write(defaultChannelSettings[INPUT_TYPE_SET] +'0');// input muxer setting
    Serial0.write(defaultChannelSettings[BIAS_SET] + '0');    // add this channel to bias generation
    Serial0.write(defaultChannelSettings[SRB2_SET] + '0');       // connect this P side to SRB2
    Serial0.write(defaultChannelSettings[SRB1_SET] + '0');        // don't use SRB1
}

// channel settings: enabled/disabled, gain, input type, SRB2, SRB1 
void OpenBCI_32::writeChannelSettings(void){
  byte setting;
  boolean use_SRB1 = false;
//proceed...first, disable any data collection
  SDATAC(); delay(1);      // exit Read Data Continuous mode to communicate with ADS

  for(byte i=0; i<8; i++){ // write 8 channel settings
    setting = 0x00;
    if(channelSettings[i][POWER_DOWN] == YES) setting |= 0x80;
    setting |= channelSettings[i][GAIN_SET]; // gain
    setting |= channelSettings[i][INPUT_TYPE_SET]; // input code
    if(channelSettings[i][SRB2_SET] == YES){
      setting |= 0x08; // close this SRB2 switch
      useSRB2[i] = true;
    }else{
      useSRB2[i] = false;
    }
    WREG(CH1SET+i, setting);  // write this channel's register settings
    
      // add or remove from inclusion in BIAS generation
      setting = RREG(BIAS_SENSP);       //get the current P bias settings
      if(channelSettings[i][BIAS_SET] == YES){
        bitSet(setting,i);    //set this channel's bit to add it to the bias generation
        useInBias[i] = true;
      }else{
        bitClear(setting,i);  // clear this channel's bit to remove from bias generation
        useInBias[i] = false;
      }
      WREG(BIAS_SENSP,setting); delay(1); //send the modified byte back to the ADS

      setting = RREG(BIAS_SENSN);       //get the current N bias settings
      if(channelSettings[i][BIAS_SET] == YES){
        bitSet(setting,i);    //set this channel's bit to add it to the bias generation
      }else{
        bitClear(setting,i);  // clear this channel's bit to remove from bias generation
      }
      WREG(BIAS_SENSN,setting); delay(1); //send the modified byte back to the ADS
     
    if(channelSettings[i][SRB1_SET] == YES){
      useSRB1 = true;
    }
  }
    if(useSRB1){
      for(int i=0; i<8; i++){
        channelSettings[i][SRB1_SET] = YES;
      }
      WREG(MISC1,0x20);     // close all SRB1 swtiches
    }else{
      for(int i=0; i<8; i++){
        channelSettings[i][SRB1_SET] = NO;
      }
      WREG(MISC1,0x00);
    }
}

void OpenBCI_32::writeChannelSettings(char N){
  byte setting;
  if ((N < 1) || (N > 8)) {Serial0.println("channel number out of range"); return;}  // must be a legit channel number
  N = constrain(N-1,0,7);  //subtracts 1 so that we're counting from 0, not 1
//proceed...first, disable any data collection
  SDATAC(); delay(1);      // exit Read Data Continuous mode to communicate with ADS
  
  setting = 0x00;
  if(channelSettings[N][POWER_DOWN] == YES) setting |= 0x80;
  setting |= channelSettings[N][GAIN_SET]; // gain
  setting |= channelSettings[N][INPUT_TYPE_SET]; // input code
  if(channelSettings[N][SRB2_SET] == YES){
    setting |= 0x08; // close this SRB2 switch
    useSRB2[N] = true;  // keep track of SRB2 usage
  }else{
    useSRB2[N] = false;
  }
  WREG(CH1SET+(byte)N, setting);  // write this channel's register settings

  // add or remove from inclusion in BIAS generation
  setting = RREG(BIAS_SENSP);       //get the current P bias settings
  if(channelSettings[N][BIAS_SET] == YES){
    useInBias[N] = true;
    bitSet(setting,N);    //set this channel's bit to add it to the bias generation
  }else{
    useInBias[N] = false;
    bitClear(setting,N);  // clear this channel's bit to remove from bias generation
  }
  WREG(BIAS_SENSP,setting); delay(1); //send the modified byte back to the ADS
  setting = RREG(BIAS_SENSN);       //get the current N bias settings
  if(channelSettings[N][BIAS_SET] == YES){
    bitSet(setting,N);    //set this channel's bit to add it to the bias generation
  }else{
    bitClear(setting,N);  // clear this channel's bit to remove from bias generation
  }
  WREG(BIAS_SENSN,setting); delay(1); //send the modified byte back to the ADS
    
  if(channelSettings[N][SRB1_SET] == YES){
    for(int i=0; i<8; i++){
      channelSettings[i][SRB1_SET] = YES;
    }
    useSRB1 = true;
    WREG(MISC1,0x20);     // close all SRB1 swtiches
  }
  if((channelSettings[N][SRB1_SET] == NO) && (useSRB1 == true)){
    for(int i=0; i<8; i++){
      channelSettings[i][SRB1_SET] = NO;
    }
    useSRB1 = false;
    WREG(MISC1,0x00);
  }
}

//  deactivate the given channel...note: stops data colleciton to issue its commands
//  N is the channel number: 1-8
// 
void OpenBCI_32::deactivateChannel(int N)
{
  byte setting;  // ,reg;  // trying not to use reg 
  if ((N < 1) || (N > 8)) return;  //check the inputs  
  //proceed...first, disable any data collection
  SDATAC(); delay(1);      // exit Read Data Continuous mode to communicate with ADS
  //shut down the channel
  N = constrain(N-1,0,7);  //subtracts 1 so that we're counting from 0, not 1
//  reg = CH1SET+(byte)N;           // select the current channel
  setting = RREG(CH1SET+(byte)N); delay(1); // get the current channel settings
  channelSettings[N][POWER_DOWN] = NO;    // added to overcome channel settings bug JM
  bitSet(setting,7);              // set bit7 to shut down channel
//  if (channelSettings[N][SRB2_SET] == YES)
  bitClear(setting,3);    // clear bit3 to disclude from SRB2 if used
  WREG(CH1SET+(byte)N,setting); delay(1);     // write the new value to disable the channel
  
  //remove the channel from the bias generation...
//  reg = BIAS_SENSP;             // set up to disconnect the P inputs from Bias generation
  setting = RREG(BIAS_SENSP); delay(1); //get the current bias settings
  bitClear(setting,N);                  //clear this channel's bit to remove from bias generation
  WREG(BIAS_SENSP,setting); delay(1);   //send the modified byte back to the ADS

//  reg = BIAS_SENSN;             // set up to disconnect the N inputs from Bias generation
  setting = RREG(BIAS_SENSN); delay(1); //get the current bias settings
  bitClear(setting,N);                  //clear this channel's bit to remove from bias generation
  WREG(BIAS_SENSN,setting); delay(1);   //send the modified byte back to the ADS
  
  leadOffSettings[N][0] = leadOffSettings[N][1] = NO;
  changeChannelLeadOffDetect();
}; 

void OpenBCI_32::activateChannel(int N) 
{
	byte setting;  // ,reg  // trying not to use reg
   //check the inputs
  if ((N < 1) || (N > 8)) return;
  N = constrain(N-1,0,7);  //shift down by one
  //proceed...first, disable any data collection
  SDATAC(); delay(1);      // exit Read Data Continuous mode to communicate with ADS
    setting = 0x00;
    channelSettings[N][POWER_DOWN] = YES; // keep track of channel on/off in this array
    setting |= channelSettings[N][GAIN_SET]; // gain
    setting |= channelSettings[N][INPUT_TYPE_SET]; // input code
    if(useSRB2[N] == true){channelSettings[N][SRB2_SET] = YES;}else{channelSettings[N][SRB2_SET] = NO;}
    if(channelSettings[N][SRB2_SET] == YES) bitSet(setting,3); // close this SRB2 switch
    WREG(CH1SET+N, setting);
    // add or remove from inclusion in BIAS generation
      if(useInBias[N]){channelSettings[N][BIAS_SET] = YES;}else{channelSettings[N][BIAS_SET] = NO;}
      setting = RREG(BIAS_SENSP);       //get the current P bias settings
      if(channelSettings[N][BIAS_SET] == YES){
        bitSet(setting,N);    //set this channel's bit to add it to the bias generation
      }else{
        bitClear(setting,N);  // clear this channel's bit to remove from bias generation
      }
      WREG(BIAS_SENSP,setting); delay(1); //send the modified byte back to the ADS
      setting = RREG(BIAS_SENSN);       //get the current N bias settings
      if(channelSettings[N][BIAS_SET] == YES){
        bitSet(setting,N);    //set this channel's bit to add it to the bias generation
      }else{
        bitClear(setting,N);  // clear this channel's bit to remove from bias generation
      }
      WREG(BIAS_SENSN,setting); delay(1); //send the modified byte back to the ADS
      
    setting = 0x00;
    if(useSRB1) setting = 0x20;
    WREG(MISC1,setting);     // close all SRB1 swtiches
};


void OpenBCI_32::changeChannelLeadOffDetect(void)
{
  byte P_setting = RREG(LOFF_SENSP);
  byte N_setting = RREG(LOFF_SENSN);
  
  for(int i=0; i<8;i++){
    if(leadOffSettings[i][PCHAN] == ON){
      bitSet(P_setting,i);
    }else{
      bitClear(P_setting,i);
    }
    if(leadOffSettings[i][NCHAN] == ON){
      bitSet(N_setting,i);
    }else{
      bitClear(N_setting,i);
    }
  }
   WREG(LOFF_SENSP,P_setting);
   WREG(LOFF_SENSN,N_setting);
}; 

void OpenBCI_32::configureLeadOffDetection(byte amplitudeCode, byte freqCode)
{
	amplitudeCode &= 0b00001100;  //only these two bits should be used
	freqCode &= 0b00000011;  //only these two bits should be used
	
	//get the current configuration of he byte
	byte reg, setting;
	reg = LOFF;
	setting = RREG(reg); //get the current bias settings
	
	//reconfigure the byte to get what we want
	setting &= 0b11110000;  //clear out the last four bits
	setting |= amplitudeCode;  //set the amplitude
	setting |= freqCode;    //set the frequency
	
	//send the config byte back to the hardware
	WREG(reg,setting); delay(1);  //send the modified byte back to the ADS
	
}

//Configure the test signals that can be inernally generated by the ADS1299
void OpenBCI_32::configureInternalTestSignal(byte amplitudeCode, byte freqCode)
{
	if (amplitudeCode == ADSTESTSIG_NOCHANGE) amplitudeCode = (RREG(CONFIG2) & (0b00000100));
	if (freqCode == ADSTESTSIG_NOCHANGE) freqCode = (RREG(CONFIG2) & (0b00000011));
	freqCode &= 0b00000011;  		//only the last two bits are used
	amplitudeCode &= 0b00000100;  	//only this bit is used
	byte message = 0b11010000 | freqCode | amplitudeCode;  //compose the code
	
	WREG(CONFIG2,message); delay(1);
	
}
 
// Start continuous data acquisition
void OpenBCI_32::startADS(void)
{
  RDATAC(); // enter Read Data Continuous mode
	delay(1);   
  START();  // start the data acquisition
	delay(1);
  isRunning = true;
}
  
// Query to see if data is available from the ADS1299...return TRUE is data is available
boolean OpenBCI_32::isDataAvailable(void)
{
  return (!(digitalRead(ADS_DRDY)));
}
  
// Stop the continuous data acquisition
void OpenBCI_32::stopADS()
{
  STOP();     // stop the data acquisition
	delay(1);   		
  SDATAC();   // stop Read Data Continuous mode to communicate with ADS
	delay(1);   
  isRunning = false;
}


//write as binary each channel's data
void OpenBCI_32::ADS_writeChannelData(void)
{
  //send rawChannelData array to radio module
  for (int i = 0; i < 24; i++)
  {
    Serial0.write(channelDataRaw[i]); 
  }
}


//print out the state of all the control registers
void OpenBCI_32::printADSregisters(void)   
{
    boolean wasRunning = false;
    boolean prevverbosityState = verbosity;
    if (isRunning){ stopADS(); wasRunning = true; }
    verbosity = true;						// set up for verbosity output
    RREGS(0x00,0x0C);     	// read out the first registers
    delay(10);  						// stall to let all that data get read by the PC
    RREGS(0x0D,0x17-0x0D);	// read out the rest
    verbosity = prevverbosityState;
    if (wasRunning){ startADS(); }
}

void OpenBCI_32::ADS_printDeviceID(void)
{ 
    boolean wasRunning;
    boolean prevVerbosityState = verbosity;
    if (isRunning){ stopADS(); wasRunning = true;}
        verbosity = true;
        ADS_getDeviceID();
        verbosity = prevVerbosityState;
    if (wasRunning){ startADS(); }
        
}
  
//System Commands
void OpenBCI_32::WAKEUP() {
    csLow(ADS_SS); 
    xfer(_WAKEUP);
    csHigh(ADS_SS); 
    delayMicroseconds(3);     //must wait 4 tCLK cycles before sending another command (Datasheet, pg. 35)
}

void OpenBCI_32::STANDBY() {    // only allowed to send WAKEUP after sending STANDBY
    csLow(ADS_SS);
    xfer(_STANDBY);
    csHigh(ADS_SS);
}

void OpenBCI_32::RESET() {      // reset all the registers to default settings
    csLow(ADS_SS);
    xfer(_RESET);
    delayMicroseconds(12);    //must wait 18 tCLK cycles to execute this command (Datasheet, pg. 35)
    csHigh(ADS_SS);
}

void OpenBCI_32::START() {      //start data conversion 
    csLow(ADS_SS);
    xfer(_START);           // KEEP ON-BOARD AND ON-DAISY IN SYNC
    csHigh(ADS_SS);
}

void OpenBCI_32::STOP() {     //stop data conversion
    csLow(ADS_SS);
    xfer(_STOP);            // KEEP ON-BOARD AND ON-DAISY IN SYNC
    csHigh(ADS_SS);
}

void OpenBCI_32::RDATAC() {
    csLow(ADS_SS);
    xfer(_RDATAC);      // read data continuous
    csHigh(ADS_SS);
  delayMicroseconds(3);   
}
void OpenBCI_32::SDATAC() {
    csLow(ADS_SS);
    xfer(_SDATAC);
    csHigh(ADS_SS);
  delayMicroseconds(3);   //must wait 4 tCLK cycles after executing this command (Datasheet, pg. 37)
}

// Register Read/Write Commands
byte OpenBCI_32::ADS_getDeviceID() {      // simple hello world com check
  byte data = RREG(0x00);
  if(verbosity){            // verbosity otuput
    Serial0.print("On Board ADS ID ");
    printHex(data); Serial0.println();
  }
  return data;
}

void OpenBCI_32::RDATA() {          //  use in Stop Read Continuous mode when DRDY goes low
  byte inByte;            //  to read in one sample of the channels
    csLow(ADS_SS);        //  open SPI
    xfer(_RDATA);         //  send the RDATA command
    for(int i=0; i<3; i++){   //  read in the status register and new channel data
      inByte = xfer(0x00);
      stat = (stat<<8) | inByte; //  read status register (1100 + LOFF_STATP + LOFF_STATN + GPIO[7:4])
    }     
  for(int i = 0; i<8; i++){
    for(int j=0; j<3; j++){   //  read in the new channel data
      inByte = xfer(0x00);
      channelDataInt[i] = (channelDataInt[i]<<8) | inByte;
    }
  }
   csHigh(ADS_SS);        //  close SPI
  
  for(int i=0; i<8; i++){
    if(bitRead(channelDataInt[i],23) == 1){  // convert 3 byte 2's compliment to 4 byte 2's compliment
      channelDataInt[i] |= 0xFF000000;
    }else{
      channelDataInt[i] &= 0x00FFFFFF;
    }
  }
}

byte OpenBCI_32::RREG(byte _address) {    //  reads ONE register at _address
    byte opcode1 = _address + 0x20;   //  RREG expects 001rrrrr where rrrrr = _address
    csLow(ADS_SS);        //  open SPI
    xfer(opcode1);          //  opcode1
    xfer(0x00);           //  opcode2
    regData[_address] = xfer(0x00);//  update mirror location with returned byte
    csHigh(ADS_SS);       //  close SPI 
  if (verbosity){           //  verbosity output
    printRegisterName(_address);
    printHex(_address);
    Serial0.print(", ");
    printHex(regData[_address]);
    Serial0.print(", ");
    for(byte j = 0; j<8; j++){
      Serial0.print(bitRead(regData[_address], 7-j));
      if(j!=7) Serial0.print(", ");
    }
    
    Serial0.println();
  }
  return regData[_address];     // return requested register value
}

// Read more than one register starting at _address
void OpenBCI_32::RREGS(byte _address, byte _numRegistersMinusOne) {
//  for(byte i = 0; i < 0x17; i++){
//    regData[i] = 0;         //  reset the regData array
//  }
    byte opcode1 = _address + 0x20;   //  RREG expects 001rrrrr where rrrrr = _address
    csLow(ADS_SS);        //  open SPI
    xfer(opcode1);          //  opcode1
    xfer(_numRegistersMinusOne);  //  opcode2
    for(int i = 0; i <= _numRegistersMinusOne; i++){
        regData[_address + i] = xfer(0x00);   //  add register byte to mirror array
    }
    csHigh(ADS_SS);       //  close SPI
  if(verbosity){            //  verbosity output
    for(int i = 0; i<= _numRegistersMinusOne; i++){
      printRegisterName(_address + i);
      printHex(_address + i);
      Serial0.print(", ");
      printHex(regData[_address + i]);
      Serial0.print(", ");
      for(int j = 0; j<8; j++){
        Serial0.print(bitRead(regData[_address + i], 7-j));
        if(j!=7) Serial0.print(", ");
      }
      Serial0.println();
      delay(30);
    }
    }
}

void OpenBCI_32::WREG(byte _address, byte _value) { //  Write ONE register at _address
    byte opcode1 = _address + 0x40;   //  WREG expects 010rrrrr where rrrrr = _address
    csLow(ADS_SS);        //  open SPI
    xfer(opcode1);          //  Send WREG command & address
    xfer(0x00);           //  Send number of registers to read -1
    xfer(_value);         //  Write the value to the register
    csHigh(ADS_SS);       //  close SPI
  regData[_address] = _value;     //  update the mirror array
  if(verbosity){            //  verbosity output
    Serial0.print("Register ");
    printHex(_address);
    Serial0.println(" modified.");
  }
}

void OpenBCI_32::WREGS(byte _address, byte _numRegistersMinusOne) {
    byte opcode1 = _address + 0x40;   //  WREG expects 010rrrrr where rrrrr = _address
    csLow(ADS_SS);        //  open SPI
    xfer(opcode1);          //  Send WREG command & address
    xfer(_numRegistersMinusOne);  //  Send number of registers to read -1 
  for (int i=_address; i <=(_address + _numRegistersMinusOne); i++){
    xfer(regData[i]);     //  Write to the registers
  } 
  digitalWrite(CS,HIGH);        //  close SPI
  if(verbosity){
    Serial0.print("Registers ");
    printHex(_address); Serial0.print(" to ");
    printHex(_address + _numRegistersMinusOne);
    Serial0.println(" modified");
  }
}


// <<<<<<<<<<<<<<<<<<<<<<<<<  END OF ADS1299 FUNCTIONS  >>>>>>>>>>>>>>>>>>>>>>>>>
// ******************************************************************************
// <<<<<<<<<<<<<<<<<<<<<<<<<  LIS3DH FUNCTIONS  >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>


void OpenBCI_32::initialize_accel(byte g){ 
  byte setting =  g | 0x08;           // mask the g range for REG4    
  pinMode(LIS3DH_SS,OUTPUT); digitalWrite(LIS3DH_SS,HIGH);
  pinMode(LIS3DH_DRDY,INPUT);   // setup dataReady interupt from accelerometer
  LIS3DH_write(TMP_CFG_REG, 0x00);  // DISable ADC inputs, enable temperature sensor
  LIS3DH_write(CTRL_REG1, 0x08);    // disable accel, low power mode
  LIS3DH_write(CTRL_REG2, 0x00);    // don't use the high pass filter
  LIS3DH_write(CTRL_REG3, 0x00);    // no interrupts yet
  LIS3DH_write(CTRL_REG4, setting);   // set scale to g, high resolution
  LIS3DH_write(CTRL_REG5, 0x00);    // no boot, no fifo
  LIS3DH_write(CTRL_REG6, 0x00);
  LIS3DH_write(REFERENCE, 0x00);
  DRDYpinValue = lastDRDYpinValue = digitalRead(LIS3DH_DRDY);  // take a reading to seed these variables
}
  
void OpenBCI_32::enable_accel(byte Hz){    // ADD ABILITY TO SET FREQUENCY & RESOLUTION
  for(int i=0; i<3; i++){
    axisData[i] = 0;            // clear the axisData array so we don't get any stale news
  }
  byte setting = Hz | 0x07;           // mask the desired frequency
  LIS3DH_write(CTRL_REG1, setting);   // set freq and enable all axis in normal mode
  LIS3DH_write(CTRL_REG3, 0x10);      // enable DRDY1 on INT1 (tied to Arduino pin3, LIS3DH_DRDY)
}

void OpenBCI_32::disable_accel(){
  LIS3DH_write(CTRL_REG1, 0x08);      // power down, low power mode
  LIS3DH_write(CTRL_REG3, 0x00);      // disable DRDY1 on INT1
}

byte OpenBCI_32::LIS3DH_getDeviceID(){
  return LIS3DH_read(WHO_AM_I);
}

boolean OpenBCI_32::LIS3DH_DataAvailable(){
  boolean x = false;
  if((LIS3DH_read(STATUS_REG2) & 0x08) > 0) x = true;  // read STATUS_REG
  return x;
}

boolean OpenBCI_32::LIS3DH_DataReady(){
  boolean r = false;
  DRDYpinValue = digitalRead(LIS3DH_DRDY);  // take a look at LIS3DH_DRDY pin
  if(DRDYpinValue != lastDRDYpinValue){     // if the value has changed since last looking
    if(DRDYpinValue == HIGH){               // see if this is the rising edge
      r = true;                             // if so, there is fresh data!
    }
    lastDRDYpinValue = DRDYpinValue;        // keep track of the changing pin
  }
  return r;
}

void OpenBCI_32::LIS3DH_writeAxisData(void){
  for(int i=0; i<3; i++){
    Serial0.write(highByte(axisData[i])); // write 16 bit axis data MSB first
    Serial0.write(lowByte(axisData[i]));  // axisData is array of type short (16bit)
    axisData[i] = 0;  // reset the axis data variables to 0
  }
}

byte OpenBCI_32::LIS3DH_read(byte reg){
  reg |= READ_REG;                    // add the READ_REG bit
  csLow(LIS3DH_SS);                            // take spi
  spi.transfer(reg);                  // send reg to read
  byte inByte = spi.transfer(0x00);   // retrieve data
  csHigh(LIS3DH_SS);                           // release spi
  return inByte;  
}

void OpenBCI_32::LIS3DH_write(byte reg, byte value){
  csLow(LIS3DH_SS);                  // take spi
  spi.transfer(reg);        // send reg to write
  spi.transfer(value);      // write value
  csHigh(LIS3DH_SS);                 // release spi
}

int OpenBCI_32::LIS3DH_read16(byte reg){    // use for reading axis data. 
  int inData;  
  reg |= READ_REG | READ_MULTI;   // add the READ_REG and READ_MULTI bits
  csLow(LIS3DH_SS);                 // take spi
  spi.transfer(reg);              // send reg to start reading from
  inData = spi.transfer(0x00) | (spi.transfer(0x00) << 8);  // get the data and arrange it
  csHigh(LIS3DH_SS);                       // release spi
  return inData;
}

int OpenBCI_32::getX(){
  return LIS3DH_read16(OUT_X_L);
}

int OpenBCI_32::getY(){
  return LIS3DH_read16(OUT_Y_L);
}

int OpenBCI_32::getZ(){
  return LIS3DH_read16(OUT_Z_L);
}

void OpenBCI_32::LIS3DH_updateAxisData(){
  axisData[0] = getX();
  axisData[1] = getY();
  axisData[2] = getZ();
}

void OpenBCI_32::LIS3DH_readAllRegs(){
  
  byte inByte;

  for (int i = STATUS_REG_AUX; i <= WHO_AM_I; i++){
    inByte = LIS3DH_read(i);
    Serial0.print("0x0");Serial0.print(i,HEX);
    Serial0.print("\t");Serial0.println(inByte,HEX);
    delay(20);
  }
  Serial0.println();

  for (int i = TMP_CFG_REG; i <= INT1_DURATION; i++){
    inByte = LIS3DH_read(i);
    // printRegisterName(i);
    Serial0.print("0x");Serial0.print(i,HEX);
    Serial0.print("\t");Serial0.println(inByte,HEX);
    delay(20);
  }
  Serial0.println();

  for (int i = CLICK_CFG; i <= TIME_WINDOW; i++){
    inByte = LIS3DH_read(i);
    Serial0.print("0x");Serial0.print(i,HEX);
    Serial0.print("\t");Serial0.println(inByte,HEX);
    delay(20);
  }
  
}



// <<<<<<<<<<<<<<<<<<<<<<<<<  END OF LIS3DH FUNCTIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>



// String-Byte converters for ADS 
void OpenBCI_32::printRegisterName(byte _address) {
    switch(_address){ 
      case ID: 
        Serial0.print("ID, "); break;
      case CONFIG1:
        Serial0.print("CONFIG1, "); break;
      case CONFIG2:
        Serial0.print("CONFIG2, "); break;
      case CONFIG3:
        Serial0.print("CONFIG3, "); break;
      case LOFF:
        Serial0.print("LOFF, "); break;
      case CH1SET:
        Serial0.print("CH1SET, "); break;
      case CH2SET:
        Serial0.print("CH2SET, "); break;
      case CH3SET:
        Serial0.print("CH3SET, "); break;
      case CH4SET:
        Serial0.print("CH4SET, "); break;
      case CH5SET:
        Serial0.print("CH5SET, "); break;
      case CH6SET:
        Serial0.print("CH6SET, "); break;
      case CH7SET:
        Serial0.print("CH7SET, "); break;
      case CH8SET:
        Serial0.print("CH8SET, "); break;
      case BIAS_SENSP:
        Serial0.print("BIAS_SENSP, "); break;
      case BIAS_SENSN:
        Serial0.print("BIAS_SENSN, "); break;
      case LOFF_SENSP:
        Serial0.print("LOFF_SENSP, "); break;
      case LOFF_SENSN:
        Serial0.print("LOFF_SENSN, "); break;
      case LOFF_FLIP:
        Serial0.print("LOFF_FLIP, "); break;
      case LOFF_STATP:
        Serial0.print("LOFF_STATP, "); break;
      case LOFF_STATN:
        Serial0.print("LOFF_STATN, "); break;
      case GPIO:
        Serial0.print("GPIO, "); break;
      case MISC1:
        Serial0.print("MISC1, "); break;
      case MISC2:
        Serial0.print("MISC2, "); break;
      case CONFIG4:
        Serial0.print("CONFIG4, "); break;
      default: 
        break;
    }
}

// Used for printing HEX in verbosity feedback mode
void OpenBCI_32::printHex(byte _data){
  Serial0.print("0x");
    if(_data < 0x10) Serial0.print("0");
    Serial0.print(_data, HEX);
}
