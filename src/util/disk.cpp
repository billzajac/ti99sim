//----------------------------------------------------------------------------
//
// File:        disk.cpp
// Date:        23-Feb-1998
// Programmer:  Marc Rousseau
//
// Description: A simple program to display the contents of a TI-Disk image
//              created by AnaDisk.  It also supports extracting files from
//              the disk and optionally converting them to 'standard' PC type
//              files.
//
// Copyright (c) 1998-2004 Marc Rousseau, All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
//
// Revision History:
//
//   15-Jan-2003    Created classes to encapsulate TI file & disks
//
//----------------------------------------------------------------------------

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "diskio.hpp"
#include "diskfs.hpp"
#include "arcfs.hpp"
#include "fileio.hpp"
#include "option.hpp"
#include "support.hpp"

DBG_REGISTER ( __FILE__ );

void ConvertFilename ( char *buffer )
{
    FUNCTION_ENTRY ( NULL, "ConvertFilename", true );

    char *end = buffer;
    while ( *end ) end++;
    while ( *--end == ' ' ) ;
    *++end = '\0';
    if ( *buffer == '-' ) *buffer = '_';
    for ( ; *buffer; buffer++ ) {
        if ( isalnum ( *buffer )) continue;
        if ( *buffer == '_' ) continue;
        if ( *buffer == '-' ) continue;
        *buffer = '_';
    }
}

static inline UINT16 GetUINT16 ( const void *_ptr )
{
    FUNCTION_ENTRY ( NULL, "GetUINT16", true );

    const UINT8 *ptr = ( const UINT8 * ) _ptr;
    return ( UINT16 ) (( ptr [0] << 8 ) | ptr [1] );
}

void DumpFile ( cFile *file, bool convertFiles )
{
    FUNCTION_ENTRY ( NULL, "DumpFile", true );

    sFileDescriptorRecord *FDR = file->GetFDR ();

    char name [ 32 ];
    strcpy ( name, FDR->FileName );
    ConvertFilename ( name );

    // Decorate the name to match the file's type
    if ( convertFiles == true ) {
        if ( FDR->FileStatus & PROGRAM_TYPE ) {
            strcat ( name, ".PROG" );
        } else {
            sprintf ( name + strlen ( name ), ".%c%c%d",
                ( FDR->FileStatus & INTERNAL_TYPE ) ? 'I' : 'D',
                ( FDR->FileStatus & VARIABLE_TYPE ) ? 'V' : 'F',
                FDR->RecordLength ? FDR->RecordLength : 256 );
        }
    }

    // Find a unique filename to use
    char filename [ 80 ];
    for ( int i = 0; i < 255; i++ ) {
        sprintf ( filename, i ? "%s.%03d" : "%s", name, i );
        FILE *testFile = fopen ( filename, "rb" );
        if ( testFile == NULL ) break;
        fclose ( testFile );
    }

    const char *mode = (( convertFiles == true ) && (( FDR->FileStatus & ( INTERNAL_TYPE | PROGRAM_TYPE )) == 0 )) ? "wt" : "wb";

    // Open the file
    FILE *outFile = fopen ( filename, mode );
    if ( outFile == NULL ) return;

    // Write the file header (v9t9 FIAD)
    if ( convertFiles == false ) {
        char header [128];
        memset ( header, 0, 128 );
        memcpy ( header, FDR, 20 );
        fwrite ( header, 128, 1, outFile );
    }

    if ( convertFiles == true ) {
        int count;
        do {
            char buffer [DEFAULT_SECTOR_SIZE];
            count = file->ReadRecord ( buffer, DEFAULT_SECTOR_SIZE );
            if ( count >= 0 ) {
                // Handle variable length, internal type files
                if (( FDR->FileStatus & ( INTERNAL_TYPE | VARIABLE_TYPE )) == ( INTERNAL_TYPE | VARIABLE_TYPE )) {
                    fputc ( count, outFile );
                }
                fwrite ( buffer, count, 1, outFile );
                // Handle display type files
                if (( FDR->FileStatus & ( INTERNAL_TYPE | PROGRAM_TYPE )) == 0 ) {
                    fprintf ( outFile, "\n" );
                }
            }
        } while ( count >= 0 );
    } else {
        int totalSectors = GetUINT16 ( &FDR->TotalSectors );
        for ( int i = 0; i < totalSectors; i++ ) {
            char buffer [DEFAULT_SECTOR_SIZE];
            file->ReadSector ( i, buffer );
            fwrite ( buffer, DEFAULT_SECTOR_SIZE, 1, outFile );
        }
    }

    fclose ( outFile );
}

void PrintDiskLayout ( cDiskMedia *disk )
{
    FUNCTION_ENTRY ( NULL, "PrintDiskLayout", true );

    eDiskDensity density     = DENSITY_UNKNOWN;
    eDiskDensity lastDensity = DENSITY_UNKNOWN;

    printf ( "\n" );

    for ( int h = 0; h < disk->NumSides (); h++ ) {
        for ( int t = 0; t < disk->NumTracks (); t++ ) {
            sTrack *track = disk->GetTrack ( t, h );
            if ( track == NULL ) continue;
            if ( density == DENSITY_UNKNOWN ) {
                density = track->Density;
            } else if (( density != track->Density ) && ( track->Density != DENSITY_UNKNOWN )) {
                density = DENSITY_MIXED;
            }
            printf ( "Track: %2d  Side: %d - ", t, h );
            if ( track->Density == DENSITY_UNKNOWN ) {
                printf ( "is not formatted\n" );
                continue;
            }
            if ( lastDensity != track->Density ) {
                if ( lastDensity != DENSITY_UNKNOWN ) {
                    printf ( "density changed -" );
                }
                lastDensity = track->Density;
            }
            int  noSectors = 0;
            for ( int s = 0; s < MAX_SECTORS; s++ ) {
                if ( track->Sector [s].Data == NULL ) continue;
                printf ( " %d", track->Sector [s].LogicalSector );
                noSectors++;
            }
            printf ( "\n" );
            for ( int s = 0; s < MAX_SECTORS; s++ ) {
                if ( track->Sector [s].Data == NULL ) continue;
                if ( track->Sector [s].Data [-1] != 0xFB ) {
                    printf ( "  contains deleted sector %d\n", track->Sector [s].LogicalSector );
                }
                if ( track->Sector [s].LogicalCylinder != t ) {
                    printf ( "  sector %d has incorrect cylinder %d\n", track->Sector [s].LogicalSector, track->Sector [s].LogicalCylinder );
                }
                if ( track->Sector [s].LogicalSide != h ) {
                    printf ( "  sector %d has incorrect side %d\n", track->Sector [s].LogicalSector, track->Sector [s].LogicalSide );
                }
                if ( track->Sector [s].Size != 1 ) {
                    printf ( "  sector %d size is %d bytes\n", track->Sector [s].LogicalSector, 128 << track->Sector [s].Size );
                }
            }
        }
    }
}

void PrintDiskInfo ( cDiskMedia *disk )
{
    FUNCTION_ENTRY ( NULL, "PrintDiskInfo", true );

    printf ( "\n" );

    const char *strFormat = "<Unknown>";
    switch ( disk->GetFormat ()) {
        case FORMAT_RAW_TRACK :
            strFormat = "PC99";
            break;
        case FORMAT_RAW_SECTOR :
            strFormat = "v9t9";
            break;
        case FORMAT_ANADISK :
            strFormat = "AnaDisk";
            break;
        case FORMAT_CF7 :
            strFormat = "CF7+";
            break;
        default :
            break;
    }

    eDiskDensity density     = DENSITY_UNKNOWN;

    for ( int h = 0; h < disk->NumSides (); h++ ) {
        for ( int t = 0; t < disk->NumTracks (); t++ ) {
            sTrack *track = disk->GetTrack ( t, h );
            if ( track == NULL ) continue;
            if ( density == DENSITY_UNKNOWN ) {
                density = track->Density;
            } else if (( density != track->Density ) && ( track->Density != DENSITY_UNKNOWN )) {
                density = DENSITY_MIXED;
            }
        }
    }

    const char *txtDensity = "<Unknown>";
    switch ( density ) {
        case DENSITY_SINGLE :
            txtDensity = "Single";
            break;
        case DENSITY_DOUBLE :
            txtDensity = "Double";
            break;
        case DENSITY_MIXED :
            txtDensity = "Mix";
            break;
        default :
            break;
    }

    printf ( "   Format: %s\n", strFormat );
    printf ( "   Tracks: %d\n", disk->NumTracks ());
    printf ( "    Sides: %d\n", disk->NumSides ());
    printf ( "  Density: %s\n", txtDensity );
}

char *addFiles [127];
char *delFiles [127];
char *extFiles [127];

bool ParseFileName ( const char *arg, void *base )
{
    FUNCTION_ENTRY ( NULL, "ParseFileName", true );

    const char *ptr = strchr ( arg, '=' );

    // Handle special case of extract all
    if ( ptr == NULL ) {
        fprintf ( stderr, "A filename needs to be specified: '%s'\n", arg );
        return false;
    }

    char **list = ( char ** ) base;

    for ( int i = 0; i < 127; i++ ) {
        if ( list [i] == NULL ) {
            list [i] = strdup ( ptr + 1 );
            return true;
        }
    }

    fprintf ( stderr, "Too many files specified\n" );

    return false;
}

void PrintUsage ()
{
    FUNCTION_ENTRY ( NULL, "PrintUsage", true );

    fprintf ( stdout, "Usage: disk [options] file\n" );
    fprintf ( stdout, "\n" );
}

int main ( int argc, char *argv[] )
{
    FUNCTION_ENTRY ( NULL, "main", true );

    bool dumpFiles    = false;
    bool checkDisk    = false;
    bool convertFiles = false;
    bool verboseMode  = false;
    bool showLayout   = false;
    eDiskFormat outputFormat = FORMAT_UNKNOWN;

    sOption optList [] = {
        { 'a', "add=*<filename>",     OPT_NONE,                      true,              addFiles,      ParseFileName,  "Add <filename> to the disk image" },
        {  0,  "check",               OPT_VALUE_SET | OPT_SIZE_BOOL, true,              &checkDisk,    NULL,           "Check the integrity of the disk structures" },
        { 'c', "convert",             OPT_VALUE_SET | OPT_SIZE_BOOL, true,              &convertFiles, NULL,           "Convert extracted files to DOS files" },
        { 'd', "dump",                OPT_VALUE_SET | OPT_SIZE_BOOL, true,              &dumpFiles,    NULL,           "Extract all files to FIAD files" },
        { 'l', "layout",              OPT_VALUE_SET | OPT_SIZE_BOOL, true,              &showLayout,   NULL,           "Display the disk sector layout" },
        {  0,  "output=PC99",         OPT_VALUE_SET | OPT_SIZE_INT,  FORMAT_RAW_TRACK,  &outputFormat, NULL,           "Convert disk to PC99 format" },
        {  0,  "output=v9t9",         OPT_VALUE_SET | OPT_SIZE_INT,  FORMAT_RAW_SECTOR, &outputFormat, NULL,           "Convert disk to v9t9 DOAD format" },
        {  0,  "output=anadisk",      OPT_VALUE_SET | OPT_SIZE_INT,  FORMAT_ANADISK,    &outputFormat, NULL,           "Convert disk to AnaDisk format /w headers" },
        { 'r', "remove=*<filename>",  OPT_NONE,                      true,              delFiles,      ParseFileName,  "Remove <filename> from the disk image" },
        { 'v', "verbose",             OPT_VALUE_SET | OPT_SIZE_BOOL, true,              &verboseMode,  NULL,           "Display information about the disk image" },
        { 'e', "extract*=<filename>", OPT_NONE,                      true,              extFiles,      ParseFileName,  "Extract <filename> to v9t9 FIAD file" }
    };

    /*
       -f, --fix

       FDR >0014->001B    >8F >1B >09 >02 >8F >27 >09 >02
    */

    if ( argc == 1 ) {
        PrintHelp ( SIZE ( optList ), optList );
        return 0;
    }

    printf ( "TI-99/4A Diskette Viewer\n" );

    cFileSystem *disk = NULL;

    int index = 1;
    while ( index < argc ) {

        index = ParseArgs ( index, argc, argv, SIZE ( optList ), optList );

        if ( index < argc ) {
            if ( disk != NULL ) {
                fprintf ( stderr, "Only one disk image file can be specified\n" );
                return -1;
            }
            disk = cFileSystem::Open ( argv [index++], "disks" );
            if ( disk == NULL ) {
                fprintf ( stderr, "Unable to open disk image file \"%s\"\n", argv [index-1] );
                return -1;
            }
        }
    }

    if ( disk == NULL ) {
        fprintf ( stderr, "No disk image file specified\n" );
        return -1;
    }

    if ( disk->IsValid () == false ) {
        char name [MAX_FILENAME+1];
        disk->GetPath ( name, sizeof ( name ));
        fprintf ( stderr, "File \"%s\" does not contain a recognized disk format\n", name );
        disk->Release ( NULL );
        return -1;
    }

    cDiskMedia *media = NULL;

    cDiskFileSystem* diskfs = dynamic_cast<cDiskFileSystem*> ( disk );
    if ( diskfs != NULL ) {
        media = diskfs->GetMedia ( NULL );
        if ( media->GetFormat () != FORMAT_UNKNOWN ) {
            if ( showLayout == true ) {
                PrintDiskLayout ( media );
            }
            if ( verboseMode == true ) {
                PrintDiskInfo ( media );
            }
        }
    }

    if ( dumpFiles == true ) {
        // Ignore any files specified with '--extract=' and dump them all
        disk->GetFilenames ( extFiles );
    }

    // Extract existing files
    if ( extFiles [0] != NULL ) {
        if ( verboseMode == true ) fprintf ( stdout, "\n" );
        if ( verboseMode == true ) fprintf ( stdout, "Extracting files:\n" );
        for ( int i = 0; extFiles [i]; i++ ) {
            cFile *file = disk->OpenFile ( extFiles [i] );
            if ( file != NULL ) {
                if ( verboseMode == true ) fprintf ( stdout, "  %s\n", extFiles [i] );
                DumpFile ( file, convertFiles );
                file->Release ( NULL );
            } else {
                fprintf ( stderr, "  Unable to locate file %s\n", extFiles [i] );
            }
        }
    }

    // Remove existing files
    if ( delFiles [0] != NULL ) {
        if ( verboseMode == true ) fprintf ( stdout, "\n" );
        if ( verboseMode == true ) fprintf ( stdout, "Removing files:\n" );
        for ( int i = 0; delFiles [i]; i++ ) {
            if ( disk->DeleteFile ( delFiles [i] ) == true ) {
                if ( verboseMode == true ) fprintf ( stdout, "  %s\n", delFiles [i] );
            } else {
                fprintf ( stderr, "  Unable to delete file %s\n", delFiles [i] );
            }
        }
    }

    // Add new files here
    if ( addFiles [0] != NULL ) {
        if ( verboseMode == true ) fprintf ( stdout, "\n" );
        if ( verboseMode == true ) fprintf ( stdout, "Adding files:\n" );
        for ( int i = 0; addFiles [i]; i++ ) {
            cFile *file = cFile::Open ( addFiles [i] );
            if ( file != NULL ) {
                if ( verboseMode == true ) fprintf ( stdout, "  %s\n", addFiles [i] );
                if ( disk->AddFile ( file ) == false ) {
                    fprintf ( stderr, "  Unable to add file %s\n", addFiles [i] );
                }
                file->Release ( NULL );
            } else {
                fprintf ( stderr, "  Unable to locate file %s\n", addFiles [i] );
            }
        }
    }

    disk->ShowDirectory ( stdout, verboseMode );
    if ( checkDisk ) {
        disk->CheckDisk ();
    }

    if ( media != NULL ) {
        if (( media->GetFormat () == FORMAT_CF7 ) &&
            ( outputFormat != FORMAT_CF7 ) && ( outputFormat != FORMAT_UNKNOWN )) {
            fprintf ( stderr, "Changing a CF7+ disk's format is not supported\n" );
            media->Release ( NULL );
            disk->Release ( NULL );
            return -1;
        }
        if (( outputFormat != FORMAT_UNKNOWN ) || ( media->HasChanged () == true )) {
            if ( media->SaveFile ( outputFormat, true ) == false ) {
                fprintf ( stderr, "Unable to write to file \"%s\"\n", media->GetName ());
                media->Release ( NULL );
                disk->Release ( NULL );
                return -1;
            }
        }
        media->Release ( NULL );
    }

    disk->Release ( NULL );

    return 0;
}
