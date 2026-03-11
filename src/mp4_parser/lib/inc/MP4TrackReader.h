
/************************************************************************
 * Copyright 2005-2006 by Freescale Semiconductor, Inc.
 * Copyright 2026 NXP
 ************************************************************************/

/*
This software module was originally developed by Apple Computer, Inc.
in the course of development of MPEG-4.
This software module is an implementation of a part of one or
more MPEG-4 tools as specified by MPEG-4.
ISO/IEC gives users of MPEG-4 free license to this
software module or modifications thereof for use in hardware
or software products claiming conformance to MPEG-4.
Those intending to use this software module in hardware or software
products are advised that its use may infringe existing patents.
The original developer of this software module and his/her company,
the subsequent editors and their companies, and ISO/IEC have no
liability for use of this software module or modifications thereof
in an implementation.
Copyright is not released for non MPEG-4 conforming
products. Apple Computer, Inc. retains full right to use the code for its own
purpose, assign or donate the code to a third party and to
inhibit third parties from using the code for non
MPEG-4 conforming products.
This copyright notice must be included in all copies or
derivative works. Copyright (c) 1999.
*/
/*
    $Id: MP4TrackReader.h,v 1.9 1999/12/17 07:47:20 mc Exp $
*/
#ifndef INCLUDED_MP4_TRACK_READER_H
#define INCLUDED_MP4_TRACK_READER_H

#include "MP4Atoms.h"

#define TRACK_READER_ENTRIES                                                                     \
    MP4Err (*destroy)(struct MP4TrackReaderStruct * self);                                       \
    MP4Err (*getNextAccessUnit)(struct MP4TrackReaderStruct * self, MP4Handle outAccessUnit,     \
                                u32 * outSize, u32 * outSampleFlags, u64 * outCTS, u64 * outDTS, \
                                u32 * outDuration);                                              \
    MP4Err (*getNextPacket)(struct MP4TrackReaderStruct * self, MP4Handle outSample,             \
                            u32 * outSize);                                                      \
    MP4Err (*setSLConfig)(struct MP4TrackReaderStruct * self, MP4SLConfig slconfig);             \
    MP4Movie movie;                                                                              \
    MP4Track track;                                                                              \
    MP4Media media;                                                                              \
    MP4Handle sampleH;                                                                           \
    MP4SLConfig slconfig;                                                                        \
    MP4EditListAtomPtr elst;                                                                     \
    u32 movieTimeScale;                                                                          \
    u32 mediaTimeScale;                                                                          \
    u32 trackSegments;                                                                           \
    u32 nextSegment;                                                                             \
    u64 segmentEndTime;                                                                          \
    u64 segmentMovieTime;                                                                        \
    u64 segmentBeginTime;                                                                        \
    u32 sequenceNumber;                                                                          \
    u32 segmentSampleCount;                                                                      \
    u32 sampleDescIndex;                                                                         \
    u32 nextSampleNumber;                                                                        \
    u32* chunk_offset;                                                                           \
    u32 odsm_offset;                                                                             \
    u32 sdsm_offset;

/*********************************************************************
comment:
u32      nextSampleNumber;  number of next sample to read, 1-based.

*********************************************************************/

typedef struct MP4TrackReaderStruct {
    TRACK_READER_ENTRIES
    u32 isODTrack;
} * MP4TrackReaderPtr;

MP4Err MP4GetTrackReader(MP4Track theTrack, u32 FrameNumber, MP4TrackReader* outReader);
#endif
