/*
 ***********************************************************************
 * Copyright (c) 2005-2013, Freescale Semiconductor, Inc.
 *
 * Copyright 2017-2018, 2026 NXP
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
    $Id: DecodingOffsetAtom.c,v 1.2 2000/05/17 08:01:27 francesc Exp $
*/

#include <stdlib.h>
#include "MP4Atoms.h"

#include "MP4TableLoad.h"

/* Max table size, in number of entries. */
#define CTTS_TAB_MAX_SIZE 54000 /* Defualt 54000 (x 8 bytes). 0.5 hour for 30fps video*/

#define CTTS_TAB_OVERLAP_SIZE (CTTS_TAB_MAX_SIZE / 10)
/* Overlap size of two loading, in number of entries. */

#define CTTS_TAB_MAX_MALLOC_SIZE (200 * 1024 * 1024)

typedef struct {
    u32 sampleCount;
    u32 decodingOffset;
} dttsEntry, *dttsEntryPtr;

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4CompositionOffsetAtomPtr self;
    err = MP4NoErr;
    self = (MP4CompositionOffsetAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->compositionOffsetEntries) {
        MP4LocalFree(self->compositionOffsetEntries);
        self->compositionOffsetEntries = NULL;
    }

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

/* entryIndex is 0-based */
static MP4Err getEntry(MP4CompositionOffsetAtomPtr self, uint32 entryIndex, dttsEntryPtr* pe) {
    MP4Err err = MP4NoErr;

    if (entryIndex >= self->entryCount)
        BAILWITHERROR(MP4EOF)

    if (self->tableSize < self->entryCount) {
        u32 target_entry_idx = entryIndex; /* only assure this sample loaded */
        u32 first_entry_idx = self->first_entry_idx;

        if ((target_entry_idx < first_entry_idx) ||
            ((target_entry_idx - first_entry_idx) >=
             CTTS_TAB_MAX_SIZE)) { /* load new table section for the target sample */
            err = load_new_entry_u64(self->inputStream, target_entry_idx, self->entryCount,
                                     self->tableSize, CTTS_TAB_OVERLAP_SIZE, self->tab_file_offset,
                                     self->compositionOffsetEntries, &self->first_entry_idx);

            if (err)
                BAILWITHERROR(MP4BadDataErr)
            // MP4MSG("ctts: target entry index %ld, new first entry idx %ld\n", target_entry_idx,
            // self->first_entry_idx);
        }
        /* compensate sample number */
        entryIndex -= self->first_entry_idx;
        // MP4MSG("ctts: target entry index %ld, first entry idx %ld, offset %ld\n",
        // target_entry_idx, self->first_entry_idx, entryIndex);
    }

    *pe = (dttsEntryPtr)&self->compositionOffsetEntries[entryIndex];
bail:
    return err;
}

static MP4Err getOffsetForSampleNumber(MP4AtomPtr s, u32 sampleNumber, s32* outOffset) {
    MP4Err err = MP4NoErr;
    u32 i;
    u32 entryCount;
    u32 currentSampleNumberOffset;
    u32 samplesPerEntry;

    MP4CompositionOffsetAtomPtr self = (MP4CompositionOffsetAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (0 >= (s32)self->entryCount)
        BAILWITHERROR(MP4BadDataErr)
    entryCount = self->entryCount;

    *outOffset = 0;

    if (self->foundEntrySampleNumber && (self->foundEntrySampleNumber <= sampleNumber)) {
        /* a stts entry found in previous searching and can be reused */
        currentSampleNumberOffset = self->foundEntrySampleNumber;
        i = self->foundEntryNumber;
    } else { /* start new search from the beginning of stts */
        currentSampleNumberOffset = 1;
        i = 0;
    }

    for (; i < entryCount; i++) {
        dttsEntryPtr p;
        err = getEntry(self, i, &p);
        if (err)
            goto bail;
        samplesPerEntry = p->sampleCount;
        // MP4MSG("entry %ld, 1st sample number %ld, %ld samples, offset %ld\n", i,
        // currentSampleNumberOffset, p->sampleCount, p->decodingOffset);

        if (currentSampleNumberOffset + samplesPerEntry > sampleNumber) {
            *outOffset = p->decodingOffset;

            self->foundEntryNumber = i;
            self->foundEntrySampleNumber = currentSampleNumberOffset;
            // MP4MSG("sample %ld, ctts offset %ld,  entry %ld match!\n\n", sampleNumber,
            // *outOffset, i);
            break;
        } else
            currentSampleNumberOffset += samplesPerEntry;
    }

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    MP4CompositionOffsetAtomPtr self = (MP4CompositionOffsetAtomPtr)s;

    u32 entry_cnt;          /* number of entries in one loading */
    u32 entries_total_size; /* bytes of entries in one loading */
    MP4FileMappingInputStreamPtr stream = (MP4FileMappingInputStreamPtr)inputStream;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32_V(self->entryCount);
    if (0 > (s32)self->entryCount)
        BAILWITHERROR(MP4BadDataErr)

    if (0 == self->entryCount) {
        MP4MSG("\t Warning!empty ctts\n");
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

    if (!((CTTS_TAB_MAX_SIZE > CTTS_TAB_OVERLAP_SIZE) && (0 < CTTS_TAB_MAX_SIZE) &&
          (0 <= CTTS_TAB_OVERLAP_SIZE))) {
        // MP4DbgLog("ctss: Invalid table size %ld & overlap size %ld\n", CTSS_TAB_MAX_SIZE,
        // CTSS_TAB_OVERLAP_SIZE);
        BAILWITHERROR(MP4NotImplementedErr);
    }

    if ((u64)entry_cnt * sizeof(u64) > self->size - self->bytesRead)
        BAILWITHERROR(MP4BadDataErr);

    if ((u64)entry_cnt * sizeof(u64) > CTTS_TAB_MAX_MALLOC_SIZE)
        BAILWITHERROR(MP4BadDataErr);

    if ((CTTS_TAB_MAX_SIZE < entry_cnt) && (inputStream->stream_flags & memOptimization_flag))
        entry_cnt = CTTS_TAB_MAX_SIZE;

    // MP4DbgLog("ctss: entry count %ld, %ld entries in 1st loading\n", self->entryCount,
    // entry_cnt);
    self->tableSize = entry_cnt;

    self->compositionOffsetEntries = (u64*)MP4LocalCalloc(entry_cnt, sizeof(u64));
    TESTMALLOC(self->compositionOffsetEntries)

    entries_total_size = entry_cnt * sizeof(u64);
    GETBYTES(entries_total_size, compositionOffsetEntries);
    if (err)
        BAILWITHERROR(MP4BadDataErr);

#ifndef CPU_BIG_ENDIAN
    reverse_endian_u32((u32*)self->compositionOffsetEntries,
                       entry_cnt * 2); /* each entry has 2 uint32 fields */
#endif

    /* skip the remaining data */
    if (self->size > self->bytesRead) {
        uint64 bytesToSkip = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(bytesToSkip);
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateCompositionOffsetAtom(MP4CompositionOffsetAtomPtr* outAtom) {
    MP4Err err;
    MP4CompositionOffsetAtomPtr self;

    self = (MP4CompositionOffsetAtomPtr)MP4LocalCalloc(1, sizeof(MP4CompositionOffsetAtom));
    TESTMALLOC(self);

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->type = MP4CompositionOffsetAtomType;
    self->name = "decoding offset";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->getOffsetForSampleNumber = getOffsetForSampleNumber;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
