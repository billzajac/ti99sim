# TI-99/Sim
a Texas Instruments Home Computer Emulator

# Introduction

This is the latest incarnation of an emulator for the TI-99/4A that I've been
working on, on and off, since 1993. At first it was a simple text-based
simulation of the TI (ti99sim-console) running under MS-DOS™. Then it was
ported to OS/2 and graphics support for the OS/2 Presentation Manager was
added. Later, it was ported to Windows and had sound support added. Now it's
been ported to Linux and Mac OS X and more features are still being added.

_DISCLAIMER_: I'm not one for writing a lot of documentation, so you're
encouraged to look at the code to see how some things work if they're not
described here. If you're interested in writing documentation, let me know.

_NOTE_: In order to run the emulator, you need to create a cartridge that
contains the console ROM & GROMs from the TI-99/4A. Texas Instruments will not
allow these to be distributed, so you'll need to find a way to get them
yourself. If you have a working copy of v9t9, instructions are included to
help you create the required cartridge. If you don't, I would suggest you
download it as it contains documentation on how to get the files you need to
do this.

# Getting Started

Before building or running TI-99/Sim you must install the SDL library. The
latest version is available at [http://www.libsdl.org/download-1.2.php](http:/
/www.libsdl.org/download-1.2.php). If you are planning on building TI-99/Sim,
you will need to install the Development Libraries version otherwise, the
Runtime Libraries package is sufficient.

## Building

If you downloaded the binary-only version, you can skip straight to
Installation. Before building you should make sure that the SDL development
code has been installed on your system and ensure that the sdl-config utility
is in you search path.

If everything is installed properly, running 'make' will build all the
executables found in the binary-only package. Any OS specific operations are
detailed in the following sections.

_NOTE_: When building from the source, all the executables are left in their
corresponding directories.

### Linux

Since this is the primary development environment, you should have few
problems building under Linux.

    
    
                **~/ti99sim>** make
              

### Macintosh OS X

In order to build ti99sim under Macintosh OS X, you'll need to install the
developer tools from Apple. You will also need to copy the SDL main files
(SDLMain.m and SDLMain.h) from the development libraries to the src/sdl
folder. Then, from a bash shell, just type:

    
    
                **~/ti99sim>** make
              

_NOTE_: I'm not a Mac OS X guru, so there is no neat little package for
TI-99/Sim. This probably means that to run it you'll need to do so from a
command window. (If anybody is interested in working on a proper Mac OS X
install is more than welcome to do so.)

### Windows

If you're building the win32 version, you'll need either a GNU-type
environment (GNU make, g++, ld, rm, ...) or Microsoft Visual C++.

If your using Microsoft Visual Studio, you can use the solution file found in
the vc9 subdirectory. These files assume that you have installed the SDL
development files in a folder called SDL-1.2.14 in a directory at the same
level as the TI-99/Sim directory.

The only GNU-type environment that I've tested is the mingw package, but
others may work as well. You will also need to make sure you define the WIN32
environment variable before building (type set WIN32=1 at the command prompt).
When you're ready, just type make in the directory where you placed the
emulator.

    
    
                **c:\ti99sim>** set WIN32=1
                **c:\ti99sim>** make
              

_NOTE_: If you are using mingw build tool, you will need to extract SDL_main.o
from libSDLmain.a (type: ar -x libSDLmain.a SDL_main.o) as well (until I can
figure out why libSDLmain.a isn't linking correctly with the mingw tools).

_NOTE_: The win32 command line build has not been tested in ages and may no
longer work correctly. Microsoft Visual Studio is the preferred development
environment for ti99sim on Windows.

## Installation

### The Directories

Installing the emulator is easy. Make sure you have sufficient privileges to
create/write to the installation directories and type:

    
    
                make install
              

This will create the default directories and install the binaries. The default
directory is /opt/ti99sim and the binaries will be installed in
/opt/ti99sim/bin. In addition to installing the binaries, symbolic links to
them will be created in /usr/local/bin (which should already be in your path).
When loading files (ROMs, cartridges, disk images, …), ti99sim will look in
the current directory, ~/.ti99sim, and finally, /opt/ti99sim. Under Windows,
the search is limited to the current directory followed by the directory
specified in the environment variable HOME (if it is defined at all). In each
of the directories searched, the following sub-directories may exist:

    
    
                ─┬─ cartridges
                 ├─ disks
                 └─ roms
              

When looking for a file, the appropriate sub-directory for that file type will
be searched. If there is no logical directory for a given file, only the base
directory will be searched.

### The Console ROM & Cartridges

Once you have run make install, you will need to create the console ROM
cartridge TI-994A.ctg (see convert-ctg below for instructions) in order to run
the emulator. This ROM cartridge contains the operating system, the BASIC
interpreter, and a description of the RAM configuration for the computer.
Without it, the emulator will not do anything useful. Once you have created
the console ROM cartridge, copy it to either ~/.ti99sim/roms or
/opt/ti99sim/roms.

You should also place a copy of spchrom.bin into this directory to enable
speech synthesis. If you don't have this file, you can use mkspch to create
one that will keep the emulator happy.

If you want to use the disk emulation features, you will need to create
another cartridge named ti-disk.ctg that contains a copy of the TI Disk
Controller ROM (the section on **convert-ctg** contains information about
creating cartridges for peripheral DSR ROMs).

A sample installation is shown below. In order to get up and running, however,
the only file needed is the TI-994A.ctg file.

    
    
                ─┬─ cartridges
                 │  ├─ Parsec.ctg
                 │  ├─ Alpiner.ctg
                 │  ├─ TI Extended BASIC.ctg
                 │  └─ Editor-Assembler.ctg
                 ├─ disks
                 │  ├─ ASSM1                   - E/A Disk 1
                 │  ├─ ASSM2                   - E/A Disk 2
                 │  ├─ DISKASSM                - Miller Graphics' Disk-Assembler
                 │  ├─ MG_DIAG                 - Miller Graphics' Advanced Diagnostics
                 │  └─ dsk1.dsk                - Default disk image
                 └─ roms
                    ├─ Gram Kracker.ctg        - Miller Graphic's Gram Kracker ROMs
                    ├─ TI-994A.ctg             - TI-99/4A ROM
                    ├─ spchrom.bin             - TI Speech Synthesizer ROM
                    └─ ti-disk.ctg             - TI ROM DSR module for disk access
              

### Up and Running

Now you're all set to run the emulator, just type:

    
    
                ti99sim-sdl
              

If everything is installed correctly, you should see a window on your screen
with the TI-99/4A startup screen followed by a beep. Enjoy!

Using the sample configuration in the previous section, the following are
valid commands:

    
    
                ti99sim-sdl --dsk2=ASSM1 --dsk3=ASSM2 Editor-Assembler.ctg   (DSK1 will default to using the dsk1.dsk image)
                ti99sim-sdl --dsk1=MG_DIAG "TI Extended BASIC.ctg"           (Note the use of quotes when using modules with spaces in their name)
                ti99sim-sdl -f Parsec.ctg                                    (OK, bring 'em on ...)
              

# Overview

## Disks

What good is a computer is you can't save information or programs? On the
TI-99/4A, there are two standard storage media: floppy disks and cassette
tapes. This section deals with floppy disks. Depending on your disk
controller, the TI could access either 3 or 4 floppy drives. These drive could
be either single-density (using FM encoding) or high-density (using MFM
encoding). Finally, disks can be either single-sided or double-sided. The
various disk sizes are listed in the following table.

TI-99/4A Disk Sizes & Nominal Values

Notation | Sides | Recording Mode | Sectors/Tracks | Tracks | Capacity
---------|-------|----------------|----------------|--------|---------
SSSD     |1      |FM              |9               |40      |90K
SSDD     |1      |MFM             |18              |40      |180K
DSSD     |2      |FM              |9               |80      |180K
DSDD     |2      |MFM             |18              |80      |360K
CF7+     |2      |CF*             |20              |80      |400K

    * CF7+ is not an actual floppy disk. This format was introduced by the CF7+/nanoPEB peripherals for the TI-99/4A. These devices use CompactFlash to store multiple disk images (referred to as volumes) on a single device. The number of images stored depends on the size of the CompactFlash memory. These disk images are larger than typical disk images and require a custom DSR to read them properly. The **disk** utility is able to read and write to these devices, but TI-99/Sim does not contain the DSR routines required to use them.

The problem with emulating the TI (besides finding a 5¼ʺ disk drive) on an IBM
PC is that most PC's don't easily allow single-density (FM) disks to be read.
In addition, modern operating system don't make it easy to access the disk at
the level required to read TI disks. To get around this, disks are read (using
special programs, usually written for MS-DOS, to get easy access to the disk)
and converted to a single file on the local hard drive. However, there is no
one standard file format for disks. The formats that I'm aware of, all of
which are supported by TI-99/Sim, are shown in table below.

In addition to supporting disk images, TI-99/Sim also supports archive files.
Archive files were developed by Barry Boone and use a Limpel-Zev compression
scheme similar to .zip files on the PC. Each file stored in the archive
contains all the information to completely recover the original file including
the file descriptor record. One way of looking at an archive file is as a
cross between a file and a disk (in fact, the **disk** and other utilities
treat these files as disks).

TI-99/4A Emulator Disk Formats

Name    | Format           | Pros                                                       | Cons
--------|------------------|------------------------------------------------------------|-------------------------------------------------------------------------
AnaDisk | Header & Data    | Retains sector ordering and special formatting information | None
v9t9    | Raw sectors      | Compact image size                                         | Unable to distinguish between SSDD & DSSD disks
PC99    | Raw tracks       | Stores a complete copy of a sector                         | Hard to parse. Lots of extra data (roughly 40% more than v9t9's format)
Archive | Compressed files | Uses the least amount of space                             | Not a real disk image
## Files

TI-99/Sim only deals with files contained within disk images. However, there
are reasons to deal with individual files rather than entire disks. As with
disks, there are a few different formats that individual files may use. The
most common format for v9t9 systems is the FIAD format. This format allows the
emulator to identify all of the attributes of the original file. A similar,
but slightly different format is known as TIFILES. This is the format used to
transfer files to/from the TI using the X-Modem protocol. Finally, files can
be stored as 'native' files. When this format is used, the TI file is
converted, as best as possible, to a native file on the host operating system.

TI-99/4A File Formats

Format   | Description
---------|---------------------------------------------------------------------------------------------------------------------------------------------------------
FIAD     | v9t9's native file format. The actual file descriptor record from the file is included in the header of the file, followed by the raw sectors that make up the file.
TIFILES  | Very similar to FIAD in structure. The difference is limited to the 128 byte header. There are at least 3 different flavors of this format. Each of flavor includes most or all of the information normally found in the file descriptor record (the original name is not always present).
'Native' | This format has no header or formatting information.


Two of the utility applications, **dumpcpu** and **list**, access files. These
files can be either standalone files or files contained within a disk. To
access either a FIAD or TIFILES files, simply use the name of the file on the
host filesystem. Files contained within a disk are identified by the name of
the containing disk and the intended filename, separated by a colon. For
example, to list a BASIC program called GAME on a disk named sample.dsk the
syntax would be: _list sample.dsk:GAME_

# Included Programs

## convert-ctg

The ti99sim emulator uses special .ctg files to store the ROM and/or GROM
images contained within the computer console and plug-in cartridges. The
**convert-ctg** program allows you to create these cartridges. It will convert
your existing v9t9 ROMs or create cartridges using output from a hex dump of a
ROM image. A set of sample files, TI-994A.dat, Mini-Memory.dat, and Gram
Kracker.dat, located in the roms directory, are supplied as examples of the
format of the hex dump file. _NOTE_: These files do _not contain complete hex
dumps_ of their corresponding ROMs but can be used as templates if you have
the required information.

To convert existing v9t9 cartridges, type convert-ctg foo.bin where foo is the
base name of a set of ROM files. The base name is usually the portion of the
filenames that are common between all files associated with a particular
cartridge. **convert-ctg** understands the naming conventions used by present
and past versions of v9t9, and will attempt to determine which version of
files that you have installed. For example, the following command will convert
the files PARSECC.BIN and PARSECG.BIN to PARSEC.ctg:

    
    
    convert-ctg /_your-path-here_/v9t9/v6.0/modules/PARSEC.BIN
            

Files to be converted can be in any directory. The new cartridge will be
created in the current working directory. To create the console ROM cartridge
you need to specify either 994a.bin or ti.hex (depending on your version of
v9t9 or TI Emulator) as the filename. For example:

    
    
    convert-ctg /_your-path-here_/v9t9/v6.0/roms/994a.bin
            

This will create the special TI-994A.ctg file that contains the console ROM,
the system GROMs, scratch-pad RAM, and 32K memory expansion RAM. Without this
file, ti99sim will not run!

There are three types of cartridges. The first is the special TI-994A.ctg
file, the second are DSR ROMs, and the third are normal cartridges. The first
two types are expected to be found in the 'roms' directories, while the last
is expected in the 'cartridges' directories.

DSR ROMs are special ROM cartridges that are associated with a particular
peripheral device. These cartridges must be associated with a particular CRU
address. The ROM in these cartridges is located at a different address than
the ROMs normally found in a cartridge and without a CRU address, the CPU
won't be able to see the ROM within the cartridge. To create a DSR ROM
cartridge, simply use the --cru=xxxx option to **convert-ctg**. This will
instruct **convert-ctg** to store the ROM at the special DSR memory range, and
mark the cartridge with the correct CRU information. In order to use the disk
emulation features, you need to create a cartridge from a TI disk ROM with the
name ti-disk.ctg with the CRU address 1100.

When creating cartridges, **convert-ctg** will attempt to add a cartridge
title by searching for valid GROM headers in the files being converted. If it
does not find any valid names, or if you want to change the one it uses, you
can specify a title on the command line after the base filename. ti99sim uses
the cartridge title when saving/loading memory images to ensure that the
correct cartridges are present when loading. If you intend to use this
feature, please make sure that all of your cartridges have unique names.

**convert-ctg** will also allow you to dump the contents of a cartridge to a specially formatted hex dump file. This feature is useful if you want to see the contents of a cartridge's ROM/GROM code, or to patch the code and rebuild the cartridge with your changes.

Command-line syntax:

    Usage: convert-ctg [options] file 
    Options: 
      --cru=base Create a DSR cartridge at the indicated CRU address
      -d --dump Create a hex dump of the cartridge
      -v --verbose=n Display extra information

## decode

If you have old TI audio tapes hanging around, **decode** will convert them
into binary files that can be used (in some future version) by ti99sim. It
attempts to intelligently find sections of stored data while ignoring any
other sounds it finds. This makes it easier to get all the data from a tape -
just create a .wav file that covers the entire tape. There is no need to
create a separate .wav file for each data file. Once it's done, you will have
a separate file for each track of audio found on the tape. If you have a TI-
BASIC or TI Extended BASIC program stored on tape, you can use the **list**
utility to display a listing of the program from the file(s) created by
**decode**.

Only .wav files are currently supported. Any .wav file stored using a standard
format (i.e. non-compressed, PCM audio using integer samples) should work.
**decode** can handle files at any sampling rate and bit depth. This means you
can keep your recordings as small as possible by using 8-bit mono 8000Hz .wav
files. If your file is not recorded in mono (i.e. stereo or multi-track),
**decode** will only attempt to use data from the first channel (i.e. left
(2,3), front left (4), left center (6), ...). Of course, a higher sampling
rate or resolution may help the decoding process in cases where the audio is
garbled. If you find that a file is not decoding properly, you may want to re-
record the tape using higher settings. If you have the proper hardware, for
example, you could record a 32-bit mono 44100Hz file (this will create a file
over 20 times larger than the one mentioned earlier, so it should only be used
as a last resort).

Command-line syntax:

    Usage: decode [options] file.wav
    Options:
      -v --verbose=n Display extra information

## disk

The emulator allows you to use disks created by a TI-99/4A computer within the
emulator. The emulator can read disks created using the dump feature of
AnaDisk - with or without the optional 8-byte header for each sector. The
latter is the format used by v9t9 disks. The emulator can also read disks that
contain raw track data (the format used by PC99). **disk** can understand both
single and double density disk formats.

The **disk** utility allows you to look at the contents of a disk image and
optionally create native files from those in the disk image. This allows you
to create files that can be used by **convert-ctg** to create cartridge files
that you can use with the emulator. There are two output formats for files
extracted from a disk. The first (default) format is the v9t9 FIAD format.
This format contains a 128-byte header that preserves all of the original file
information. The second format is a naked file. These files use only a
filename extension to convey the original file format. The actual contents of
the file are converted to a conventional file as best as possible. Files using
this format _may_ be readable on the host computer (normally, DISPLAY type
files are readable).

CF7+ devices can be read/written using the **disk** utility. The syntax for
accessing a volume on the device is: <device-file>#<volume>. Where <device-
file> can be either the device node where the CompactFlash card is located
(e.g. /dev/sdb on a Linux machine) or a file image taken from an actual
device. The <volume> is a number (starting at 1) indicating which volume on
the device to access.

The **disk** utility can also be used to convert disk images between formats.
By specifying --output={anadisk|v9t9|PC99}, a disk image can be converted to
any format (_NOTE_: Currently CF7+ devices/images cannot be converted).

Command-line syntax:

    Usage: disk [options] file
    Options:
      -a --add=<filename> Add <filename> to the disk image
      --check Check the integrity of the disk structures
      -c --convert Convert extracted files to DOS files
      -d --dump Extract all files to FIAD files
      -l --layout Display the disk sector layout
      --output=PC99 Convert disk to PC99 format
      --output=v9t9 Convert disk to v9t9 DOAD format
      --output=anadisk Convert disk to AnaDisk format /w headers
      -r --remove=<filename> Remove <filename> from the disk image
      -v --verbose Display information about the disk image
      -e --extract=<filename> Extract <filename> to v9t9 FIAD file

## dumpcpu

This program will allow you to disassemble any CPU ROM section in a cartridge
file, a DISPLAY/FIXED FIAD file, or RAM from a save image file. Through the
use of an external configuration file, you can assign labels and EQUs to make
the disassembly more readable.

_NOTE_: This program is a work in progress and does not always create good
listings.

Command-line syntax:

    Usage: dumpcpu [options] file
    Options:
      --code Treat data as CODE by default
      --image Treat 'file' as a memory image file
      --no-overwrite Stop loading DF80 files when data is overwritten
      --range=lo-hi Only dump the indicate range
      --load=address Treat address as the start of code
      --scratch-pad Include scratch pad RAM in disassembly
      -v --verbose Display information about the file being analyzed

## dumpgrom

Ever wondered what the code behind that game looks like? This program is
similar to **dumpcpu** but disassembles the GPL code in the GROM cartridge. It
uses the standard GROM headers to find starting addresses and strings.

_NOTE_: This program is a work in progress and does not always create good
listings.

Command-line syntax:

    Usage: dumpgrom [options] file
    Options:
      --range=lo-hi Only dump the indicate range

## dumpspch

Here is a utility that can help you analyze the contents of your speech
synthesizer's ROM (spchrom.bin). This program will create a text file,
spchrom.dat, which contains the contents of the ROM that can later be used by
**mkspch** to create a new speech ROM. It can create files in two different
formats. The first (default) is a simple hexadecimal listing of the speech
data for each phrase contained in the ROM file. The second breaks down the raw
data into a 'parsed' format that exposes the individual LPC-10 speech frames.
Each speech frame represents 25ms worth of sound. If you're feeling
adventurous, you can edit the speech frames in the generated .dat file, create
a new spchrom.bin file, and see how things sound!

For those that are interested, the parsed speech frames are of the form:

    
    
              STOP CODE      S                - Signals the end of the speech data
              ZERO CODE      Z                - Indicates 1 frames worth of silence
              REPEAT Frame   Pxx Exx R        - Repeats the previous frame using the indicated pitch and energy
              Unvoiced Frame Pxx Exx <Kxx>4   - An unvoiced (fricative) frame with 4 reflection coefficients
              Voiced Frame   Pxx Exx <Kxx>10  - A voiced frame with 10 reflection coefficients
            

_NOTE_: If you create a file using the 'spch' format, the resulting file will
only contain the valid speech data. The actual TI speech ROM data contains a
few extra bytes following several phrases. The parsed output file will not
include this extraneous data. A spchrom.bin file created from the generated
spchrom.dat file will not match the original ROM byte-for-byte, but will be a
completely functional copy.

Command-line syntax:

    Usage: dumpspch [options] file
    Options:
      --format=hex Speech data listed in hexadecimal
      --format=spch Decoded speech data
      -o --output=<filename> Create output file <filename>
      -v --verbose Display extra information

## list

This utility can be used to list TI-BASIC (and TI Extended BASIC) programs.
Files can be either a native file that has been extracted from a disk image
(using 'disk -d xyz.dsk'), or a file inside a disk image (using
xyz.dsk:filename, or cf7#5:filename). Find the file you want listed (i.e.
HELLO) and type 'list HELLO'. If you have selected a valid BASIC file, a
listing similar to the one printed by the BASIC interpreter should print.

Command-line syntax:

    Usage: list [options] file
    Options:
      -v --verbose=n Display extra information about the file

## mkspch

If you can't find a copy of spchrom.bin, you can use this utility to create
one that can be used by the ti99sim. The spchrom.dat file located in the roms
directory contains the information required to create a stripped down speech
ROM. This file does not contain any actual speech data, but instead contains
the bare minimum number of phrases required for the proper operation of TI
Extended BASIC's _CALL SAY_ logic. If you have your own LPC-10 coded speech
samples (I have no idea how you would create them - _yet_, so please don't
ask), you can use this utility to create a speech ROM using your own voice
samples!

Command-line syntax:

    Usage: mkspch [options] file
    Options:
      -o --output=<filename> Create output file <filename>
      -v --verbose Display extra information

## say

Do you miss being able to type CALL SAY("HELLO") and hear the TI's speech
synthesizer? Well, **say** is your answer. This program attempts to mimic the
behavior of the TI Extended BASIC routine SAY. It will look through the
spchrom.bin file for the indicated pattern and do it's best to produce
synthesized speech. Some phrases in the speech library contain spaces. If you
want to say one of these ("Texas Instruments" for example), you will need to
put quotes around it on the command line, otherwise say will look for two or
more separate words that are probably not present.

Command-line syntax:

    Usage: say [options] <Text-to-speak>
    Options:
      -s --sample=<freq> Select sampling frequency for audio playback
      -v --verbose=n Display extra information
      --volume=n Set the audio volume

## ti99sim-console

A simple text-based version of the emulator. It includes an interface, similar
to Miller Graphics' Explorer, which allows you to step through code at the
assembly or GPL instruction level. It has two modes: command and run. While it
is in command mode, the CPU is paused. You can edit any of the register
values, clear the PC breakpoint, load/save memory images, exit... In run mode,
the CPU is running and all keystrokes go to the TI. Pressing <Esc> will stop
the CPU and return to command mode. Since its text-mode only, it does not
support graphics or sprites. If you just want to see what the computer is
doing, this is the program to do it with.

Command-line syntax:

    Usage: ti99sim-console [options] [cartrigde.ctg] [memory.img]
    Options:
      --dskn=<filename> Use <filename> disk image for DSKn
      --NTSC Emulate a NTSC display (60Hz)
      --PAL Emulate a PAL display (50Hz)
      -v --verbose=n Display extra information

_NOTE_: If you try to load a memory image, you must make sure that any
cartridge(s) that were running when the image was made are also specified.

Command Mode:

  * C - Clear the PC interrupt
  * \<Space> - Single step
  * G - Enter Run Mode
  * R - Edit CPU registers
  * Q - Exit the program
  * L - Load Image
  * S - Save Image
  * B - Set 'BASIC' display bias
  * N - Set 'normal' display bias

Run Mode:

  * \<Esc> - Return to command mode

## ti99sim-sdl

The SDL-based emulator

Command-line syntax:

    Usage: ti99sim-sdl [options] [cartridge] [image]
    Options:
      -4 Double width/height window
      --dskn=<filename> Use <filename> disk image for DSKn
      --framerate={n/d|p} Reduce frame rate to fraction n/d or percentage p
      -f --fullscreen=n Fullscreen
      --joystickn=i Use system joystick i as TI joystick n
      --list-joysticks Print a list of all detected joysticks
      --list-resolutions Print a list of available fullscreen resolutions
      --no-joystick Disable hardware joystick support
      -q --no-sound Turn off all sound/speech
      --no-speech Disable speech synthesis
      --NTSC Emulate a NTSC display (60Hz)
      --PAL Emulate a PAL display (50Hz)
      -p --palette=n Select a color palette (1-3)
      -s --sample=<freq> Select sampling frequency for audio playback
      --scale2x Use the Scale2X algorithm to scale display
      -v --verbose=n Display extra information
      --volume=n Set the audio volume

_NOTE_: If you try to load a memory image, you must make sure that any
cartridge(s) that were running when the image was made are also specified.

There is no GUI yet. The following keys are defined:

  * ESC - exit
  * F2 - Save memory image
  * F3 - Load memory image
  * F10 - Reboot

For those of you that don't have easy access to the TI-99/4A keyboard or
overlay, here is a summary of the special function keys:

  * \<Alt>-1 - DEL
  * \<Alt>-2 - INS
  * \<Alt>-3 - ERASE
  * \<Alt>-4 - CLEAR
  * \<Alt>-5 - BEGIN
  * \<Alt>-6 - PROC'D
  * \<Alt>-7 - AID
  * \<Alt>-8 - REDO
  * \<Alt>-9 - BACK
  * \<Alt>-= - QUIT

  * \<Alt>-W - ~
  * \<Alt>-E - <Up Arrow>
  * \<Alt>-R - [
  * \<Alt>-T - ]
  * \<Alt>-U - _
  * \<Alt>-I - ?
  * \<Alt>-O - '
  * \<Alt>-P - "

  * \<Alt>-A - |
  * \<Alt>-S - <Left Arrow>
  * \<Alt>-D - <Right Arrow>
  * \<Alt>-F - {
  * \<Alt>-G - }

  * \<Alt>-Z - \
  * \<Alt>-X - <Down Arrow>
  * \<Alt>-C - `

### Disk support

Using the ti-disk.ctg cartridge, the emulator is capable of reading and
writing single-density disk images. The TI disk controller is not capable of
accessing double-density disks. Internally, the emulator is ready to support
double-density disks, but I don't have access to a controller ROM that
supports them, so I haven't been able to test them yet.

The emulator supports all operations on a disk image. When the emulator shuts
down, any changes made to the disk image are saved. A disk can be made write-
protected by making the file read-only.

_NOTE: The disk routines can read and write files in the PC99 format, but
disks formatted in the emulator by routines other than those in the DSR ROM
(like those formatted using a program like Miller Graphics' Advanced
Diagnostics) may not be readable by PC99 or other PC99-compatible software._

### RS-232 support

Sorry, this feature is not implemented - yet.

### Gram Kracker™ emulation

If you have created the Gram Kracker cartridge properly and placed the
cartridge in the roms directory (the one where you placed the TI-994A.ctg
file), the following extra keys are active:

  * \<Ctrl>-F1 - Toggle Gram Kracker On/Off
  * \<Ctrl>-F2 - Toggle GROM 0/Operating System
  * \<Ctrl>-F3 - Toggle GROMs 1&2/TI-BASIC
  * \<Ctrl>-F4 - Disable write-protect/Select BANK 1
  * \<Ctrl>-F5 - Enable write-protect
  * \<Ctrl>-F6 - Disable write-protect/Select BANK 2
  * \<Ctrl>-F7 - Toggle loader On/Off

Consult your Gram Kracker manual for a description of each of these switches.

To create a Gram Kracker cartridge, the loader ROM must be added to the Gram
Kracker.dat file supplied in the roms directory. Getting the loader ROM is
left as an exercise for the reader. (_Hints_: Save it to disk, transfer it to
the PC using AnaDisk, extract it using the **disk** utility, and a) make a hex
dump after stripping off the header or b) create a normal cartridge file using
the **convert-ctg** utility then create a .dat file using the -d option. OR
Use the dump feature built into the Edit Memory option of the Gram Kracker
loader to send a hex dump to the serial port and capture it on the PC to a
disk file).

* * *

Homepage: [http://www.mrousseau.org/programs/ti99sim/](http://www.mrousseau.or
g/programs/ti99sim/)

Contact: [ti99sim@mrousseau.nonsense.org](mailto:ti99sim at
mrousseau.org?subject=ti99sim)

