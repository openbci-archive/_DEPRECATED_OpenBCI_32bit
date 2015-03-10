OpenBCI_32bit
=============

Repository containing the firmware for the 32bit OpenBCI board.

Note that to USE the OpenBCI system, you will generally use the OpenBCI USB Dongle. The dongle requries that you install the FTDI drivers for your particular operating system: http://www.ftdichip.com/FTDrivers.htm

Before you upload the firmware, you need to place the OpenBCI board variant files inside the mpide program folder:

 * This will allow you to find the OpenBCI 32 board in mpide dropdown selection tree!

Place the OpenBCI folder into the mpide application:

    On A Mac, right click the application mpide, and select 'Show Package Contents' 
    place the entire OpenBCI folder in the variants folder here:
    Mpide/Contents/Resources/Java/hardware/pic32/variants


    On a Windows, place the entire OpenBCI folder in the variants folder here:
    C:\Program Files\mpide-blah\hardware\pic32\variants


Remove the files OpenBCI_32 and SD from the OpenBCI_32_Library folder and put it in your documents/mpide/libraries folder.

Put the OpenBCI_32_SD into your documents/mpide folder and restart mpide to be able to select the sketch.

When you upload the firmware, select the 'OpenBCI 32' from the Tools -> Board -> chipKIT menu, 
select the serial port of the dongle, 
then press upload!

We are uploading the sketch over air! There is a chance that the mpide will timeout during the upload process!
If this happens, you will need to unplug the dongle, and re-insert it to stop the upload.
Then, power cycle the OpenBCI board, as it is best that the Board radio comes on line after the dongle radio.
Then try again to upload. This is a known issue, and we can confirm that all boards shipped will take the upload
process, it just might take a couple of times to stick.
