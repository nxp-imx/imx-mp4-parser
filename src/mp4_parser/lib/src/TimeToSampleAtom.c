/*
 ***********************************************************************
 * Copyright (c) 2005-2014, Freescale Semiconductor, Inc.
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
    $Id: TimeToSampleAtom.c,v 1.2 2000/05/17 08:01:31 francesc Exp $
*/

#include <stdlib.h>
#include "MP4Atoms.h"

#include "MP4TableLoad.h"

/* Max table size, in number of entries. For stsz, stco, stss, each entry is 4 bytes (u32).  */
#define STTS_TAB_MAX_SIZE 54000 /* Defualt 54000 (x 8 bytes). 0.5 hour for 30fps video*/

#define STTS_TAB_OVERLAP_SIZE (STTS_TAB_MAX_SIZE / 10)
/* Overlap size of two loading, in number of entries. */

#define STTS_TAB_MAX_MALLOC_SIZE (200 * 1024 * 1024)

typedef struct {
    u32 sampleCount;
    s32 sampleDuration;
} sttsEntry, *sttsEntryPtr;

/* entryIndex is 0-based */
static MP4Err getEntry(MP4TimeToSampleAtomPtr self, uint32 entryIndex, sttsEntryPtr* pe) {
    MP4Err err = MP4NoErr;
    uint32 dwIdx;
    u32 tmpVal;
    sttsEntryPtr peTmp;

    if (entryIndex >= self->entryCount)
        BAILWITHERROR(MP4EOF)

    if (self->tableSize < self->entryCount) {
        u32 target_entry_idx = entryIndex; /* only assure this sample loaded */
        u32 first_entry_idx = self->first_entry_idx;

        if ((target_entry_idx < first_entry_idx) ||
            ((target_entry_idx - first_entry_idx) >=
             STTS_TAB_MAX_SIZE)) { /* load new table section for the target sample */
            err = load_new_entry_u64(self->inputStream, target_entry_idx, self->entryCount,
                                     self->tableSize, STTS_TAB_OVERLAP_SIZE, self->tab_file_offset,
                                     self->sampleDurationEntries, &self->first_entry_idx);

            if (err)
                BAILWITHERROR(MP4BadDataErr)
            // MP4MSG("stts: target entry index %ld, new first entry idx %ld\n", target_entry_idx,
            // self->first_entry_idx);

            // fix ENGR00182564
            // since use load_new_entry_u64, so 0-3 4-7 is shift to 7-4 3-0, but we need 3-0 7-4
            for (dwIdx = 0; dwIdx < STTS_TAB_MAX_SIZE; dwIdx++) {
                peTmp = (sttsEntryPtr)&self->sampleDurationEntries[dwIdx];
                tmpVal = peTmp->sampleCount;
                peTmp->sampleCount = peTmp->sampleDuration;
                peTmp->sampleDuration = tmpVal;
            }
        }
        /* compensate sample number */
        entryIndex -= self->first_entry_idx;
        // MP4MSG("stts: target entry index %ld, first entry idx %ld, offset %ld\n",
        // target_entry_idx, self->first_entry_idx, entryIndex);
    }

    *pe = (sttsEntryPtr)&self->sampleDurationEntries[entryIndex];
bail:
    return err;
}

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4TimeToSampleAtomPtr self;
    err = MP4NoErr;
    self = (MP4TimeToSampleAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->sampleDurationEntries) {
        MP4LocalFree(self->sampleDurationEntries);
        self->sampleDurationEntries = NULL;
    }

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err getTotalDuration(struct MP4TimeToSampleAtom* self, u64* outDuration) {
    MP4Err err = MP4NoErr;

    *outDuration = self->totalDuration;

    TEST_RETURN(err);
    return err;
}

static MP4Err getTimeForSampleNumber(MP4AtomPtr s, u32 sampleNumber, u64* outSampleCTS,
                                     s32* outSampleDuration) {
    MP4Err err = MP4NoErr;
    u32 i; /* index of stts entry matched */
    sttsEntryPtr pe;
    u64 entryTime;         /* total time of the checked entries */
    u32 entrySampleNumber; /* 1st sample number of the entry */
    u64 sampleCTS;         /* Amanda: seems to be DTS, need further check */
    s32 sampleDuration;
    u32 entryCount;
    u32 entry_found = 0; /* Amanda: whether the desired entry is found or not */

    MP4TimeToSampleAtomPtr self = (MP4TimeToSampleAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (0 >= (s32)self->entryCount)
        BAILWITHERROR(MP4BadDataErr)
    entryCount = self->entryCount;
    if (0 == entryCount)
        BAILWITHERROR(MP4_ERR_EMTRY_TRACK)

    if (self->foundEntry && (self->foundEntrySampleNumber < sampleNumber)) {
        /* a stts entry found in previous searching and can be reused */
        entryTime = self->foundEntryTime;
        entrySampleNumber = self->foundEntrySampleNumber;
        i = self->foundEntryNumber;
    } else { /* start new search from the beginning of stts */
        entryTime = 0;
        entrySampleNumber = 1;
        i = 0;
    }

    // MP4MSG("start sample number %d, entry time %lld, i %d ...\n", entrySampleNumber, entryTime,
    // i);
    sampleCTS = -1;
    sampleDuration = 0;

    for (; i < entryCount; i++) {
        err = getEntry(self, i, &pe);
        if (err)
            goto bail;

        if ((entrySampleNumber + pe->sampleCount) > sampleNumber) {
            /* this is the desired entry */
            u64 sampleOffset;
            entry_found = 1;
            sampleOffset = (u64)(sampleNumber - entrySampleNumber);
            sampleCTS = entryTime + (sampleOffset * (u64)pe->sampleDuration);
            sampleDuration = pe->sampleDuration;
            // MP4MSG("sample %d, CTS %lld, duration %ld (entry 1st sample %ld, entry time %lld)\n",
            // sampleNumber, sampleCTS, sampleDuration, entrySampleNumber, entryTime);
            break;
        } else {
            /* go to next entry */
            entrySampleNumber += pe->sampleCount;
            entryTime += (pe->sampleCount * pe->sampleDuration);
            // MP4MSG("entry %d (count %d, duration %ld), next sample %ld, start time %lld\n", i,
            // pe->sampleCount, pe->sampleDuration, entrySampleNumber, entryTime);
        }
    }

    if (entry_found) {
        if (outSampleCTS)
            *outSampleCTS = sampleCTS;
        if (outSampleDuration)
            *outSampleDuration = sampleDuration;
        self->foundEntry = pe;
        self->foundEntryTime = entryTime;
        self->foundEntrySampleNumber = entrySampleNumber;
        self->foundEntryNumber = i;
        // MP4MSG("sample %d, CTS %lld, duration %ld\n", sampleNumber, sampleCTS, sampleDuration);
    } else /* This sample does NOT exist, invalidate all the entry info, not to reuse for next
              search. Assuming EOS */
    {
        if (outSampleCTS)
            *outSampleCTS = -1;
        if (outSampleDuration)
            *outSampleDuration = -1;
        self->foundEntry = NULL;
        self->foundEntryTime = -1;
        self->foundEntrySampleNumber = -1;
        self->foundEntryNumber = -1;

        err = MP4EOF;
    }

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err findSamples(MP4AtomPtr s, u64 desiredTime, s64* outPriorSample, s64* outExactSample,
                          s64* outNextSample, u32* outSampleNumber, s32* outSampleDuration) {
    MP4Err err;
    u32 entryCount;
    u32 i;

    sttsEntryPtr pe;
    u64 entryTime;
    u32 sampleNumber;
    u32 finalValue = 0;
    u32 finalTimeValue = 0;

    MP4TimeToSampleAtomPtr self = (MP4TimeToSampleAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = MP4NoErr;

    if (0 >= (s32)self->entryCount)
        BAILWITHERROR(MP4BadDataErr)
    entryCount = self->entryCount;
    if (0 == entryCount)
        BAILWITHERROR(MP4_ERR_EMTRY_TRACK)

    entryTime = 0;
    *outPriorSample = -1;
    *outExactSample = -1;
    *outNextSample = -1;
    *outSampleNumber = -1;
    (void)outSampleDuration;
    sampleNumber = 1;

    for (i = 0; i < entryCount; i++) {
        u32 x = 1;
        err = getEntry(self, i, &pe);
        if (err)
            goto bail;

        while (x <= pe->sampleCount) {
            entryTime = finalValue;
            if ((entryTime + pe->sampleDuration * x) > desiredTime) {
                *outSampleNumber = sampleNumber;
                return 0;
            } else {
                sampleNumber++;
                finalTimeValue = pe->sampleDuration * x;
                x++;
            }
        }
        finalValue += finalTimeValue;
    }

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    MP4TimeToSampleAtomPtr self = (MP4TimeToSampleAtomPtr)s;

    u32 entryCountLoaded = 0; /* for debug only */

    u32 entry_cnt;          /* number of entries in one loading */
    u32 entries_total_size; /* bytes of entries in one loading */
    MP4FileMappingInputStreamPtr stream = (MP4FileMappingInputStreamPtr)inputStream;
    u32 nb_entires_to_read = 0; /* number of entries read in scanning the table */
    u32 i;
    sttsEntryPtr entries;
    u32 sampleCount;
    u32 sampleDuration;

    u32 totalSampleCount = 0;
    u64 toatalDuration = 0;

    u64* tmp_buffer = NULL;

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

    GET32_V(self->entryCount);
    if (0 > (s32)self->entryCount)
        BAILWITHERROR(MP4BadDataErr)

    if (0 == self->entryCount) { /* empty table means this track is empty */
        MP4MSG("\t Warning!empty stts\n");
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

    if (!((STTS_TAB_MAX_SIZE > STTS_TAB_OVERLAP_SIZE) && (0 < STTS_TAB_MAX_SIZE) &&
          (0 <= STTS_TAB_OVERLAP_SIZE))) {
        // MP4DbgLog("stss: Invalid table size %ld & overlap size %ld\n", STSS_TAB_MAX_SIZE,
        // STSS_TAB_OVERLAP_SIZE);
        BAILWITHERROR(MP4NotImplementedErr);
    }

    if ((u64)entry_cnt * sizeof(u64) > self->size - self->bytesRead)
        BAILWITHERROR(MP4BadDataErr);

    if ((u64)entry_cnt * sizeof(u64) > STTS_TAB_MAX_MALLOC_SIZE)
        BAILWITHERROR(MP4BadDataErr);

    if ((STTS_TAB_MAX_SIZE < entry_cnt) &&
        (inputStream->stream_flags & memOptimization_flag)) /* only load part of the entries */
        entry_cnt = STTS_TAB_MAX_SIZE;

    // MP4DbgLog("stss: entry count %ld, %ld entries in 1st loading\n", self->entryCount,
    // entry_cnt);
    self->tableSize = entry_cnt;

    self->sampleDurationEntries = (u64*)MP4LocalCalloc(entry_cnt, sizeof(u64));
    TESTMALLOC(self->sampleDurationEntries)

    /* read the whole 'stss' table into memory. Data in file are big endian */
    entries_total_size = entry_cnt * sizeof(u64);
    GETBYTES(entries_total_size, sampleDurationEntries);
    if (err)
        BAILWITHERROR(MP4BadDataErr); /* index table must be entire */

#ifndef CPU_BIG_ENDIAN
    reverse_endian_u32((u32*)self->sampleDurationEntries,
                       entry_cnt * 2); /* each entry has 2 uint32 fields */
#endif

    entries = (sttsEntryPtr)self->sampleDurationEntries;
    for (i = 0; i < entry_cnt; i++) {
        sampleCount = entries->sampleCount;
        sampleDuration = entries->sampleDuration;
        totalSampleCount += sampleCount;
        toatalDuration += (u64)sampleCount * sampleDuration;
        // MP4MSG("stts: entry %ld, sample count %ld, duration %ld\n", entryCountLoaded,
        // sampleCount, sampleDuration);
        entryCountLoaded++;

        entries++;
    }

    /* allocate temp buffer & scan the rest of the table to get the total duration. */
    nb_entires_to_read = self->entryCount - entry_cnt;
    if (nb_entires_to_read) {
        tmp_buffer = (u64*)MP4LocalCalloc(STTS_TAB_MAX_SIZE, sizeof(u64));
        TESTMALLOC(tmp_buffer)
    }
    while (nb_entires_to_read) {
        if (nb_entires_to_read > STTS_TAB_MAX_SIZE)
            entry_cnt = STTS_TAB_MAX_SIZE;
        else
            entry_cnt = nb_entires_to_read;
        // MP4MSG("stts: count of entries loaded %ld, left %ld, to load %ld\n", entryCountLoaded,
        // nb_entires_to_read, entry_cnt);
        entries_total_size = entry_cnt * sizeof(u64);
        err = inputStream->readData(inputStream, entries_total_size, tmp_buffer, NULL);
        if (err)
            BAILWITHERROR(MP4BadDataErr); /* index table must be entire */
        self->bytesRead += entries_total_size;

#ifndef CPU_BIG_ENDIAN
        reverse_endian_u32((u32*)tmp_buffer, entry_cnt * 2);
#endif

        entries = (sttsEntryPtr)tmp_buffer;
        for (i = 0; i < entry_cnt; i++) {
            sampleCount = entries->sampleCount;
            sampleDuration = entries->sampleDuration;
            totalSampleCount += sampleCount;
            toatalDuration += (u64)sampleCount * sampleDuration;
            // MP4MSG("stts: entry %ld, sample count %ld, duration %ld\n", entryCountLoaded,
            // sampleCount, sampleDuration);
            entryCountLoaded++;

            entries++;
        }
        nb_entires_to_read -= entry_cnt;
    }

    self->totalDuration = toatalDuration;
    MP4MSG("stts: total sample count %d, total duration %lld\n", totalSampleCount,
           self->totalDuration);

bail:
    TEST_RETURN(err);

    if (tmp_buffer) {
        MP4LocalFree(tmp_buffer);
        tmp_buffer = NULL;
    }

#ifdef FSL_MP4_PARSER_DBG_TIME
#if defined(__WINCE) || defined(WIN32)
    end_time = timeGetTime();
    MP4DbgLog("stts %ld ms\n", end_time - start_time);
#else
    gettimeofday(&end_time, &time_zone);
    if (end_time.tv_usec >= start_time.tv_usec) {
        MP4DbgLog("stts: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec,
                  end_time.tv_usec - start_time.tv_usec);
    } else {
        MP4DbgLog("stts: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec - 1,
                  end_time.tv_usec + 1000000000 - start_time.tv_usec);
    }
#endif
#endif

    return err;
}

MP4Err MP4CreateTimeToSampleAtom(MP4TimeToSampleAtomPtr* outAtom) {
    MP4Err err;
    MP4TimeToSampleAtomPtr self;

    self = (MP4TimeToSampleAtomPtr)MP4LocalCalloc(1, sizeof(MP4TimeToSampleAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->type = MP4TimeToSampleAtomType;
    self->name = "time to sample";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->findSamples = findSamples;
    self->getTimeForSampleNumber = getTimeForSampleNumber;
    self->getTotalDuration = getTotalDuration;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
