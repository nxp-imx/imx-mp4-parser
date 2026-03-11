/*
 ***********************************************************************
 * Copyright (c) 2005-2013, Freescale Semiconductor, Inc.
 * Copyright 2017, 2024, 2026 NXP
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
    $Id: SampleSizeAtom.c,v 1.2 2000/05/17 08:01:30 francesc Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"
#include "MP4TableLoad.h"

/* Max table size, in number of entries. For stsz, stco, stss, each entry is 4 bytes (u32).  */
#define STSZ_TAB_MAX_SIZE 18000  // 108000   /* Defualt 108000 (X 4 bytes). 1 hour for 30fps video*/

#define STSZ_TAB_OVERLAP_SIZE (STSZ_TAB_MAX_SIZE / 10)
/* Overlap size of two loading, in number of entries. */
#define STSZ_TAB_MAX_MALLOC_SIZE (200 * 1024 * 1024)

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4SampleSizeAtomPtr self;
    err = MP4NoErr;
    self = (MP4SampleSizeAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->sizes) {
        MP4LocalFree(self->sizes);
        self->sizes = NULL;
    }
    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err getSampleSize(MP4AtomPtr s, u32 sampleNumber, u32* outSize) {
    MP4Err err;
    MP4SampleSizeAtomPtr self = (MP4SampleSizeAtomPtr)s;

    err = MP4NoErr;
    if ((self == NULL) || (outSize == NULL) || (sampleNumber > self->sampleCount) ||
        (sampleNumber == 0))
        BAILWITHERROR(MP4BadParamErr)

    if (self->sampleSize) /* samples are the same size */
    {
        *outSize = self->sampleSize;
    } else /* samples have different sizes */
    {
        if (0 == self->sampleCount)
            BAILWITHERROR(MP4_ERR_EMTRY_TRACK)

        if (self->tableSize < self->sampleCount) /* only part of table in memory */
        {
            u32 target_entry_idx = sampleNumber - 1; /* only assure this sample loaded */
            u32 first_entry_idx = self->first_entry_idx;

            if ((target_entry_idx < first_entry_idx) ||
                ((target_entry_idx - first_entry_idx) >=
                 STSZ_TAB_MAX_SIZE)) { /* load new table section for the target sample */
                err = load_new_entry_u32(self->inputStream, target_entry_idx, self->sampleCount,
                                         self->tableSize, STSZ_TAB_OVERLAP_SIZE,
                                         self->tab_file_offset, self->sizes,
                                         &self->first_entry_idx);

                if (err)
                    BAILWITHERROR(MP4BadDataErr)
            }
            /* compensate sample number */
            sampleNumber = sampleNumber - self->first_entry_idx;
        }

        *outSize = self->sizes[sampleNumber - 1];
    }
bail:
    TEST_RETURN(err);

    return err;
}

/*
Amanda: we must make sure both the starting sample in chunk & target sample are both loaded.
In other word the overlap must be large enough for the lagest chunk ! Hard to assure.
And the jitter may happen.
Jitter means target sample and starting sample of the chunk can not be loaded at the same
possiblity. Possibilty is low assumeing overlap size bigger enough than chunk size*/
static MP4Err getSampleSizeAndOffset(MP4AtomPtr s, u32 sampleNumber, u32* outSize,
                                     u32 startingSampleNumber, u32* outOffsetSize) {
    MP4Err err;
    MP4SampleSizeAtomPtr self = (MP4SampleSizeAtomPtr)s;

    err = MP4NoErr;
    if ((self == NULL) || (outSize == NULL) || (sampleNumber > self->sampleCount) ||
        (sampleNumber == 0))
        // BAILWITHERROR( MP4BadParamErr )
        BAILWITHERROR(MP4EOF)
    if ((startingSampleNumber == 0) || (startingSampleNumber > sampleNumber) ||
        (outOffsetSize == NULL))
        BAILWITHERROR(MP4BadParamErr)

    if (self->sampleSize) /* samples are the same size */
    {
        *outSize = self->sampleSize;
        *outOffsetSize = (sampleNumber - startingSampleNumber) * self->sampleSize;
    } else /* samples have different sizes */
    {
        u32 i;
        LONGLONG offset = 0; /* sample offset within the chunk, in bytes */

        if (0 == self->sampleCount)
            BAILWITHERROR(MP4_ERR_EMTRY_TRACK)

        if (self->tableSize < self->sampleCount) {
            /* only part of table in memory.
            we must make sure both start sample of the chunk & target sample are in the memory.*/
            u32 start_entry_idx = startingSampleNumber - 1;
            u32 target_entry_idx = sampleNumber - 1;
            u32 first_entry_idx = self->first_entry_idx; /* 1st entry index loaded in memory */
            int jitter_found = 0;

            if ((target_entry_idx - start_entry_idx) >= STSZ_TAB_MAX_SIZE) {
                /* the chunk is too huge. record file offset of the first entry
                 * in memory table. */
                /* load the target sample to memory first */
                if ((target_entry_idx < first_entry_idx) ||
                    ((target_entry_idx - first_entry_idx) >=
                     STSZ_TAB_MAX_SIZE)) { /* load new table section for the target sample */
                    err = load_new_entry_u32(self->inputStream, target_entry_idx, self->sampleCount,
                                             self->tableSize, STSZ_TAB_OVERLAP_SIZE,
                                             self->tab_file_offset, self->sizes,
                                             &self->first_entry_idx);

                    if (err)
                        BAILWITHERROR(MP4BadDataErr)
                }

                /* update the file offset of the first entry in table */
                if ((first_entry_idx != self->first_entry_idx) || /* table is reloaded */
                    (startingSampleNumber != self->start_sample_number) /* chunk is changed */) {
                    u32 tmp_size;
                    first_entry_idx = self->first_entry_idx; /* update 1st loaded entry index */
                                                             // fix ENGR00229993
                    for (i = startingSampleNumber; i <= first_entry_idx; i++)  // target_entry_idx
                    {
                        err = getSampleSize(s, i, &tmp_size);
                        if (err)
                            BAILWITHERROR(MP4BadDataErr);
                        offset += tmp_size;
                    }
                    self->start_sample_number = startingSampleNumber;
                    self->first_entry_offset = offset;
                }

                /* if first entry index changed, adjust offset */
                if (first_entry_idx != self->first_entry_idx) {
                    err = load_new_entry_u32(self->inputStream, target_entry_idx, self->sampleCount,
                                             self->tableSize, STSZ_TAB_OVERLAP_SIZE,
                                             self->tab_file_offset, self->sizes,
                                             &self->first_entry_idx);

                    if (err)
                        BAILWITHERROR(MP4BadDataErr)

                    // fix ENGR00229993
                    // target_entry_idx is already in current table
                    if (first_entry_idx != self->first_entry_idx) {
                        if (first_entry_idx > self->first_entry_idx) {
                            u32 dwEntryNum = first_entry_idx - self->first_entry_idx;
                            if (dwEntryNum > STSZ_TAB_MAX_SIZE) {
                                //     printf("==== unexpect status, dwEntryNum(%d) >
                                //     STSZ_TAB_MAX_SIZE(%d)\n", (int)dwEntryNum,
                                //     (int)STSZ_TAB_MAX_SIZE);
                                BAILWITHERROR(MP4BadDataErr);
                            }

                            for (i = 0; i < dwEntryNum; i++) offset -= self->sizes[i];
                        } else {
                            u32* pdwSampleSize = NULL;
                            u32 dwEntryNum = self->first_entry_idx - first_entry_idx;
                            u64 qwOffset = self->tab_file_offset + first_entry_idx * sizeof(u32);

                            // printf("==== enter first_entry_idx(%d) < self->first_entry_idx(%d)
                            // case\n",
                            //   (int)first_entry_idx, (int)self->first_entry_idx);

                            pdwSampleSize = (u32*)MP4LocalMalloc(dwEntryNum * sizeof(u32));
                            if (pdwSampleSize == NULL)
                                BAILWITHERROR(MP4NoMemoryErr)

                            err = load_entries_u32(self->inputStream, qwOffset, dwEntryNum,
                                                   pdwSampleSize);
                            if (err)
                                BAILWITHERROR(MP4BadDataErr)

                            for (i = 0; i < dwEntryNum; i++) offset += pdwSampleSize[i];

                            MP4LocalFree(pdwSampleSize);
                        }

                        first_entry_idx = self->first_entry_idx;
                        self->first_entry_offset = offset;
                    }
                }

                /* all the index table in memory or target entry already loaded. */
                *outSize = self->sizes[target_entry_idx - first_entry_idx];
                offset = self->first_entry_offset;
                for (i = 0; i < target_entry_idx - first_entry_idx; i++) offset += self->sizes[i];
                *outOffsetSize = offset;
                err = MP4NoErr;
                goto bail;
            }

            if ((start_entry_idx < first_entry_idx) ||
                ((start_entry_idx - first_entry_idx) >=
                 STSZ_TAB_MAX_SIZE)) { /* start sample NOT in memory, load it first*/
                err = load_new_entry_u32(self->inputStream, start_entry_idx, self->sampleCount,
                                         self->tableSize, STSZ_TAB_OVERLAP_SIZE,
                                         self->tab_file_offset, self->sizes,
                                         &self->first_entry_idx);

                if (err)
                    BAILWITHERROR(MP4BadDataErr)
                first_entry_idx = self->first_entry_idx; /* update 1st loaded entry index */

                /* jitter will happen?  */
                if ((target_entry_idx - first_entry_idx) >=
                    STSZ_TAB_MAX_SIZE) { /* target sample not loaded yet */
                    jitter_found = 1;
                }
            }

            if (!jitter_found) {
                /* now start sample of the chunk must be in memory.
                Then, target sample in memory? */
                if ((target_entry_idx < first_entry_idx) ||
                    ((target_entry_idx - first_entry_idx) >=
                     STSZ_TAB_MAX_SIZE)) { /* load new table section for the target sample */
                    ;
                    err = load_new_entry_u32(self->inputStream, target_entry_idx, self->sampleCount,
                                             self->tableSize, STSZ_TAB_OVERLAP_SIZE,
                                             self->tab_file_offset, self->sizes,
                                             &self->first_entry_idx);

                    if (err)
                        BAILWITHERROR(MP4BadDataErr)
                    first_entry_idx = self->first_entry_idx; /* update 1st loaded entry index */

                    /* jitter will happen? */
                    if (start_entry_idx <
                        first_entry_idx) { /* start entry of the chunk is not in memory now.*/
                        jitter_found = 1;
                    }
                }
            } /* otherwise, meaningless to load the target sample. The possibility for it to load
                 with starting sample is very low. */

            if (!jitter_found) {
                /* both samples are loaded. compensate sample number */
                startingSampleNumber = startingSampleNumber - self->first_entry_idx;
                sampleNumber = sampleNumber - self->first_entry_idx;
            } else { /* get sample size one by one. Not care performance here because this
                        possibility is almost zero.*/
                u32 tmp_size;
                err = getSampleSize(s, sampleNumber, outSize);
                if (err)
                    BAILWITHERROR(MP4BadDataErr);

                for (i = startingSampleNumber; i < sampleNumber; i++) {
                    err = getSampleSize(s, i, &tmp_size);
                    if (err)
                        BAILWITHERROR(MP4BadDataErr);
                    offset += tmp_size;
                }
                *outOffsetSize = offset;
                return MP4NoErr;
            }
        }

        /* all the index table in memory or target entry already loaded. */
        *outSize = self->sizes[sampleNumber - 1];
        for (i = startingSampleNumber - 1; i < sampleNumber - 1; i++) offset += self->sizes[i];
        *outOffsetSize = offset;
    }
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err finishScan(MP4AtomPtr s) {
    MP4Err err = MP4NoErr;
    MP4SampleSizeAtomPtr self = (MP4SampleSizeAtomPtr)s;
    MP4InputStreamPtr inputStream = self->inputStream;

    u32* tmp_buffer = NULL;     /* temp bufffer to scan table if it can not load in one time */
    u32 nb_entires_to_read = 0; /* number of entries read in scanning the table */
    u32 entry_cnt;              /* number of entries in one loading */
    u32 i;
    u32* entries;

    u32 sampleSize;
    u32 maxSampleSize;
    LONGLONG totalBytes;
    s64 fileOffset;
    u32 sampleCount;

    if (self->scanFinished)
        goto bail;

    maxSampleSize = self->maxSampleSize;
    totalBytes = self->totalBytes;

    if (self->sampleCount <= STSZ_TAB_MAX_SIZE)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    /* allocate temp buffer & scan the rest of the table to get max sample size & total bytes. */
    nb_entires_to_read = self->sampleCount - STSZ_TAB_MAX_SIZE;
    MP4MSG("stsz %d entries left\n", nb_entires_to_read);
    if (nb_entires_to_read) {
        tmp_buffer = (u32*)MP4LocalCalloc(STSZ_TAB_MAX_SIZE, sizeof(u32));
        TESTMALLOC(tmp_buffer)
    }

    sampleCount = STSZ_TAB_MAX_SIZE;
    fileOffset = self->tab_file_offset + STSZ_TAB_MAX_SIZE * sizeof(u32);
    while (nb_entires_to_read) {
        if (nb_entires_to_read > STSZ_TAB_MAX_SIZE)
            entry_cnt = STSZ_TAB_MAX_SIZE;
        else
            entry_cnt = nb_entires_to_read;

        err = load_entries_u32(inputStream, fileOffset, entry_cnt, tmp_buffer);
        if (err)
            goto bail;

        entries = tmp_buffer;
        for (i = 0; i < entry_cnt; i++) {
            sampleSize = *entries;
            totalBytes += sampleSize;
            // MP4MSG("samp %d, size %d\n", sampleCount, sampleSize);

            if (maxSampleSize < sampleSize) {
                if (MP4_MAX_SAMPLE_SIZE < sampleSize) {
                    MP4MSG("samp %d, invalid size %d (2)\n", sampleCount, sampleSize);
                    BAILWITHERROR(MP4_ERR_WRONG_SAMPLE_SIZE)
                }
                maxSampleSize = sampleSize;
            }
            entries++;
            sampleCount++;
        }

        nb_entires_to_read -= entry_cnt;
        fileOffset += entry_cnt * sizeof(u32);
    }

    self->maxSampleSize = maxSampleSize;
    self->totalBytes = totalBytes;
    self->scanFinished = TRUE;

bail:
    if (tmp_buffer)
        MP4LocalFree(tmp_buffer);
    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    MP4SampleSizeAtomPtr self = (MP4SampleSizeAtomPtr)s;

#ifdef FSL_MP4_PARSER_DBG_TIME
#if defined(__WINCE) || defined(WIN32)
    u32 start_time = timeGetTime();
    u32 end_time;
    u32 start_time_max_size;
#else
    struct timezone time_zone;
    struct timeval start_time;
    struct timeval end_time;
    struct timeval start_time_max_size;
    gettimeofday(&start_time, &time_zone);
#endif
#endif

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32(sampleSize);
    GET32(sampleCount);

    if (0 > (s32)self->sampleCount)
        BAILWITHERROR(MP4BadDataErr)
    if (0 == self->sampleCount) { /* empty table means this track is empty */
        MP4MSG("\t Warning!empty stsz\n");
        if (self->size > self->bytesRead) {
            uint64 bytesToSkip = self->size - self->bytesRead;
            SKIPBYTES_FORWARD(bytesToSkip);
        }
        self->scanFinished = TRUE;
        goto bail;
    }

    // MP4MSG("stsz size %lld, sample size %d, sample count %d\n", self->size, self->sampleSize,
    // self->sampleCount);

    if (self->sampleSize) /* Samples have the same size */
    {
        MP4MSG("All samples has same size %d\n", self->sampleSize);
        self->maxSampleSize = self->sampleSize;
        self->totalBytes = self->sampleSize * self->sampleCount;
        self->scanFinished = TRUE;
    } else /* Samples have different sizes. Load in the table */
    {
        u32 entry_cnt;          /* number of entries in one loading */
        u32 entries_total_size; /* bytes of entries in one loading */
        MP4FileMappingInputStreamPtr stream = (MP4FileMappingInputStreamPtr)inputStream;
        u32 i;
        u32* entries;
        u32 sampleSize;
        u32 maxSampleSize = 0;
        LONGLONG totalBytes = 0;

        self->inputStream = inputStream;
        self->tab_file_offset = /* back table's file offset */
                MP4LocalGetCurrentFilePos((FILE*)(stream->ptr), stream->mapping->parser_context);

        entry_cnt = self->sampleCount;

        if (!((STSZ_TAB_MAX_SIZE > STSZ_TAB_OVERLAP_SIZE) && (0 < STSZ_TAB_MAX_SIZE) &&
              (0 <= STSZ_TAB_OVERLAP_SIZE))) {
            // MP4DbgLog("stsz: Invalid table size %ld & overlap size %ld\n", STSZ_TAB_MAX_SIZE,
            // STSZ_TAB_OVERLAP_SIZE);
            BAILWITHERROR(MP4NotImplementedErr);
        }

        if ((u64)entry_cnt * sizeof(u32) > self->size - self->bytesRead)
            BAILWITHERROR(MP4BadDataErr);

        if ((u64)entry_cnt * sizeof(u32) > STSZ_TAB_MAX_MALLOC_SIZE)
            BAILWITHERROR(MP4BadDataErr);

        if ((STSZ_TAB_MAX_SIZE < entry_cnt) &&
            (inputStream->stream_flags & memOptimization_flag)) /* only load part of the entries */
            entry_cnt = STSZ_TAB_MAX_SIZE;

        // MP4DbgLog("stsz entry count %ld, table size %ld\n", self->sampleCount, entry_cnt);
        self->tableSize = entry_cnt;

        if (entry_cnt == self->sampleCount)
            self->scanFinished = TRUE;

        self->sizes = (u32*)MP4LocalCalloc(entry_cnt, sizeof(u32));
        TESTMALLOC(self->sizes)

        /* read the whole 'stsz' table into memory. Data in file are big endian */
        entries_total_size = entry_cnt * sizeof(u32);
        GETBYTES(entries_total_size, sizes);
        if (err)
            BAILWITHERROR(MP4BadDataErr); /* index table must be entire */

#ifndef CPU_BIG_ENDIAN
        reverse_endian_u32(self->sizes, entry_cnt);
#endif

        entries = self->sizes;
        for (i = 0; i < entry_cnt; i++) {
            sampleSize = *entries;
            totalBytes += sampleSize;
            // MP4MSG("samp %d, size %d\n", i, sampleSize);

            if (maxSampleSize < sampleSize) {
                if (MP4_MAX_SAMPLE_SIZE < sampleSize) {
                    /* still permit the atom tree to establish for cleanup later */
                    MP4MSG("samp %d, invalid size %d (1)\n", i, sampleSize);
                    // BAILWITHERROR( MP4_ERR_WRONG_SAMPLE_SIZE)
                }
                maxSampleSize = sampleSize;
            }
            entries++;
        }

#ifdef FSL_MP4_PARSER_DBG_TIME
#if defined(__WINCE) || defined(WIN32)
        start_time_max_size = timeGetTime();
#else
        gettimeofday(&start_time_max_size, &time_zone);
#endif
#endif

        self->maxSampleSize = maxSampleSize;
        self->totalBytes = totalBytes;
        // MP4MSG("max sample size %ld bytes, total %lld bytes\n", self->maxSampleSize,
        // self->totalBytes);
    }

    if (self->size > self->bytesRead) {
        u64 bytesToSkip = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(bytesToSkip);
    }

bail:
    TEST_RETURN(err);
    if (err) {
        if (self && self->sizes) {
            MP4LocalFree(self->sizes);
            self->sizes = NULL;
        }
    }

#ifdef FSL_MP4_PARSER_DBG_TIME
#if defined(__WINCE) || defined(WIN32)
    end_time = timeGetTime();
    MP4DbgLog("stsz %ld ms\n", end_time - start_time);
    MP4DbgLog("stsz %ld ms to get max sample size\n", end_time - start_time_max_size);
#else
    gettimeofday(&end_time, &time_zone);
    if (end_time.tv_usec >= start_time.tv_usec) {
        MP4DbgLog("stsz: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec,
                  end_time.tv_usec - start_time.tv_usec);
    } else {
        MP4DbgLog("stsz: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec - 1,
                  end_time.tv_usec + 1000000000 - start_time.tv_usec);
    }

    if (0 == self->sampleSize) /* non-fixed sample size */
    {
        if (end_time.tv_usec >= start_time_max_size.tv_usec) {
            MP4DbgLog("stsz to get max sample size: %ld sec, %ld us\n",
                      end_time.tv_sec - start_time_max_size.tv_sec,
                      end_time.tv_usec - start_time_max_size.tv_usec);
        } else {
            MP4DbgLog("stsz to get max sample size: %ld sec, %ld us\n",
                      end_time.tv_sec - start_time_max_size.tv_sec - 1,
                      end_time.tv_usec + 1000000000 - start_time_max_size.tv_usec);
        }
    }

#endif
#endif

    return err;
}

MP4Err MP4CreateSampleSizeAtom(MP4SampleSizeAtomPtr* outAtom) {
    MP4Err err;
    MP4SampleSizeAtomPtr self;

    self = (MP4SampleSizeAtomPtr)MP4LocalCalloc(1, sizeof(MP4SampleSizeAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4SampleSizeAtomType;
    self->name = "sample size";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->getSampleSize = getSampleSize;
    self->getSampleSizeAndOffset = getSampleSizeAndOffset;
    self->finishScan = finishScan;
    self->start_sample_number = 0;
    self->first_entry_offset = 0;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
