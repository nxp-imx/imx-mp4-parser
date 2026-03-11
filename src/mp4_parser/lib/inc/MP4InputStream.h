
/************************************************************************
 * Copyright (c) 2005-2014, Freescale Semiconductor, Inc.
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
    $Id: MP4InputStream.h,v 1.7 1999/03/01 01:18:21 mc Exp $
*/
#ifndef INCLUDED_MP4_INPUTSTREAM_H
#define INCLUDED_MP4_INPUTSTREAM_H

#include "FileMappingObject.h"
#include "MP4Movies.h"

enum Flags {
    debug_flag = 0x01,
    mediadata_flag = 0x02,
    metadata_flag = 0x04,
    live_flag = 0x08,
    memOptimization_flag = 0x10,
    fragmented_flag = 0x20,
    flag_3gp = 0x40,
};

#define COMMON_INPUTSTREAM_FIELDS                                                                \
    u64 available;                                                                               \
    u32 debugging;                                                                               \
    u32 indent;                                                                                  \
    s64 file_offset;                                                                             \
    u32 stream_flags;                                                                            \
    u8 buf[16];                                                                                  \
    u32 buf_mask;                                                                                \
    void (*msg)(struct MP4InputStreamRecord * self, char* msg);                                  \
    struct FileMappingObjectRecord* (*getFileMappingObject)(struct MP4InputStreamRecord * self); \
    void (*destroy)(struct MP4InputStreamRecord * self);                                         \
    MP4Err (*read8)(struct MP4InputStreamRecord * self, u32 * outVal, char* msg);                \
    MP4Err (*read16)(struct MP4InputStreamRecord * self, u32 * outVal, char* msg);               \
    MP4Err (*read32)(struct MP4InputStreamRecord * self, u32 * outVal, char* msg);               \
    MP4Err (*readData)(struct MP4InputStreamRecord * self, u32 bytes, void* outData, char* msg); \
    s64 (*checkAvailableBytes)(struct MP4InputStreamRecord * self, int64 bytesRequested);        \
    MP4Err (*seekTo)(struct MP4InputStreamRecord * self, s64 offset, u32 flag, char* msg);       \
    s64 (*getFilePos)(struct MP4InputStreamRecord * self, char* msg);

/************************************************************************************
explanation :
u32 available;         // available data length to read in bytes. If source is a local file, it's
the file length at first, then decrease as reading the file. u32 debugging;     // debug flag s32
file_offset;      // relative offset to the current position before next reading can be performed. 0
means reading sequentially. Amanda: support negative offset. u32 mediadata_flag;     //whether media
data atom ('mdat') is found in stream. u32 metadata_flag;  //whether meta data atom ('moov') is
found in stream. u32 sample_length;  //number of samples in this stream (track?), shall got from
'stsz' but from 'stts' now. Amanda
************************************************************************************/

typedef struct MP4InputStreamRecord {
    COMMON_INPUTSTREAM_FIELDS
} MP4InputStream, *MP4InputStreamPtr;

typedef struct MP4FileMappingInputStreamRecord {
    COMMON_INPUTSTREAM_FIELDS
    char* base;
    char* ptr;
    struct FileMappingObjectRecord* mapping;
} MP4FileMappingInputStream, *MP4FileMappingInputStreamPtr;

MP4Err MP4CreateFileMappingInputStream(struct FileMappingObjectRecord* mapping,
                                       MP4InputStreamPtr* outStream, uint32 flag);

#endif
