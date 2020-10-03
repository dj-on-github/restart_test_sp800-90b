
/*
    restart_slicer - An program to format 1000 binary restart files for
                     the SP800-90B restart test.
    
    Contact dj@deadhat.com
    Copyright (C) 2020  David Johnston
    
    Contributors:
    David Johnston. 

    Licensing:
    restart_slicer is under GNU General Public License ("GPL").


    GNU General Public License ("GPL") copyright permissions statement:
    **************************************************************************
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/* make isnan() visible */
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <getopt.h>
#include <glob.h>
#include <wordexp.h>
#include <math.h>
#include <iostream>
#include <iomanip>

void display_usage() {
fprintf(stderr,"Usage: restart_slicer [-l <bits_per_symbol 1-8>][-B|-L][-v][-h][-o <out filename>] [filename_glob_pattern]\n");
fprintf(stderr,"       -l , --length <bits_per_symbol 1-8> Set the number of bits to encode in eat output byte\n");
fprintf(stderr,"       -s , --skip <bits_per_symbol 1-8> Number of bytes to skip in each binary file\n");
fprintf(stderr,"       -r , --reverse                      Interpret input binary data as big endian (MSB first) (default is little endian)\n");
fprintf(stderr,"       -B , --bigendian                    Unpack output multi-bit symbols as big-endian (msb first)\n");
fprintf(stderr,"       -L , --littleendian                 Unpack output multi-bit symbols as little-endian (lsb first) (default)\n");
fprintf(stderr,"       -v , --verbose                      Output information to stderr\n");
fprintf(stderr,"       -h , --help                         Output this information\n");
fprintf(stderr,"\n");
fprintf(stderr,"Convert 1000 binary data files to NIST Oddball restart format in SP800-90B one-symbol-per-byte format.\n");
fprintf(stderr,"  Author: David Johnston, dj@deadhat.com\n");
fprintf(stderr,"\n");
}

void printsample(unsigned char *thesample)
{
    int tempindex;
    int j;
    int i;
   tempindex = 0;
    for (j=0;j<16;j++)
    {
            for (i=0;i<16;i++) printf("%02X",thesample[tempindex++]);
            printf("\n");
    }
}

/********
* main() is mostly about parsing and qualifying the command line options.
*/

int main(int argc, char** argv)
{
    using std::cout;
    using std::cerr;
    using std::endl;
    using std::setw;

    unsigned char buffer[2048];

    #define BITBUFFER_SIZE 8192
    unsigned char bitbuffer[BITBUFFER_SIZE];
    int bitbuffer_index = 0;

    unsigned char outbuffer[1000001];
    int outindex = 0;
    int bytecount = 0;
    int done=0;
    size_t len;
    unsigned char abit;

    int symbol_count;
    int j;
    int runcount = 0;
    int last_abyte = -1;
    int max_runcount = 0;
    int location = 0;

    int opt;
    int i;
    int filenumber;

    FILE *ifp;
    FILE *ofp;
    int using_outfile = 0;  /* use stdout instead of outputfile*/
    int using_infile;
    char filename[8192];
    char infilemask[8192];
    char infilename[8192];
    
    int bps = 1;   
    int abyte;
    
    int littleendian=1;
    int gotL=0;
    int gotB=0;
    int verbose = 0;
    int reverse = 0;

    /* Zero out the strings */    
    filename[0] = (char)0;
    infilemask[0] = (char)0;
    infilename[0] = (char)0;

    /* get the options and arguments */
    int longIndex;

    int skip_bytes = 0;
    int filenamecount =0;

    int amount;

    //printf("choose(1000,849) = %f\n",choose(1000,849));
    //exit(1);

    char optString[] = "o:k:l:w:s:BLrvh";
    static const struct option longOpts[] = {
    { "output", no_argument, NULL, 'o' },
    { "reverse", no_argument, NULL, 'r' },
    { "bigendian", no_argument, NULL, 'B' },
    { "littleendian", no_argument, NULL, 'L' },
    { "bits_per_symbol", required_argument, NULL, 'l' },
    { "skip", required_argument, NULL, 's' },
    { "verbose", no_argument, NULL, 'v' },
    { "help", no_argument, NULL, 'h' },
    { NULL, no_argument, NULL, 0 }
    };

    opt = getopt_long( argc, argv, optString, longOpts, &longIndex );
    while( opt != -1 ) {
        switch( opt ) {
            case 'o':
                using_outfile = 1;
                strcpy(filename,optarg);
                break;
            case 'l':
                bps = atoi(optarg);
                if ((bps < 1) || (bps > 8)) {
                    perror("Error, bits per symbol bust be between 1 and 8");
                    display_usage();
                    exit(-1);
                };
                break;
            case 's':
                skip_bytes = atoi(optarg);
                if (skip_bytes < 0) {
                    perror("Error, skip_bytes must be positive");
                    display_usage();
                    exit(-1);
                }
                break;
            case 'r':
                reverse=1;
                break;
            case 'L':
                littleendian=1;
                gotL=1;
                break;
            case 'B':
                littleendian=0;
                gotB=1;
                break;
            case 'v':
                verbose=1;
                break;
                                
            case 'h':   /* fall-through is intentional */
            case '?':
                display_usage();
                exit(0);
                 
            default:
                /* You won't actually get here. */
                break;
        }
         
        opt = getopt_long( argc, argv, optString, longOpts, &longIndex );
    } // end while
    
    if (gotB==1 && gotL==1) {
        fprintf(stderr,"ERROR, Can't be both big endian (-B) and little endian (-L) at the same time\n");
        exit(-1);
    }

    if (optind < argc) {
        strcpy(infilemask,argv[optind]);
        using_infile = 1;
    }
        
    if (verbose==1) {
        fprintf(stderr,"Verbose mode enabled\n");
        if (reverse==1) fprintf(stderr, "Input data interpreted as big-endian (msb first)\n");
        if (littleendian==0) fprintf(stderr, "Output multi-bit symbols encoded as big endian (MSB first)\n");
        if (littleendian==1) fprintf(stderr, "Output multi-bit symbols encoded as little endian (LSB first) (default)\n");
        if (((gotB==1) || (gotL==1)) && (bps > 1)){
            printf("Warning: -L and -B arguments have so effect with 1 bit output symbols\n"); 
        }
        if (using_infile==1) {
            fprintf(stderr,"Reading binary data from file: %s\n", infilename);
        }
        if (using_outfile==1) {
            fprintf(stderr,"Writing NIST 1 symbol per byte data to file: %s\n", filename);
        }
        fprintf(stderr,"Bits per symbol = %d\n",bps); 
    }

    /* open the output file if needed */

    if (using_outfile==1)
    {
        ofp = fopen(filename, "w");
        if (ofp == NULL) {
            perror("failed to open output file for writing");
            exit(-1);
        }
    }

    /* find the input files */
        // Since it's multiple files, you can't use std in.
    if (using_infile==0)
    {
        fprintf(stderr,"Error, must provide an input file mask using shell rules, to match the 1000 binary files\n");
        exit(-1);
    }

    wordexp_t p;
    char **w;
    int experr;

    experr = wordexp(infilemask, &p, WRDE_NOCMD | WRDE_SHOWERR | WRDE_UNDEF);

    if (experr) {
        fprintf(stderr,"Error, input filename error returned from wordexp()\n");
        exit(-1);
    }

    w = p.we_wordv;

    filenamecount = p.we_wordc;

    if (filenamecount != 1000) {
        fprintf(stderr,"ERROR filename did not expand to 1000 files - it expanded to %d files\n",filenamecount);
        exit(-1);
    }

    for (filenumber=0;filenumber<1000;filenumber++) {
        if (verbose) fprintf(stderr,"File# %d, Filename %s\t",filenumber,w[filenumber]);

        /* open the input file if needed */
        strcpy(infilename,w[filenumber]);

        ifp =  fopen(infilename, "rb");
            
        if (ifp == NULL) {
            fprintf(stderr,"failed to open input file for reading");
            exit(-1);
        }

        // 1000 samples + skip_bytes is all the data we need
        amount = (1+((1000*bps)/8))+skip_bytes; 
        len = fread(buffer, 1, (size_t)amount , ifp);
        amount = (1+((1000*bps)/8))+skip_bytes;
        if (verbose) cerr <<"read " << len << "/" << amount << endl;
        
        if (len != amount) {
            cerr << "Error only " << len << " bytes read" << endl;
            exit(-1);
        }

        // Read in buffer bytes into the FIFO of bits. One bit per byte.        
        bitbuffer_index = 0;
        
        if (verbose) fprintf(stderr," skip_bytes=%d ",skip_bytes);
        for (i=skip_bytes;i<len;i++) {
            abyte = buffer[i];
            if (verbose) fprintf(stderr,"%02x",abyte);
            for(j=0;j<8;j++) {
                if (reverse==0) {
                    abit = (abyte & 0x01);
                    abyte = abyte >> 1;
                } else {
                    abit = (abyte & 0x80) >> 7;
                    abyte = abyte << 1;
                }

                bitbuffer[bitbuffer_index] = (unsigned char)abit;
                bitbuffer_index++;
            }
        }
        if (verbose) fprintf(stderr,"\n");

        // Work out how many full symbols are in the FIFO.
        symbol_count = bitbuffer_index / bps;
        if (verbose) fprintf(stderr,"Found %d symbols in buffer\n",symbol_count);
        if (symbol_count < 1000) {
            fprintf(stderr,"Not enough symbols in file %s, need 1000, got %d\n",infilename,symbol_count);
            exit(-1);
        }

        //Pull bits from the but buffer and Write out the symbols (of 1 bit) as bytes;
        for (i=0;i<1000;i++) {
            abyte = 0;
            for(j=0;j<bps;j++) {
                abyte = abyte << 1;
                abyte = abyte | bitbuffer[(i*bps)+j];
            }
            outbuffer[i] = abyte;
        }

        if (using_outfile)
            fwrite(outbuffer, 1000,1,ofp);
        else
            fwrite(outbuffer, 1000,1,stdout);
        
        //outindex = 0;
        fclose(ifp);

    }
    cout << "Wrote restart file " << filename << " to disk." << endl;    
    if (using_outfile==1) fclose(ofp);
    wordfree(&p);
}


