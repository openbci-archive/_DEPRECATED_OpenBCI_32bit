OpenBCI_32bit
=============

This branch implements the PROG button as an external trigger source.

>>>>>>  YOU MUST ALSO USE THE OpenBCI_32 LIBRARY UPDATED IN THIS BRANCH  <<<<<<<<<

Note that to USE the OpenBCI system, you will generally use the OpenBCI USB Dongle. The dongle requries that you install the FTDI drivers for your particular operating system: http://www.ftdichip.com/FTDrivers.htm

Remove the files OpenBCI_32 and SD from the OpenBCI_32_Library folder and put it in your documents/mpide/libraries folder.

Put the OpenBCI_32_SD into your documents/mpide folder and restart mpide to be able to select the sketch.

Before you upload the firmware, you need to make a change to an library file inside the mpide program folder:

 * We're using the DSPI library, but our MISO and MOSI pins are not default
 * Adjust file Mpide.app/Contents/Resources/Java/hardware/pic32/variants/DP32/Board_Defs.h
 * Find the DSPI0 pin definition section and change the defines thusly:
 *      #define _DSPI0_MISO_IN    PPS_IN_SDI1
 *      #define _DSPI0_MISO_PIN   5 // [Changed for OpenBCI. Was 10] RA1  SDI1 SDI1R = RPA1 = 0 
 *      #define _DSPI0_MOSI_OUT   PPS_OUT_SDO1
 *      #define _DSPI0_MOSI_PIN   10  // [Changed for OpenBCI. Was 18] RA4  SDO1 RPA4R = SDO1 = 3
 * This will be necessary until chipKIT or Diligent allows user selection of MOSI/MISO
 * Or until we get our own OpenBCI Board selection from the mpide (!)

When you upload the firmware, select the chipKIT DP32 from the Tools -> Board -> chipKIT menu, 
select the serial port of the dongle, 
then press upload.

We are uploading the sketch over air! There is a chance that the mpide will timeout during the upload process!
If this happens, you will need to unplug the dongle, and re-insert it to stop the upload.
Then, power cycle the OpenBCI board, as it is best that the Board radio comes on line after the dongle radio.
Then try again to upload. This is a known issue, and we can confirm that all boards shipped will take the upload
process, it just might take a couple of times to stick.


TESTTESTTEST
