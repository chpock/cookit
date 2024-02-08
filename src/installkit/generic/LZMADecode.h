/* 
LzmaDecode.h
LZMA Decoder interface
LZMA SDK 4.01 Copyright (c) 1999-2004 Igor Pavlov (2004-02-15)

Converted to a state machine by Amir Szekely
*/

#ifndef __LZMADECODE_H
#define __LZMADECODE_H

/***********************
 *    Configuration    *
 ***********************/

/* #define _LZMA_PROB32 */
/* It can increase speed on some 32-bit CPUs, 
   but memory usage will be doubled in that case */

/***********************
 *  Configuration End  *
 ***********************/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef lzmaalloc
#define lzmaalloc malloc
#endif

#ifndef lzmafree
#define lzmafree free
#endif

#ifndef LZMACALL
#  define LZMACALL
#endif

#ifndef UInt32
#ifdef _LZMA_UINT32_IS_ULONG
#define UInt32 unsigned long
#else
#define UInt32 unsigned int
#endif
#endif

#ifdef _LZMA_PROB32
#define CProb UInt32
#else
#define CProb unsigned short
#endif

typedef unsigned char LByte;

#define LZMA_STREAM_END 1
#define LZMA_OK 0
#define LZMA_DATA_ERROR -1
/* we don't really care what the problem is... */
/* #define LZMA_RESULT_NOT_ENOUGH_MEM -2 */
#define LZMA_NOT_ENOUGH_MEM -1

typedef struct
{
  /* mode control */
  int mode;
  int last;
  int last2;
  int last3;

  /* properties */
  UInt32 dynamicDataSize;
  UInt32 dictionarySize;

  /* io */
  LByte *next_in;    /* next input byte */
  UInt32 avail_in;  /* number of bytes available at next_in */

  LByte *next_out;   /* next output byte should be put there */
  UInt32 avail_out; /* remaining free space at next_out */

  UInt32 totalOut;  /* total output - not always correct when lzmaDecode returns */

  /* saved state */
  LByte previousByte;
  LByte matchByte;
  CProb *probs;
  CProb *prob;
  int mi;
  int posState;
  int temp1;
  int temp2;
  int temp3;
  int lc;
  int state;
  int isPreviousMatch;
  int len;
  UInt32 rep0;
  UInt32 rep1;
  UInt32 rep2;
  UInt32 rep3;
  UInt32 posStateMask;
  UInt32 literalPosMask;
  UInt32 dictionaryPos;

  /* range coder */
  UInt32 range;
  UInt32 code;

  /* allocated buffers */
  LByte *dictionary;
  LByte *dynamicData;
} lzma_stream;

void LZMACALL lzmaInit(lzma_stream *);
int LZMACALL lzmaDecode(lzma_stream *);

#ifdef __cplusplus
}
#endif

#endif
