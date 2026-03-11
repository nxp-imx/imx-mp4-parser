/*
 ***********************************************************************
 * Copyright (c) 2005-2013, Freescale Semiconductor, Inc.
 *
 * Copyright 2017-2018, 2024, 2026 NXP
 ***********************************************************************
 */

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
    $Id: ChunkOffsetAtom.c,v 1.2 2000/05/17 08:01:27 francesc Exp $
*/

#include <stdlib.h>
#include "MP4Atoms.h"
#include "MP4TableLoad.h"

/* Max table size, in number of entries. For stsz, stco, stss, each entry is 4 bytes (u32).  */
#define STCO_TAB_MAX_SIZE 18000  // 108000
/* Defualt 108000 (x 4 Bytes). Assuming one sample per chunk, video 30fps -> 1 hour.
But actually, stco is larger than stsc becuase a chunk usually contains
more than one samples.*/

#define STCO_TAB_OVERLAP_SIZE (STCO_TAB_MAX_SIZE / 10)

/* Overlap size of two loading, in number of entries. */

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4ChunkOffsetAtomPtr self;
    err = MP4NoErr;
    self = (MP4ChunkOffsetAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->offsets) {
        MP4LocalFree(self->offsets);
        self->offsets = NULL;
    }
    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

/* chunkNumber, IN, number of chunk, 1-based */
static MP4Err getChunkOffset(MP4AtomPtr s, u32 chunkNumber, u64* outOffset) {
    MP4Err err;
    MP4ChunkOffsetAtomPtr self = (MP4ChunkOffsetAtomPtr)s;

    err = MP4NoErr;
    if ((self == NULL) || (outOffset == NULL) || (chunkNumber == 0) ||
        (chunkNumber > self->entryCount))
        BAILWITHERROR(MP4BadParamErr)

    if (0 == self->entryCount)
        BAILWITHERROR(MP4_ERR_EMTRY_TRACK)

    if (self->tableSize < self->entryCount) /* only part of table in memory */
    {
        u32 target_entry_idx = chunkNumber - 1;
        u32 first_entry_idx = self->first_entry_idx;

        if ((target_entry_idx < first_entry_idx) ||
            ((target_entry_idx - first_entry_idx) >=
             STCO_TAB_MAX_SIZE)) { /* load new table section for the target sample */
            err = load_new_entry_u32(self->inputStream, target_entry_idx, self->entryCount,
                                     self->tableSize, STCO_TAB_OVERLAP_SIZE, self->tab_file_offset,
                                     self->offsets, &self->first_entry_idx);

            if (err)
                BAILWITHERROR(MP4BadDataErr)
        }
        /* compensate entry number */
        chunkNumber = chunkNumber - self->first_entry_idx;
    }

    *outOffset = self->offsets[chunkNumber - 1];
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    u32 entry_cnt;
    u32 nb_entires_to_read = 0; /* number of entries read in scanning the table */
    u32 entries_total_size;     /* total size of entries read in memory, in bytes */
    MP4FileMappingInputStreamPtr stream = (MP4FileMappingInputStreamPtr)inputStream;

    MP4ChunkOffsetAtomPtr self = (MP4ChunkOffsetAtomPtr)s;

#ifdef FSL_MP4_PARSER_DBG_TIME
#if defined(__WINCE) || defined(WIN32)
    u32 start_time = timeGetTime();
    u32 end_time;
#else
    struct timezone time_zone;
    struct timeval start_time;
    struct timeval end_time;
    gettimeofday(&start_time, &time_zone);
#endif
#endif

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32(entryCount);
    // MP4MSG("stco size %lld, entry count %d\n", self->size, self->entryCount);

    if (0 > (s32)self->entryCount)
        BAILWITHERROR(MP4BadDataErr)
    if (0 == self->entryCount) { /* empty table means this track is empty */
        MP4MSG("\t Warning!empty stco\n");
        if (self->size > self->bytesRead) {
            uint64 bytesToSkip = self->size - self->bytesRead;
            SKIPBYTES_FORWARD(bytesToSkip);
        }
        goto bail;
    }

    self->inputStream = inputStream;
    self->tab_file_offset = /* back table's file offset */
            MP4LocalGetCurrentFilePos((FILE*)(stream->ptr), stream->mapping->parser_context);

    entry_cnt = self->entryCount;

    if (!((STCO_TAB_MAX_SIZE > STCO_TAB_OVERLAP_SIZE) /* Table & overalp size valid? */
          && (0 < STCO_TAB_MAX_SIZE) && (0 <= STCO_TAB_OVERLAP_SIZE))) {
        // MP4DbgLog("STCO: Invalid table size %ld & overlap size %ld\n", STCO_TAB_MAX_SIZE,
        // STCO_TAB_OVERLAP_SIZE);
        BAILWITHERROR(MP4NotImplementedErr);
    }

    if ((u64)entry_cnt * sizeof(u32) > self->size - self->bytesRead)
        BAILWITHERROR(MP4BadDataErr);

    if ((STCO_TAB_MAX_SIZE < entry_cnt) &&
        (inputStream->stream_flags & memOptimization_flag)) /* only load part of the entries */
        entry_cnt = STCO_TAB_MAX_SIZE;

    // MP4DbgLog("stco: %ld entries in 1st loading\n", entry_cnt);
    self->tableSize = entry_cnt;

    self->offsets = (u32*)MP4LocalCalloc(entry_cnt, sizeof(u32));
    TESTMALLOC(self->offsets);

    /* read the whole or part of 'stco' table into memory. Data in file are big endian */
    entries_total_size = entry_cnt * sizeof(u32);
    GETBYTES(entries_total_size, offsets);

#ifndef CPU_BIG_ENDIAN
    reverse_endian_u32(self->offsets, entry_cnt);
#endif

    self->firstChunkOffset = self->offsets[0];
    MP4MSG("\t1st chunk offset %d\n", self->firstChunkOffset);

    nb_entires_to_read = self->entryCount - entry_cnt;
    if (nb_entires_to_read) {
        /* skip the part not read */
        u64 bytes_to_skip = (u64)nb_entires_to_read * sizeof(u32);
        // MP4MSG("bytes_to_skip %lld, bytes not read %lld\n", bytes_to_skip, self->size -
        // self->bytesRead);

        // SKIPBYTES_FORWARD(bytes_to_skip);
        if (inputStream->available >= bytes_to_skip)
            inputStream->available -= bytes_to_skip;
        else
            BAILWITHERROR(MP4BadDataErr); /* index table must be entire */

        MP4LocalSeekFile((FILE*)stream->ptr, bytes_to_skip, SEEK_CUR,
                         stream->mapping->parser_context);
        self->bytesRead += bytes_to_skip;
    }

bail:
    TEST_RETURN(err);

    if (err) {
        if (self != NULL && self->offsets) {
            MP4LocalFree(self->offsets);
            self->offsets = NULL;
        }
    }

#ifdef FSL_MP4_PARSER_DBG_TIME
#if defined(__WINCE) || defined(WIN32)
    end_time = timeGetTime();
    MP4DbgLog("stco %ld ms\n", end_time - start_time);
#else
    gettimeofday(&end_time, &time_zone);
    if (end_time.tv_usec >= start_time.tv_usec) {
        MP4DbgLog("stco: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec,
                  end_time.tv_usec - start_time.tv_usec);
    } else {
        MP4DbgLog("stco: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec - 1,
                  end_time.tv_usec + 1000000000 - start_time.tv_usec);
    }
#endif
#endif

    return err;
}

MP4Err MP4CreateChunkOffsetAtom(MP4ChunkOffsetAtomPtr* outAtom) {
    MP4Err err;
    MP4ChunkOffsetAtomPtr self;

    self = (MP4ChunkOffsetAtomPtr)MP4LocalCalloc(1, sizeof(MP4ChunkOffsetAtom));
    TESTMALLOC(self);

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4ChunkOffsetAtomType;
    self->name = "chunk offset";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->getChunkOffset = getChunkOffset;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
