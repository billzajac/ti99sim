//----------------------------------------------------------------------------
//
// File:        mkspch.cpp
// Date:        06-May-2001
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 2001-2004 Marc Rousseau, All Rights Reserved.
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "option.hpp"
#include "support.hpp"

DBG_REGISTER ( __FILE__ );

struct sNode {
    size_t       p_length;
    const char  *phrase;
    sNode       *prev;
    sNode       *next;
    const UINT8 *data;
    size_t       d_length;
    bool         copy;
    size_t       phrase_offset;
    size_t       data_offset;
};

const char *ReadPhrase ( const char *ptr, sNode *node )
{
    FUNCTION_ENTRY ( NULL, "ReadPhrase", false );

    if ( *ptr == '"' ) {
        const char *end = ptr + 1;
        while (( *end != '\0' ) && ( *end != '"' )) end++;
        node->p_length = end - ptr - 1;
        node->phrase   = node->p_length ? strndup ( ptr + 1, node->p_length ) : NULL;
        ptr = ( *end == '"' ) ? end + 1 : end;
    } else {
        const char *end = ptr + 1;
        while (( *end != '\0' ) && ! isspace ( *end )) end++;
        node->p_length = end - ptr;
        node->phrase   = node->p_length ? strndup ( ptr, node->p_length ) : NULL;
        ptr = end;
    }

    while ( isspace ( *ptr )) ptr++;

    return ptr;
}

void ReadHexData ( const char *ptr, sNode *node )
{
    FUNCTION_ENTRY ( NULL, "ReadHexData", false );

    UINT8 buffer [0x8000];
    int  data_length = 0;

    while ( *ptr ) {
        unsigned int byte  = 0;
        int count = sscanf ( ptr, "%02X", &byte );

        if ( count != 1 ) {
            fprintf ( stderr, "bad data!\n" );
            return;
        }

        buffer [data_length++] = ( UINT8 ) byte;

        ptr += 2;
        while ( isspace ( *ptr )) ptr++;
    }

    UINT8 *data = new UINT8 [ data_length ];
    memcpy ( data, buffer, data_length );

    node->data     = data;
    node->d_length = data_length;
}

static UINT8 *vsmDataPtr;
static int    vsmBitsLeft;
static int    vsmData;

void WriteBits ( int data, int count )
{
    FUNCTION_ENTRY ( NULL, "WriteBits", false );

    int mask = 1 << ( count - 1 );

    if ( data >= ( 1 << count )) {
        DBG_WARNING ( "data (" << data << ") contains more than " << count << " bits" );
    }

    while ( count-- ) {

        vsmBitsLeft--;
        vsmData <<= 1;

        if ( data & mask ) {
            vsmData |= 1;
        }

        mask >>= 1;

        if ( vsmBitsLeft == 0 ) {
            *vsmDataPtr++ = ( UINT8 ) vsmData;
            vsmBitsLeft   = 8;
        }
    }
}

const char *ReadCoefficient ( const char *ptr, int bits )
{
    FUNCTION_ENTRY ( NULL, "ReadCoefficient", false );

    if ( *ptr != 'K' ) throw "Syntax error";

    int coefficient = atoi ( ptr + 1 );

    WriteBits ( coefficient, bits );

    while ( ! isspace ( *ptr )) ptr++;
    while ( isspace ( *ptr )) ptr++;

    return ptr;
}

const char *ParseFrame ( const char *ptr )
{
    FUNCTION_ENTRY ( NULL, "ParseFrame", false );

    switch ( *ptr ) {
        case 'Z' :
            ptr++;
            WriteBits ( 0x00, 4 );
            break;
        case 'S' :
            ptr++;
            WriteBits ( 0x0F, 4 );
            break;
        case 'P' :
            {
                int pitch = atoi ( ptr + 1 );

                while ( ! isspace ( *ptr )) ptr++;
                while ( isspace ( *ptr )) ptr++;

                if ( *ptr != 'E' ) throw "Syntax error";

                int energy = atoi ( ptr + 1 );

                while ( ! isspace ( *ptr )) ptr++;
                while ( isspace ( *ptr )) ptr++;

                WriteBits ( energy, 4 );

                if ( *ptr == 'R' ) {
                    ptr++;
                    WriteBits ( 1, 1 );
                    WriteBits ( pitch, 6 );
                    break;
                } else {
                    WriteBits ( 0, 1 );
                    WriteBits ( pitch, 6 );

                    ptr = ReadCoefficient ( ptr, 5 );
                    ptr = ReadCoefficient ( ptr, 5 );
                    ptr = ReadCoefficient ( ptr, 4 );
                    ptr = ReadCoefficient ( ptr, 4 );

                    if ( pitch != 0 ) {
                        ptr = ReadCoefficient ( ptr, 4 );
                        ptr = ReadCoefficient ( ptr, 4 );
                        ptr = ReadCoefficient ( ptr, 4 );
                        ptr = ReadCoefficient ( ptr, 3 );
                        ptr = ReadCoefficient ( ptr, 3 );
                        ptr = ReadCoefficient ( ptr, 3 );
                    }

                }
            }
            break;
        default :
            throw "Error parsing speech frame";
            break;
    }

    while ( isspace ( *ptr )) ptr++;

    return ptr;
}

void ReadParsedData ( const char *ptr, sNode *node )
{
    FUNCTION_ENTRY ( NULL, "ReadParsedData", false );

    try {

        UINT8 buffer [0x8000];

        vsmDataPtr  = buffer;
        vsmBitsLeft = 8;
        vsmData     = 0;

        while ( *ptr ) {
            ptr = ParseFrame ( ptr );
        }

        if ( vsmBitsLeft != 8 ) {
            *vsmDataPtr++ = ( UINT8 ) ( vsmData << vsmBitsLeft );
        }

        int data_length = vsmDataPtr - buffer;

        UINT8 *data = new UINT8 [ data_length ];
        memcpy ( data, buffer, data_length );

        node->data     = data;
        node->d_length = data_length;
    }

    catch ( const char *msg ) {
        fprintf ( stderr, "Phrase: \"%*.*s\" - %s\n", ( int ) node->p_length, ( int ) node->p_length, node->phrase, msg );
    }
}

void ReadData ( const char *ptr, sNode *node )
{
    FUNCTION_ENTRY ( NULL, "ReadData", false );

    // We'll figure this out later
    node->copy     = false;
    node->data     = NULL;
    node->d_length = 0;

    if ( strncmp ( ptr, "<null>", 6 ) == 0 ) {
        UINT8 *data = new UINT8 [1];

        data [0] = 0xF0;

        node->data     = data;
        node->d_length = 1;
    } else if ( *ptr == '-' ) {
        ptr += 1;
        while ( isspace ( *ptr )) ptr++;
        if ( isxdigit ( *ptr )) {
            ReadHexData ( ptr, node );
        } else {
            ReadParsedData ( ptr, node );
        }
    }
}

sNode *ReadNode ( FILE *file )
{
    FUNCTION_ENTRY ( NULL, "ReadNode", true );

    static int line = 0;

    static char buffer [0x8000 * 3 + 128 ];
    const char *ptr;

    // Read the next line from the file - ignore blank or comment lines
    do {
        line++;
        if ( fgets ( buffer, sizeof ( buffer ), file ) == NULL ) return NULL;
        ptr = buffer;
        while ( isspace ( *ptr )) ptr++;
    } while (( *ptr == '#' ) || ( *ptr == '\0' ));

    // Remove the trailing '\n'
    buffer [ strlen ( buffer ) - 1 ] = '\0';

    sNode *node = new sNode;
    memset ( node, 0, sizeof ( sNode ));

    ptr = ReadPhrase ( ptr, node );

    if ( node->phrase == NULL ) {
        fprintf ( stdout, "Invalid phrase on line %d\n", line );
        exit ( -1 );
    }

    ReadData ( ptr, node );

    if ( node->data == NULL ) {
        fprintf ( stdout, "Invalid speech data on line %d\n", line );
        exit ( -1 );
    }

    if ( node->d_length > 255 ) {
        fprintf ( stdout, "Phrase \"%*.*s\" contains too much speech data on line %d\n", ( int ) node->p_length, ( int ) node->p_length, node->phrase, line );
        exit ( -1 );
    }

    return node;
}

int sort_node ( const void *ptr1, const void *ptr2 )
{
    FUNCTION_ENTRY ( NULL, "sort_node", false );

    const sNode *node1 = * ( const sNode ** ) ptr1;
    const sNode *node2 = * ( const sNode ** ) ptr2;

    return strcmp ( node1->phrase, node2->phrase );
}

sNode *SplitList ( sNode **list, int size )
{
    FUNCTION_ENTRY ( NULL, "SplitList", true );

    if ( size == 0 ) return NULL;

    int rootIndex = size / 2;
    sNode *root = list [ rootIndex ];

    root->prev = SplitList ( list, rootIndex );
    root->next = SplitList ( list + rootIndex + 1, size - rootIndex - 1 );

    return root;
}

size_t CalculateOffsets ( sNode *root, sNode **list, int count )
{
    FUNCTION_ENTRY ( NULL, "CalculateOffsets", true );

    // Calculate the phrase offsets
    size_t offset = 1;
    root->phrase_offset = offset;
    offset += 1 + strlen ( root->phrase ) + 2 + 2 + 4;
    for ( int i = 0; i < count; i++ ) {
        sNode *node = list [i];
        if ( node == root ) continue;
        node->phrase_offset = offset;
        offset += 1 + strlen ( node->phrase ) + 2 + 2 + 4;
    }

    // Calculate the data offsets
    for ( int i = 0; i < count; i++ ) {
        sNode *node = list [i];
        // Look for a match in an earlier phrase
        for ( int j = 0; j < i; j++ ) {
            if ( list [j]->d_length >= node->d_length ) {
                // Look for this phrase at the end of a previous one
                size_t size_offset = list [j]->d_length - node->d_length;
                if ( memcmp (( char * ) list [j]->data + size_offset, node->data, node->d_length ) == 0 ) {
                    node->copy = true;
                    node->data_offset = list [j]->data_offset;
                }
            }
        }
        if ( node->copy == false ) {
            node->data_offset = offset;
            offset += node->d_length;
        }
    }

    return offset;
}

UINT8 *StoreNode ( const sNode *node, UINT8 *ptr )
{
    FUNCTION_ENTRY ( NULL, "StoreNode", true );

    ptr += sprintf (( char * ) ptr, "%c%s", ( char ) node->p_length, node->phrase );

    size_t offset;

    offset = node->prev ? node->prev->phrase_offset : 0;
    *ptr++ = ( UINT8 ) ( offset >> 8 );
    *ptr++ = ( UINT8 ) ( offset & 0xFF );

    offset = node->next ? node->next->phrase_offset : 0;
    *ptr++ = ( UINT8 ) ( offset >> 8 );
    *ptr++ = ( UINT8 ) ( offset & 0xFF );

    offset = node->data_offset;
    *ptr++ = ( UINT8 ) ( '\0' );              // Dunno what this is yet
    *ptr++ = ( UINT8 ) ( offset >> 8 );
    *ptr++ = ( UINT8 ) ( offset & 0xFF );
    *ptr++ = ( UINT8 ) ( node->d_length );

    return ptr;
}

void MakeROM ( const sNode *root, sNode **list, int count, UINT8 *rom )
{
    FUNCTION_ENTRY ( NULL, "MakeROM", true );

    UINT8 *ptr = rom;

    *ptr++ = 0xAA;

    // Store the phrases
    ptr = StoreNode ( root, ptr );
    for ( int i = 0; i < count; i++ ) {
        const sNode *node = list [i];
        if ( node == root ) continue;
        ptr = StoreNode ( node, ptr );
    }

    // Store the data
    for ( int i = 0; i < count; i++ ) {
        const sNode *node = list [i];
        if ( node->copy == false ) {
            memcpy ( ptr, node->data, node->d_length );
            ptr += node->d_length;
        }
    }
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

    fprintf ( stdout, "Usage: mkspch [options] file\n" );
    fprintf ( stdout, "\n" );
}

size_t vsmBytesLeft;

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

size_t DumpSpeechData ( FILE *file, sNode *node )
{
    FUNCTION_ENTRY ( NULL, "DumpSpeechData", true );

    vsmBytesLeft = node->d_length;
    vsmDataPtr   = const_cast < UINT8 * > ( node->data );
    vsmBitsLeft  = 0;

    try {

        while ( DumpFrame ( file ) == false ) {
        }

    }

    catch ( const char *msg ) {
        fprintf ( stderr, "Phrase: \"%*.*s\" - %s\n", ( int ) node->p_length, ( int ) node->p_length, node->phrase, msg );
    }

if ( vsmBytesLeft > 0 ) fprintf ( stdout, "Phrase: \"%*.*s\" has %d extra bytes\n", ( int ) node->p_length, ( int ) node->p_length, node->phrase, ( int ) vsmBytesLeft );

    return vsmBytesLeft;
}

size_t CheckData ( sNode *node, int *phrases, int *unique, size_t *data_used, const UINT8 *rom, UINT8 *flags )
{
    FUNCTION_ENTRY ( NULL, "CheckData", true );

    size_t wasted = 0;

    if ( node->prev ) wasted += CheckData ( node->prev, phrases, unique, data_used, rom, flags );

    *phrases   += 1;
    *data_used += 1 + node->p_length + 6;

    size_t offset = node->data_offset;

    if ( flags [offset] == 0 ) {
        *unique    += 1;
        *data_used += node->d_length;
        wasted     += DumpSpeechData ( NULL, node );
        memset ( flags + offset, 1, node->d_length );

        if (( vsmBytesLeft > 0 ) && ( verbose != 0 )) {
            fprintf ( stderr, "%d bytes left processing phrase %*.*s\n", ( int ) vsmBytesLeft, ( int ) node->p_length, ( int ) node->p_length, node->phrase );
        }

    }

    if ( node->next ) wasted += CheckData ( node->next, phrases, unique, data_used, rom, flags );

    return wasted;
}

void PrintStats ( sNode *root, const UINT8 *rom )
{
    UINT8 flags [ 0x8000 ];
    memset ( flags, 0, sizeof ( flags ));

    int phrases      = 0;
    int unique       = 0;
    size_t data_used = 1;

    size_t data_wasted = CheckData ( root, &phrases, &unique, &data_used, rom, flags );

    if (( data_wasted > 0 ) && ( verbose != 0 )) fprintf ( stdout, "\n" );

    fprintf ( stdout, "%7d Phrases (%d unique)\n", phrases, unique );
    fprintf ( stdout, "%7d Bytes used (%d bytes excess)\n", ( int ) data_used, ( int ) data_wasted );
    fprintf ( stdout, "%7d Bytes free (potentially %d bytes)\n", ( int ) ( 0x8000 - data_used ), ( int ) ( 0x8000 - data_used + data_wasted ));
    fprintf ( stdout, "\n" );
}

int main ( int argc, char *argv[] )
{
    FUNCTION_ENTRY ( NULL, "main", true );

    char outputFile [256] = "spchrom.bin";

    sOption optList [] = {
        { 'o', "output=*<filename>",  OPT_NONE,                      true, outputFile,    ParseFileName,  "Create output file <filename>" },
        { 'v', "verbose",             OPT_VALUE_SET | OPT_SIZE_BOOL, true, &verbose,      NULL,           "Display extra information" }
    };

    if ( argc == 1 ) {
        PrintHelp ( SIZE ( optList ), optList );
        return 0;
    }

    printf ( "TI-99/4A Speech ROM Utility\n" );

    int index = 1;
    index = ParseArgs ( index, argc, argv, SIZE ( optList ), optList );

    if ( index >= argc ) {
        fprintf ( stderr, "No input file specified\n" );
        return -1;
    }

    FILE *datFile = fopen ( argv [index], "rt" );
    if ( datFile == NULL ) {
        fprintf ( stderr, "Unable to open input file \"%s\"\n", argv [index] );
        return -1;
    }

    sNode *list [ 3277 ];
    int count = 0;

    // Read in the basic phrases and their speech data
    for ( EVER ) {
        sNode *node = ReadNode ( datFile );
        if ( node == NULL ) break;
        list [count++] = node;
    }

    fclose ( datFile );

    // Put them in alphabetical order
    qsort ( list, count, sizeof ( sNode * ), sort_node );

    // Build the binary tree
    sNode *root = SplitList ( list, count );

    size_t size = CalculateOffsets ( root, list, count );

    if ( size > 0x8000 ) {
        fprintf ( stderr, "There is too much data in the file (over by %d bytes)\n", ( int ) ( size - 0x8000 ));
        return -1;
    }

    fprintf ( stdout, "\n" );
    fprintf ( stdout, "%5d Phrases read\n", count );
    fprintf ( stdout, "%7d Bytes used\n", ( int ) size );
    fprintf ( stdout, "\n" );

    UINT8 ROM [ 0x8000 ];
    memset ( ROM, 0, sizeof ( ROM ));

    MakeROM ( root, list, count, ROM );
PrintStats ( root, ROM );

    for ( int i = 0; i < count; i++ ) {
        free (( void * ) list [i]->phrase );
        delete [] const_cast < UINT8 * > ( list [i]->data );
        delete list [i];
    }

    FILE *romFile = fopen ( outputFile, "wb" );
    if ( romFile == NULL ) {
        fprintf ( stderr, "Unable to open output file \"%s\"\n", outputFile );
        return -1;
    }

    fwrite ( ROM, sizeof ( ROM ), 1, romFile );

    fclose ( romFile );

    return 0;
}
