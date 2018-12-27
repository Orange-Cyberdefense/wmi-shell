/*
 * base64.c
 *
 * Base64 encoding/decoding command line filter
 *
 * Copyright (c) 2002 Matthias Gaertner 29.06.2002
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Small dedicated Base64 encoder and decoder for the command line.
 *
 * Usage examples:
 *
 * Encode CRL file to Base64 on stdout
 *  base64 foo.crl
 *  base64 -e foo.crl
 *  base64 -in foo.crl
 *  base64 -e -in foo.crl
 *
 * Encode certificate file to PEM-ish Base64 output file foo.pem
 *  base64 -E "CERTIFICATE" foo.crt foo.pem
 *
 * Encode file, output no line breaks in Base64 area. Not recommended with -E.
 *  base64 -n 0 foo.crt
 *
 * Encode file, maximum number of characters per line is 32.
 *  base64 -n 32 foo.crt
 *
 * Decode file to output file
 *  base64 -d foo.pem foo.crt
 *  base64 -d -i foo.pem foo.crt
 *  base64 -d -i foo.pem -o foo.crt
 *
 * Decode file and pipe to next program
 *  base64 -d foo.pem | md5sum
 *  ls / | base64 | base64 -d
 *
 * Invoke as b642bin: decoding is the default.
 *  b642bin foo.pem foo.bin
 *
 * Note: when decoding, this decoder ignores all non-Base64 caharcters.
 * After a hyphen (-) all characters are ignored up to the next \n.
 * The first non-ignored equals sign (=) indicates the end of b64 data.
 */

/*
 * Installation:
 * Simply compile with
 *  gcc -O2 -o base64 base64.c
 * then install the executable somewhere on your PATH.
 * Optionally, make a symbolic link to it under the name b642bin
 *  ln -s base64 b642bin
 *
 * De-installation:
 * Remove the installade program and the optional link.
 *
 * Other files are neither needed nor created.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef WIN32
#ifndef _WIN32
#define _WIN32 1
#endif
#endif

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif


#ifdef _WIN32
#define STRCMP strcmp
#define USE_CRLF 1
#else
#define STRCMP strcmp
#define USE_LF 1
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL ((void*)0)
#endif


#define B64_OK             0
#define B64_ERR_CMDLINE    1
#define B64_ERR_INPUT      2
#define B64_ERR_OUTPUT     3
#define B64_ERR_MEMORY     4
#define B64_ERR_READING    5
#define B64_ERR_WRITING    6
#define B64_ERR_SYNTAX     7

#define ENCODE_BUFFER_SIZE_IN (8192*3)
#define ENCODE_BUFFER_SIZE_OUT (8192*6)

#define DECODE_BUFFER_SIZE_IN (8192*4)
#define DECODE_BUFFER_SIZE_OUT (8192*3)


/* Option variables */
static int g_fDecode = FALSE;
static int g_fUseCRLF = FALSE;

static char *g_pszFilenameIn = NULL;
static FILE *g_fIn = NULL;

static char *g_pszFilenameOut = NULL;
static FILE *g_fOut = NULL;

static char *g_pszCharsPerLine = NULL;
static unsigned int g_nCharsPerLine = 64;

static char *g_pszHeaderLine = NULL;
static unsigned int g_nHeaderLine = 0;


static void help( void )
{
    fprintf(stderr,"Base64 [options] [input file] [output file]\n");
    fprintf(stderr,"  options are:\n");
    fprintf(stderr,"  -i <filename> input file (default: stdin)\n");
    fprintf(stderr,"  -o <filename> output file (default: stdout)\n");
    fprintf(stderr,"  -e            encode binary to Base64 (default)\n");
    fprintf(stderr,"  -d            decode Base64 to binary\n");
    fprintf(stderr,"  -n <n>        encode n characters per line (0:no line breaks,default:64)\n");
    fprintf(stderr,"  -E <STRING>   encode and put -----BEGIN/END <STRING>----- around output\n");
    fprintf(stderr,"  --            indicate end of options\n");
    fprintf(stderr,"Call as b642bin to preselect decoding\n");
    fprintf(stderr,"(c) Matthias Gaertner 2002 - v1.00\n\n");
    fprintf(stderr,"This program is distributed in the hope that it will be useful,\n");
    fprintf(stderr,"but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
    fprintf(stderr,"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
    fprintf(stderr,"GNU General Public License for more details.\n");
}

static int parseCmdLine( int argc, char **argv )
{
    int r = B64_OK;
    int n = 1;
    int z = 0;  // 0: normal, 1: 2nd argument expected,
                // 4: end of params detected (--)
    char **ppSecondArg = NULL;

    while( n < argc )
    {
        if( z == 4 )
        {
            if( g_pszFilenameIn == NULL )
            {
                g_pszFilenameIn = argv[n];
            }
            else if( g_pszFilenameIn != NULL && g_pszFilenameOut == NULL )
            {
                g_pszFilenameOut = argv[n];
            }
        }
        else if( z > 0 )
        {
            *ppSecondArg = argv[n];
            if( z == 2 )
            {
                // decode bytes per line
                char *p = *ppSecondArg;
                g_nCharsPerLine = 0;
                while( *p >= '0' && *p <= '9' )
                {
                    g_nCharsPerLine = (g_nCharsPerLine*10)+(*p-'0');
                    p++;
                }
                if( *p != '\0' || p == *ppSecondArg )
                {
                    r = B64_ERR_CMDLINE;
                    break;
                }
                if( g_nCharsPerLine > 0 && g_nCharsPerLine < 4 )
                {
                    g_nCharsPerLine = 4;
                }
            }
            if( z == 3 )
            {
                g_nHeaderLine = strlen( *ppSecondArg );
            }
            z = 0;
        }
        else if( STRCMP( argv[n], "-?" ) == 0 )
        {
            help();
            r = B64_ERR_CMDLINE;
            break;
        }
        else if( STRCMP( argv[n], "--help" ) == 0 )
        {
            help();
            r = B64_ERR_CMDLINE;
            break;
        }
        else if( STRCMP( argv[n], "-i" ) == 0 )
        {
            z=1;
            ppSecondArg = &g_pszFilenameIn;
        }
        else if( STRCMP( argv[n], "-o" ) == 0 )
        {
            z=1;
            ppSecondArg = &g_pszFilenameOut;
        }
        else if( STRCMP( argv[n], "-e" ) == 0 )
        {
            g_fDecode = FALSE;
        }
        else if( STRCMP( argv[n], "-E" ) == 0 )
        {
            g_fDecode = FALSE;
            z = 3;
            ppSecondArg = &g_pszHeaderLine;
        }
        else if( STRCMP( argv[n], "-d" ) == 0 )
        {
            g_fDecode = TRUE;
        }
        else if( STRCMP( argv[n], "-n" ) == 0 )
        {
            z = 2;
            ppSecondArg = &g_pszCharsPerLine;
        }
        else if( STRCMP( argv[n], "--" ) == 0 )
        {
            z = 4;
        }
        else if( argv[n][0] == '-' )
        {
            help();
            r = B64_ERR_CMDLINE;
            break;
        }
        else if( g_pszFilenameIn == NULL )
        {
            g_pszFilenameIn = argv[n];
        }
        else if( g_pszFilenameIn != NULL && g_pszFilenameOut == NULL )
        {
            g_pszFilenameOut = argv[n];
        }
        else
        {
            help();
            r = B64_ERR_CMDLINE;
            break;
        }
        n++;
    }
    if( z > 0 )
    {
        r = B64_ERR_CMDLINE;
    }
    return r;
}

static int openFiles()
{
    if( g_pszFilenameIn != NULL )
    {
        g_fIn = fopen( g_pszFilenameIn, "rb" );
        if( g_fIn == NULL )
        {
            return B64_ERR_INPUT;
        }
    }
    else
    {
        g_fIn = stdin;
#ifdef WIN32
        _setmode(fileno(stdin), _O_BINARY);
#endif
    }

    if( g_pszFilenameOut != NULL )
    {
        g_fOut = fopen( g_pszFilenameOut, "wb" );
        if( g_fOut == NULL )
        {
            return B64_ERR_OUTPUT;
        }
    }
    else
    {
        g_fOut = stdout;
#ifdef WIN32
        setmode(fileno(stdout), O_BINARY);
#endif
    }

    return B64_OK;
}

static void closeFiles( int r )
{
    if( g_fIn != NULL && g_pszFilenameIn != NULL )
    {
        fclose(g_fIn);
        g_fIn = NULL;
    }

    if( g_fOut != NULL && g_pszFilenameOut != NULL )
    {
        fclose(g_fOut);
        g_fOut = NULL;
        if( r != B64_OK )
        {
            unlink( g_pszFilenameOut );
        }
    }
}

static const char* to_b64 =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int encodeB64()
{
    int r = B64_OK;
    char *pbBufferIn = (char*) malloc( ENCODE_BUFFER_SIZE_IN );
    char *pbBufferOut = (char*) malloc( ENCODE_BUFFER_SIZE_OUT );

    for(;;)
    {
        if( pbBufferIn == NULL || pbBufferOut == NULL )
        {
            r = B64_ERR_MEMORY;
            break;
        }

        if( g_nHeaderLine > 0 )
        {
            size_t n = fwrite( (void*)"-----BEGIN ", 1, 11, g_fOut );
            if( ferror( g_fOut ) || n < 11 )
            {
                r = B64_ERR_WRITING;
                break;
            }
            n = fwrite( (void*)g_pszHeaderLine, 1, g_nHeaderLine, g_fOut );
            if( ferror( g_fOut ) || n < g_nHeaderLine )
            {
                r = B64_ERR_WRITING;
                break;
            }
            n = fwrite( (void*)"-----", 1, 5, g_fOut );
            if( ferror( g_fOut ) || n < 5 )
            {
                r = B64_ERR_WRITING;
                break;
            }
            if( g_fUseCRLF )
            {
                n = fwrite( (void*)"\r", 1, 1, g_fOut );
                if( ferror( g_fOut ) || n < 1 )
                {
                    r = B64_ERR_WRITING;
                    break;
                }
            }
            n = fwrite( (void*)"\n", 1, 1, g_fOut );
            if( ferror( g_fOut ) || n < 1 )
            {
                r = B64_ERR_WRITING;
                break;
            }
        }

        for(;;)
        {
            unsigned long nDiv = 0;
            unsigned long nRem = 0;
            unsigned long nChars = 0;
            unsigned char* pIn = (unsigned char*) pbBufferIn;
            unsigned int nOut = 0;
            size_t nWritten = 0;

            size_t nRead = fread( (void*)pIn, 1, ENCODE_BUFFER_SIZE_IN, g_fIn );
            if( ferror( g_fIn ) )
            {
                r = B64_ERR_READING;
                break;
            }
            if( nRead == 0 )
            {
                break;
            }

            nDiv = ((unsigned long)nRead) / 3;
            nRem = ((unsigned long)nRead) % 3;
            nChars = 0;

            while( nDiv > 0 )
            {
                pbBufferOut[nOut+0] = to_b64[ (pIn[0] >> 2) & 0x3f];
                pbBufferOut[nOut+1] = to_b64[((pIn[0] << 4) & 0x30) + ((pIn[1] >> 4) & 0xf)];
                pbBufferOut[nOut+2] = to_b64[((pIn[1] << 2) & 0x3c) + ((pIn[2] >> 6) & 0x3)];
                pbBufferOut[nOut+3] = to_b64[  pIn[2] & 0x3f];
                pIn += 3;
                nOut += 4;
                nDiv--;
                nChars += 4;
                if( nChars >= g_nCharsPerLine && g_nCharsPerLine != 0 )
                {
                    nChars = 0;
                    if( g_fUseCRLF )
                    {
                        pbBufferOut[nOut++] = '\r';
                    }
                    pbBufferOut[nOut++] = '\n';
                }
            }

            switch( nRem )
            {
                case 2:
                    pbBufferOut[nOut+0] = to_b64[ (pIn[0] >> 2) & 0x3f];
                    pbBufferOut[nOut+1] = to_b64[((pIn[0] << 4) & 0x30) + ((pIn[1] >> 4) & 0xf)];
                    pbBufferOut[nOut+2] = to_b64[ (pIn[1] << 2) & 0x3c];
                    pbBufferOut[nOut+3] = '=';
                    nOut += 4;
                    nChars += 4;
                    if( nChars >= g_nCharsPerLine && g_nCharsPerLine != 0 )
                    {
                        nChars = 0;
                        if( g_fUseCRLF )
                        {
                            pbBufferOut[nOut++] = '\r';
                        }
                        pbBufferOut[nOut++] = '\n';
                    }
                    break;
                case 1:
                    pbBufferOut[nOut+0] = to_b64[ (pIn[0] >> 2) & 0x3f];
                    pbBufferOut[nOut+1] = to_b64[ (pIn[0] << 4) & 0x30];
                    pbBufferOut[nOut+2] = '=';
                    pbBufferOut[nOut+3] = '=';
                    nOut += 4;
                    nChars += 4;
                    if( nChars >= g_nCharsPerLine && g_nCharsPerLine != 0 )
                    {
                        nChars = 0;
                        if( g_fUseCRLF )
                        {
                            pbBufferOut[nOut++] = '\r';
                        }
                        pbBufferOut[nOut++] = '\n';
                    }
                    break;
            }

            if( nRem > 0 || feof( g_fIn ) )
            {
                if( nChars > 0 )
                {
                    nChars = 0;
                    if( g_fUseCRLF )
                    {
                        pbBufferOut[nOut++] = '\r';
                    }
                    pbBufferOut[nOut++] = '\n';
                }
            }

            nWritten = (size_t) (nOut);
            if( nWritten > 0 )
            {
                size_t n = fwrite( (void*)pbBufferOut, 1, nWritten, g_fOut );
                if( ferror( g_fOut ) || n < nWritten )
                {
                    r = B64_ERR_WRITING;
                    break;
                }
            }

            if( nRem > 0 || feof( g_fIn ) )
            {
                break;
            }
        }
        break;
    }

    if( r == B64_OK )
    {
        while( g_nHeaderLine > 0 )
        {
            size_t n = fwrite( (void*)"-----END ", 1, 9, g_fOut );
            if( ferror( g_fOut ) || n < 9 )
            {
                r = B64_ERR_WRITING;
                break;
            }
            n = fwrite( (void*)g_pszHeaderLine, 1, g_nHeaderLine, g_fOut );
            if( ferror( g_fOut ) || n < g_nHeaderLine )
            {
                r = B64_ERR_WRITING;
                break;
            }
            n = fwrite( (void*)"-----", 1, 5, g_fOut );
            if( ferror( g_fOut ) || n < 5 )
            {
                r = B64_ERR_WRITING;
                break;
            }
            if( g_fUseCRLF )
            {
                n = fwrite( (void*)"\r", 1, 1, g_fOut );
                if( ferror( g_fOut ) || n < 1 )
                {
                    r = B64_ERR_WRITING;
                    break;
                }
            }
            n = fwrite( (void*)"\n", 1, 1, g_fOut );
            if( ferror( g_fOut ) || n < 1 )
            {
                r = B64_ERR_WRITING;
                break;
            }
            break;
        }
    }

    if( pbBufferIn != NULL )
    {
        free( pbBufferIn );
    }
    if( pbBufferOut != NULL )
    {
        free( pbBufferOut );
    }
    return r;
}

static int decodeB64()
{
    int r = B64_OK;
    int z = 0;  // 0 Normal, 1 skip MIME separator (---) to end of line
    char c = '\0';
    unsigned char data[3];
    unsigned int nData = 0;

    for(;;)
    {
        unsigned char bits = 'z';
        size_t nRead = fread( (void*)&c, 1, 1, g_fIn );
        if( ferror( g_fIn ) )
        {
            r = B64_ERR_READING;
            break;
        }
        if( nRead == 0 )
        {
            break;
        }

        if( z > 0 )
        {
            if( c == '\n' )
            {
                z = 0;
            }
        }
        else if( c >= 'A' && c <= 'Z' )
        {
            bits = (unsigned char) (c - 'A');
        }
        else if( c >= 'a' && c <= 'z' )
        {
            bits = (unsigned char) (c - 'a' + (char)26);
        }
        else if( c >= '0' && c <= '9' )
        {
            bits = (unsigned char) (c - '0' + (char)52);
        }
        else if( c == '+' )
        {
            bits = (unsigned char) 62;
        }
        else if( c == '/' )
        {
            bits = (unsigned char) 63;
        }
        else if( c == '-' )
        {
            z = 1;
        }
        else if( c == '=' )
        {
            break;
        }
        else
        {
            bits = (unsigned char) 'y';
        }

        if( bits < (unsigned char) 64 )
        {
            switch(nData++)
            {
                case 0:
                    data[0] = (bits << 2) & 0xfc;
                    break;
                case 1:
                    data[0] |= (bits >> 4) & 0x03;
                    data[1] = (bits << 4) & 0xf0;
                    break;
                case 2:
                    data[1] |= (bits >> 2) & 0x0f;
                    data[2] = (bits << 6) & 0xc0;
                    break;
                case 3:
                    data[2] |= bits & 0x3f;
                    break;
            }

            if( nData == 4 )
            {
                size_t n = fwrite( (void*)data, 1, 3, g_fOut );
                if( ferror( g_fOut ) || n < 3 )
                {
                    r = B64_ERR_WRITING;
                    break;
                }

                nData = 0;
            }
        }

        if( feof( g_fIn ) )
        {
            break;
        }
    }
    if( r == B64_OK && nData > 0 )
    {
        if( nData == 1 )
        {
            r = B64_ERR_SYNTAX;
        }
        else
        {
            size_t n = fwrite( (void*)data, 1, nData-1, g_fOut );
            if( ferror( g_fOut ) || n < (nData-1) )
            {
                r = B64_ERR_WRITING;
            }
        }
    }
    return r;
}

static void setOptionsFromProgname( char *fn )
{
#ifdef _WIN32
    char c = '\\';
    char *pn = "b642bin.exe";
#else
    char c = '/';
    char *pn = "b642bin";
#endif

    char *p = strrchr( fn, c );
    if( p == NULL )
    {
        p = fn;
    }
    else
    {
        p++;
    }
    if( STRCMP( p, pn ) == 0 )
    {
        g_fDecode = TRUE;
    }
}


int main( int argc, char ** argv )
{
    int r = B64_OK;
    int nArgv = 1;
    for(;;)
    {
#ifdef USE_CRLF
        g_fUseCRLF = TRUE;
#endif
        setOptionsFromProgname( argv[0] );
        if( (r=parseCmdLine( argc, argv )) != 0 )
        {
            break;
        }
        if( (r=openFiles()) != 0 )
        {
            break;
        }

        if( g_fDecode != TRUE )
        {
            if( (r=encodeB64()) != 0 )
            {
                break;
            }
        }
        else
        {
            if( (r=decodeB64()) != 0 )
            {
                break;
            }
        }

        break;
    }
    closeFiles(r);
    return r;
}

/**/
