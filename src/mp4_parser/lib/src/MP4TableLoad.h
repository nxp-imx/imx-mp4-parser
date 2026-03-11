/************************************************************************
* Copyright (c) 2005-2012, Freescale Semiconductor, Inc.
*
* Copyright 2017-2018, 2026 NXP
* SPDX-License-Identifier: BSD-3-Clause
************************************************************************/

/*  Limit the size of index table loaded into memory,
    to save memory and loading time. Only for local playback. */


void reverse_endian_u16(u16 * entries, u32 nb_entries);

void reverse_endian_u32(u32 * entries, u32 nb_entries);

void reverse_endian_u64(u64 * entries, u32 nb_entries);

MP4Err load_next_tab_section_u32(MP4InputStreamPtr inputStream,
                                 int forward,
                                 u32 nb_total_entries,
                                 u32 tab_size,
                                 u32 tab_ovlp_size,
                                 LONGLONG tab_file_offset,
                                 u32 * tab_entries,
                                 u32 * first_entry_idx);

void extend_4bits_entry_to_byte(u8 * buffer, u32 size);


MP4Err load_new_entry(  MP4InputStreamPtr inputStream,
                            u32 target_entry_idx,
                            u32 nb_total_entries,
                            u32 tab_size,
                            u32 tab_ovlp_size,
                            LONGLONG tab_file_offset,
                            u8 * tab_entries,
                            u32 * first_entry_idx,
                            u32 field_size);

MP4Err load_new_entry_u32(  MP4InputStreamPtr inputStream,
                            u32 target_entry_idx,
                            u32 nb_total_entries,
                            u32 tab_size,
                            u32 tab_ovlp_size,                                 
                            LONGLONG tab_file_offset,
                            u32 * tab_entries,  
                            u32 * first_entry_idx);

MP4Err load_new_entry_u64(  MP4InputStreamPtr inputStream, 
                            u32 target_entry_idx,
                            u32 nb_total_entries,
                            u32 tab_size,
                            u32 tab_ovlp_size,                                 
                            LONGLONG tab_file_offset,
                            u64 * tab_entries,  
                            u32 * first_entry_idx);

MP4Err load_new_entry_dwords(  MP4InputStreamPtr inputStream, 
                            u32 target_entry_idx,
                            u32 nb_total_entries,
                            u32 tab_size,
                            u32 tab_ovlp_size,                                 
                            LONGLONG tab_file_offset,
                            u32 * tab_entries,  
                            u32 * first_entry_idx,
                            u32 nb_dwords_per_entry);

MP4Err load_entries_u32(MP4InputStreamPtr inputStream,
                        LONGLONG file_offset,
                        u32 nb_entries,
                        u32 * entries);

MP4Err load_entries(MP4InputStreamPtr inputStream,
                        LONGLONG file_offset,
                        u32 nb_entries,
                        u8 * entries,
                        u32 bits_width);

