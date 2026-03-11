/*
 ***********************************************************************
 * Copyright 2018, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

//#define DEBUG

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"
#include "MP4TableLoad.h"

/* Max table size, in number of entries. For stz2, stco, stss, each entry is 4 bytes (u32).  */
#define STZ2_TAB_MAX_SIZE 18000  // 108000   /* Defualt 108000 (X 4 bytes). 1 hour for 30fps video*/

#define STZ2_TAB_OVERLAP_SIZE (STZ2_TAB_MAX_SIZE / 10)
/* Overlap size of two loading, in number of entries. */
#define STZ2_TAB_MAX_MALLOC_SIZE (200 * 1024 * 1024)

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4CompactSampleSizeAtomPtr self;
    err = MP4NoErr;
    self = (MP4CompactSampleSizeAtomPtr)s;
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
    MP4CompactSampleSizeAtomPtr self = (MP4CompactSampleSizeAtomPtr)s;

    err = MP4NoErr;
    if ((self == NULL) || (outSize == NULL) || (sampleNumber > self->sampleCount) ||
        (sampleNumber == 0))
        BAILWITHERROR(MP4BadParamErr)

    if (0 == self->sampleCount)
        BAILWITHERROR(MP4_ERR_EMTRY_TRACK)

    if (self->tableSize < self->sampleCount) /* only part of table in memory */
    {
        u32 target_entry_idx = sampleNumber - 1; /* only assure this sample loaded */
        u32 first_entry_idx = self->first_entry_idx;

        if ((target_entry_idx < first_entry_idx) ||
            ((target_entry_idx - first_entry_idx) >=
             STZ2_TAB_MAX_SIZE)) { /* load new table section for the target sample */
            err = load_new_entry(self->inputStream, target_entry_idx, self->sampleCount,
                                 self->tableSize, STZ2_TAB_OVERLAP_SIZE, self->tab_file_offset,
                                 self->sizes, &self->first_entry_idx, self->fieldSize);

            if (err)
                BAILWITHERROR(MP4BadDataErr)
        }
        /* compensate sample number */
        sampleNumber = sampleNumber - self->first_entry_idx;
    }

    if (self->fieldSize == 16)
        *outSize = *((u16*)self->sizes + sampleNumber - 1);
    else
        *outSize = self->sizes[sampleNumber - 1];

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
    MP4CompactSampleSizeAtomPtr self = (MP4CompactSampleSizeAtomPtr)s;

    err = MP4NoErr;
    if ((self == NULL) || (outSize == NULL) || (sampleNumber > self->sampleCount) ||
        (sampleNumber == 0))
        // BAILWITHERROR( MP4BadParamErr )
        BAILWITHERROR(MP4EOF)
    if ((startingSampleNumber == 0) || (startingSampleNumber > sampleNumber) ||
        (outOffsetSize == NULL))
        BAILWITHERROR(MP4BadParamErr)

        {
            u32 i;
            LONGLONG offset = 0; /* sample offset within the chunk, in bytes */

            if (0 == self->sampleCount)
                BAILWITHERROR(MP4_ERR_EMTRY_TRACK)

            if (self->tableSize < self->sampleCount) {
                /* only part of table in memory.
                we must make sure both start sample of the chunk & target sample are in the
                memory.*/
                u32 start_entry_idx = startingSampleNumber - 1;
                u32 target_entry_idx = sampleNumber - 1;
                u32 first_entry_idx = self->first_entry_idx; /* 1st entry index loaded in memory */
                int jitter_found = 0;

                if ((target_entry_idx - start_entry_idx) >= STZ2_TAB_MAX_SIZE) {
                    /* the chunk is too huge. record file offset of the first entry
                     * in memory table. */
                    /* load the target sample to memory first */
                    if ((target_entry_idx < first_entry_idx) ||
                        ((target_entry_idx - first_entry_idx) >=
                         STZ2_TAB_MAX_SIZE)) { /* load new table section for the target sample */
                        err = load_new_entry(self->inputStream, target_entry_idx, self->sampleCount,
                                             self->tableSize, STZ2_TAB_OVERLAP_SIZE,
                                             self->tab_file_offset, (void*)self->sizes,
                                             &self->first_entry_idx, self->fieldSize);

                        if (err)
                            BAILWITHERROR(MP4BadDataErr)
                    }

                    /* update the file offset of the first entry in table */
                    if ((first_entry_idx != self->first_entry_idx) || /* table is reloaded */
                        (startingSampleNumber !=
                         self->start_sample_number) /* chunk is changed */) {
                        u32 tmp_size;
                        first_entry_idx = self->first_entry_idx; /* update 1st loaded entry index */
                        // fix ENGR00229993
                        for (i = startingSampleNumber; i <= first_entry_idx;
                             i++)  // target_entry_idx
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
                        err = load_new_entry(self->inputStream, target_entry_idx, self->sampleCount,
                                             self->tableSize, STZ2_TAB_OVERLAP_SIZE,
                                             self->tab_file_offset, (void*)self->sizes,
                                             &self->first_entry_idx, self->fieldSize);

                        if (err)
                            BAILWITHERROR(MP4BadDataErr)

                        // fix ENGR00229993
                        // target_entry_idx is already in current table
                        if (first_entry_idx != self->first_entry_idx) {
                            if (first_entry_idx > self->first_entry_idx) {
                                u32 dwEntryNum = first_entry_idx - self->first_entry_idx;
                                if (dwEntryNum > STZ2_TAB_MAX_SIZE) {
                                    //     printf("==== unexpect status, dwEntryNum(%d) >
                                    //     STZ2_TAB_MAX_SIZE(%d)\n", (int)dwEntryNum,
                                    //     (int)STZ2_TAB_MAX_SIZE);
                                    BAILWITHERROR(MP4BadDataErr);
                                }

                                for (i = 0; i < dwEntryNum; i++) {
                                    if (self->fieldSize == 16)
                                        offset -= *((u16*)self->sizes + i);
                                    else
                                        offset -= self->sizes[i];
                                }
                            } else {
                                u32 dwEntryNum = self->first_entry_idx - first_entry_idx;
                                u64 qwOffset = self->tab_file_offset +
                                               first_entry_idx * self->fieldSize / 8;

                                // printf("==== enter first_entry_idx(%d) <
                                // self->first_entry_idx(%d) case\n",
                                //   (int)first_entry_idx, (int)self->first_entry_idx);

                                u8* pdwSampleSize = NULL;
                                pdwSampleSize =
                                        (u8*)MP4LocalMalloc(dwEntryNum * self->fieldSize / 8);
                                if (pdwSampleSize == NULL)
                                    BAILWITHERROR(MP4NoMemoryErr)

                                err = load_entries(
                                        self->inputStream, qwOffset,
                                        self->fieldSize == 4 ? dwEntryNum / 2 : dwEntryNum,
                                        pdwSampleSize, (self->fieldSize + 7) / 8 * 8);
                                if (err)
                                    BAILWITHERROR(MP4BadDataErr)
                                if (self->fieldSize == 4)
                                    extend_4bits_entry_to_byte(pdwSampleSize, dwEntryNum);

                                if (self->fieldSize == 16) {
                                    u16* pSize = (u16*)pdwSampleSize;
                                    for (i = 0; i < dwEntryNum; i++) offset += pSize[i];
                                } else /* 8, 4 */
                                {
                                    for (i = 0; i < dwEntryNum; i++) offset += pdwSampleSize[i];
                                }
                                MP4LocalFree(pdwSampleSize);
                            }

                            first_entry_idx = self->first_entry_idx;
                            self->first_entry_offset = offset;
                        }
                    }

                    /* all the index table in memory or target entry already loaded. */
                    *outSize = self->sizes[target_entry_idx - first_entry_idx];
                    offset = self->first_entry_offset;
                    for (i = 0; i < target_entry_idx - first_entry_idx; i++)
                        offset += self->sizes[i];
                    *outOffsetSize = offset;
                    err = MP4NoErr;
                    goto bail;
                }

                if ((start_entry_idx < first_entry_idx) ||
                    ((start_entry_idx - first_entry_idx) >=
                     STZ2_TAB_MAX_SIZE)) { /* start sample NOT in memory, load it first*/
                    err = load_new_entry(self->inputStream, start_entry_idx, self->sampleCount,
                                         self->tableSize, STZ2_TAB_OVERLAP_SIZE,
                                         self->tab_file_offset, (void*)self->sizes,
                                         &self->first_entry_idx, self->fieldSize);

                    if (err)
                        BAILWITHERROR(MP4BadDataErr)
                    first_entry_idx = self->first_entry_idx; /* update 1st loaded entry index */

                    /* jitter will happen?  */
                    if ((target_entry_idx - first_entry_idx) >=
                        STZ2_TAB_MAX_SIZE) { /* target sample not loaded yet */
                        jitter_found = 1;
                    }
                }

                if (!jitter_found) {
                    /* now start sample of the chunk must be in memory.
                    Then, target sample in memory? */
                    if ((target_entry_idx < first_entry_idx) ||
                        ((target_entry_idx - first_entry_idx) >=
                         STZ2_TAB_MAX_SIZE)) { /* load new table section for the target sample */
                        ;
                        err = load_new_entry(self->inputStream, target_entry_idx, self->sampleCount,
                                             self->tableSize, STZ2_TAB_OVERLAP_SIZE,
                                             self->tab_file_offset, (void*)self->sizes,
                                             &self->first_entry_idx, self->fieldSize);

                        if (err)
                            BAILWITHERROR(MP4BadDataErr)
                        first_entry_idx = self->first_entry_idx; /* update 1st loaded entry index */

                        /* jitter will happen? */
                        if (start_entry_idx <
                            first_entry_idx) { /* start entry of the chunk is not in memory now.*/
                            jitter_found = 1;
                        }
                    }
                } /* otherwise, meaningless to load the target sample. The possibility for it to
                     load with starting sample is very low. */

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
            if (self->fieldSize == 16) {
                u16* sizes = (u16*)self->sizes;
                *outSize = sizes[sampleNumber - 1];
                for (i = startingSampleNumber - 1; i < sampleNumber - 1; i++) offset += sizes[i];
            } else {
                *outSize = self->sizes[sampleNumber - 1];
                for (i = startingSampleNumber - 1; i < sampleNumber - 1; i++)
                    offset += self->sizes[i];
            }
            *outOffsetSize = offset;
            MP4MSG("sampleNumber %d, outSize %d, outOffsetSize %d\n", sampleNumber, *outSize,
                   *outOffsetSize);
        }
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err finishScan(MP4AtomPtr s) {
    MP4Err err = MP4NoErr;
    MP4CompactSampleSizeAtomPtr self = (MP4CompactSampleSizeAtomPtr)s;
    MP4InputStreamPtr inputStream = self->inputStream;

    u8* tmp_buffer = NULL;      /* temp bufffer to scan table if it can not load in one time */
    u32 nb_entires_to_read = 0; /* number of entries read in scanning the table */
    u32 entry_cnt;              /* number of entries in one loading */
    u32 i;
    u8* entries;

    u32 sampleSize;
    u32 maxSampleSize;
    LONGLONG totalBytes;
    s64 fileOffset;
    u32 sampleCount;

    if (self->scanFinished)
        goto bail;

    maxSampleSize = self->maxSampleSize;
    totalBytes = self->totalBytes;

    if (self->sampleCount <= STZ2_TAB_MAX_SIZE)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    /* allocate temp buffer & scan the rest of the table to get max sample size & total bytes. */
    nb_entires_to_read = self->sampleCount - STZ2_TAB_MAX_SIZE;
    MP4MSG("stz2 %d entries left\n", nb_entires_to_read);
    if (nb_entires_to_read) {
        tmp_buffer = (u8*)MP4LocalMalloc(STZ2_TAB_MAX_SIZE * self->fieldSize / 8);
        TESTMALLOC(tmp_buffer)
    }

    sampleCount = STZ2_TAB_MAX_SIZE;
    fileOffset = self->tab_file_offset + STZ2_TAB_MAX_SIZE * self->fieldSize / 8;
    while (nb_entires_to_read) {
        if (nb_entires_to_read > STZ2_TAB_MAX_SIZE)
            entry_cnt = STZ2_TAB_MAX_SIZE;
        else
            entry_cnt = nb_entires_to_read;
        // MP4MSG("entries left %ld, to read %d\n", nb_entires_to_read, entry_cnt);

        if (self->fieldSize != 4)
            err = load_entries(inputStream, fileOffset, entry_cnt, tmp_buffer, self->fieldSize);
        else {
            err = load_entries(inputStream, fileOffset, entry_cnt / 2, tmp_buffer, 8);
            extend_4bits_entry_to_byte(tmp_buffer, entry_cnt);
        }
        if (err)
            goto bail;

        entries = tmp_buffer;
        for (i = 0; i < entry_cnt; i++) {
            sampleSize = self->fieldSize == 16 ? *((u16*)entries) : *entries;
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
            if (self->fieldSize == 16)
                entries++;
            sampleCount++;
        }

        nb_entires_to_read -= entry_cnt;
        fileOffset += entry_cnt * self->fieldSize / 8;
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
    MP4CompactSampleSizeAtomPtr self = (MP4CompactSampleSizeAtomPtr)s;
    u32 entry_cnt;          /* number of entries in one loading */
    u32 entries_total_size; /* bytes of entries in one loading */
    MP4FileMappingInputStreamPtr stream = (MP4FileMappingInputStreamPtr)inputStream;
    u32 i;
    u8* entries;
    u32 sampleSize;
    u32 maxSampleSize = 0;
    LONGLONG totalBytes = 0;

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
    MP4MSG("CompactSampleSizeAtom createFromInputStream\n");
    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GETBYTES(3, reserved1);

    GET8(fieldSize);
    GET32(sampleCount);

    if (self->fieldSize != 4 && self->fieldSize != 8 && self->fieldSize != 16)
        BAILWITHERROR(MP4BadDataErr)

    if (0 > (s32)self->sampleCount)
        BAILWITHERROR(MP4BadDataErr)
    if (0 == self->sampleCount) { /* empty table means this track is empty */
        MP4MSG("\t Warning!empty stz2\n");
        if (self->size > self->bytesRead) {
            uint64 bytesToSkip = self->size - self->bytesRead;
            SKIPBYTES_FORWARD(bytesToSkip);
        }
        self->scanFinished = TRUE;
        goto bail;
    }

    MP4MSG("stz2 size %lld, field size %d, sample count %d\n", self->size, self->fieldSize,
           self->sampleCount);

    self->inputStream = inputStream;
    self->tab_file_offset = /* back table's file offset */
            MP4LocalGetCurrentFilePos((FILE*)(stream->ptr), stream->mapping->parser_context);

    entry_cnt = self->sampleCount;
    if (self->fieldSize == 4)
        entry_cnt += entry_cnt % 2; /* sizes fill up to integral number of bytes */

    if (!((STZ2_TAB_MAX_SIZE > STZ2_TAB_OVERLAP_SIZE) && (0 < STZ2_TAB_MAX_SIZE) &&
          (0 <= STZ2_TAB_OVERLAP_SIZE))) {
        // MP4DbgLog("stz2: Invalid table size %ld & overlap size %ld\n", STZ2_TAB_MAX_SIZE,
        // STZ2_TAB_OVERLAP_SIZE);
        BAILWITHERROR(MP4NotImplementedErr);
    }

    if ((u64)entry_cnt * self->fieldSize / 8 > self->size - self->bytesRead)
        BAILWITHERROR(MP4BadDataErr);

    if ((u64)entry_cnt * self->fieldSize / 8 > STZ2_TAB_MAX_MALLOC_SIZE)
        BAILWITHERROR(MP4BadDataErr);

    if ((STZ2_TAB_MAX_SIZE < entry_cnt) &&
        (inputStream->stream_flags & memOptimization_flag)) /* only load part of the entries */
        entry_cnt = STZ2_TAB_MAX_SIZE;

    MP4DbgLog("stz2 sampleCount %d, table size %d\n", self->sampleCount, entry_cnt);
    self->tableSize = entry_cnt;

    if (entry_cnt == self->sampleCount)
        self->scanFinished = TRUE;

    if (self->fieldSize == 16)
        entries_total_size = entry_cnt * 2;
    else /* 4 or 8 */
        entries_total_size = entry_cnt;

    self->sizes = (u8*)MP4LocalMalloc(entries_total_size);
    TESTMALLOC(self->sizes)

    /* read the whole 'stz2' table into memory. Data in file are big endian */
    if (self->fieldSize == 4) {
        GETBYTES(entries_total_size / 2, sizes);
    } else {
        GETBYTES(entries_total_size, sizes);
    }
    if (err)
        BAILWITHERROR(MP4BadDataErr); /* index table must be entire */

#ifndef CPU_BIG_ENDIAN
    if (self->fieldSize == 16)
        reverse_endian_u16((u16*)self->sizes, entry_cnt);
#endif

    if (self->fieldSize == 4)
        extend_4bits_entry_to_byte(self->sizes, entries_total_size);

    entries = self->sizes;
    for (i = 0; i < entry_cnt; i++) {
        if (self->fieldSize == 16)
            sampleSize = *(u16*)entries;
        else
            sampleSize = *entries;
        totalBytes += sampleSize;
        MP4MSG("samp %d, size %d\n", i, sampleSize);

        if (maxSampleSize < sampleSize) {
            if (MP4_MAX_SAMPLE_SIZE < sampleSize) {
                /* still permit the atom tree to establish for cleanup later */
                MP4MSG("samp %d, invalid size %d (1)\n", i, sampleSize);
                // BAILWITHERROR( MP4_ERR_WRONG_SAMPLE_SIZE)
            }
            maxSampleSize = sampleSize;
        }
        entries++;
        /* entries type is u8*  */
        if (self->fieldSize == 16)
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
    MP4MSG("max sample size %d bytes, total %lld bytes\n", self->maxSampleSize, self->totalBytes);

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
    MP4DbgLog("stz2 %ld ms\n", end_time - start_time);
    MP4DbgLog("stz2 %ld ms to get max sample size\n", end_time - start_time_max_size);
#else
    gettimeofday(&end_time, &time_zone);
    if (end_time.tv_usec >= start_time.tv_usec) {
        MP4DbgLog("stz2: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec,
                  end_time.tv_usec - start_time.tv_usec);
    } else {
        MP4DbgLog("stz2: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec - 1,
                  end_time.tv_usec + 1000000000 - start_time.tv_usec);
    }

    if (0 == self->sampleSize) /* non-fixed sample size */
    {
        if (end_time.tv_usec >= start_time_max_size.tv_usec) {
            MP4DbgLog("stz2 to get max sample size: %ld sec, %ld us\n",
                      end_time.tv_sec - start_time_max_size.tv_sec,
                      end_time.tv_usec - start_time_max_size.tv_usec);
        } else {
            MP4DbgLog("stz2 to get max sample size: %ld sec, %ld us\n",
                      end_time.tv_sec - start_time_max_size.tv_sec - 1,
                      end_time.tv_usec + 1000000000 - start_time_max_size.tv_usec);
        }
    }

#endif
#endif

    return err;
}

MP4Err MP4CreateCompactSampleSizeAtom(MP4CompactSampleSizeAtomPtr* outAtom) {
    MP4Err err;
    MP4CompactSampleSizeAtomPtr self;

    self = (MP4CompactSampleSizeAtomPtr)MP4LocalCalloc(1, sizeof(MP4CompactSampleSizeAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4CompactSampleSizeAtomType;
    self->name = "compact sample size";
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
