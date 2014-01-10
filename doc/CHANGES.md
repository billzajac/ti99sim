TI-99/Sim Changelog

Version 0.0.11

07-05-2012 - Fixed crashes in **disk**, **list**, and **convert-ctg**

07-02-2012 - Fixed logic for CapsLock

08-10-2011 - Added support for directories to **disk** utility

07-31-2011 - Fixed a crash in the **list** utility

07-30-2011 - Added support for CF7+ disk images (supported by both **disk**
and **list**)

07-25-2011 - Improved Mac OS X build process

03-05-2010 - Added support for using Windows .PIF files when creating
cartridges in **convert-ctg**

02-24-2010 - Fixed a bug in the TB opcode handling and the polarity of some
signals in the TMS9901 emulation**

02-18-2010 - Added support for REF/DEF table while loading most TI-Tagged
files in **dumpcpu**

02-17-2010 - Added support for most existing tags in TI-Tagged files in
**dumpcpu**

02-17-2010 - Fixed logic when reading variable size records (fixes bug in
**disk** when exporting and converting variable sized files)

11-30-2009 - Fixed timing issues when loading saved images and resetting the
CPU

09-18-2009 - Added 4K addressing mode support to TMS9918a emulation

08-10-2009 - Added checks for errors reported by file I/O functions

04-23-2009 - Fixed a crash when an invalid disk device cartridge is loaded

03-18-2009 - Fixed TimerHook in SDL build to handle fast systems properly &
track VDP refreshes more accurately

10-13-2008 - Updated Linux and Mac Makefile system

08-10-2004 - Added timestamp support to basic file system code

08-08-2004 - Added cross-linked file check option (--check) to **disk**
utility

07-29-2004 - Added --verbose output to **list** utility

07-28-2004 - Updated **list** utility to more closely match the output from TI
BASIC

06-22-2004 - Minor clean up - eliminated several lint/compiler warnings

01-05-2004 - Updated code to compile & run properly on 64-bit platforms

09-25-2003 - Updated cartridge format to use LZW compression

09-24-2003 - Added compression ratio information to verbose directory listing
for archive files

09-24-2003 - Modified LZW encoder & decoder to handle variable max bit-length
at run-time

09-23-2003 - Fixed bug in cDecodeLZW that prevented some valid files from
decoding the last symbol properly

09-23-2003 - Fixed cDecodeLZW's handling of corrupt data

09-22-2003 - Added cEncodeLZW encoder class

Version 0.0.10

09-15-2003 - Added cArchiveFileSystem class to handle .ark files as disks

09-15-2003 - Updated list of files to strip/install under Win32

09-14-2003 - Wrote LZW routines to decode .ark files

09-10-2003 - Added support for REF records in TI-Tagged files to **dumpcpu**

09-01-2003 - Added ability to add files to disk images in **disk**

Version 0.0.9

08-29-2003 - Fix **convert-ctg**'s handling of Gram Kracker files (10
character filename limit & PC99 formats)

08-03-2003 - Add 'Scale3x' code and optimized 'Scale2x' routines

05-01-2003 - Improved quality of speech synthesis when using External Data
mode

01-16-2003 - Fixed bug in **list** utility (failing to NULL terminate the
string)

12-05-2002 - Added 'volume' and 'scale2x' command-line option

12-02-2002 - Added option to select an alternate color palette

12-01-2002 - Fixed window down-scaling

12-01-2002 - Implemented Scale2x code for double-size windows

11-21-2002 - Changed joystick code to handle X/Y mappings better

11-22-2002 - Changed sound code to use unsigned samples

11-24-2002 - Added more interpolation code to speech synthesis code to improve
quality (slightly)

11-13-2002 - Fixed command line parsing to notify user of invalid arguments

11-13-2002 - Added diagnostic messages to ParseJoystick in the case of bad
input

09-17-2002 - Added window resizing support to **ti99sim-sdl**

09-17-2002 - Added new fullscreen option to select a specific resolution

09-17-2002 - Added better auto-selection of screen resolution in fullscreen
mode

09-17-2002 - Changed cBitmap::Copy to handle arbitrary integer upscaling

09-15-2002 - Modified startup code for **ti99sim-sdl** to handle platforms
that don't support joysticks properly

09-15-2002 - Fixed code in decode.cpp to byte-swap structure members on big
endian processors

09-15-2002 - Reworked Makefile.linux to simplify adding new utilities, added
uninstall target, and added soft links in /usr/local/bin

09-14-2002 - Ported code to run on PowerPC platform (MacOSX)

09-14-2002 - Removed bitfields from code & replaced with masks and shifts

09-14-2002 - Fixed display problem in **ti99sim-console**

Version 0.0.8 - 20-Aug-2002

08-03-2002 - Fixed bug in **mkspch** that created incorrect binary tree

08-03-2002 - Added parsed data mode to **dumpspch** and **mkspch**

07-31-2002 - Created **dumpspch** utility

07-31-2002 - Fixed several memory leaks/invalid memory accesses in utilities

07-21-2002 - Fixed frame skip logic in **ti99sim-sdl**

06-14-2002 - Added command line syntax for all programs to README.html

06-10-2002 - Updated README.html

06-01-2002 - Updated **ti99sim-console**'s keyboard handling code

05-27-2002 - Fixed **decode** to handle noisy preamble data without crashing

02-11-2002 - Improved WAV file support in **decode**

02-01-2002 - Improved **decode**'s ability to recover data from WAV files

01-16-2002 - Changed disk I/O routines to properly handle 80 track disk images

01-13-2002 - Added user-selectable audio sampling frequency

01-12-2002 - Improved choppy audio under Windows NT (increased default buffer
size)

01-12-2002 - Fixed GROM address auto-increment to stay within the current GROM

01-11-2002 - Added AddFile and DeleteFile routines to disk utility

01-05-2002 - Fixed clock cycle counting for memory writes (simulate the read
that the TMS9900 does before writing)

12-31-2001 - Fixed bug in RemoveCartridge that could cause a core dump/GP
Fault

12-30-2001 - Replaced keyboard routines with code from Felix Nawothnig (better
international keyboard support)

12-20-2001 - Added timer mode support to TMS9901 code

12-18-2001 - Added TMS9901 class to emulate the programable interupt
controller

12-16-2001 - Fixed problem with Mini-Memory's battery backed RAM

Version 0.0.7 - 06-Dec-2001

12-06-2001 - Added multicolor mode to support the TMS9918A emulation

11-29-2001 - Fixed UpdateSprites to only display first 4 sprites on a line

11-29-2001 - Added 5th sprite detection & coincidence checking logic

11-26-2001 - Fixed bug related to sprite display (was incorrectly wrapping)

08-19-2001 - Fixed bug in **convert-ctg** when creating a DSR cartdidge

08-13-2001 - Improved console based screen display when switching modes

08-06-2001 - Added list utility to display TI BASIC/Extended BASIC programs

08-01-2001 - Fixed 9901 interrupt signal logic (MG Explorer now runs)

08-01-2001 - Changed memory access routines to properly emulate scratch pad
RAM aliasing

07-31-2001 - Updated LoadImage code and added optional disassembly of code in
scratchpad RAM in **dumpcpu**

07-30-2001 - Reorganized SaveImage/LoadImage routines to be more robust

Version 0.0.6 - 27-Jul-2001

07-25-2001 - Added code to patch the TI disk ROM at run-time to fix a problem
with formatting

07-22-2001 - Fixed a bug in the video code that could cause memory corruption

07-19-2001 - Added image file and FIAD support to **dumpcpu**

07-18-2001 - Updated disk device to use new disk I/O routines

07-16-2001 - Added command line option handling routines

07-15-2001 - Corrected DSxD logic for locating sectors on double-sided disks

07-15-2001 - Added write support for all disk formats

07-14-2001 - Re-wrote disk I/O routines - Added V9T9 and PC99 format read
support

07-06-2001 - Fixed error messages so that they display properly under Win32
version of SDL

07-06-2001 - Added new command line arguments and help screen to **ti99sim-
sdl**

07-06-2001 - Added NTSC/PAL option for display

07-03-2001 - Modified makefiles to work with mingw under Windows

07-02-2001 - Added frame skip feature to video display code

06-24-2001 - Added hack to work around bug in MSVC++ in opcodes

Version 0.0.5 - 16-Jun-2001

06-06-2001 - Removed byte-swapping on little-endian machines (invalidates the
.asm routines)

06-02-2001 - Fixed read & write 16-bit trap for little-endian machines

06-02-2001 - Removed sprite vertical wrap-around

05-14-2001 - Added calls to Sleep in speech synthesizer to allow the TMS5220
code to stall the CPU execution path

05-11-2001 - Fixed emulated joystick code

05-10-2001 - Switched to SDL v1.2.0

05-10-2001 - Added fullscreen support

05-10-2001 - Added real joystick support

05-08-2001 - Fixed code that depended on the compiler's order of evaluation of
an expression involving multiple function calls

05-07-2001 - Modified console GetKey routines to match behavior accross
platforms

05-07-2001 - Fixed code that was LITTLE_ENDIAN dependent (**ti99sim-console**
now runs under SunOS on a Sparc)

05-07-2001 - Fixed a problem with the shift key state in **ti99sim-sdl**

05-06-2001 - Added **mkspch** utility to create spchrom.bin files

05-05-2001 - Reworked Makefiles and moved defines to rules.mak

11-27-2000 - Added basic TMS5220 support

Version 0.0.4 - 24-Nov-2000

11-12-2000 - Replaced ti-roms.dat with 3 files to show different available
options

11-12-2000 - Changed name of **convert** to **convert-ctg** to avoid conflicts
with ImageMagick

11-10-2000 - Improved **convert**'s choice of output filenames

11-10-2000 - Updated **convert** to work with new cartridge format

11-09-2000 - Changed internal structure of cartridges to better support GRAM
Kracker emulation

10-31-2000 - Converted README to html

09-28-2000 - Added -d option to **convert**

09-24-2000 - Added support for battery-backed RAM cartridges

09-20-2000 - Added *nix directory structure support

09-20-2000 - Added save/load memory image support

09-01-2000 - Reworked sound code to reduce CPU overhead

08-10-2000 - Added utility to read .wav files of recorded audio tapes

08-06-2000 - Corrected emulated clock speed

08-05-2000 - Re-ordered OpCode lookup table to reduce average lookup times

08-01-2000 - Improved emulation speed accuracy by tracking memory access clock
cycles

08-01-2000 - Replaced x86 assembly code with C code

07-28-2000 - Changed Makefiles to use native CPU instead of 'i686'

Version 0.0.3 - 28-Jun-2000

06-27-2000 - Fixed bug in CheckGrom in **convert**

06-27-2000 - Added command line options for silence and 4x window size

06-26-2000 - Changed **convert** to check lower and upper case versions of
alternate filenames

06-25-2000 - Changed sound code to use floating point to track periods

06-23-2000 - Added noise support

Version 0.0.2 - 21-Jun-2000

06-21-2000 - Cleaned up address aliasing for VDP memory mapped accesses

06-20-2000 - Added 1:2 upscale to bitmap Copy method

06-20-2000 - Added basic sound support

06-19-2000 - Added window manager caption

Version 0.0.1 - 18-Jun-2000

06-18-2000 - First release

* * *

Homepage: [http://www.mrousseau.org/programs/ti99sim/](http://www.mrousseau.or
g/programs/ti99sim/)

Contact: [ti99sim@mrousseau.nonsense.org](mailto:ti99sim at
mrousseau.org?subject=ti99sim)

