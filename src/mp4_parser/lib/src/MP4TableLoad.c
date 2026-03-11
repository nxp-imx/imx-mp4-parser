/*
***********************************************************************
* Copyright (c) 2005-2012, Freescale Semiconductor, Inc.
* Copyright 2017-2018, 2026 NXP
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************
*/


#include <stdlib.h>
#include <string.h>
#include "MP4InputStream.h"
#include "MP4Impl.h"
#include "MP4TableLoad.h"

void reverse_endian_u16(u16* entries, u32 nb_entries) {
    u16* p;
    u32 entry_idx;
    u16 entry_data;

    p = entries;
    for (entry_idx = 0; entry_idx < nb_entries; entry_idx++, p++) {
        entry_data = *p;
        *p = ((entry_data & 0xff00) >> 8) + ((entry_data & 0x00ff) << 8);
    }
    return;
}

void reverse_endian_u32(u32* entries, u32 nb_entries) {
    u32* p;
    u32 entry_idx;
    u32 entry_data;

    p = entries;
    for (entry_idx = 0; entry_idx < nb_entries; entry_idx++, p++) {
        entry_data = *p;
        *p = ((entry_data & 0xff000000) >> 24) + ((entry_data & 0x00ff0000) >> 8) +
             ((entry_data & 0x0000ff00) << 8) + ((entry_data & 0x000000ff) << 24);
    }
    return;
}

/*****************************************************************************
Load a number of UINT32 items from the input stream
Arguments:
inputStream,    IN, input stream
file_offset,    IN, absolute offset of the input stream of the 1st entry to read
nb_entries,   IN, number of entries to read
entries,     IN/OUT, buffer to load the entries
******************************************************************************/
MP4Err load_entries_u32(MP4InputStreamPtr inputStream, LONGLONG file_offset, u32 nb_entries,
                        u32* entries) {
    MP4Err err = MP4NoErr;
    MP4FileMappingInputStreamPtr stream = (MP4FileMappingInputStreamPtr)inputStream;

    // MP4DbgLog("reload\n");
    /*ENGR63137: use seek callback instead of directly modifying callback context */
    MP4LocalSeekFile((FILE*)stream->ptr, file_offset, SEEK_SET, stream->mapping->parser_context);
    MP4LocalReadFile(entries, sizeof(u32), nb_entries, (FILE*)stream->ptr,
                     stream->mapping->parser_context);

#ifndef CPU_BIG_ENDIAN
    reverse_endian_u32(entries, nb_entries);
#endif

bail:
    return err;
}

/*
 bits_width can be 8, 16, 32
*/
MP4Err load_entries(MP4InputStreamPtr inputStream, LONGLONG file_offset, u32 nb_entries,
                    u8* entries, u32 bits_width) {
    MP4Err err = MP4NoErr;
    MP4FileMappingInputStreamPtr stream = (MP4FileMappingInputStreamPtr)inputStream;

    // MP4DbgLog("reload\n");
    MP4LocalSeekFile((FILE*)stream->ptr, file_offset, SEEK_SET, stream->mapping->parser_context);
    MP4LocalReadFile(entries, bits_width / 8, nb_entries, (FILE*)stream->ptr,
                     stream->mapping->parser_context);

#ifndef CPU_BIG_ENDIAN
    if (bits_width == 16)
        reverse_endian_u16((u16*)entries, nb_entries);
    else if (bits_width == 32)
        reverse_endian_u32((u32*)entries, nb_entries);
#endif

bail:
    return err;
}

void reverse_endian_u64(u64* entries, u32 nb_entries) {
    u64* p;
    u32 entry_idx;
    u64 entry_data;

    p = entries;
    for (entry_idx = 0; entry_idx < nb_entries; entry_idx++, p++) {
        entry_data = *p;
        *p = ((entry_data & 0xff00000000000000LL) >> 56) +
             ((entry_data & 0x00ff000000000000LL) >> 40) +
             ((entry_data & 0x0000ff0000000000LL) >> 24) +
             ((entry_data & 0x000000ff00000000LL) >> 8) +
             ((entry_data & 0x00000000ff000000LL) << 8) +
             ((entry_data & 0x0000000000ff0000LL) << 24) +
             ((entry_data & 0x000000000000ff00LL) << 40) +
             ((entry_data & 0x00000000000000ffLL) << 56);
    }
    return;
}

static MP4Err load_entries_u64(MP4InputStreamPtr inputStream, LONGLONG file_offset, u32 nb_entries,
                               u64* entries) {
    MP4Err err = MP4NoErr;
    MP4FileMappingInputStreamPtr stream = (MP4FileMappingInputStreamPtr)inputStream;

    // MP4DbgLog("reload\n");
    /*ENGR63137: use seek callback instead of directly modifying callback context */
    MP4LocalSeekFile((FILE*)stream->ptr, file_offset, SEEK_SET, stream->mapping->parser_context);
    MP4LocalReadFile(entries, sizeof(u64), nb_entries, (FILE*)stream->ptr,
                     stream->mapping->parser_context);

#ifndef CPU_BIG_ENDIAN
    reverse_endian_u64(entries, nb_entries);
#endif

bail:
    return err;
}

static MP4Err load_entries_dwords(MP4InputStreamPtr inputStream, LONGLONG file_offset,
                                  u32 nb_entries, u32 nb_dwords_per_entry, u32* entries) {
    MP4Err err = MP4NoErr;
    MP4FileMappingInputStreamPtr stream = (MP4FileMappingInputStreamPtr)inputStream;

    // MP4DbgLog("reload\n");
    /*ENGR63137: use seek callback instead of directly modifying callback context */
    MP4LocalSeekFile((FILE*)stream->ptr, file_offset, SEEK_SET, stream->mapping->parser_context);
    MP4LocalReadFile(entries, sizeof(u32) * nb_dwords_per_entry, nb_entries, (FILE*)stream->ptr,
                     stream->mapping->parser_context);

#ifndef CPU_BIG_ENDIAN
    reverse_endian_u32(entries, nb_entries * nb_dwords_per_entry);
#endif

bail:
    return err;
}

/*****************************************************************************
WARNING:
This function can not be used in parsing phase.
Becuase it will change value of "inputstream->available", which is referred in parsing phase by all
atom parsing process.

Description:
Load next section of index table from inputstream to memory.

Arguments:
inputStream, IN, input stream (eg.a file)
forward, IN, direction, 1 for next section, 0 for previous section.
tab_size, IN, table size in memory, in number of entries.
tab_ovlp_size, IN, overlap size between current and next loading, in number of entries.
                   It may change in the function, because the actual overlap size maybe affected by
boundary. tab_file_offset, IN, absolute file offset of the original table in the input stream.
tab_entries, IN, start address of the table in memory
first_entry_idx, IN/OUT, index of the 1st entry loaded into memory.
******************************************************************************/
MP4Err load_next_tab_section_u32(MP4InputStreamPtr inputStream, int forward, u32 nb_total_entries,
                                 u32 tab_size, u32 tab_ovlp_size, LONGLONG tab_file_offset,
                                 u32* tab_entries, u32* first_entry_idx) {
    MP4Err err = MP4NoErr;

    u32 old_start_entry_idx;
    u32 old_end_entry_idx; /* actual end index +1 */
    u32 new_start_entry_idx;
    u32 new_end_entry_idx;

    u32 ovlp_start_offset =
            0; /* relative offset from the start of idx table of the overlapped part */
    u32* des_entry;
    u32* src_entry;
    u32 i;

    u32 nb_entries_to_load = 0; /* number of entries to load, excluding the overlap part */
    LONGLONG file_offset;       /* absolute file offset for reading non-overlapped part */

    // static u32 reload_count=0;
    // reload_count++;
    // printf("reload %ld\n", reload_count);

    if (tab_size == nb_total_entries) {
        // MP4DbgLog( "no index sections can be loaded (all in memory)\n");
        return MP4NoErr; /* the whole index table is already loaded */
    }

    old_start_entry_idx = *first_entry_idx;
    old_end_entry_idx = old_start_entry_idx + tab_size;

    if (forward) { /* load forward section */
        if (old_end_entry_idx >= nb_total_entries) {
            // MP4DbgLog( "no index entries forward can be loaded\n");
            return MP4BadParamErr;
        }
        /* read section forward */
        // MP4DbgLog( "\n load next idx section forward...\n");
        new_end_entry_idx = old_end_entry_idx + (tab_size - tab_ovlp_size);
        if (new_end_entry_idx > nb_total_entries)
            new_end_entry_idx = nb_total_entries; /* right boundary */
        new_start_entry_idx = new_end_entry_idx - tab_size;
        assert(new_start_entry_idx > old_start_entry_idx);
        assert(old_end_entry_idx >= new_start_entry_idx); /* with overlap or not */

        /* copy the overlapped part, avoid reading file & adjusting endianess */
        tab_ovlp_size = old_end_entry_idx - new_start_entry_idx;
        ovlp_start_offset = tab_size - tab_ovlp_size;

        src_entry = tab_entries + ovlp_start_offset;
        des_entry = tab_entries;
        for (i = 0; i < tab_ovlp_size; i++) {
            memcpy(des_entry, src_entry, sizeof(u32));
            src_entry++;
            des_entry++;
        }
        *first_entry_idx = new_start_entry_idx;

        /* load non-overlapped part from file */
        nb_entries_to_load = tab_size - tab_ovlp_size;
        file_offset = tab_file_offset +
                      old_end_entry_idx * sizeof(u32); /* from the end of current section */
        err = load_entries_u32(inputStream, file_offset, nb_entries_to_load,
                               (tab_entries + tab_ovlp_size));

        if (err) {
            // MP4DbgLog( "ERR!lib read idx err (forward)\n");
            goto bail;
        }
    } else { /* load backward section */
        if (!old_start_entry_idx) {
            // MP4DbgLog( "no index entries backward can be loaded\n");
            return MP4NoErr;
        }
        /* read section backward */
        // MP4DbgLog( "\n load next idx section backward...\n");
        new_start_entry_idx = old_start_entry_idx - (tab_size - tab_ovlp_size);
        if (0 > (s32)new_start_entry_idx)
            new_start_entry_idx = 0; /* left boundary */
        new_end_entry_idx = new_start_entry_idx + tab_size;
        assert(new_start_entry_idx < old_start_entry_idx);
        assert(new_end_entry_idx >= old_start_entry_idx); /* with overlap or not */

        /* copy the overlapped part */
        tab_ovlp_size = new_end_entry_idx - old_start_entry_idx;
        des_entry = tab_entries + tab_size - 1;
        src_entry = tab_entries + tab_ovlp_size - 1;
        for (i = 0; i < tab_ovlp_size; i++) {
            memcpy(des_entry, src_entry, sizeof(u32));
            des_entry--;
            src_entry--;
        }
        *first_entry_idx = new_start_entry_idx;

        /* load non-overlapped part from file */
        nb_entries_to_load = tab_size - tab_ovlp_size;
        file_offset = tab_file_offset +
                      new_start_entry_idx * sizeof(u32); /* from the start of new section */
        err = load_entries_u32(inputStream, file_offset, nb_entries_to_load, tab_entries);

        if (err) {
            // MP4DbgLog( "ERR!lib read idx err (backward)\n");
            goto bail;
        }
    }

bail:
    // MP4DbgLog( "old index start %ld, end %ld, new start %ld, end %ld\n",old_start_entry_idx,
    // old_end_entry_idx,new_start_entry_idx, new_end_entry_idx);

    // MP4DbgLog( "ovlp size %ld, start from %ld, non-ovlp size %ld\n",tab_ovlp_size,
    // ovlp_start_offset, nb_entries_to_load);

    return err;
}

void extend_4bits_entry_to_byte(u8* buffer, u32 size) {
    s32 j = size - 1;
    for (; j >= 0; j--) {
        if (j % 2 == 0)
            buffer[j] = buffer[j / 2] >> 4;
        else
            buffer[j] = buffer[j / 2] & 0x0F;
    }
}

/*
 field_size can be 4,8,16
*/
MP4Err load_new_entry(MP4InputStreamPtr inputStream, u32 target_entry_idx, u32 nb_total_entries,
                      u32 tab_size, u32 tab_ovlp_size, LONGLONG tab_file_offset, u8* tab_entries,
                      u32* first_entry_idx, u32 field_size) {
    MP4Err err = MP4NoErr;

    u32 old_start_entry_idx = *first_entry_idx;
    u32 old_end_entry_idx;
    u32 new_start_entry_idx; /* actual end index +1 */
    u32 new_end_entry_idx;

    u32 nb_entries_to_load; /* number of entries of next loading, excluding the overlap part */
    LONGLONG file_offset;   /* absolute file offset for reading non-overlapped part */
    u8* entries_to_load;    /* buffer to load entries, excluding the overlap part */
    u32 in_memory_field_size = field_size >= 8 ? field_size : 8; /* 4 bits entry stores in byte */

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

    if (tab_size >= nb_total_entries)
        return MP4NoErr; /* all entries already loaded */
    if (target_entry_idx >= nb_total_entries)
        return MP4BadParamErr; /* NO such idx entry at all */
    if ((target_entry_idx >= old_start_entry_idx) &&
        ((target_entry_idx - old_start_entry_idx) < tab_size))
        return MP4NoErr; /* This index already loaded */

    // MP4DbgLog("mp4: need to load entry %d\n", target_entry_idx);
    old_end_entry_idx = old_start_entry_idx + tab_size;

    /* calculate new range according to direction, assure overlap to avoid jitter */
    if (target_entry_idx > old_start_entry_idx) /* forward */
    {
        new_start_entry_idx = target_entry_idx - tab_ovlp_size;
        if (0 > (s32)new_start_entry_idx) /* check left boundary */
            new_start_entry_idx = 0;
        new_end_entry_idx = new_start_entry_idx + tab_size;

        if (new_end_entry_idx > nb_total_entries) /* check right boundary */
        {
            new_end_entry_idx = nb_total_entries;
            new_start_entry_idx =
                    new_end_entry_idx - tab_size; /* load as much as possible, assuming totol entry
                                                     count > table size */
            assert(0 <= (s32)new_start_entry_idx);
        }
    } else /* backward */
    {
        new_end_entry_idx = target_entry_idx + tab_ovlp_size +
                            1; /* MUST +1, otherwise for overlap 0, target won't be loaded */
        if (new_end_entry_idx > nb_total_entries) /* check right boundary */
            new_end_entry_idx = nb_total_entries;

        new_start_entry_idx = new_end_entry_idx - tab_size;
        if (0 > (s32)new_start_entry_idx) /* check left boundary */
        {
            new_start_entry_idx = 0;
            new_end_entry_idx =
                    new_start_entry_idx + tab_size; /* load as much as possible, assuming totol
                                                       entry count > table size */
            assert(new_end_entry_idx <= nb_total_entries);
        }
    }

    // MP4DbgLog("reload for target entry %d. new start %d, end %d\n", target_entry_idx,
    // new_start_entry_idx, new_end_entry_idx);

    /* overlap really exist ?
    If yes, copy the overlapped part, avoid reading file & adjusting endianess */
    if ((new_start_entry_idx < old_end_entry_idx) && (new_end_entry_idx > old_start_entry_idx)) {
        u8* des_entry;
        u8* src_entry;
        u32 i;

        if (new_start_entry_idx >= old_start_entry_idx) /* forward */
        {
            u32 ovlp_start_offset; /* relative offset from the start of idx table of the overlapped
                                      part */
            tab_ovlp_size = old_end_entry_idx - new_start_entry_idx;
            ovlp_start_offset = tab_size - tab_ovlp_size;

            src_entry = tab_entries + ovlp_start_offset;
            des_entry = tab_entries;
            for (i = 0; i < tab_ovlp_size; i++) {
                memcpy(des_entry, src_entry, in_memory_field_size / 8);
                src_entry++;
                des_entry++;
            }

            // nb_entries_to_load=tab_size-tab_ovlp_size;
            file_offset = tab_file_offset + old_end_entry_idx * in_memory_field_size /
                                                    8; /* from the end of current section */
            entries_to_load = tab_entries + tab_ovlp_size;
        } else /*backward */
        {
            /* copy the overlapped part */
            tab_ovlp_size = new_end_entry_idx - old_start_entry_idx;

            des_entry = tab_entries + tab_size - 1;
            src_entry = tab_entries + tab_ovlp_size - 1;
            for (i = 0; i < tab_ovlp_size; i++) {
                memcpy(des_entry, src_entry, in_memory_field_size / 8);
                des_entry--;
                src_entry--;
            }

            /* load non-overlapped part from file */
            // nb_entries_to_load=tab_size-tab_ovlp_size;
            file_offset = tab_file_offset + new_start_entry_idx * in_memory_field_size /
                                                    8; /* from the start of new section */
            entries_to_load = tab_entries;
        }
    } else /* no overlap part */
    {
        tab_ovlp_size = 0;
        file_offset = tab_file_offset + new_start_entry_idx * in_memory_field_size /
                                                8; /* from the start of new section */
        entries_to_load = tab_entries;
    }

    /* load the non-overlapped part and adjust endianess */
    nb_entries_to_load = tab_size - tab_ovlp_size;
    err = load_entries(inputStream, file_offset,
                       (field_size == 4) ? nb_entries_to_load / 2 : nb_entries_to_load,
                       entries_to_load, in_memory_field_size);
    if (err) {
        // MP4DbgLog( "ERR! lib read table err\n");
        goto bail;
    }

    if (field_size == 4)
        extend_4bits_entry_to_byte(entries_to_load, nb_entries_to_load);

    *first_entry_idx = new_start_entry_idx;
    // MP4DbgLog( "\t target %ld: old index start %ld, end %ld, new start %ld, end %ld\n\n",
    // target_entry_idx, old_start_entry_idx, old_end_entry_idx,new_start_entry_idx,
    // new_end_entry_idx);

bail:

//#ifdef FSL_MP4_PARSER_DBG_TIME
#if 0
#if defined(__WINCE) || defined(WIN32)
    end_time = timeGetTime();
    MP4DbgLog("reload %ld ms\n", end_time-start_time);
#else
    gettimeofday(&end_time, &time_zone);
    if(end_time.tv_usec >= start_time.tv_usec)
    {
        MP4DbgLog("reload: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec,
                                                         end_time.tv_usec - start_time.tv_usec);
    }
    else
    {
        MP4DbgLog("reload: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec -1,
                                                         end_time.tv_usec + 1000000000 - start_time.tv_usec);
    }

#endif
#endif

    return err;
}

/*********************************************************************
load_target_entry_u32:
(Only for simple index of video stream)

Given a target entry index (0-based).
If this entry is already loaded, nothing to do;
If not, load the section of index table with this entry in the center (considerring table border).

Arguments:
inputStream, IN, input stream (eg.a file)
target_entry_idx, IN, index of the target entry to load, 0-based.
tab_size, IN, table size in memory, in number of entries.
tab_ovlp_size, IN, overlap size between current and next loading, in number of entries.
                   It may change in the function, because the actual overlap size maybe affected by
boundary. tab_file_offset, IN, absolute file offset of the original table in the input stream.
tab_entries, IN, start address of the table in memory
first_entry_idx, IN/OUT, index of the 1st entry loaded into memory.
*********************************************************************/

MP4Err load_new_entry_u32(MP4InputStreamPtr inputStream, u32 target_entry_idx, u32 nb_total_entries,
                          u32 tab_size, u32 tab_ovlp_size, LONGLONG tab_file_offset,
                          u32* tab_entries, u32* first_entry_idx) {
    MP4Err err = MP4NoErr;

    u32 old_start_entry_idx = *first_entry_idx;
    u32 old_end_entry_idx;
    u32 new_start_entry_idx; /* actual end index +1 */
    u32 new_end_entry_idx;

    u32 nb_entries_to_load; /* number of entries of next loading, excluding the overlap part */
    LONGLONG file_offset;   /* absolute file offset for reading non-overlapped part */
    u32* entries_to_load;   /* buffer to load entries, excluding the overlap part */

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

    if (tab_size >= nb_total_entries)
        return MP4NoErr; /* all entries already loaded */
    if (target_entry_idx >= nb_total_entries)
        return MP4BadParamErr; /* NO such idx entry at all */
    if ((target_entry_idx >= old_start_entry_idx) &&
        ((target_entry_idx - old_start_entry_idx) < tab_size))
        return MP4NoErr; /* This index already loaded */

    // MP4DbgLog("mp4: need to load entry %d\n", target_entry_idx);
    old_end_entry_idx = old_start_entry_idx + tab_size;

    /* calculate new range according to direction, assure overlap to avoid jitter */
    if (target_entry_idx > old_start_entry_idx) /* forward */
    {
        new_start_entry_idx = target_entry_idx - tab_ovlp_size;
        if (0 > (s32)new_start_entry_idx) /* check left boundary */
            new_start_entry_idx = 0;
        new_end_entry_idx = new_start_entry_idx + tab_size;

        if (new_end_entry_idx > nb_total_entries) /* check right boundary */
        {
            new_end_entry_idx = nb_total_entries;
            new_start_entry_idx =
                    new_end_entry_idx - tab_size; /* load as much as possible, assuming totol entry
                                                     count > table size */
            assert(0 <= (s32)new_start_entry_idx);
        }
    } else /* backward */
    {
        new_end_entry_idx = target_entry_idx + tab_ovlp_size +
                            1; /* MUST +1, otherwise for overlap 0, target won't be loaded */
        if (new_end_entry_idx > nb_total_entries) /* check right boundary */
            new_end_entry_idx = nb_total_entries;

        new_start_entry_idx = new_end_entry_idx - tab_size;
        if (0 > (s32)new_start_entry_idx) /* check left boundary */
        {
            new_start_entry_idx = 0;
            new_end_entry_idx =
                    new_start_entry_idx + tab_size; /* load as much as possible, assuming totol
                                                       entry count > table size */
            assert(new_end_entry_idx <= nb_total_entries);
        }
    }

    // MP4DbgLog("reload for target entry %d. new start %d, end %d\n", target_entry_idx,
    // new_start_entry_idx, new_end_entry_idx);

    /* overlap really exist ?
    If yes, copy the overlapped part, avoid reading file & adjusting endianess */
    if ((new_start_entry_idx < old_end_entry_idx) && (new_end_entry_idx > old_start_entry_idx)) {
        u32* des_entry;
        u32* src_entry;
        u32 i;

        if (new_start_entry_idx >= old_start_entry_idx) /* forward */
        {
            u32 ovlp_start_offset; /* relative offset from the start of idx table of the overlapped
                                      part */
            tab_ovlp_size = old_end_entry_idx - new_start_entry_idx;
            ovlp_start_offset = tab_size - tab_ovlp_size;

            src_entry = tab_entries + ovlp_start_offset;
            des_entry = tab_entries;
            for (i = 0; i < tab_ovlp_size; i++) {
                memcpy(des_entry, src_entry, sizeof(u32));
                src_entry++;
                des_entry++;
            }

            // nb_entries_to_load=tab_size-tab_ovlp_size;
            file_offset = tab_file_offset +
                          old_end_entry_idx * sizeof(u32); /* from the end of current section */
            entries_to_load = tab_entries + tab_ovlp_size;
        } else /*backward */
        {
            /* copy the overlapped part */
            tab_ovlp_size = new_end_entry_idx - old_start_entry_idx;

            des_entry = tab_entries + tab_size - 1;
            src_entry = tab_entries + tab_ovlp_size - 1;
            for (i = 0; i < tab_ovlp_size; i++) {
                memcpy(des_entry, src_entry, sizeof(u32));
                des_entry--;
                src_entry--;
            }

            /* load non-overlapped part from file */
            // nb_entries_to_load=tab_size-tab_ovlp_size;
            file_offset = tab_file_offset +
                          new_start_entry_idx * sizeof(u32); /* from the start of new section */
            entries_to_load = tab_entries;
        }
    } else /* no overlap part */
    {
        tab_ovlp_size = 0;
        file_offset = tab_file_offset +
                      new_start_entry_idx * sizeof(u32); /* from the start of new section */
        entries_to_load = tab_entries;
    }

    /* load the non-overlapped part and adjust endianess */
    nb_entries_to_load = tab_size - tab_ovlp_size;
    err = load_entries_u32(inputStream, file_offset, nb_entries_to_load, entries_to_load);
    if (err) {
        // MP4DbgLog( "ERR! lib read table err\n");
        goto bail;
    }

    *first_entry_idx = new_start_entry_idx;
    // MP4DbgLog( "\t target %ld: old index start %ld, end %ld, new start %ld, end %ld\n\n",
    // target_entry_idx, old_start_entry_idx, old_end_entry_idx,new_start_entry_idx,
    // new_end_entry_idx);

bail:

//#ifdef FSL_MP4_PARSER_DBG_TIME
#if 0
#if defined(__WINCE) || defined(WIN32)
    end_time = timeGetTime();
    MP4DbgLog("reload %ld ms\n", end_time-start_time);
#else
    gettimeofday(&end_time, &time_zone);
    if(end_time.tv_usec >= start_time.tv_usec)
    {
        MP4DbgLog("reload: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec, 
                                                         end_time.tv_usec - start_time.tv_usec);  
    }
    else
    {
        MP4DbgLog("reload: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec -1, 
                                                         end_time.tv_usec + 1000000000 - start_time.tv_usec); 
    }

#endif
#endif

    return err;
}

MP4Err load_new_entry_u64(MP4InputStreamPtr inputStream, u32 target_entry_idx, u32 nb_total_entries,
                          u32 tab_size, u32 tab_ovlp_size, LONGLONG tab_file_offset,
                          u64* tab_entries, u32* first_entry_idx) {
    MP4Err err = MP4NoErr;

    u32 old_start_entry_idx = *first_entry_idx;
    u32 old_end_entry_idx;
    u32 new_start_entry_idx; /* actual end index +1 */
    u32 new_end_entry_idx;

    u32 nb_entries_to_load; /* number of entries of next loading, excluding the overlap part */
    LONGLONG file_offset;   /* absolute file offset for reading non-overlapped part */
    u64* entries_to_load;   /* buffer to load entries, excluding the overlap part */

#ifdef _WINCE_FSL_MP4_PARSER_DBG_TIME
    u32 start_time = timeGetTime();
    u32 end_time;
#endif

    if (tab_size >= nb_total_entries)
        return MP4NoErr; /* all entries already loaded */
    if (target_entry_idx >= nb_total_entries)
        return MP4BadParamErr; /* NO such idx entry at all */
    if ((target_entry_idx >= old_start_entry_idx) &&
        ((target_entry_idx - old_start_entry_idx) < tab_size))
        return MP4NoErr; /* This index already loaded */

    // MP4DbgLog("mp4: need to load entry %d\n", target_entry_idx);
    old_end_entry_idx = old_start_entry_idx + tab_size;

    /* calculate new range according to direction, assure overlap to avoid jitter */
    if (target_entry_idx > old_start_entry_idx) /* forward */
    {
        new_start_entry_idx = target_entry_idx - tab_ovlp_size;
        if (0 > (s32)new_start_entry_idx) /* check left boundary */
            new_start_entry_idx = 0;
        new_end_entry_idx = new_start_entry_idx + tab_size;

        if (new_end_entry_idx > nb_total_entries) /* check right boundary */
        {
            new_end_entry_idx = nb_total_entries;
            new_start_entry_idx =
                    new_end_entry_idx - tab_size; /* load as much as possible, assuming totol entry
                                                     count > table size */
            assert(0 <= (s32)new_start_entry_idx);
        }
    } else /* backward */
    {
        new_end_entry_idx = target_entry_idx + tab_ovlp_size +
                            1; /* MUST +1, otherwise for overlap 0, target won't be loaded */
        if (new_end_entry_idx > nb_total_entries) /* check right boundary */
            new_end_entry_idx = nb_total_entries;

        new_start_entry_idx = new_end_entry_idx - tab_size;
        if (0 > (s32)new_start_entry_idx) /* check left boundary */
        {
            new_start_entry_idx = 0;
            new_end_entry_idx =
                    new_start_entry_idx + tab_size; /* load as much as possible, assuming totol
                                                       entry count > table size */
            assert(new_end_entry_idx <= nb_total_entries);
        }
    }

    // MP4DbgLog("reload for target entry %d. new start %d, end %d\n", target_entry_idx,
    // new_start_entry_idx, new_end_entry_idx);

    /* overlap really exist ?
    If yes, copy the overlapped part, avoid reading file & adjusting endianess */
    if ((new_start_entry_idx < old_end_entry_idx) && (new_end_entry_idx > old_start_entry_idx)) {
        u64* des_entry;
        u64* src_entry;
        u32 i;

        if (new_start_entry_idx >= old_start_entry_idx) /* forward */
        {
            u32 ovlp_start_offset; /* relative offset from the start of idx table of the overlapped
                                      part */
            tab_ovlp_size = old_end_entry_idx - new_start_entry_idx;
            ovlp_start_offset = tab_size - tab_ovlp_size;

            src_entry = tab_entries + ovlp_start_offset;
            des_entry = tab_entries;
            for (i = 0; i < tab_ovlp_size; i++) {
                memcpy(des_entry, src_entry, sizeof(u64));
                src_entry++;
                des_entry++;
            }

            // nb_entries_to_load=tab_size-tab_ovlp_size;
            file_offset = tab_file_offset +
                          old_end_entry_idx * sizeof(u64); /* from the end of current section */
            entries_to_load = tab_entries + tab_ovlp_size;
        } else /*backward */
        {
            /* copy the overlapped part */
            tab_ovlp_size = new_end_entry_idx - old_start_entry_idx;

            des_entry = tab_entries + tab_size - 1;
            src_entry = tab_entries + tab_ovlp_size - 1;
            for (i = 0; i < tab_ovlp_size; i++) {
                memcpy(des_entry, src_entry, sizeof(u64));
                des_entry--;
                src_entry--;
            }

            /* load non-overlapped part from file */
            // nb_entries_to_load=tab_size-tab_ovlp_size;
            file_offset = tab_file_offset +
                          new_start_entry_idx * sizeof(u64); /* from the start of new section */
            entries_to_load = tab_entries;
        }
    } else /* no overlap part */
    {
        tab_ovlp_size = 0;
        file_offset = tab_file_offset +
                      new_start_entry_idx * sizeof(u64); /* from the start of new section */
        entries_to_load = tab_entries;
    }

    /* load the non-overlapped part and adjust endianess */
    nb_entries_to_load = tab_size - tab_ovlp_size;
    err = load_entries_u64(inputStream, file_offset, nb_entries_to_load, entries_to_load);
    if (err) {
        // MP4DbgLog( "ERR! lib read table err\n");
        goto bail;
    }

    *first_entry_idx = new_start_entry_idx;
    // MP4DbgLog( "\t target %ld: old index start %ld, end %ld, new start %ld, end %ld\n\n",
    // target_entry_idx, old_start_entry_idx, old_end_entry_idx,new_start_entry_idx,
    // new_end_entry_idx);

bail:

#ifdef _WINCE_FSL_MP4_PARSER_DBG_TIME
    end_time = timeGetTime();
    MP4DbgLog("reload %ld ms\n", end_time - start_time);
#endif

    return err;
}

MP4Err load_new_entry_dwords(MP4InputStreamPtr inputStream, u32 target_entry_idx,
                             u32 nb_total_entries, u32 tab_size, u32 tab_ovlp_size,
                             LONGLONG tab_file_offset, u32* tab_entries, u32* first_entry_idx,
                             u32 nb_dwords_per_entry) {
    MP4Err err = MP4NoErr;

    u32 old_start_entry_idx = *first_entry_idx;
    u32 old_end_entry_idx;
    u32 new_start_entry_idx; /* actual end index +1 */
    u32 new_end_entry_idx;

    u32 nb_entries_to_load; /* number of entries of next loading, excluding the overlap part */
    LONGLONG file_offset;   /* absolute file offset for reading non-overlapped part */
    u32* entries_to_load;   /* buffer to load entries, excluding the overlap part */

#ifdef _WINCE_FSL_MP4_PARSER_DBG_TIME
    u32 start_time = timeGetTime();
    u32 end_time;
#endif

    if (tab_size >= nb_total_entries)
        return MP4NoErr; /* all entries already loaded */
    if (target_entry_idx >= nb_total_entries)
        return MP4BadParamErr; /* NO such idx entry at all */
    if ((target_entry_idx >= old_start_entry_idx) &&
        ((target_entry_idx - old_start_entry_idx) < tab_size))
        return MP4NoErr; /* This index already loaded */

    // MP4DbgLog("mp4: need to load entry %d\n", target_entry_idx);
    old_end_entry_idx = old_start_entry_idx + tab_size;

    /* calculate new range according to direction, assure overlap to avoid jitter */
    if (target_entry_idx > old_start_entry_idx) /* forward */
    {
        new_start_entry_idx = target_entry_idx - tab_ovlp_size;
        if (0 > (s32)new_start_entry_idx) /* check left boundary */
            new_start_entry_idx = 0;
        new_end_entry_idx = new_start_entry_idx + tab_size;

        if (new_end_entry_idx > nb_total_entries) /* check right boundary */
        {
            new_end_entry_idx = nb_total_entries;
            new_start_entry_idx =
                    new_end_entry_idx - tab_size; /* load as much as possible, assuming totol entry
                                                     count > table size */
            assert(0 <= (s32)new_start_entry_idx);
        }
    } else /* backward */
    {
        new_end_entry_idx = target_entry_idx + tab_ovlp_size +
                            1; /* MUST +1, otherwise for overlap 0, target won't be loaded */
        if (new_end_entry_idx > nb_total_entries) /* check right boundary */
            new_end_entry_idx = nb_total_entries;

        new_start_entry_idx = new_end_entry_idx - tab_size;
        if (0 > (s32)new_start_entry_idx) /* check left boundary */
        {
            new_start_entry_idx = 0;
            new_end_entry_idx =
                    new_start_entry_idx + tab_size; /* load as much as possible, assuming totol
                                                       entry count > table size */
            assert(new_end_entry_idx <= nb_total_entries);
        }
    }

    // MP4DbgLog("reload for target entry %d. new start %d, end %d\n", target_entry_idx,
    // new_start_entry_idx, new_end_entry_idx);

    /* overlap really exist ?
    If yes, copy the overlapped part, avoid reading file & adjusting endianess */
    if ((new_start_entry_idx < old_end_entry_idx) && (new_end_entry_idx > old_start_entry_idx)) {
        u32* des_entry;
        u32* src_entry;
        u32 i;

        if (new_start_entry_idx >= old_start_entry_idx) /* forward */
        {
            u32 ovlp_start_offset; /* relative offset from the start of idx table of the overlapped
                                      part */
            tab_ovlp_size = old_end_entry_idx - new_start_entry_idx;
            ovlp_start_offset = tab_size - tab_ovlp_size;

            src_entry = tab_entries + ovlp_start_offset * nb_dwords_per_entry;
            des_entry = tab_entries;
            for (i = 0; i < tab_ovlp_size; i++) {
                memcpy(des_entry, src_entry, sizeof(u32) * nb_dwords_per_entry);
                src_entry += nb_dwords_per_entry;
                des_entry += nb_dwords_per_entry;
            }

            // nb_entries_to_load=tab_size-tab_ovlp_size;
            file_offset = tab_file_offset +
                          old_end_entry_idx * sizeof(u32) *
                                  nb_dwords_per_entry; /* from the end of current section */
            entries_to_load = tab_entries + tab_ovlp_size * nb_dwords_per_entry;
        } else /*backward */
        {
            /* copy the overlapped part */
            tab_ovlp_size = new_end_entry_idx - old_start_entry_idx;

            des_entry = tab_entries + (tab_size - 1) * nb_dwords_per_entry;
            src_entry = tab_entries + (tab_ovlp_size - 1) * nb_dwords_per_entry;
            for (i = 0; i < tab_ovlp_size; i++) {
                memcpy(des_entry, src_entry, sizeof(u32) * nb_dwords_per_entry);
                des_entry -= nb_dwords_per_entry;
                src_entry -= nb_dwords_per_entry;
            }

            /* load non-overlapped part from file */
            // nb_entries_to_load=tab_size-tab_ovlp_size;
            file_offset = tab_file_offset +
                          new_start_entry_idx * sizeof(u32) *
                                  nb_dwords_per_entry; /* from the start of new section */
            entries_to_load = tab_entries;
        }
    } else /* no overlap part */
    {
        tab_ovlp_size = 0;
        file_offset =
                tab_file_offset + new_start_entry_idx * sizeof(u32) *
                                          nb_dwords_per_entry; /* from the start of new section */
        entries_to_load = tab_entries;
    }

    /* load the non-overlapped part and adjust endianess */
    nb_entries_to_load = tab_size - tab_ovlp_size;
    err = load_entries_dwords(inputStream, file_offset, nb_entries_to_load, nb_dwords_per_entry,
                              entries_to_load);
    if (err) {
        // MP4DbgLog( "ERR! lib read table err\n");
        goto bail;
    }

    *first_entry_idx = new_start_entry_idx;
    // MP4DbgLog( "\t target %ld: old index start %ld, end %ld, new start %ld, end %ld\n\n",
    // target_entry_idx, old_start_entry_idx, old_end_entry_idx,new_start_entry_idx,
    // new_end_entry_idx);

bail:

#ifdef _WINCE_FSL_MP4_PARSER_DBG_TIME
    end_time = timeGetTime();
    MP4DbgLog("reload %ld ms\n", end_time - start_time);
#endif

    return err;
}
