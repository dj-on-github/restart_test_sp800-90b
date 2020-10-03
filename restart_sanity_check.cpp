
/*
    restart_sanity_checker - An implementation of the SP800-90B restart sanity check algorithm.
    
    Contact dj@deadhat.com
    Copyright (C) 2020  David Johnston
    Also uses the mpreal library. See mpreal.h for license.
    
    Contributors:
    David Johnston. 

    Licensing:
    restart_sanity_checker is under GNU General Public License ("GPL").


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
#include "mpreal.h"
#include <iostream>
#include <iomanip>

using mpfr::mpreal;
void display_usage() {
fprintf(stderr,"Usage: restart_sanity_checker -e <H_I> <filename>\n");
fprintf(stderr,"       -e , --H_I              Output Initial Entropy Estimate\n");
fprintf(stderr,"       -v , --verbose          Output information to stderr\n");
fprintf(stderr,"       -h , --help             Output this information\n");
fprintf(stderr,"\n");
fprintf(stderr,"Perform Restart Sanity test on matrix file in NIST Oddball restart format in SP800-90B.\n");
fprintf(stderr,"  Author: David Johnston, dj@deadhat.com\n");
fprintf(stderr,"\n");
}

// n Choose k algorithm
mpreal choose(mpreal n,mpreal k){

    uint64_t i;
    //uint64_t prod = 1;
    mpreal prod = 1.0;
    for (i=1;i<=k;i++) {
        prod = prod * ((n-(k-i))/i);
    }
    return prod;
}

/********
* main() is mostly about parsing and qualifying the command line options.
*/

int main(int argc, char** argv)
{
    using mpfr::mpreal;
    using std::cout;
    using std::cerr;
    using std::endl;
    using std::setw;

    unsigned char buffer[1000008];

    size_t len;
    unsigned char abit;

    int i;
    int j;

    int opt;
    unsigned char matrix[1000][1000];

    FILE *ifp;
    
    char filename[8192];
    
    int bps = 1;   
    int abyte;
    
    int littleendian=1;
    int gotL=0;
    int gotB=0;
    int verbose = 0;
    int reverse = 0;

    int using_infile = 0;

    /* Zero out the strings */    
    filename[0] = (char)0;

    /* get the options and arguments */
    int longIndex;

    int skip_bytes = 0;
    int filenamecount =0;

    int amount;

    double hi = 0.8;

    //printf("choose(1000,849) = %f\n",choose(1000,849));
    //exit(1);

    char optString[] = "e:vh";
    static const struct option longOpts[] = {
    { "H_I", required_argument, NULL, 'e' },
    { "verbose", no_argument, NULL, 'v' },
    { "help", no_argument, NULL, 'h' },
    { NULL, no_argument, NULL, 0 }
    };

    opt = getopt_long( argc, argv, optString, longOpts, &longIndex );
    while( opt != -1 ) {
        switch( opt ) {
            case 'e':
                hi = atof(optarg);
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
    
    if (optind < argc) {
        strcpy(filename,argv[optind]);
        using_infile = 1;
    }
        
    if (verbose==1) {
        cerr << "Verbose mode enabled" << endl;
        if (reverse==1) fprintf(stderr, "Input data interpreted as big-endian (msb first)\n");
        if (littleendian==0) fprintf(stderr, "Output multi-bit symbols encoded as big endian (MSB first)\n");
        if (littleendian==1) fprintf(stderr, "Output multi-bit symbols encoded as little endian (LSB first) (default)\n");
        if (((gotB==1) || (gotL==1)) && (bps > 1)){
            printf("Warning: -L and -B arguments have so effect with 1 bit output symbols\n"); 
        }
        if (using_infile==1) {
            fprintf(stderr,"Reading binary data from file: %s\n", filename);
        }
    }
    

    /* find the input files */
    if (using_infile==0)
    {
        fprintf(stderr,"Error, must provide an input file name\n");
        exit(-1);
    }


    ifp =  fopen(filename, "rb");
            
    if (ifp == NULL) {
        cerr << "ERROR: Filed to open input file " << filename << " for reading" << endl;
        exit(-1);
    }


    // Read file
    amount = 1000000;
    len = fread(buffer, 1, (size_t)amount , ifp);
    if (verbose) cerr <<"read " << len << "/" << amount <<  " symbols from " << filename << endl;
        
    if (len != amount) {
        cerr << "ERROR: Only " << len << " bytes read" << endl;
        exit(-1);
    }

    fclose(ifp);

    // Find bits per symbol
    unsigned char bigor = 0;
    for (i=0;i<1000000;i++) {
        bigor = bigor | buffer[i];
    }

    if      (bigor < 2)   bps = 1;
    else if (bigor < 4)   bps = 2;
    else if (bigor < 8)   bps = 3;
    else if (bigor < 16)  bps = 4;
    else if (bigor < 32)  bps = 5;
    else if (bigor < 64)  bps = 6;
    else if (bigor < 128) bps = 7;
    else                  bps = 8;

    // Read data into matrix

    int row;
    int column;
    int index = 0;
    for (row=0; row<1000; row++) {
        for (column=0; column<1000; column++) {
            matrix[row][column] = buffer[index++];
        }
    }

    // Restart Test
    //
    //
    const int digits = 2000;
    mpreal::set_default_prec(mpfr::digits2bits(digits));

    mpreal bigp;
    mpreal bigp_increment;
    mpreal small_p;
    mpreal alpha = 0.000005;
    mpreal first;
    mpreal second;
    mpreal third;

    int row_max = 0;
    int row_max_max = 0;
    int row_total=0;
    int column_max = 0;
    int column_max_max = 0;
    int column_total=0;
    int xmax = 0;

    int frequency[256];
    
    if (verbose) cerr << "Counting row and columns symbols maximums." << endl;

    int max_f;
    
    for (row=0;row<1000;row++) {
        row_total = 0;
        row_max = 0;
        
        for (i=0;i<256;i++) frequency[i] = 0;
        
        for (column = 0;column < 1000; column++) {
            abyte = matrix[row][column];
            frequency[(int)abyte]++;
            if  (frequency[(int)abyte] > row_max) row_max = frequency[(int)abyte];
        }   
        if (row_max > row_max_max) row_max_max = row_max;
        
    }

    for (column=0;column<1000;column++) {
        column_total = 0;
        column_max = 0;
        
        for (i=0;i<256;i++) frequency[i] = 0;
        
        for (row = 0;row < 1000; row++) {
            abyte = matrix[row][column];
            frequency[(int)abyte]++;
            if  (frequency[(int)abyte] > column_max) column_max = frequency[(int)abyte];
        }   
        if (column_max > column_max_max) column_max_max = column_max;
    }
    
    if (column_max_max > row_max_max) xmax = column_max_max;
    else xmax = row_max_max;

    if (verbose) cout << "Computing P(X <= Xmax)." << endl;

    small_p = pow((mpreal)2.0,(mpreal)-hi);
    alpha = (mpreal)0.000005;

    bigp = 0.0;

    for (j=xmax;j<=1000;j++) {

        first  = (mpreal)(choose((mpreal)1000,(mpreal)j));
        second = pow(small_p,(mpreal)(j));
        third  = pow(((mpreal)1.0)-small_p,(mpreal)(1000-j)); 

        bigp_increment = first*second*third;
        bigp+=bigp_increment;

        if (verbose) {
            cerr <<  "j="        << setw(5)   << j;
            cerr <<  "  bigp="  << setw(12)   << bigp;
            cerr <<  "  bigp_increment=" << setw(12)  << bigp_increment;
            cerr << "  choose(1000," << setw(4) << j << ")=" << setw(12)   << first;
            cerr << "  pow("<<small_p<<","  << setw(4) << j << ") = "<< setw(12)  << second;
            cerr << "\tpow(1-p,(1000-j))="  << setw(12)       << third;
            cerr << endl;
        }
    }

    //fprintf(stderr,"\n choose(6,4) = %f\n",choose(6,4));
    cerr << endl;
    cerr << "    ---- Results -----" << endl;
    cerr << setw(18) << "Bits per symbol = "<< setw(8) << bps << endl; 
    cerr << setw(18) << "H_I = "            << setw(8) << hi << endl; 
    cerr << setw(18) << "alpha = "          << setw(8) << "0.000005" << endl; 
    cerr << setw(18) << "p = "              << setw(8) <<  small_p << endl;
    cerr << setw(18) << "row_max_max = "    << setw(8) << row_max_max << endl;
    cerr << setw(18) << "column_max_max = " << setw(8) << column_max_max << endl;
    cerr << setw(18) << "Xmax = "           << setw(8) << xmax << endl;
    cerr << setw(18) << "P(x => xmax) = "   << setw(8) << bigp << endl;

    if (bigp < alpha) cerr << setw(18) << "Result = " << setw(8) << "FAIL" << endl;
    else cerr << setw(18) << "Result = " << setw(8) << "PASS" << endl;
}


