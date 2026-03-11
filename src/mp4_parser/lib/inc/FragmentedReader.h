/************************************************************************
 * Copyright (c) 2014-2016, Freescale Semiconductor, Inc.
 * Copyright 2017, 2024, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ************************************************************************/

#ifndef FRAGMENTED_READER_H
#define FRAGMENTED_READER_H
#include "MP4Atoms.h"
#include "MP4FragmentedQueue.h"
#include "MP4InputStream.h"
#include "MP4TagList.h"

typedef struct _FragmentedTrackReaderStruct {
    u32 id;
    u32 count;
    u32 num;

    MP4FragmentedQueue* queue;

    MP4MediaInformationAtomPtr minf;
    u32 dataReferenceIndex;

    void* appContext;
    uint8* sampleBuffer; /* current sample buffer, from application, not released or output yet. */
    void* bufferContext; /* buffer context, from application on requesting this buffer */
    uint32 bufferSize;
    uint32 bufferOffset; /* offset in bytes. A buffer may contain more than one samples, for PCM
                            audio. */

    uint32 nalLengthFieldSize;
    u32 timescale;

    u64 ts;  // timestamp in units of timescale
    int64 startTime;

    // used to parse trun atom
    s64 moof_offset;
    s64 base_offset;
    s64 running_offset;
    u32 default_duration;
    u32 default_size;
    u32 default_flags;
    u32 moof_length;

    MP4TrackFragmentSample streamSample;
    uint32 sampleBytesLeft;
    uint64 sampleFileOffset;
    uint32 nalBytesLeft;

    bool bEnable;
    bool bEntrypted;

    u32 default_IsEncrypted;
    u32 default_IV_size;
    u8 default_KID[16];

    u32 default_saiz_size;
    u32 saizTableSize;
    u8* saizTable;

    u32 default_EnctyptedByteBlock;
    u32 default_SkipByteBlock;
    u32 default_key_iv_Length;
    u8* default_key_crypto_iv;
    bool bSubsampleEncryption;
    u32 mediaType;

    char* codecData;
    u32 codecDataSize;
} FragmentedTrackReaderStruct;

typedef struct _FragmentedReaderStruct {
    MP4PrivateMovieRecordPtr moov;

    void* appContext;

    MP4AtomPtr sidx; /* 'sidx' atom */
    MP4AtomPtr mfra; /* 'mfra' atom */
    MP4InputStreamPtr inputStream;

    u64 first_moof_offset;
    s64 curr_moof_offset;

    u32 track_count;
    u64 duration;  // duration without timescale;
    FragmentedTrackReaderStruct* track_reader[MP4_PARSER_MAX_STREAM];
    u32 flags;

    u32 lastSeekTrack;
    s64 lastSeekTs;
    FILE* fpDump;
} FragmentedReaderStruct;

MP4Err MP4CreateFragmentedReader(MP4PrivateMovieRecordPtr theMovie, u32 flag, void* appContext,
                                 FragmentedReaderStruct** outReader);
MP4Err MP4DeleteFragmentedReader(FragmentedReaderStruct* outReader);
MP4Err enableFragmentedTrack(FragmentedReaderStruct* self, u32 trackNum, bool enable);
MP4Err addFragmentedTrack(FragmentedReaderStruct* self, u32 trackNum,
                          MP4MediaInformationAtomPtr minf, u32 dataReferenceIndex,
                          u32 nalLengthFieldSize, u32 timescale, MP4TrackExtendsAtomPtr trex,
                          u32 mediaType);
MP4Err isSeekable(FragmentedReaderStruct* reader, bool* seekable);
MP4Err getFragmentedDuration(FragmentedReaderStruct* reader, u64* usDuration);
MP4Err getFragmentedTrackNextSample(FragmentedReaderStruct* reader, u32 trackNum, uint8** outBuffer,
                                    void** bufferContext, uint32* outSize, uint64* usStartTime,
                                    uint64* usDuration, uint32* sampleFlags);
MP4Err getFragmentedTrackNextSyncSample(FragmentedReaderStruct* reader, uint32 direction,
                                        u32 trackNum, uint8** outBuffer, void** bufferContext,
                                        uint32* outSize, uint64* outTime, uint64* outDuration,
                                        uint32* sampleFlags);

MP4Err seekFragmentedTrack(FragmentedReaderStruct* reader, u32 trackNum, u32 flags,
                           uint64* targetTime);
MP4Err getFragmentedTrackQueueOffset(FragmentedReaderStruct* reader, u32 trackNum, uint64* offset);

MP4Err CheckIsEncryptedTrack(FragmentedTrackReaderStruct* self);
MP4Err addExtTrackTags(FragmentedReaderStruct* reader, u32 trackNum, TrackExtTagList* list);
MP4Err getFragmentedTrackSampleCryptoInfo(FragmentedReaderStruct* reader, u32 trackNum, uint8** iv,
                                          uint32* ivSize, uint8** clearBuffer, uint32* clearSize,
                                          uint8** encryptedBuffer, uint32* encryptedSize);
MP4Err flushFragmentedTrack(FragmentedReaderStruct* reader, u32 trackNum);

#endif
