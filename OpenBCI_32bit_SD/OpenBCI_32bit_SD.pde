/*
 * 
 *  >>>> THIS CODE USED TO STREAM OpenBCI V3_32 DATA TO DONGLE <<<<
 *  >>>> May include the 60Hz notch filter by Chip Audette  <<<<
 *
 * This code is written to target a PIC32MX250F128B with UDB32-MX2-DIP bootloader
 * To Program, user must manually reset the PIC32 on the OpenBCI 32bit Board
 * press RST, then press PROG, then release RST, then release PROG 
 * Adjust as needed if you are testing on different hardware.
 *
 *
 * Made by Joel Murphy, Luke Travis, Conor Russomanno Summer, 2014. 
 * Note that to USE the OpenBCI system, you will generally use the OpenBCI USB Dongle.
 *The dongle requries that you install the FTDI drivers for your particular operating system: 
 * http://www.ftdichip.com/FTDrivers.htm

 * Before you upload the firmware, you need to place the OpenBCI board variant files inside the mpide program folder:

 * This will allow you to find the OpenBCI 32 board in mpide dropdown selection tree!
 * Place the OpenBCI folder into the mpide application:

 *     On A Mac, right click the application mpide, and select 'Show Package Contents' 
 *     place the entire OpenBCI folder in the variants folder here:
 *     Mpide/Contents/Resources/Java/hardware/pic32/variants


 *     On a Windows, place the entire OpenBCI folder in the variants folder here:
 *     C:\Program Files\mpide-blah\hardware\pic32\variants
 *     Remove the files OpenBCI_32 and SD from the OpenBCI_32_Library folder and put it in your documents/mpide/libraries folder.

 * Put the OpenBCI_32_SD folder into your documents/mpide folder and restart mpide to be able to select the sketch.

 * When you upload the firmware, select the 'OpenBCI 32' from the Tools -> Board -> chipKIT menu,
 * Then select the serial port of the dongle, then press upload!
 * We are uploading the sketch over air! There is a chance that the mpide will timeout during the upload process!
 * If this happens, you will need to unplug the dongle, and re-insert it to stop the upload. 
 * Then, power cycle the OpenBCI board, as it is best that the Board radio comes on line after the dongle radio. 
 * Then try again to upload.
 * This is a known issue, and we can confirm that all boards shipped will take the upload process,
 * it just might take a couple of times to stick.
 *
 * Any SDcard code is based on RawWrite example in SDFat library 
 * ASCII commands are received on the serial port to configure and control
 * Serial protocol uses '+' immediately before and after the command character
 * We call this the 'burger' protocol. the '+' are the buns. Example:
 * To begin streaming data, send '+b+'
 * The addition of the '+'s is handled by the RFduino radios in OpenBCI V3
 * This software is provided as-is with no promise of workability
 * Use at your own risk, wysiwyg.

 */

#include <SD.h>
#include <DSPI.h>
#include <EEPROM.h>
#include "OpenBCI_32.h" 


//------------------------------------------------------------------------------
//  << SD CARD BUSINESS >> has bee taken out. See OBCI_SD_LOG_CMRR 
//  SD_SS on pin 7 defined in OpenBCI library
boolean SDfileOpen = false;
char fileSize = '0';  // SD file size indicator
int blockCounter = 0;
//------------------------------------------------------------------------------
//  << OpenBCI BUSINESS >>
boolean is_running = false;    // this flag is set in serialEvent on reciept of ascii prompt
OpenBCI_32 OBCI; //Uses SPI bus and pins to say data is ready. 
byte sampleCounter = 0;
// these are used to change individual channel settings from PC
char currentChannelToSet;    // keep track of what channel we're loading settings for
boolean getChannelSettings = false; // used to receive channel settings command
int channelSettingsCounter; // used to retrieve channel settings from serial port
int leadOffSettingsCounter;
boolean getLeadOffSettings = false;

// these are all subject to the radio requirements: 31byte max packet length (maxPacketLength - 1 for packet checkSum)
#define OUTPUT_NOTHING (0)  // quiet
#define OUTPUT_BINARY (1)  // normal transfer mode
#define OUTPUT_BINARY_SYNTHETIC (2)  // needs portage
int outputType;

//------------------------------------------------------------------------------
//  << LIS3DH Accelerometer Business >>
//  LIS3DH_SS on pin 5 defined in OpenBCI library
volatile boolean auxAvailable = false;
volatile boolean addAccel = true;
boolean useAccelOnly = false;
//------------------------------------------------------------------------------
//  << PUT FILTER STUFF HERE >>
boolean useFilters = false;
//------------------------------------------------------------------------------

int LED = 11;  // blue LED alias
int PGCpin = 12;  // PGC pin goes high when PIC is in bootloader mode
//------------------------------------------------------------------------------

void setup(void) {

  Serial0.begin(115200);  // using hardware uart number 0
  pinMode(LED, OUTPUT); digitalWrite(LED,HIGH);    // blue LED
  pinMode(PGCpin,OUTPUT); digitalWrite(PGCpin,LOW);// used to tell RFduino if we are in bootloader mode
  
  startFromScratch();
}



void loop() {
  
  if(is_running){
    
      while(!(OBCI.isDataAvailable())){}   // wait for DRDY pin...
      
      OBCI.updateChannelData(); // get the fresh ADS results
      if(OBCI.useAccel && OBCI.LIS3DH_DataAvailable()){
        OBCI.LIS3DH_updateAxisData();    // fresh axis data goes into the X Y Z 
        addAccel = true;    // adds accel data to SD card if you like that kind of thing
      }
      if(SDfileOpen){
        writeDataToSDcard(sampleCounter);  // 
      }
      OBCI.sendChannelData(sampleCounter);
      sampleCounter++;
  }
  
eventSerial();

}


// some variables to help find 'burger protocol' commands
int plusCounter = 0;
char testChar;
unsigned long commandTimer;

void eventSerial(){
  while(Serial0.available()){      
    char inChar = (char)Serial0.read();
    
    if(plusCounter == 1){  // if we have received the first 'bun'
      testChar = inChar;   // this might be the 'patty', stop laughing
      plusCounter++;       // get ready to look for another 'bun' 
      commandTimer = millis();  // don't wait too long! 
    }
  
    if(inChar == '+'){  // if we see a 'bun' on the serial
      plusCounter++;    // make a note of it
      if(plusCounter == 3){  // looks like we got a command character
        if(millis() - commandTimer < 5){  // if it's not too late,
          if(getChannelSettings){ // if we just got an 'x' expect channel setting parameters
            loadChannelSettings(testChar);  // go get em!
          }else if(getLeadOffSettings){  // if we just got a 'z' expect lead-off setting parameters
            loadLeadOffSettings(testChar); // go get em!
          }else{
            getCommand(testChar);    // decode the command
          }
        }
        plusCounter = 0;  // get ready for the next one
      }
    }
  }
}
    
    
void getCommand(char token){
    switch (token){
//TURN CHANNELS ON/OFF COMMANDS
      case '1':
        changeChannelState_maintainRunningState(1,DEACTIVATE); break;
      case '2':
        changeChannelState_maintainRunningState(2,DEACTIVATE); break;
      case '3':
        changeChannelState_maintainRunningState(3,DEACTIVATE); break;
      case '4':
        changeChannelState_maintainRunningState(4,DEACTIVATE); break;
      case '5':
        changeChannelState_maintainRunningState(5,DEACTIVATE); break;
      case '6':
        changeChannelState_maintainRunningState(6,DEACTIVATE); break;
      case '7':
        changeChannelState_maintainRunningState(7,DEACTIVATE); break;
      case '8':
        changeChannelState_maintainRunningState(8,DEACTIVATE); break;
      case '!':
        changeChannelState_maintainRunningState(1,ACTIVATE); break;
      case '@':
        changeChannelState_maintainRunningState(2,ACTIVATE); break;
      case '#':
        changeChannelState_maintainRunningState(3,ACTIVATE); break;
      case '$':
        changeChannelState_maintainRunningState(4,ACTIVATE); break;
      case '%':
        changeChannelState_maintainRunningState(5,ACTIVATE); break;
      case '^':
        changeChannelState_maintainRunningState(6,ACTIVATE); break;
      case '&':
        changeChannelState_maintainRunningState(7,ACTIVATE); break;
      case '*':
        changeChannelState_maintainRunningState(8,ACTIVATE); break;
             
// TEST SIGNAL CONTROL COMMANDS
      case '0':
        activateAllChannelsToTestCondition(ADSINPUT_SHORTED,ADSTESTSIG_NOCHANGE,ADSTESTSIG_NOCHANGE); break;
      case '-':
        activateAllChannelsToTestCondition(ADSINPUT_TESTSIG,ADSTESTSIG_AMP_1X,ADSTESTSIG_PULSE_SLOW); break;
      case '=':
        activateAllChannelsToTestCondition(ADSINPUT_TESTSIG,ADSTESTSIG_AMP_1X,ADSTESTSIG_PULSE_FAST); break;
      case 'p':
        activateAllChannelsToTestCondition(ADSINPUT_TESTSIG,ADSTESTSIG_AMP_2X,ADSTESTSIG_DCSIG); break;
      case '[':
        activateAllChannelsToTestCondition(ADSINPUT_TESTSIG,ADSTESTSIG_AMP_2X,ADSTESTSIG_PULSE_SLOW); break;
      case ']':
        activateAllChannelsToTestCondition(ADSINPUT_TESTSIG,ADSTESTSIG_AMP_2X,ADSTESTSIG_PULSE_FAST); break;

// SD CARD COMMANDS
      case 'A': case'S': case'F': case'G': case'H': case'J': case'K': case'L': case 'a': case 'h':
        SDfileOpen = true; fileSize = token; setupSDcard(fileSize); // make SDfileOpen contingent on setupSDcard?
        break;
      case 'j':
        if(SDfileOpen){
          SDfileOpen = false; 
          closeSDfile(); 
        }
        break;

// CHANNEL SETTING COMMANDS
      case 'x':  // expect 6 parameters
        if(!is_running) {Serial0.println("ready to accept new channel settings");}
        channelSettingsCounter = 0;
        getChannelSettings = true; break;
      case 'X':  // latch channel settings
        if(!is_running) {Serial0.println("updating channel settings");}
        writeChannelSettingsToADS(currentChannelToSet); break;
      case 'd':  // reset all channel settings to default
        if(!is_running) {Serial0.println("updating channel settings to default");}
        setChannelsToDefaultSetting(); break;
      case 'D':  // report the default settings
        sendDefaultChannelSettings(); break;
      case 'c':  // use 8 channel mode
        break;
      case 'C':  // use 16 channel mode
        break;
        
// LEAD OFF IMPEDANCE DETECTION COMMANDS
      case 'z':  // expect 2 parameters
        if(!is_running) {Serial0.println("ready to accept new impedance detect settings");}
        leadOffSettingsCounter = 0;  // reset counter
        getLeadOffSettings = true;
        break;
      case 'Z':  // latch impedance parameters
        if(!is_running) {Serial0.println("updating impedance detect settings");}
        changeChannelLeadOffDetect_maintainRunningState();
        break;

// STREAM DATA AND FILTER COMMANDS
      case 'v': 
        startFromScratch();
        break;
      case 'n':  // turn on accelerator
        sampleCounter = 0;
        useAccelOnly = true;
        if(SDfileOpen) stampSD(ACTIVATE);
        OBCI.enable_accel(RATE_25HZ);
        break;
      case 'N':  // turn off accelerator
        if(SDfileOpen) stampSD(DEACTIVATE);
        useAccelOnly = false;
        OBCI.disable_accel();
        break;
      case 'b':  // stream data
        if(SDfileOpen) stampSD(ACTIVATE);
        OBCI.enable_accel(RATE_25HZ);      // fire up the accelerometer
        startRunning(OUTPUT_BINARY);       // turn on the fire hose
        break;
     case 's':  // stop streaming data
        if(SDfileOpen) stampSD(DEACTIVATE);
        OBCI.disable_accel();
        stopRunning();
        break;
      case 'f':
         useFilters = true;  
         break;
      case 'g':
         useFilters = false;
         break;
         
//  QUERY THE ADS AND ACCEL REGITSTERS         
     case '?':
        printRegisters();
        break;
     default:
       break;
     }
  }// end of getCommand
  
void sendEOT(){
  Serial0.print("$$$");
}

void loadChannelSettings(char c){
  Serial0.print("load setting ");
  if(channelSettingsCounter == 0){  // if it's the first byte in this channel's array, this byte is the channel number to set
    currentChannelToSet = c - '1'; // we just got the channel to load settings into (shift number down for array usage)
    channelSettingsCounter++;
    if(!is_running) {
      Serial0.print("for channel ");
      Serial0.println(currentChannelToSet+1,DEC);
    }
    return;
  }
//  setting bytes are in order: POWER_DOWN, GAIN_SET, INPUT_TYPE_SET, BIAS_SET, SRB2_SET, SRB1_SET
  if(!is_running) {
    Serial0.print(channelSettingsCounter-1);
    Serial0.print(" with "); Serial0.println(c);
  }
  c -= '0';
  if(channelSettingsCounter-1 == GAIN_SET){ c <<= 4; }
  OBCI.channelSettings[currentChannelToSet][channelSettingsCounter-1] = c;
  channelSettingsCounter++;
  if(channelSettingsCounter == 7){  // 1 currentChannelToSet, plus 6 channelSetting parameters
    if(!is_running) Serial0.print("done receiving settings for channel ");Serial0.println(currentChannelToSet+1,DEC);
    getChannelSettings = false;
  }
}

void writeChannelSettingsToADS(char chan){
  boolean is_running_when_called = is_running;
  int cur_outputType = outputType; 
  stopRunning();                   //must stop running to change channel settings
  chan += 1;     // this shifting nonsense has to stop
  OBCI.writeChannelSettings(chan);    // change the channel settings on ADS
  
  if (is_running_when_called == true) {
    startRunning(cur_outputType);  //restart, if it was running before
  }
}

void setChannelsToDefaultSetting(){
  boolean is_running_when_called = is_running;
  int cur_outputType = outputType; 
  stopRunning();  //must stop running to change channel settings
  
  OBCI.setChannelsToDefault();   // default channel settings
  
  if (is_running_when_called == true) {
    startRunning(cur_outputType);  //restart, if it was running before
  }
}

void loadLeadOffSettings(char c){
   if(leadOffSettingsCounter == 0){  // if it's the first byte in this channel's array, this byte is the channel number to set
    currentChannelToSet = c - '1'; // we just got the channel to load settings into (shift number down for array usage)
    if(!is_running) Serial0.print("changing LeadOff settings for channel "); Serial0.println(currentChannelToSet+1,DEC);
    leadOffSettingsCounter++;
    return;
  }
//  setting bytes are in order: PCHAN, NCHAN
  if(!is_running) {
    Serial0.print("load setting "); Serial0.print(leadOffSettingsCounter-1);
    Serial0.print(" with "); Serial0.println(c);
  }
  c -= '0';
  OBCI.leadOffSettings[currentChannelToSet][leadOffSettingsCounter-1] = c;
  leadOffSettingsCounter++;
  if(leadOffSettingsCounter == 3){  // 1 currentChannelToSet, plus 2 leadOff setting parameters
    if(!is_running) Serial0.print("done receiving leadOff settings for channel ");Serial0.println(currentChannelToSet+1,DEC);
    getLeadOffSettings = false; // release the serial COM
  }
}

boolean stopRunning(void) {
  if(is_running == true){
    OBCI.stopStreaming();                    // stop the data acquisition  //
    is_running = false;
    }
    return is_running;
  }

boolean startRunning(int OUT_TYPE) {
  if(is_running == false){
    outputType = OUT_TYPE;
    OBCI.startStreaming();    
    is_running = true;
  }
    return is_running;
}

int changeChannelState_maintainRunningState(int chan, int start)
{
  boolean is_running_when_called = is_running;
  int cur_outputType = outputType;
  
  //must stop running to change channel settings
  stopRunning();
  if (start == true) {
      Serial0.print("Activating channel ");
      Serial0.println(chan);
    OBCI.activateChannel(chan);
  } else {
      Serial0.print("Deactivating channel ");
      Serial0.println(chan);
    OBCI.deactivateChannel(chan);
  }
  //restart, if it was running before
  if (is_running_when_called == true) {
    startRunning(cur_outputType);
  }
}

// CALLED WHEN COMMAND CHARACTER IS SEEN ON THE SERIAL PORT
int activateAllChannelsToTestCondition(int testInputCode, byte amplitudeCode, byte freqCode)
{
  boolean is_running_when_called = is_running;
  int cur_outputType = outputType;
  
  //must stop running to change channel settings
  stopRunning();
  //set the test signal to the desired state
  OBCI.configureInternalTestSignal(amplitudeCode,freqCode);    
  //loop over all channels to change their state
  for (int Ichan=1; Ichan <= 8; Ichan++) {
    OBCI.channelSettings[Ichan-1][INPUT_TYPE_SET] = testInputCode;
//    OBCI.activateChannel(Ichan,gainCode,testInputCode,false);  //Ichan must be [1 8]...it does not start counting from zero
  }
  OBCI.writeChannelSettings();
  //restart, if it was running before
  if (is_running_when_called == true) {
    startRunning(cur_outputType);
  }
}

int changeChannelLeadOffDetect_maintainRunningState()
{
  boolean is_running_when_called = is_running;
  int cur_outputType = outputType;
  
  //must stop running to change channel settings
  stopRunning();

  OBCI.changeChannelLeadOffDetect();
  
  //restart, if it was running before
  if (is_running_when_called == true) {
    startRunning(cur_outputType);
  }
}

void sendDefaultChannelSettings(){
  boolean is_running_when_called = is_running;
  int cur_outputType = outputType;
  
  OBCI.reportDefaultChannelSettings();  // reads CH1SET 
  sendEOT();
  delay(10);
  
  //restart, if it was running before
  if (is_running_when_called == true) {
    startRunning(cur_outputType);
  }
}

void printRegisters(){
  boolean is_running_when_called = is_running;
  int cur_outputType = outputType;
  
  //must stop running to change channel settings
  stopRunning();
  
  if(is_running == false){
    // print the ADS and LIS3DH registers
    OBCI.printAllRegisters();
  }
  sendEOT();
  delay(20);
    //restart, if it was running before
  if (is_running_when_called == true) {
    startRunning(cur_outputType);
  }
}

void startFromScratch(){
   delay(1000);
  Serial0.print("OpenBCI V3 32bit Board\tInitializing...\n");
  OBCI.useAccel = true;  // option to add accelerometer dat to stream
  OBCI.useAux = false;    // option to add user data to stream not implimented yet
  OBCI.initialize();  
  OBCI.configureLeadOffDetection(LOFF_MAG_6NA, LOFF_FREQ_31p2HZ);  
  Serial0.print("ADS1299 Device ID: 0x"); Serial0.println(OBCI.ADS_getDeviceID(),HEX);
  Serial0.print("LIS3DH Device ID: 0x"); Serial0.println(OBCI.LIS3DH_getDeviceID(),HEX);
  sendEOT();
    
}



// DO FILTER STUFF HERE IF YOU LIKE

// end






