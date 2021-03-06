#ifndef PRIV_TYPES_H
#define PRIV_TYPES_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <math.h>

#define samp_per_frame2  576

#include "bitstream.h"

/* TODO: Assembler code did not seem to like
 * moving all static variables to allocated struct..
 * Needs fixing, so disabled for now. However, it does
 * not seem to make much of a difference anyway..
 
#if defined(__arm__)
#include "mult_sarm_gcc.h"
#else
*/

#include "mult_noarch_gcc.h"

/* #endif */

/* #define DEBUG if you want the library to dump info to stdout */

#define false 0
#define true 1

#define PI          3.14159265358979
#define PI4         0.78539816339745
#define PI12        0.26179938779915
#define PI36        0.087266462599717
#define PI64        0.049087385212
#define SQRT2       1.41421356237
#define LN2         0.69314718
#define LN_TO_LOG10 0.2302585093
#define BLKSIZE     1024
#define HAN_SIZE    512
#define SCALE_BLOCK 12
#define SCALE_RANGE 64
#define SCALE       32768
#define SBLIMIT     32

#ifndef bool
typedef unsigned char bool;
#endif

#ifndef MAX_CHANNELS
#define MAX_CHANNELS 2
#endif

#ifndef MAX_GRANULES
#define MAX_GRANULES 2
#endif

/*
  A BitstreamElement contains encoded data
  to be written to the bitstream.
  'length' bits of 'value' will be written to
  the bitstream msb-first.
*/
typedef struct
{
    unsigned long int value;
    unsigned int length;
} BF_BitstreamElement;

/*
  A BitstreamPart contains a group
  of 'nrEntries' of BitstreamElements.
  Each BitstreamElement will be written
  to the bitstream in the order it appears
  in the 'element' array.
*/
typedef struct
{
    unsigned long int nrEntries;
    BF_BitstreamElement *element;
} BF_BitstreamPart;

/*
  This structure contains all the information needed by the
  bitstream formatter to encode one frame of data. You must
  fill this out and provide a pointer to it when you call
  the formatter.
  Maintainers: If you add or remove part of the side
  information, you will have to update the routines that
  make local copies of that information (in formatBitstream.c)
*/

typedef struct BF_FrameData
{
    int              frameLength;
    int              nGranules;
    int              nChannels;
    BF_BitstreamPart *header;
    BF_BitstreamPart *frameSI;
    BF_BitstreamPart *channelSI[MAX_CHANNELS];
    BF_BitstreamPart *spectrumSI[MAX_GRANULES][MAX_CHANNELS];
    BF_BitstreamPart *scaleFactors[MAX_GRANULES][MAX_CHANNELS];
    BF_BitstreamPart *codedData[MAX_GRANULES][MAX_CHANNELS];
    BF_BitstreamPart *userSpectrum[MAX_GRANULES][MAX_CHANNELS];
    BF_BitstreamPart *userFrameData;
} BF_FrameData;

/*
  This structure contains information provided by
  the bitstream formatter. You can use this to
  check to see if your code agrees with the results
  of the call to the formatter.
*/
typedef struct BF_FrameResults
{
    int SILength;
    int mainDataLength;
    int nextBackPtr;
} BF_FrameResults;

typedef struct BF_PartHolder
{
    int max_elements;
    BF_BitstreamPart *part;
} BF_PartHolder;

typedef struct {
    int  channels;
    long samplerate;
} priv_shine_wave_t;

typedef struct {
    int    mode;      /* + */ /* Stereo mode */
    int    bitr;      /* + */ /* Must conform to known bitrate - see Main.c */
    int    emph;      /* + */ /* De-emphasis */
    int    padding;
    long   bits_per_frame;
    long   bits_per_slot;
    double frac_slots_per_frame;
    double slot_lag;
    int    whole_slots_per_frame;
    int    bitrate_index;     /* + */ /* See Main.c and Layer3.c */
    int    samplerate_index;  /* + */ /* See Main.c and Layer3.c */
    int    crc;
    int    ext;
    int    mode_ext;
    int    copyright;  /* + */
    int    original;   /* + */
} priv_shine_mpeg_t;

typedef struct
{
  int frameLength;
  int SILength;
  int nGranules;
  int nChannels;
  BF_PartHolder *headerPH;
  BF_PartHolder *frameSIPH;
  BF_PartHolder *channelSIPH[MAX_CHANNELS];
  BF_PartHolder *spectrumSIPH[MAX_GRANULES][MAX_CHANNELS];
} MYSideInfo;

typedef struct side_info_link
{
    struct side_info_link *next;
    MYSideInfo           side_info;
} side_info_link;

typedef struct {
    int BitCount;
    int ThisFrameSize;
    int BitsRemaining;
    side_info_link *side_queue_head;
    side_info_link *side_queue_free;
} formatbits_t;

typedef struct {
  BF_FrameData    frameData;
  BF_FrameResults frameResults;

  BF_PartHolder *headerPH;
  BF_PartHolder *frameSIPH;
  BF_PartHolder *channelSIPH[ MAX_CHANNELS ];
  BF_PartHolder *spectrumSIPH[ MAX_GRANULES ][ MAX_CHANNELS ];
  BF_PartHolder *scaleFactorsPH[ MAX_GRANULES ][ MAX_CHANNELS ];
  BF_PartHolder *codedDataPH[ MAX_GRANULES ][ MAX_CHANNELS ];
  BF_PartHolder *userSpectrumPH[ MAX_GRANULES ][ MAX_CHANNELS ];
  BF_PartHolder *userFrameDataPH;

  side_info_link *side_queue_head;
  side_info_link *side_queue_free;
} l3stream_t;

typedef struct {
  long *xr;                    /* magnitudes of the spectral values */
  long xrsq[samp_per_frame2];  /* xr squared */
  long xrabs[samp_per_frame2]; /* xr absolute */
  long xrmax;                  /* maximum of xrabs array */
  long en_tot[2]; /* gr */
  long en[2][21];
  long xm[2][21];
  long xrmaxl[2];
  double steptab[128]; /* 2**(-x/4)  for x = -127..0 */
  long steptabi[128];  /* 2**(-x/4)  for x = -127..0 */
  long int2idx[10000]; /* x**(3/4)   for x = 0..9999 */
} l3loop_t;

typedef struct {
  long ca[8];
  long cs[8];
  long cos_l[18][36];
} mdct_t;

typedef struct {
  int off[2];
  long fl[SBLIMIT][64];
  long x[2][HAN_SIZE];
  long z[2][HAN_SIZE];
  long ew[HAN_SIZE];
} subband_t; 

/* Side information */
typedef struct {
        unsigned part2_3_length;
        unsigned big_values;
        unsigned count1;
        unsigned global_gain;
        unsigned scalefac_compress;
        unsigned table_select[3];
        unsigned region0_count;
        unsigned region1_count;
        unsigned preflag;
        unsigned scalefac_scale;
        unsigned count1table_select;

        unsigned part2_length;
        unsigned sfb_lmax;
        unsigned address1;
        unsigned address2;
        unsigned address3;
        int quantizerStepSize;
        unsigned slen[4];
} gr_info;

typedef struct {
    int main_data_begin; /* unsigned -> int */
    unsigned private_bits;
    int resvDrain;
    unsigned scfsi[2][4];
    struct {
        struct {
            gr_info tt;
        } ch[2];
    } gr[2];
} shine_side_info_t;

typedef struct {
    double  l[2][2][21];
} shine_psy_ratio_t;

typedef struct {
        double  l[2][2][21];
} shine_psy_xmin_t;

typedef struct {
    int l[2][2][22];            /* [cb] */
    int s[2][2][13][3];         /* [window][cb] */
} shine_scalefac_t;


typedef struct shine_global_flags { 
  priv_shine_wave_t    wave;
  priv_shine_mpeg_t    mpeg;
  bitstream_t    bs;
  shine_side_info_t side_info;
  int            sideinfo_len;
  int            mean_bits;
  shine_psy_ratio_t ratio;
  shine_scalefac_t  scalefactor;
  int16_t       *buffer[2];
  double         pe[2][2];
  int            l3_enc[2][2][samp_per_frame2];
  long           l3_sb_sample[2][3][18][SBLIMIT];
  long           mdct_freq[2][2][samp_per_frame2];
  int            ResvSize;
  int            ResvMax;
  formatbits_t   formatbits;
  l3stream_t     l3stream;
  l3loop_t       l3loop;
  mdct_t         mdct;
  subband_t      subband;
} shine_global_config;

#endif
