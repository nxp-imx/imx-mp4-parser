/*
 ***********************************************************************
 * Copyright (c) 2005-2013, Freescale Semiconductor, Inc.
 * Copyright 2017, 2026 NXP
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
    $Id: SyncSampleAtom.c,v 1.3 2000/07/25 14:05:40 francesc Exp $
*/

#include <stdlib.h>
#include "MP4Atoms.h"
#include "MP4TableLoad.h"

/* Max table size, in number of entries. For stsz, stss, stss, each entry is 4 bytes (u32).  */
#define STSS_TAB_MAX_SIZE \
    3600 /* Defualt 3600. Half an hour, assuming a key frame per 0.5 second (normal case)*/

#define STSS_TAB_OVERLAP_SIZE (STSS_TAB_MAX_SIZE / 10)
/* Overlap size of two loading, in number of entries.
NOTE: stss overlap can not be 0 because we need check neighbouring entries
to find the nearest sync point.*/

#define STSS_TAB_MAX_MALLOC_SIZE (200 * 1024 * 1024)
#define STSS_CACHE_SEARCH_RESULT /* cache the previous search result to speed up next search */

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4SyncSampleAtomPtr self;
    err = MP4NoErr;
    self = (MP4SyncSampleAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->sampleNumbers) {
        MP4LocalFree(self->sampleNumbers);
        self->sampleNumbers = NULL;
    }
    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

/***************************************************************************************
Function: isSyncSample
Find the nearest sync sample (previous or next) to the target sample.
For the last sample of the stream, find the nearest previous sync sample.

Arguments:
sampleNumber, IN, 1-based.
outSync, OUT, pointer to the nearest sample number
***************************************************************************************/
static MP4Err isSyncSample(MP4AtomPtr s, u32 sampleNumber, u32* outSync) {
    MP4Err err = MP4NoErr;
    u32 i;
    u32* p;
    u32 syncSample;
    u32 next_syncSample;
    u32 sample_next_diff;
    u32 sample_pre_diff;

    MP4SyncSampleAtomPtr self = (MP4SyncSampleAtomPtr)s;
    u32 total_entry_count = 0;
    u32 cached_sync_sample_number = 0;

    if ((self == NULL) || (outSync == NULL) || ((s32)sampleNumber <= 0)) {
        BAILWITHERROR(MP4BadParamErr);
    }

    total_entry_count = self->entryCount;
    cached_sync_sample_number = self->cached_sync_sample_number;

    *outSync = 0;

    if (!self->entryCount) {
        *outSync = 1; /* no sync sample available ! Assume only the 1st sample is sync */
        return err;
    }

    i = 0; /* by default, searching from the 1st entry */
#ifdef STSS_CACHE_SEARCH_RESULT
    /* The whole index table alreay loaded in memory. One more extra entry at the end with value 0.
     */
    if (sampleNumber >= cached_sync_sample_number)
        i = self->cached_entry_idx; /* from the cached entry */
#endif

    if (self->tableSize < total_entry_count) /* only part of table loaded in memory. */
    {
        u32 target_entry_idx;
        u32 first_entry_idx = self->first_entry_idx;
        u32* entries_loaded = self->sampleNumbers;
        u32 first_entry_loaded; /* first entry data loaded */

        /* Check 1st entry loaded, to speed up search on seeking, avoid reloading table */
        if (i == self->cached_entry_idx)
            syncSample = cached_sync_sample_number;
        else {
            // assert(0 == i);
            first_entry_loaded = entries_loaded[0];
            if (sampleNumber >= first_entry_loaded) {
                i = first_entry_idx;
                syncSample = first_entry_loaded; /* start from the 1st loaded entry */
            } else {                             /* the actual 1st entry must be loaded at first */

                target_entry_idx = i;
                if ((target_entry_idx < first_entry_idx) ||
                    ((target_entry_idx - first_entry_idx) >=
                     STSS_TAB_MAX_SIZE)) { /* load new table section for the target entry */
                    err = load_new_entry_u32(self->inputStream, target_entry_idx, total_entry_count,
                                             self->tableSize, STSS_TAB_OVERLAP_SIZE,
                                             self->tab_file_offset, entries_loaded,
                                             &self->first_entry_idx);

                    if (err)
                        BAILWITHERROR(MP4BadDataErr)
                    first_entry_idx = self->first_entry_idx;
                }
                syncSample = entries_loaded[target_entry_idx -
                                            first_entry_idx]; /* compensate entry number */
            }
        }

        if (sampleNumber == syncSample) /* 1st or cached entry exactly match */
        {
            *outSync = sampleNumber;
#ifdef STSS_CACHE_SEARCH_RESULT
            self->cached_entry_idx = i;
            self->cached_sync_sample_number = syncSample; /* cache the result */
#endif
            // MP4DbgLog("samp %ld, sync samp %ld\n", sampleNumber, *outSync);
            return MP4NoErr;
        }

        i++;

        for (; i < total_entry_count; i++) {
            target_entry_idx = i;
            if ((target_entry_idx < first_entry_idx) ||
                ((target_entry_idx - first_entry_idx) >=
                 STSS_TAB_MAX_SIZE)) { /* load new table section for the target entry */
                err = load_new_entry_u32(self->inputStream, target_entry_idx, total_entry_count,
                                         self->tableSize, STSS_TAB_OVERLAP_SIZE,
                                         self->tab_file_offset, entries_loaded,
                                         &self->first_entry_idx);

                if (err)
                    BAILWITHERROR(MP4BadDataErr)
                first_entry_idx = self->first_entry_idx;
            }
            next_syncSample = entries_loaded[target_entry_idx -
                                             first_entry_idx]; /* compensate entry number */

            if (sampleNumber == next_syncSample) /* next entry exactly match */
            {
                *outSync = next_syncSample;
#ifdef STSS_CACHE_SEARCH_RESULT
                self->cached_entry_idx = i; /* cache the result */
                self->cached_sync_sample_number = next_syncSample;
#endif
                break;
            } else if ((sampleNumber < next_syncSample) &&
                       (sampleNumber >
                        syncSample)) /* find the nearest sync sample between current & next one */
            {
                sample_next_diff = next_syncSample - sampleNumber;
                sample_pre_diff = sampleNumber - syncSample;
                if (sample_next_diff > sample_pre_diff)
                    *outSync = syncSample;
                else
                    *outSync = next_syncSample;
#ifdef STSS_CACHE_SEARCH_RESULT
                self->cached_entry_idx = i - 1; /* cache the previous sync point */
                self->cached_sync_sample_number = syncSample;
#endif
                break;
            }
            syncSample = next_syncSample;
        }
        if (*outSync ==
            0) /* this sample is the last sample in stream, use the nearest previous sync sample */
        {
            *outSync = syncSample;
#ifdef STSS_CACHE_SEARCH_RESULT
            self->cached_entry_idx = total_entry_count - 1; /* last entry idx */
            self->cached_sync_sample_number = syncSample;
#endif
        }
        // MP4DbgLog("samp %ld, sync samp %ld\n", sampleNumber, *outSync);
        return MP4NoErr;
    }

    /* All table entries are loaded in memory */
    p = self->sampleNumbers + i;
    syncSample = *p;

    if (sampleNumber == syncSample) /* 1st or cached entry exactly match */
    {
        *outSync = sampleNumber;
#ifdef STSS_CACHE_SEARCH_RESULT
        self->cached_entry_idx = i; /* cache the result */
        self->cached_sync_sample_number = syncSample;
#endif
        // MP4DbgLog("samp %ld, sync samp %ld\n", sampleNumber, *outSync);
        return MP4NoErr;
    }
    i++;
    p++;
    for (; i < total_entry_count; i++) {
        next_syncSample = *p;
        if (sampleNumber == next_syncSample) /* next entry exactly match */
        {
            *outSync = next_syncSample;
#ifdef STSS_CACHE_SEARCH_RESULT
            self->cached_entry_idx = i; /* cache the result */
            self->cached_sync_sample_number = next_syncSample;
#endif
            break;
        } else if ((sampleNumber < next_syncSample) &&
                   (sampleNumber >
                    syncSample)) /* find the nearest sync sample between current & next one */
        {
            sample_next_diff = next_syncSample - sampleNumber;
            sample_pre_diff = sampleNumber - syncSample;
            if (sample_next_diff > sample_pre_diff)
                *outSync = syncSample;
            else
                *outSync = next_syncSample;
#ifdef STSS_CACHE_SEARCH_RESULT
            self->cached_entry_idx = i - 1; /* cache the previous sync point */
            self->cached_sync_sample_number = syncSample;
#endif
            break;
        }
        p++;
        syncSample = next_syncSample;
    }

    if (*outSync ==
        0) /* this sample is the last sample in stream, use the nearest previous sync sample */
    {
        *outSync = syncSample;
#ifdef STSS_CACHE_SEARCH_RESULT
        self->cached_entry_idx = total_entry_count - 1; /* last entry idx */
        self->cached_sync_sample_number = syncSample;
#endif
    }

    // MP4DbgLog("samp %ld, sync samp %ld\n", sampleNumber, *outSync);

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err nextSyncSample(MP4AtomPtr s, u32 sampleNumber, u32* outSync, s32 forward) {
    MP4Err err = MP4NoErr;
    s32 i;

    MP4SyncSampleAtomPtr self = (MP4SyncSampleAtomPtr)s;
    u32 total_entry_count = 0;

    if ((self == NULL) || (outSync == NULL) || ((s32)sampleNumber <= 0)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    total_entry_count = self->entryCount;

    if (!self->entryCount) {
        if (forward == TRUE) {
            if (sampleNumber > 1) {
                err = PARSER_EOS;
            }
        } else {
            if (sampleNumber == 1)
                err = PARSER_BOS;
        }
        *outSync = 1; /* no sync sample available ! Assume only the 1st sample is sync */
        return err;
    }

    *outSync = 0;
    if (forward == TRUE) {
        if (self->tableSize < total_entry_count) /* only part of table loaded in memory. */
        {
            u32 first_sampleNumber, last_sampleNumber;
            s32 valid_entry;

            for (;;) {
                first_sampleNumber = self->sampleNumbers[0];
                if (self->first_entry_idx + STSS_TAB_MAX_SIZE <= total_entry_count) {
                    valid_entry = STSS_TAB_MAX_SIZE;
                    last_sampleNumber = self->sampleNumbers[STSS_TAB_MAX_SIZE - 1];
                } else {
                    valid_entry = total_entry_count - self->first_entry_idx;
                    last_sampleNumber = self->sampleNumbers[valid_entry - 1];
                }

                if (self->first_entry_idx + valid_entry == total_entry_count)
                    /* the last table part loaded */
                    break;

                if (sampleNumber >= first_sampleNumber && sampleNumber <= last_sampleNumber)
                    /* sample is in this table */
                    break;

                /* load new table section for the target entry */
                {
                    u32 target_entry_idx = self->first_entry_idx + STSS_TAB_MAX_SIZE;
                    u32* entries_loaded = self->sampleNumbers;

                    err = load_new_entry_u32(self->inputStream, target_entry_idx, total_entry_count,
                                             self->tableSize, 1, self->tab_file_offset,
                                             entries_loaded, &self->first_entry_idx);

                    if (err)
                        BAILWITHERROR(MP4BadDataErr)
                }
            }

            for (i = 0; i < valid_entry; i++) {
                if (self->sampleNumbers[i] >= sampleNumber) {
                    *outSync = self->sampleNumbers[i];
                    break;
                }
            }

            if (*outSync == 0) /* this sample is the last sample in stream, use the nearest previous
                                  sync sample */
            {
                *outSync = self->sampleNumbers[valid_entry - 1];
                err = PARSER_EOS;
            }
        }

        else {
            for (i = 0; i < (s32)total_entry_count; i++) {
                if (self->sampleNumbers[i] >= sampleNumber) {
                    // MP4MSG("stss, sample number %ld, sync sample number %ld (forward)\n",
                    // sampleNumber, self->sampleNumbers[i]);
                    *outSync = self->sampleNumbers[i];
                    break;
                }
            }

            if (*outSync == 0) /* this sample is the last sample in stream, use the nearest previous
                                  sync sample */
            {
                *outSync = self->sampleNumbers[total_entry_count - 1];
                err = PARSER_EOS;
            }
        }

    } else {
        if (self->tableSize < total_entry_count) /* only part of table loaded in memory. */
        {
            u32 first_sampleNumber, last_sampleNumber;
            s32 valid_entry;

            for (;;) {
                first_sampleNumber = self->sampleNumbers[0];
                if (self->first_entry_idx + STSS_TAB_MAX_SIZE <= total_entry_count) {
                    valid_entry = STSS_TAB_MAX_SIZE;
                    last_sampleNumber = self->sampleNumbers[STSS_TAB_MAX_SIZE - 1];
                } else {
                    valid_entry = total_entry_count - self->first_entry_idx;
                    last_sampleNumber = self->sampleNumbers[valid_entry - 1];
                }

                if (sampleNumber > first_sampleNumber && sampleNumber <= last_sampleNumber)
                    /* sample is in this table */
                    break;

                if (self->first_entry_idx == 0)
                    break;

                /* load new table section for the target entry */
                {
                    u32 target_entry_idx;
                    u32* entries_loaded = self->sampleNumbers;

                    if (self->first_entry_idx > STSS_TAB_MAX_SIZE)
                        target_entry_idx = self->first_entry_idx - STSS_TAB_MAX_SIZE + 1;
                    else
                        target_entry_idx = 0;

                    err = load_new_entry_u32(self->inputStream, target_entry_idx, total_entry_count,
                                             self->tableSize, 0, self->tab_file_offset,
                                             entries_loaded, &self->first_entry_idx);

                    if (err)
                        BAILWITHERROR(MP4BadDataErr)
                }
            }

            for (i = valid_entry - 1; i >= 0; i--) {
                if (self->sampleNumbers[i] < sampleNumber) {
                    *outSync = self->sampleNumbers[i];
                    // MP4MSG("stss, sample number %ld, sync sample number %ld (backward)\n",
                    // sampleNumber, self->sampleNumbers[i]);
                    break;
                }
            }

            if (*outSync == 0) /* this sample is the first sample in stream, use the nearest
                                  previous sync sample */
            {
                *outSync = self->sampleNumbers[0];
                err = PARSER_BOS;
            }
        }

        else {
            for (i = total_entry_count - 1; i >= 0; i--) {
                if (self->sampleNumbers[i] <= sampleNumber) {
                    *outSync = self->sampleNumbers[i];
                    break;
                }
            }

            if (*outSync ==
                0) /* this sample is the first sample in stream, use the nearest next sync sample */
            {
                *outSync = self->sampleNumbers[0];
                err = PARSER_BOS;
            }
        }
    }

    // MP4DbgLog("samp %ld, sync samp %ld\n", sampleNumber, *outSync);

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
    MP4SyncSampleAtomPtr self = (MP4SyncSampleAtomPtr)s;

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

    GET32(entryCount);

    self->inputStream = inputStream;
    self->tab_file_offset = /* back table's file offset */
            MP4LocalGetCurrentFilePos((FILE*)(stream->ptr), stream->mapping->parser_context);

    entry_cnt = self->entryCount;
    if (!entry_cnt) {
        MP4MSG("stts is empty!\n");
        goto bail;
    }

    //----------------------------------
    if (!((STSS_TAB_MAX_SIZE > STSS_TAB_OVERLAP_SIZE) /* Table & overalp size valid? */
          && (0 < STSS_TAB_MAX_SIZE) &&
          (0 < STSS_TAB_OVERLAP_SIZE))) /*NOTE: stss overlap can not be 0 because we need check
                                           neighbouring entry to find the nearest */
    {
        // MP4DbgLog("STSS: Invalid table size %ld & overlap size %ld\n", STSS_TAB_MAX_SIZE,
        // STSS_TAB_OVERLAP_SIZE);
        BAILWITHERROR(MP4NotImplementedErr);
    }

    if ((u64)entry_cnt * sizeof(u32) > self->size - self->bytesRead)
        BAILWITHERROR(MP4BadDataErr);

    if ((u64)entry_cnt * sizeof(u32) > STSS_TAB_MAX_MALLOC_SIZE)
        BAILWITHERROR(MP4BadDataErr);

    if ((STSS_TAB_MAX_SIZE < entry_cnt) &&
        (inputStream->stream_flags & memOptimization_flag)) /* only load part of the entries */
        entry_cnt = STSS_TAB_MAX_SIZE;

    // MP4DbgLog("stss: total entry count %ld, %ld entries in 1st loading\n", self->entryCount,
    // entry_cnt);
    self->tableSize = entry_cnt;

    self->sampleNumbers = (u32*)MP4LocalCalloc(entry_cnt, sizeof(u32));
    TESTMALLOC(self->sampleNumbers)

    entries_total_size = entry_cnt * sizeof(u32);
    GETBYTES(entries_total_size, sampleNumbers); /* read the whole 'stss' table into memory. Data in
                                                    file are big endian */

#ifndef CPU_BIG_ENDIAN /* Adjust endianess. */
    reverse_endian_u32(self->sampleNumbers, entry_cnt);
#endif
    self->cached_entry_idx = 0;
    self->cached_sync_sample_number = self->sampleNumbers[0];

    nb_entires_to_read = self->entryCount - entry_cnt;
    if (nb_entires_to_read) {
        /* skip the part not read */
        u64 bytes_to_skip = nb_entires_to_read * sizeof(u32);
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
        if (self && self->sampleNumbers) {
            MP4LocalFree(self->sampleNumbers);
            self->sampleNumbers = NULL;
        }
    }

#ifdef FSL_MP4_PARSER_DBG_TIME
#if defined(__WINCE) || defined(WIN32)
    end_time = timeGetTime();
    MP4DbgLog("stss %ld ms\n", end_time - start_time);
#else
    gettimeofday(&end_time, &time_zone);
    if (end_time.tv_usec >= start_time.tv_usec) {
        MP4DbgLog("stss: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec,
                  end_time.tv_usec - start_time.tv_usec);
    } else {
        MP4DbgLog("stss: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec - 1,
                  end_time.tv_usec + 1000000000 - start_time.tv_usec);
    }
#endif
#endif

    return err;
}

MP4Err MP4CreateSyncSampleAtom(MP4SyncSampleAtomPtr* outAtom) {
    MP4Err err;
    MP4SyncSampleAtomPtr self;

    self = (MP4SyncSampleAtomPtr)MP4LocalCalloc(1, sizeof(MP4SyncSampleAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4SyncSampleAtomType;
    self->name = "sync sample";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->isSyncSample = isSyncSample;
    self->nextSyncSample = nextSyncSample;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
