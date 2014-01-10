//----------------------------------------------------------------------------
//
// File:        dumpspch.cpp
// Date:        31-Jul-2002
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 2002-2004 Marc Rousseau, All Rights Reserved.
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
//----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "option.hpp"

DBG_REGISTER ( __FILE__ );

struct sNode {
    int          p_length;
    const char  *phrase;
    sNode       *prev;
    sNode       *next;
    int          d_length;
    const UINT8 *data;
    bool         copy;
    int          phrase_offset;
    int          data_offset;
};

static int          dataFormat;

static const UINT8 *vsmDataPtr;
static int          vsmBytesLeft;
static int          vsmBitsLeft;
static int          vsmData;

static inline UINT16 GetUINT16 ( const void *_ptr )
{
    FUNCTION_ENTRY ( NULL, "GetUINT16", true );

    const UINT8 *ptr = ( const UINT8 * ) _ptr;
    return ( UINT16 ) (( ptr [0] << 8 ) | ptr [1] );
}

sNode *ReadNode ( const UINT8 *rom, int offset )
{
    FUNCTION_ENTRY ( NULL, "ReadNode", true );

    sNode *node = new sNode;
    memset ( node, 0, sizeof ( sNode ));

    int length = rom [ offset ];

    const UINT8 *ptr = &rom [ offset + length + 1 ];

    int prevOffset = GetUINT16 ( ptr );
    int nextOffset = GetUINT16 ( ptr + 2 );
    int dataOffset = GetUINT16 ( ptr + 5 );

    node->p_length = length;
    node->phrase   = ( char * ) rom + offset + 1;
    node->data     = rom + dataOffset;
    node->d_length = ptr [7];
    node->copy     = false;

    if ( prevOffset ) node->prev   = ReadNode ( rom, prevOffset );
    if ( nextOffset ) node->next   = ReadNode ( rom, nextOffset );

    return node;
}

int ReadBits ( int count )
{
    FUNCTION_ENTRY ( NULL, "ReadBits", false );

    UINT8 data = 0;

    while ( count-- ) {

        if ( vsmBitsLeft == 0 ) {
            if ( vsmBytesLeft == 0 ) {
                throw "End of speech data reached with no STOP CODE frame";
            }
            vsmBytesLeft--;
            vsmData      = *vsmDataPtr++;
            vsmBitsLeft  = 8;
        }

        data <<= 1;
        if ( vsmData & 0x80 ) {
            data |= 1;
        }

        vsmData <<= 1;
        vsmBitsLeft--;
    }

    return data;
}

bool DumpFrame ( FILE *file )
{
    FUNCTION_ENTRY ( NULL, "DumpFrame", false );

    int index = ReadBits ( 4 );

    if ( index == 0x00 ) {
        // ZERO energy frame
        if ( file != NULL ) fprintf ( file, " Z " );
        return false;
    }

    if ( index == 0x0F ) {
        // STOP frame
        if ( file != NULL ) fprintf ( file, " S" );
        return true;
    }

    int repeat = ReadBits ( 1 );
    int pitch  = ReadBits ( 6 );

    if ( file != NULL ) fprintf ( file, " P%d E%d%s ", pitch, index, repeat ? " R" : "" );

    if ( repeat == 0 ) {

        // Voiced/Unvoiced frame
        if ( file != NULL ) {
            fprintf ( file, "K%d ", ReadBits ( 5 ));
            fprintf ( file, "K%d ", ReadBits ( 5 ));
            fprintf ( file, "K%d ", ReadBits ( 4 ));
            fprintf ( file, "K%d ", ReadBits ( 4 ));
        } else {
            ReadBits ( 18 );
        }

        if ( pitch != 0 ) {

            if ( file != NULL ) {
                // Voiced frame
                fprintf ( file, "K%d ", ReadBits ( 4 ));
                fprintf ( file, "K%d ", ReadBits ( 4 ));
                fprintf ( file, "K%d ", ReadBits ( 4 ));
                fprintf ( file, "K%d ", ReadBits ( 3 ));
                fprintf ( file, "K%d ", ReadBits ( 3 ));
                fprintf ( file, "K%d ", ReadBits ( 3 ));
            } else {
                ReadBits ( 21 );
            }

        }

    }

    return false;
}

int DumpSpeechData ( FILE *file, sNode *node )
{
    FUNCTION_ENTRY ( NULL, "DumpSpeechData", true );

    vsmBytesLeft = node->d_length;
    vsmDataPtr   = node->data;
    vsmBitsLeft  = 0;

    try {

        while ( DumpFrame ( file ) == false ) {
        }

    }

    catch ( const char *msg ) {
        fprintf ( stderr, "Phrase: \"%*.*s\" - %s\n", node->p_length, node->p_length, node->phrase, msg );
    }

    return vsmBytesLeft;
}

void DumpPhrase ( sNode *node, FILE *spchFile )
{
    FUNCTION_ENTRY ( NULL, "DumpPhrase", true );

    if ( node->prev ) DumpPhrase ( node->prev, spchFile );

    fprintf ( spchFile, "\"%*.*s\"%*.*s -", node->p_length, node->p_length, node->phrase, 20 - node->p_length, 20 - node->p_length, "" );

    if ( dataFormat == 0 ) {
        for ( int i = 0; i < node->d_length; i++ ) {
            fprintf ( spchFile, " %02X", node->data [i] );
        }
    } else if ( dataFormat == 1 ) {
        DumpSpeechData ( spchFile, node );
    } else {
        DBG_ERROR ( "Unrecognized data format (" << dataFormat << ")" );
    }

    fprintf ( spchFile, "\n" );

    if ( node->next ) DumpPhrase ( node->next, spchFile );
}

void DumpROM ( sNode *root, FILE *spchFile )
{
    FUNCTION_ENTRY ( NULL, "DumpROM", true );

    fprintf ( spchFile, "# TMS5220 Speech ROM data file\n" );
    fprintf ( spchFile, "\n" );

    DumpPhrase ( root, spchFile );
}

int CheckData ( sNode *node, int *phrases, int *unique, int *data_used, const UINT8 *rom, UINT8 *flags )
{
    FUNCTION_ENTRY ( NULL, "CheckData", true );

    int wasted = 0;

    if ( node->prev ) wasted += CheckData ( node->prev, phrases, unique, data_used, rom, flags );

    *phrases   += 1;
    *data_used += 1 + node->p_length + 6;

    int offset = node->data - rom;

    if ( flags [offset] == 0 ) {
        *unique    += 1;
        *data_used += node->d_length;
        wasted     += DumpSpeechData ( NULL, node );
        memset ( flags + offset, 1, node->d_length );

        if (( vsmBytesLeft > 0 ) && ( verbose != 0 )) {
            fprintf ( stderr, "%d bytes left processing phrase %*.*s\n", vsmBytesLeft, node->p_length, node->p_length, node->phrase );
        }

    }

    if ( node->next ) wasted += CheckData ( node->next, phrases, unique, data_used, rom, flags );

    return wasted;
}

void PrintStats ( sNode *root, const UINT8 *rom )
{
    UINT8 flags [ 0x8000 ];
    memset ( flags, 0, sizeof ( flags ));

    int phrases     = 0;
    int unique      = 0;
    int data_used   = 1;

    int data_wasted = CheckData ( root, &phrases, &unique, &data_used, rom, flags );

    if (( data_wasted > 0 ) && ( verbose != 0 )) fprintf ( stdout, "\n" );

    fprintf ( stdout, "%7d Phrases (%d unique)\n", phrases, unique );
    fprintf ( stdout, "%7d Bytes used (%d bytes excess)\n", data_used, data_wasted );
    fprintf ( stdout, "%7d Bytes free (potentially %d bytes)\n", 0x8000 - data_used, 0x8000 - data_used + data_wasted );
    fprintf ( stdout, "\n" );
}

void FreeTree ( sNode *node )
{
    if ( node->prev != NULL ) FreeTree ( node->prev );
    if ( node->next != NULL ) FreeTree ( node->next );

    delete node;
}

bool ParseFileName ( const char *arg, void *filename )
{
    FUNCTION_ENTRY ( NULL, "ParseFileName", true );

    const char *ptr = strchr ( arg, '=' );

    // Handle special case of extract all
    if ( ptr == NULL ) {
        fprintf ( stderr, "A filename needs to be specified: '%s'\n", arg );
        return false;
    }

    strcpy (( char * ) filename, ptr + 1 );

    return true;
}

void PrintUsage ()
{
    FUNCTION_ENTRY ( NULL, "PrintUsage", true );

    fprintf ( stdout, "Usage: dumpspch [options] file\n" );
    fprintf ( stdout, "\n" );
}

int main ( int argc, char *argv[] )
{
    FUNCTION_ENTRY ( NULL, "main", true );

    char outputFile [256] = "spchrom.dat";

    sOption optList [] = {
        {  0,  "format=hex",          OPT_VALUE_SET | OPT_SIZE_INT,  0,    &dataFormat,   NULL,           "Speech data listed in hexadecimal" },
        {  0,  "format=spch",         OPT_VALUE_SET | OPT_SIZE_INT,  1,    &dataFormat,   NULL,           "Decoded speech data" },
        { 'o', "output=*<filename>",  OPT_NONE,                      true, outputFile,    ParseFileName,  "Create output file <filename>" },
        { 'v', "verbose",             OPT_VALUE_SET | OPT_SIZE_BOOL, true, &verbose,      NULL,           "Display extra information" }
    };

    if ( argc == 1 ) {
        PrintHelp ( SIZE ( optList ), optList );
        return 0;
    }

    printf ( "TI-99/4A Speech ROM Dump Utility\n" );

    int index = 1;
    index = ParseArgs ( index, argc, argv, SIZE ( optList ), optList );

    if ( index >= argc ) {
        fprintf ( stderr, "No input file specified\n" );
        return -1;
    }

    FILE *romFile = fopen ( argv [index], "rb" );
    if ( romFile == NULL ) {
        fprintf ( stderr, "Unable to open input file \"%s\"\n", argv [index] );
        return -1;
    }

    UINT8 ROM [ 0x8000 ];
    memset ( ROM, 0, sizeof ( ROM ));

    if ( fread ( ROM, sizeof ( ROM ), 1, romFile ) != 1 ) {
        fprintf ( stderr, "Error reading from file \"%s\"\n", argv [index] );
        fclose ( romFile );
        return -1;
    }

    fclose ( romFile );

    sNode *root = ReadNode ( ROM, 1 );

    FILE *datFile = fopen ( outputFile, "wt" );
    if ( datFile == NULL ) {
        fprintf ( stderr, "Unable to open output file \"%s\"\n", outputFile );
        FreeTree ( root );
        return -1;
    }

    DumpROM ( root, datFile );

    fclose ( datFile );

    fprintf ( stdout, "\n" );

    PrintStats ( root, ROM );

    FreeTree ( root );

    return 0;
}
