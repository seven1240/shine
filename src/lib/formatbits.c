/*********************************************************************
  Copyright (c) 1995 ISO/IEC JTC1 SC29 WG1
  formatBitstream.c
**********************************************************************/
/*
  Revision History:

  Date        Programmer                Comment
  ==========  ========================= ===============================
  1995/09/06  mc@fivebats.com           created
  1995/09/18  mc@fivebats.com           bugfix: WriteMainDataBits
  1995/09/20  mc@fivebats.com           bugfix: store_side_info
  2012/07/26  toots@rastageeks.org      clarified license.
*/

/* In email <CAKheQp9H5-8eUeXtCahAhS-0pKOW1kZVC9xY0D0oLVfE_kPPMA@mail.gmail.com>,
 * Mike Coleman <mc@fivebats.com> added the following regarding the code in this file:
 * 
 * "As I recall, the intention of the ISO at the time was to be permissive
 * with the code -- far more permissive than LGPL. In other words, do
 * what you want with the code."
 *
 * I have thus removed a "All Rights Reserved" sentence above. I am leaving this file
 * without a license header as it could also be re-licensed with a more liberal
 * license by other interested parties.
 *   -- Romain Beauxis <toots@rastageeks.org> Thu Jul 26 19:20:11 CDT 2012 */

/*#define DEBUG*/

#include "types.h"
#include "formatbits.h"

void shine_formatbits_initialise(shine_global_config *config)
{
  config->formatbits.BitCount        = 0;
  config->formatbits.ThisFrameSize   = 0;
  config->formatbits.BitsRemaining   = 0;
  config->formatbits.side_queue_head = NULL;
  config->formatbits.side_queue_free = NULL;
}

void shine_side_info_free(side_info_link *cur, shine_global_config *config)
{
  int ch, gr;

  side_info_link *next;
  BF_FrameData *info = &config->l3stream.frameData;

  while (cur) {
    shine_BF_freePartHolder(cur->side_info.headerPH);
    shine_BF_freePartHolder(cur->side_info.frameSIPH);
      for ( ch = 0; ch < info->nChannels; ch++ )
        shine_BF_freePartHolder(cur->side_info.channelSIPH[ch]);
      for ( gr = 0; gr < info->nGranules; gr++ )
        for ( ch = 0; ch < info->nChannels; ch++ )
          shine_BF_freePartHolder(cur->side_info.spectrumSIPH[gr][ch]);

    next = cur->next;
    free(cur);
    cur  = next;
  }
}

void shine_formatbits_close(shine_global_config *config)
{
  shine_side_info_free(config->formatbits.side_queue_head, config);
  shine_side_info_free(config->formatbits.side_queue_free, config);
}

/* forward declarations */
static int store_side_info( BF_FrameData *frameInfo, shine_global_config *config );
static int main_data( BF_FrameData *frameInfo, BF_FrameResults *results, shine_global_config *config);
static void WriteMainDataBits( unsigned long int val, unsigned int nbits, BF_FrameResults *results, shine_global_config *config);

/*
 * BF_BitStreamFrame:
 * ------------------
 * This is the public interface to the bitstream
 * formatting package. It writes one frame of main data per call.
 *
 * Assumptions:
 * - The back pointer is zero on the first call
 * - An integral number of bytes is written each frame
 *
 * You should be able to change the frame length, side info
 * length, #channels, #granules on a frame-by-frame basis.
 *
 * See formatBitstream.h for more information about the data
 * structures and the bitstream syntax.
 */
void shine_BF_BitstreamFrame(shine_global_config *config)
{
  BF_FrameData *frameInfo  = &config->l3stream.frameData;
  BF_FrameResults *results = &config->l3stream.frameResults;
  /* get ptr to bit writing function */
  /* save SI and compute its length */
  results->SILength = store_side_info( frameInfo, config );

  /* write the main data, inserting SI to maintain framing */
  results->mainDataLength = main_data( frameInfo, results, config );

  /*
   * Caller must ensure that back SI and main data are
   * an integral number of bytes, since the back pointer
   * can only point to a byte boundary and this code
   * does not add stuffing bits
   */
  /* assert( (BitsRemaining % 8) == 0 );*/
}

/*
 * shine_BF_PartLength:
 * --------------
 */
int shine_BF_PartLength( BF_BitstreamPart *part )
{
  BF_BitstreamElement *ep = part->element;
  int i, bits;

  bits = 0;
  for (i = 0; i < part->nrEntries; i++, ep++)
    bits += ep->length;
  return bits;
}

MYSideInfo *shine_get_side_info(shine_global_config *config);
int shine_write_side_info(shine_global_config *config);

/*
 * shine_writePartMainData:
 * ------------------
 */
int shine_writePartMainData(BF_BitstreamPart *part, BF_FrameResults *results, shine_global_config *config)
{
  BF_BitstreamElement *ep;
  int i, bits;

  /* assert(results); */
  /* assert(part); */

  bits = 0;
  ep = part->element;
  for ( i = 0; i < part->nrEntries; i++, ep++ )
    {
      WriteMainDataBits( ep->value, ep->length, results, config );
      bits += ep->length;
    }
  return bits;
}

int shine_writePartSideInfo(BF_BitstreamPart *part, BF_FrameResults *results, shine_global_config *config)
{
  BF_BitstreamElement *ep;
  int i, bits;

  /* assert( part ); */

  bits = 0;
  ep = part->element;
  for ( i = 0; i < part->nrEntries; i++, ep++ )
    {
      shine_putbits( &config->bs, ep->value, ep->length);
      bits += ep->length;
    }
  return bits;
}

int main_data(BF_FrameData *fi, BF_FrameResults *results, shine_global_config *config)
{
  int gr, ch, bits;
  bits = 0;
  results->mainDataLength = 0;

  for (gr = 0; gr < fi->nGranules; gr++)
    for (ch = 0; ch < fi->nChannels; ch++)
      {
        bits += shine_writePartMainData( fi->scaleFactors[gr][ch], results, config );
        bits += shine_writePartMainData( fi->codedData[gr][ch],    results, config );
        bits += shine_writePartMainData( fi->userSpectrum[gr][ch], results, config );
      }
  bits += shine_writePartMainData( fi->userFrameData, results, config );
  return bits;
}

/*
  This is a wrapper around PutBits() that makes sure that the
  framing header and side info are inserted at the proper
  locations
*/

void WriteMainDataBits(unsigned long int val,
                       unsigned int nbits,
                       BF_FrameResults *results,
                       shine_global_config *config)
{
  /* assert( nbits <= 32 ); */
  if (config->formatbits.BitCount == config->formatbits.ThisFrameSize)
    {
      config->formatbits.BitCount = shine_write_side_info(config);
      config->formatbits.BitsRemaining = config->formatbits.ThisFrameSize - config->formatbits.BitCount;
    }
  if (nbits == 0) return;
  if (nbits > config->formatbits.BitsRemaining)
    {
      unsigned extra = val >> (nbits - config->formatbits.BitsRemaining);
      nbits -= config->formatbits.BitsRemaining;
      shine_putbits( &config->bs, extra, config->formatbits.BitsRemaining);
      config->formatbits.BitCount = shine_write_side_info(config);
      config->formatbits.BitsRemaining = config->formatbits.ThisFrameSize - config->formatbits.BitCount;
      shine_putbits( &config->bs, val, nbits);
    }
  else
    shine_putbits( &config->bs, val, nbits);
  config->formatbits.BitCount += nbits;
  config->formatbits.BitsRemaining -= nbits;
}

int shine_write_side_info(shine_global_config *config)
{
  MYSideInfo *si;
  int bits, ch, gr;

  bits = 0;
  si = shine_get_side_info(config);
  config->formatbits.ThisFrameSize = si->frameLength;
  bits += shine_writePartSideInfo( si->headerPH->part,  NULL, config );
  bits += shine_writePartSideInfo( si->frameSIPH->part, NULL, config );

  for ( ch = 0; ch < si->nChannels; ch++ )
    bits += shine_writePartSideInfo( si->channelSIPH[ch]->part, NULL, config );

  for ( gr = 0; gr < si->nGranules; gr++ )
    for ( ch = 0; ch < si->nChannels; ch++ )
      bits += shine_writePartSideInfo( si->spectrumSIPH[gr][ch]->part, NULL, config );
  return bits;
}

int store_side_info(BF_FrameData *info, shine_global_config *config)
{
  int ch, gr;
  side_info_link *l = NULL;
  /* obtain a side_info_link to store info */
  side_info_link *f = config->formatbits.side_queue_free;
  int bits = 0;

  if (f == NULL)
    { /* must allocate another */
      l = (side_info_link *) calloc( 1, sizeof(side_info_link) );
#ifdef DEBUG
      static int n_si = 0;
      n_si += 1;
      printf("allocating side_info_link number %d, %p\n", n_si, l );
#endif
      l->next = NULL;
      l->side_info.headerPH  = shine_BF_newPartHolder( info->header->nrEntries );
      l->side_info.frameSIPH = shine_BF_newPartHolder( info->frameSI->nrEntries );
      for ( ch = 0; ch < info->nChannels; ch++ )
        l->side_info.channelSIPH[ch] = shine_BF_newPartHolder( info->channelSI[ch]->nrEntries );
      for ( gr = 0; gr < info->nGranules; gr++ )
        for ( ch = 0; ch < info->nChannels; ch++ )
          l->side_info.spectrumSIPH[gr][ch] = shine_BF_newPartHolder( info->spectrumSI[gr][ch]->nrEntries );
    }
  else
    { /* remove from the free list */
      config->formatbits.side_queue_free = f->next;
      f->next = NULL;
      l = f;
    }
  /* copy data */
  l->side_info.frameLength = info->frameLength;
  l->side_info.nGranules   = info->nGranules;
  l->side_info.nChannels   = info->nChannels;
  l->side_info.headerPH    = shine_BF_LoadHolderFromBitstreamPart( l->side_info.headerPH,  info->header );
  l->side_info.frameSIPH   = shine_BF_LoadHolderFromBitstreamPart( l->side_info.frameSIPH, info->frameSI );

  bits += shine_BF_PartLength( info->header );
  bits += shine_BF_PartLength( info->frameSI );

  for ( ch = 0; ch < info->nChannels; ch++ )
    {
      l->side_info.channelSIPH[ch] = shine_BF_LoadHolderFromBitstreamPart(l->side_info.channelSIPH[ch], info->channelSI[ch]);
      bits += shine_BF_PartLength(info->channelSI[ch]);
    }

  for ( gr = 0; gr < info->nGranules; gr++ )
    for ( ch = 0; ch < info->nChannels; ch++ )
      {
        l->side_info.spectrumSIPH[gr][ch] = shine_BF_LoadHolderFromBitstreamPart(l->side_info.spectrumSIPH[gr][ch], info->spectrumSI[gr][ch]);
        bits += shine_BF_PartLength( info->spectrumSI[gr][ch] );
      }
  l->side_info.SILength = bits;
  /* place at end of queue */
  f = config->formatbits.side_queue_head;
  if ( f == NULL )
    {  /* empty queue */
      config->formatbits.side_queue_head = l;
    }
  else
    { /* find last element */
      while ( f->next )
        f = f->next;
      f->next = l;
    }
  return bits;
}

MYSideInfo* shine_get_side_info(shine_global_config *config)
{
  side_info_link *f = config->formatbits.side_queue_free;
  side_info_link *l = config->formatbits.side_queue_head;

  /* If we stop here it means you didn't provide enough headers to support the
    amount of main data that was written. */
  /* assert(l); */

  /* update queue head */
  config->formatbits.side_queue_head = l->next;

  /* Append l to the free list. You can continue to use it until store_side_info
    is called again, which will not happen again for this frame. */
  config->formatbits.side_queue_free = l;
  l->next = f;
  return &l->side_info;
}

/* Allocate a new holder of a given size */
BF_PartHolder *shine_BF_newPartHolder(unsigned long int max_elements)
{
  BF_PartHolder *newPH = calloc(1, sizeof(BF_PartHolder));
  /* assert( newPH ); */
  newPH->max_elements = max_elements;
  newPH->part = calloc(1, sizeof(BF_BitstreamPart));
  /* assert( newPH->part ); */
  newPH->part->element = calloc((int)max_elements, sizeof(BF_BitstreamElement));
  /* assert( newPH->part->element ); */
  newPH->part->nrEntries = 0;
  return newPH;
}

BF_PartHolder *shine_BF_NewHolderFromBitstreamPart( BF_BitstreamPart *thePart )
{
  BF_PartHolder *newHolder = shine_BF_newPartHolder( thePart->nrEntries );
  return shine_BF_LoadHolderFromBitstreamPart( newHolder, thePart );
}

BF_PartHolder *shine_BF_LoadHolderFromBitstreamPart( BF_PartHolder *theHolder, BF_BitstreamPart *thePart )
{
  BF_BitstreamElement *pElem;
  int i;

  theHolder->part->nrEntries = 0;
  for ( i = 0; i < thePart->nrEntries; i++ )
    {
      pElem = &(thePart->element[i]);
      theHolder = shine_BF_addElement( theHolder, pElem );
    }
  return theHolder;
}

/* Grow or shrink a part holder. Always creates a new one of the right length
  and frees the old one after copying the data. */
BF_PartHolder *shine_BF_resizePartHolder( BF_PartHolder *oldPH, int max_elements )
{
  int elems, i;
  BF_PartHolder *newPH;

#ifdef DEBUG
  printf("Resizing part holder from %d to %d\n", oldPH->max_elements, max_elements );
#endif
  /* create new holder of the right length */
  newPH = shine_BF_newPartHolder( max_elements );

  /* copy values from old to new */
  elems = (oldPH->max_elements > max_elements) ? max_elements : oldPH->max_elements;
  newPH->part->nrEntries = elems;
  for ( i = 0; i < elems; i++ )
    newPH->part->element[i] = oldPH->part->element[i];

  /* free old holder */
  shine_BF_freePartHolder( oldPH );

  return newPH;
}

BF_PartHolder *shine_BF_freePartHolder( BF_PartHolder *thePH )
{
  free( thePH->part->element );
  free( thePH->part );
  free( thePH );
  return NULL;
}

/* Add theElement to thePH, growing the holder if necessary. Returns ptr to the
  holder, which may not be the one you called it with! */
BF_PartHolder *shine_BF_addElement( BF_PartHolder *thePH, BF_BitstreamElement *theElement )
{
  BF_PartHolder *retPH = thePH;
  int needed_entries = thePH->part->nrEntries + 1;
  int extraPad = 8;  /* add this many more if we need to resize */

  /* grow if necessary */
  if ( needed_entries > thePH->max_elements )
    retPH = shine_BF_resizePartHolder( thePH, needed_entries + extraPad );

  /* copy the data */
  retPH->part->element[retPH->part->nrEntries++] = *theElement;
  return retPH;
}

/* Add a bit value and length to the element list in thePH */
BF_PartHolder *shine_BF_addEntry( BF_PartHolder *thePH,
                            unsigned long int value,
                            unsigned int length )
{
  BF_BitstreamElement myElement;
  myElement.value  = value;
  myElement.length = length;
  if ( length )
    return shine_BF_addElement( thePH, &myElement );
  else
    return thePH;
}
