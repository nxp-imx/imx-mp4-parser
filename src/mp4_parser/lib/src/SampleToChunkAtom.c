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
    $Id: SampleToChunkAtom.c,v 1.2 2000/05/17 08:01:31 francesc Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

#include "MP4TableLoad.h"

/* Max table size, in number of entries. For stsz, stco, stss, each entry is 4 bytes (u32).  */
#define STSC_TAB_MAX_SIZE \
    80000 /* Default 80000, in entries. No direct relation to sample duration */
#define STSC_TAB_OVERLAP_SIZE (STSC_TAB_MAX_SIZE / 10)
#define STSC_TAB_MAX_MALLOC_SIZE (200 * 1024 * 1024)

#define DWORDS_PER_STSC_ENRTY 3

/* stsc entry in file layout */
typedef struct {
    u32 firstChunk;
    u32 samplesPerChunk;
    u32 sampleDescriptionIndex;

} stscEntry, *stscEntryPtr;

static u32 getEntryCount(struct MP4SampleToChunkAtom* self) {
    return self->entryCount;
}

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4SampleToChunkAtomPtr self;

    err = MP4NoErr;
    self = (MP4SampleToChunkAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->sampleToChunkEntries) {
        MP4LocalFree(self->sampleToChunkEntries);
        self->sampleToChunkEntries = NULL;
    }

    if (self->super)
        self->super->destroy(s);

bail:
    TEST_RETURN(err);

    return;
}

static MP4Err getEntry(MP4SampleToChunkAtomPtr self, uint32 entryIndex, stscEntryPtr* pe) {
    MP4Err err = MP4NoErr;

    if (entryIndex >= self->entryCount)
        BAILWITHERROR(MP4EOF)

    if (self->tableSize < self->entryCount) {
        u32 target_entry_idx = entryIndex; /* only assure this sample loaded */
        u32 first_entry_idx = self->first_entry_idx;

        if ((target_entry_idx < first_entry_idx) ||
            ((target_entry_idx - first_entry_idx) >=
             STSC_TAB_MAX_SIZE)) { /* load new table section for the target sample */
            err = load_new_entry_dwords(self->inputStream, target_entry_idx, self->entryCount,
                                        self->tableSize, STSC_TAB_OVERLAP_SIZE,
                                        self->tab_file_offset, self->sampleToChunkEntries,
                                        &self->first_entry_idx, DWORDS_PER_STSC_ENRTY);

            if (err)
                BAILWITHERROR(MP4BadDataErr)
            // MP4MSG("stsc: target entry index %ld, new first entry idx %ld\n", target_entry_idx,
            // self->first_entry_idx);
        }
        /* compensate sample number */
        entryIndex -= self->first_entry_idx;
        // MP4MSG("stsc: target entry index %ld, first entry idx %ld, offset %ld\n",
        // target_entry_idx, self->first_entry_idx, entryIndex);
    }

    *pe = (stscEntryPtr)(self->sampleToChunkEntries + entryIndex * DWORDS_PER_STSC_ENRTY);
bail:
    return err;
}

/* Note by Amanda:
When the 'stsc' atom is setup, neither the total chunk number nor total sample number is known.
(We make no assumption that the 'stsz' and 'stco' shall lay befire 'stsc').
So if the matched entry is the last one,
the chunk number returned may be invalid (larger than actural chunk number)
and the return code is still MP4NoErr.
We assume the function to get the chunk offset shall fail for this case.
TODO: self->numChunks get this from the stco atom
*/
static MP4Err lookupSample(MP4AtomPtr s, u32 sampleNumber, u32* outChunkNumber,
                           u32* outSampleDescriptionIndex, u32* outFirstSampleNumberInChunk,
                           u32* lastSampleNumberInChunk) {
    MP4Err err = MP4NoErr;
    MP4SampleToChunkAtomPtr self = (MP4SampleToChunkAtomPtr)s;
    u32 i;
    u32 entryCount;
    stscEntryPtr p;
    stscEntryPtr nextPe;

    u32 firstSampleThisChunkRun;
    u32 firstChunk = 0; /* fisst chunk number this entry */
    u32 samplesPerChunk = 0;
    u32 sampleDescriptionIndex = 0;

    u32 numChunksThisEntry = 0;  /* if the last entry matched, this value remains ZERO */
    u32 firstSampleNumNextEntry; /* first sample number of next entry */

    // u32 sampleDescriptionIndex;
    u32 chunkOffset;

    if ((self == NULL) || (sampleNumber == 0) || (outChunkNumber == NULL))
        BAILWITHERROR(MP4BadParamErr)

    entryCount = self->entryCount;
    if (0 == entryCount)
        BAILWITHERROR(MP4_ERR_EMTRY_TRACK)

    if (self->foundEntryNumber >= entryCount) {
        err = MP4BadDataErr;
        goto bail;
    }

    if (self->foundEntryFirstSampleNumber && (self->foundEntryFirstSampleNumber <= sampleNumber)) {
        firstSampleThisChunkRun = self->foundEntryFirstSampleNumber;
        i = self->foundEntryNumber;
    } else {
        firstSampleThisChunkRun = 1;
        i = 0;
    }

    *outChunkNumber = 0;
    *outSampleDescriptionIndex = 0;

    // MP4MSG("target sample number %d, cached 1st sample number %ld\n", sampleNumber,
    // firstSampleThisChunkRun);
    p = NULL;
    nextPe = NULL;
    firstSampleNumNextEntry = firstSampleThisChunkRun;

    for (; i < entryCount; i++) {
        if (nextPe) {
            p = nextPe;
            nextPe = NULL;
        } else {
            err = getEntry(self, i, &p);
            if (err)
                goto bail;
            // MP4MSG("stsc entry %d, 1st chunk %ld, %d samples per chunk, desp id %ld\n", i,
            // p->firstChunk, p->samplesPerChunk, p->sampleDescriptionIndex);
        }

        /* attention, reading "nextPe" may change the memory content, so must backup content, not
         * pointer! */
        firstChunk = p->firstChunk;
        samplesPerChunk = p->samplesPerChunk;
        sampleDescriptionIndex = p->sampleDescriptionIndex;

        if (0 == samplesPerChunk)
            BAILWITHERROR(MP4BadParamErr)

        if (i < (entryCount - 1)) {
            err = getEntry(self, i + 1, &nextPe);
            if (err)
                goto bail;
            // MP4MSG("stsc entry %d, 1st chunk %ld, %d samples per chunk, desp id %ld\n", i+1,
            // nextPe->firstChunk, nextPe->samplesPerChunk, nextPe->sampleDescriptionIndex);

            numChunksThisEntry = nextPe->firstChunk - firstChunk;
            firstSampleNumNextEntry += samplesPerChunk * numChunksThisEntry;
            // MP4MSG("entry 1st sample number %ld, last sample number %ld\n",
            // firstSampleThisChunkRun, firstSampleNumNextEntry - 1);

            if (sampleNumber < firstSampleNumNextEntry) /* target chunk in this entry */
                break;

            /* check the next entry */
            firstSampleThisChunkRun = firstSampleNumNextEntry;
        } else /* current entry is the last one, must match! */
        {
            // MP4MSG("last entry match\n");
            break;
        }
    }

    chunkOffset = (sampleNumber - firstSampleThisChunkRun) / samplesPerChunk;
    *outChunkNumber = chunkOffset + firstChunk;
    *outSampleDescriptionIndex = sampleDescriptionIndex;
    *outFirstSampleNumberInChunk = chunkOffset * samplesPerChunk + firstSampleThisChunkRun;
    *lastSampleNumberInChunk = *outFirstSampleNumberInChunk + samplesPerChunk - 1;

    /* update cache*/
    self->foundEntryNumber = i;
    self->foundEntryFirstSampleNumber = firstSampleThisChunkRun;
    // MP4MSG("entry %d match, chunk %ld, desp ID %ld, first sample %ld, last sample %ld\n\n", i,
    // *outChunkNumber, *outSampleDescriptionIndex, *outFirstSampleNumberInChunk,
    // *lastSampleNumberInChunk);

bail:
    TEST_RETURN(err);

    return err;
}

/* Amanda: for test only */
#ifdef DBG_TEST_SAMPLE_TO_CHUNK_ATOM
static MP4Err dbgTestSampleToChunkAtom(MP4SampleToChunkAtomPtr s) {
    MP4Err err = MP4NoErr;
    u32 i;
    u32 entryCount;
    u32 max_sample_number;

    err = MP4GetListEntryCount(s->entryList, &entryCount);
    if (err)
        goto bail;
    {
        stscEntryPtr p = NULL;
        for (i = 0; i < entryCount; i++) {
            err = MP4GetListEntry(s->entryList, i, (char**)&p);
            if (err)
                printf("fail to get entry %d\n", i);
            else
                printf("entry %d: 1st chk %ld, samples per chk %ld, desp %ld, last spl %ld\n", i,
                       p->firstChunk, p->samplesPerChunk, p->sampleDescriptionIndex,
                       p->lastSampleNumber);
        }
    }

    if (1 >= entryCount)
        max_sample_number = 200;
    else if (20 >= entryCount)
        max_sample_number = 2000;
    else
        max_sample_number = 3500;

    for (i = 0; i < max_sample_number; i += 50) {
        u32 outChunkNumber;
        u32 outSampleDescriptionIndex;
        u32 outFirstSampleNumberInChunk;
        u32 lastSampleNumberInChunk;
        MP4Err err_test;
        err_test = lookupSample(s, i, &outChunkNumber, &outSampleDescriptionIndex,
                                &outFirstSampleNumberInChunk, &lastSampleNumberInChunk);
        if (!err_test)
            printf("sample %d: chk %ld, desp %ld, 1st sample %ld, last sample %ld\n", i,
                   outChunkNumber, outSampleDescriptionIndex, outFirstSampleNumberInChunk,
                   lastSampleNumberInChunk);
        else
            printf("sample %d fail\n");
    }

    for (i = max_sample_number; i > 0; i -= 50) {
        u32 outChunkNumber;
        u32 outSampleDescriptionIndex;
        u32 outFirstSampleNumberInChunk;
        u32 lastSampleNumberInChunk;
        MP4Err err_test;
        err_test = lookupSample(s, i, &outChunkNumber, &outSampleDescriptionIndex,
                                &outFirstSampleNumberInChunk, &lastSampleNumberInChunk);
        if (!err_test)
            printf("sample %d: chk %ld, desp %ld, 1st sample %ld, last sample %ld\n", i,
                   outChunkNumber, outSampleDescriptionIndex, outFirstSampleNumberInChunk,
                   lastSampleNumberInChunk);
        else
            printf("sample %d fail\n");
    }

bail:
    TEST_RETURN(err);

    return err;
}
#endif

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;

    u32 entryCount;
    MP4SampleToChunkAtomPtr self = (MP4SampleToChunkAtomPtr)s;
    MP4FileMappingInputStreamPtr stream = (MP4FileMappingInputStreamPtr)inputStream;
    stscEntryPtr tmpBuffer = NULL; /* buffer to load 1st part or all of the index table */
    uint32 numEntriesToRead;
    uint32 bytesToRead;

#ifdef FSL_MP4_PARSER_DBG_TIME
#if defined(__WINCE) || defined(WIN32)
    u32 start_time = timeGetTime();
    u32 end_time;
#else
    struct timezone time_zone;
    struct timeval start_time;
    struct timeval end_time;
    struct timeval start_time_max_size;
    gettimeofday(&start_time, &time_zone);
#endif
#endif

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32_V(entryCount); /* get the count of entries in 'stsc' */
    self->entryCount = entryCount;
    // MP4MSG("\t stsc atom size %lld, bytes read %lld, left %lld\n", self->size, self->bytesRead,
    // self->size - self->bytesRead);
    if (0 > (s32)self->entryCount)
        BAILWITHERROR(MP4BadDataErr)
    if (0 == self->entryCount) { /* empty table means this track is empty */
        MP4MSG("\t Warning!empty stsc\n");
        if (self->size > self->bytesRead) {
            uint64 bytesToSkip = self->size - self->bytesRead;
            SKIPBYTES_FORWARD(bytesToSkip);
        }
        goto bail;
    }

    self->inputStream = inputStream;
    self->tab_file_offset = /* back table's file offset */
            MP4LocalGetCurrentFilePos((FILE*)(stream->ptr), stream->mapping->parser_context);

    if ((u64)entryCount * sizeof(stscEntry) > self->size - self->bytesRead)
        BAILWITHERROR(MP4BadDataErr);

    if ((u64)entryCount * sizeof(stscEntry) > STSC_TAB_MAX_MALLOC_SIZE)
        BAILWITHERROR(MP4BadDataErr);

    /* read 1st part or all of the index table */
    if ((inputStream->stream_flags & memOptimization_flag))
        numEntriesToRead = entryCount < STSC_TAB_MAX_SIZE ? entryCount : STSC_TAB_MAX_SIZE;
    else
        numEntriesToRead = entryCount;

    MP4MSG("\t stsc entry count %d, buffer size %d\n", self->entryCount, numEntriesToRead);
    self->tableSize = numEntriesToRead;

    tmpBuffer = (stscEntryPtr)MP4LocalMalloc(numEntriesToRead * sizeof(stscEntry));
    TESTMALLOC(tmpBuffer);

    bytesToRead = numEntriesToRead * sizeof(stscEntry);
    GETBYTES_V_MSG(bytesToRead, tmpBuffer, NULL);

#ifndef CPU_BIG_ENDIAN
    reverse_endian_u32(
            (u32*)tmpBuffer,
            numEntriesToRead * DWORDS_PER_STSC_ENRTY); /* each entry has 3 uint32 fields */
#endif

    /* skip the left size of stsc atom */
    if (self->size > self->bytesRead) {
        u64 sizeToSkip = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(sizeToSkip);
    }

    self->sampleToChunkEntries = (u32*)tmpBuffer;

    // dbgTestSampleToChunkAtom(s);  /* For test only */

bail:
    TEST_RETURN(err);
    if (err && tmpBuffer)
        MP4LocalFree(tmpBuffer);

#ifdef FSL_MP4_PARSER_DBG_TIME
#if defined(__WINCE) || defined(WIN32)
    end_time = timeGetTime();
    MP4DbgLog("stsc %ld ms\n", end_time - start_time);
#else
    gettimeofday(&end_time, &time_zone);
    if (end_time.tv_usec >= start_time.tv_usec) {
        MP4DbgLog("stsc: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec,
                  end_time.tv_usec - start_time.tv_usec);
    } else {
        MP4DbgLog("stsc: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec - 1,
                  end_time.tv_usec + 1000000000 - start_time.tv_usec);
    }
#endif
#endif

    return err;
}

MP4Err MP4CreateSampleToChunkAtom(MP4SampleToChunkAtomPtr* outAtom) {
    MP4Err err;
    MP4SampleToChunkAtomPtr self;

    self = (MP4SampleToChunkAtomPtr)MP4LocalCalloc(1, sizeof(MP4SampleToChunkAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->type = MP4SampleToChunkAtomType;
    self->name = "sample to chunk";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->lookupSample = lookupSample;
    self->getEntryCount = getEntryCount;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
