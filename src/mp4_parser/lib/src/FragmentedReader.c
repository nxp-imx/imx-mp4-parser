/************************************************************************
 * Copyright (c) 2014-2016, Freescale Semiconductor, Inc.
 * Copyright 2017-2018, 2020-2024, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ************************************************************************/
//#define DEBUG

#include "FragmentedReader.h"
#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"
#include "MP4DataHandler.h"
#include "MP4FragmentedQueue.h"
#include "MP4Impl.h"
#include "MP4Movies.h"
#include "MP4TableLoad.h"

#include <time.h>

extern ParserOutputBufferOps g_outputBufferOps;

#define NAL_START_CODE_SIZE 4
#define MAX_GET_QUEUE_SIZE_CNT 5
#define FILL_NAL_START_CODE(buffer) \
    {                               \
        buffer[0] = 0;              \
        buffer[1] = 0;              \
        buffer[2] = 0;              \
        buffer[3] = 1;              \
    }

#define TYPE(t) (t == MEDIA_AUDIO) ? "audio" : "video"

//#define DUMP_STREAM

#ifdef DUMP_STREAM
#define DUMP_STREAM_DATA                                               \
    if (track_reader->mediaType == MEDIA_AUDIO) {                      \
        if (!reader->fpDump)                                           \
            reader->fpDump = fopen("./stream_dump", "w");              \
        if (reader->fpDump)                                            \
            fwrite(sampleBuffer, (size_t)dataSize, 1, reader->fpDump); \
    }
#else
#define DUMP_STREAM_DATA
#endif

MP4Err addFragmentedTrack(FragmentedReaderStruct* self, u32 trackNum,
                          MP4MediaInformationAtomPtr minf, u32 dataReferenceIndex,
                          u32 nalLengthFieldSize, u32 timescale, MP4TrackExtendsAtomPtr trex,
                          u32 mediaType) {
    MP4Err err = MP4NoErr;
    FragmentedTrackReaderStruct* track_reader = NULL;

    if (!self || !minf)
        BAILWITHERROR(MP4BadParamErr)

    MP4MSG("%s addFragTrack nal %d, timescale %d, id %d\n", TYPE(mediaType), nalLengthFieldSize,
           timescale, trackNum);

    track_reader =
            (FragmentedTrackReaderStruct*)MP4LocalCalloc(1, sizeof(FragmentedTrackReaderStruct));

    TESTMALLOC(track_reader)

    self->track_reader[self->track_count] = track_reader;

    track_reader->num = self->track_count;

    self->track_count++;

    track_reader->id = trackNum;
    track_reader->appContext = self->appContext;
    track_reader->ts = 0;
    track_reader->startTime = -1;

    track_reader->minf = minf;
    track_reader->dataReferenceIndex = dataReferenceIndex;
    track_reader->nalLengthFieldSize = nalLengthFieldSize;
    track_reader->timescale = timescale;
    track_reader->bEnable = FALSE;
    track_reader->bEntrypted = FALSE;
    track_reader->bSubsampleEncryption = FALSE;
    track_reader->mediaType = mediaType;
    track_reader->codecData = NULL;
    track_reader->codecDataSize = 0;

    if (trex != NULL) {
        track_reader->default_duration = trex->default_sample_duration;
        track_reader->default_size = trex->default_sample_size;
        track_reader->default_flags = trex->default_sample_flags;
    }
    err = MP4CreateFragmentQueue(&track_reader->queue);
    if (err)
        BAILWITHERROR(MP4BadParamErr)

    err = CheckIsEncryptedTrack(track_reader);
    if (err)
        err = PARSER_SUCCESS;

bail:

    if (err) {
        if (track_reader) {
            MP4ClearQueue(track_reader->queue);
            MP4LocalFree(track_reader->queue);
            MP4LocalFree(track_reader);
        }
    }

    TEST_RETURN(err);
    MP4MSG("addFragmentedTrack ret=%d\n", err);

    return err;
}

MP4Err getFragmentedTrack(FragmentedReaderStruct* self, u32 id,
                          FragmentedTrackReaderStruct** track) {
    FragmentedTrackReaderStruct* track_reader;
    u32 i = 0;
    MP4Err err = MP4InvalidMediaErr;

    if (!self || NULL == track)
        return MP4BadParamErr;

    for (i = 0; i < self->track_count; i++) {
        track_reader = self->track_reader[i];
        if (track_reader->id == id) {
            *track = track_reader;
            err = MP4NoErr;
            break;
        }
    }

    return err;
}

static int32 getNalLength(MP4DataHandlerPtr dhlr, u32 nalLengthFieldSize, u64 fileOffset,
                          u32* nalLength) {
    int32 err = PARSER_SUCCESS;
    u8 nalLengthFieldBuffer[4];
    u32 nal_length = 0;
    u32 k;

    err = dhlr->copyData(dhlr, fileOffset, (char*)nalLengthFieldBuffer, nalLengthFieldSize);
    if (err)
        goto bail;

    for (k = 0; k < nalLengthFieldSize; k++) {
        nal_length = (nal_length << 8) + nalLengthFieldBuffer[k];
    }

bail:
    *nalLength = nal_length;
    // MP4MSG("nal length %ld\n", nal_length);
    return err;
}

static MP4Err NalStartCodeModifyAndMerge(FragmentedTrackReaderStruct* track_reader,
                                         MP4DataHandlerPtr dhlr, uint64 fileOffset,
                                         uint8* sampleBuffer, u32 sampleBufferLen, u32 sampleSize,
                                         u32* outSampleSize) {
    MP4Err err = PARSER_SUCCESS;
    uint32 nalLengthFieldSize = track_reader->nalLengthFieldSize;
    uint32 nalLength;
    u32 bytesLeft = sampleSize;
    u32 dwCount = 0;

    *outSampleSize = 0;

    do {
        err = getNalLength(dhlr, nalLengthFieldSize, fileOffset, &nalLength);

        fileOffset += nalLengthFieldSize;
        bytesLeft -= nalLengthFieldSize;

        if ((0 == nalLength) || (nalLength > bytesLeft) || /* bytes left is less than NALU size */
            (nalLengthFieldSize + nalLength >= bytesLeft)) /* NALU length size is larger than sample
                                                              size after reading current NALU */
        {
            /* nalLength == 0 is illegal, handle this error inside core
             * parser, and return the whole sample left */
            nalLength = bytesLeft;
        }

        // Fix me? we just return err.
        // If try to fix, must process by unit of nal, too sophisticated.
        // Since we assume MAXNALNUM_PERFRAME is 512, so little chance to overflow.
        if (*outSampleSize + NAL_START_CODE_SIZE + nalLength > sampleBufferLen) {
            MP4MSG("NalStartCodeModifyAndMerge, sample size %d is large than alloced buffer len "
                   "%d\n",
                   *outSampleSize + NAL_START_CODE_SIZE + nalLength, sampleBufferLen);

            BAILWITHERROR(PARSER_ERR_UNKNOWN)
        }

        FILL_NAL_START_CODE(sampleBuffer)

                ;
        sampleBuffer += NAL_START_CODE_SIZE;
        *outSampleSize += NAL_START_CODE_SIZE;

        err = dhlr->copyData(dhlr, fileOffset, (char*)sampleBuffer, nalLength);
        if (err)
            goto bail;

        sampleBuffer += nalLength;
        *outSampleSize += nalLength;

        fileOffset += nalLength;
        bytesLeft -= nalLength;

        dwCount++;
        if (dwCount > MAXNALNUM_PERFRAME) {
            BAILWITHERROR(PARSER_ERR_UNKNOWN)
        }

    } while (bytesLeft > 0);

bail:
    return err;
}

static MP4Err parseTrunAtom(FragmentedTrackReaderStruct* track_reader,
                            MP4TrackFragmentRunAtomPtr trunAtomPtr) {
    MP4Err err = PARSER_SUCCESS;
    u32 flags = 0;
    s32 data_offset = 0;
    bool ismv = FALSE;
    u32 i;
    u64 curTs = 0;
    u32 prevSampleDur = 0;
    MP4TrackFragmentRunAtomPtr trun = trunAtomPtr;

    if (track_reader == NULL || trun == NULL)
        BAILWITHERROR(MP4BadParamErr)

    flags = trun->flags;
#define FLAG_DATA_OFFSET 0x000001
#define FLAG_SAMPLE_DESCRIPTION_INDEX 0x000002
#define FLAG_FIRST_SAMPLE 0x000004
#define FLAG_SAMPLE_DURATION 0x000100
#define FLAG_SAMPLE_SIZE 0x000200
#define FLAG_SAMPLE_FLAG 0x000400
#define FLAG_COMPOSITION_TIME_OFFSET 0x000800
#define FLAG_EMPTY_DURATION 0x100000

    MP4MSG("%s parseTrunAtom flags=%x count=%d enabled=%d\n", TYPE(track_reader->mediaType), flags,
           trun->sample_count, track_reader->bEnable);

    if (flags & FLAG_DATA_OFFSET) {
        /* note this is really signed */
        data_offset = trun->data_offset;

        /* default base offset = first byte of moof */
        if (track_reader->base_offset == -1) {
            track_reader->base_offset = track_reader->moof_offset;
        }
        track_reader->running_offset = track_reader->base_offset + data_offset;
    } else {
        /* if no offset at all, that would mean data starts at moof start,
         * which is a bit wrong and is ismv crappy way, so compensate
         * assuming data is in mdat following moof */
        if (track_reader->base_offset == -1) {
            track_reader->base_offset = track_reader->moof_offset + track_reader->moof_length + 8;
            ismv = TRUE;
        }
        if (track_reader->running_offset == -1)
            track_reader->running_offset = track_reader->base_offset;
    }

    for (i = 0; i < trun->sample_count; i++) {
        MP4TrackFragmentSample sample = *(trun->sample + i);
        MP4TrackFragmentSample sample_to_queue;

        u32 dur;
        u32 size;
        u32 sflags;
        bool keyFrame;

        memset(&sample_to_queue, 0, sizeof(MP4TrackFragmentSample));
        /* first read sample data */
        if (flags & FLAG_SAMPLE_DURATION) {
            dur = sample.duration;
        } else {
            dur = track_reader->default_duration;
        }
        if (flags & FLAG_SAMPLE_SIZE) {
            size = sample.size;
        } else {
            size = track_reader->default_size;
        }
        if (flags & FLAG_FIRST_SAMPLE) {
            if (i == 0) {
                sflags = trun->first_sample_flags;
            } else {
                sflags = track_reader->default_flags;
            }
        } else if (flags & FLAG_SAMPLE_FLAG) {
            sflags = sample.flags;
        } else {
            sflags = track_reader->default_flags;
        }

        /* fill the sample information */
        sample_to_queue.offset = track_reader->running_offset;
        sample_to_queue.size = size;
        sample_to_queue.duration = dur;
        /* sample-is-difference-sample */
        /* ismv seems to use 0x40 for keyframe, 0xc0 for non-keyframe,
         * now idea how it relates to bitfield other than massive LE/BE confusion */
        keyFrame = ismv ? ((sflags & 0xff) == 0x40) : !(sflags & 0x10000);

        sample_to_queue.flags = keyFrame ? FLAG_SYNC_SAMPLE : 0;
        sample_to_queue.compositionOffset = sample.compositionOffset;

        if (track_reader->startTime >= 0) { /* calculate ts from tfdt start time */
            if (i == 0)
                curTs = track_reader->startTime;
            else
                curTs += prevSampleDur;
            sample_to_queue.ts = curTs;
            prevSampleDur = dur;
        } else {
            sample_to_queue.ts = -1;
        }

        MP4MSG("parseTrunAtom addOneSample i=%d/%d offset=%lld,size=%d,ts=%lld,dur=%d,flag=%x,c "
               "offset=%d\n",
               i, trun->sample_count, sample_to_queue.offset, sample_to_queue.size,
               (long long)sample_to_queue.ts, sample_to_queue.duration, sample_to_queue.flags,
               sample_to_queue.compositionOffset);

        track_reader->running_offset += size;

        // timestamp += dur;
        if (track_reader->bEnable)
            MP4AddOneSample(track_reader->queue, &sample_to_queue);
    }

bail:
    return err;
}

static MP4Err parseCencAuxiliaryInformation(FragmentedTrackReaderStruct* track_reader,
                                            MP4SampleAuxiliaryInfoSizeAtomPtr saizPtr,
                                            MP4SampleAuxiliaryInfoOffsetsAtomPtr saioPtr) {
    MP4Err err = PARSER_SUCCESS;
    u32 sampleQueueSize = 0;
    u32 sampleCount = 0;
    u32 i, j;
    u64 offset = 0;
    u32 infoSize = 0;
    MP4TrackFragmentSample sample;
    MP4MediaInformationAtomPtr minf;
    MP4DataHandlerPtr dhlr;
    if (track_reader == NULL || saizPtr == NULL || saioPtr == NULL)
        BAILWITHERROR(MP4BadParamErr)

    sampleCount = saizPtr->sample_count;

    MP4MSG("[%p]parseCencAuxiliaryInformation BEGIN sample count=%d", track_reader, sampleCount);

    err = saioPtr->getSampleOffsets(saioPtr, 0, &offset);
    if (err)
        BAILWITHERROR(MP4BadParamErr)

    MP4MSG("[%p]parseCencAuxiliaryInformation offset=%lld,moof=%lld", track_reader, offset,
           track_reader->moof_offset);
    offset += track_reader->moof_offset;

    // IV_size only support 0,8,16
    if (track_reader->default_IV_size != 0 && track_reader->default_IV_size != 8 &&
        track_reader->default_IV_size != 16)
        BAILWITHERROR(MP4BadParamErr)

    err = MP4GetQueueSize(track_reader->queue, &sampleQueueSize);
    if (err)
        BAILWITHERROR(MP4BadParamErr)

    minf = track_reader->minf;
    err = minf->openDataHandler((MP4AtomPtr)minf, track_reader->dataReferenceIndex);
    if (err)
        goto bail;
    dhlr = (MP4DataHandlerPtr)minf->dataHandler;

    MP4MSG("[%p]parseCencAuxiliaryInformation sampleQueueSize=%d,default_IV_size=%d", track_reader,
           sampleQueueSize, track_reader->default_IV_size);

    for (i = 0; i < sampleCount; i++) {
        err = MP4GetQueueSize(track_reader->queue, &sampleQueueSize);
        if (err)
            BAILWITHERROR(MP4BadParamErr)

        if (i >= sampleQueueSize) {
            MP4MSG("BREAK SAMPLE SIZE=%d", sampleQueueSize);
            break;
        }
        memset(&sample, 0, sizeof(MP4TrackFragmentSample));

        err = dhlr->copyData(dhlr, offset, (char*)sample.iv, track_reader->default_IV_size);
        if (err) {
            BAILWITHERROR(MP4IOErr)
        }

        offset += track_reader->default_IV_size;
        infoSize = saizPtr->default_sample_info_size;
        if (infoSize == 0)
            infoSize = (u32)saizPtr->sample_info_size[i];
        if (infoSize > track_reader->default_IV_size) {
            u16 subSampleNum;
            sample.drm_flags = FRAGMENT_SAMPLE_DRM_FLAG_SUB;

            err = dhlr->copyData(dhlr, offset, (char*)&subSampleNum, sizeof(u16));
            reverse_endian_u16(&subSampleNum, 1);

            if (err)
                BAILWITHERROR(MP4IOErr)
            offset += 2;
            for (j = 0; j < subSampleNum; j++) {
                u16 clearDataSize;
                u32 encryptedDataSize;
                if (j >= FRAGMENT_SAMPLE_DRM_MAX_SUBSAMPLE)
                    break;

                err = dhlr->copyData(dhlr, offset, (char*)&clearDataSize, sizeof(u16));
                if (err) {
                    BAILWITHERROR(MP4IOErr)
                }

                reverse_endian_u16(&clearDataSize, 1);

                offset += 2;
                sample.clearArray[j] = clearDataSize;
                sample.clearsSize++;

                err = dhlr->copyData(dhlr, offset, (char*)&encryptedDataSize, sizeof(u32));
                if (err) {
                    BAILWITHERROR(MP4IOErr)
                }

                reverse_endian_u32(&encryptedDataSize, 1);

                offset += 4;
                sample.encryptedArray[j] = encryptedDataSize;
                sample.encryptedSize++;
                MP4MSG("parseCencAuxiliaryInformation clear=%d,enc=%d", clearDataSize,
                       encryptedDataSize);
            }
        } else {
            sample.drm_flags = FRAGMENT_SAMPLE_DRM_FLAG_FULL;
        }

        err = MP4UpdateSampleDRMInfo(track_reader->queue, i, &sample);
        if (err) {
            BAILWITHERROR(MP4BadParamErr)
        }
    }

    MP4MSG("[%p]parseCencAuxiliaryInformation END i=%d err=%d", track_reader, i, err);
bail:
    return err;
}

static MP4Err parseSampleEncryption(FragmentedTrackReaderStruct* track_reader,
                                    MP4SampleEncryptionAtomPtr senc) {
    MP4Err err = PARSER_SUCCESS;
    u32 sampleQueueSize = 0;
    u32 sampleCount = 0;
    u32 defaultIVSize = 0;
    u32 i = 0, j;
    u32 offset = 0;
    u16 subSampleCount = 0;
    u8* sencDataPtr = NULL;
    MP4TrackFragmentSample sample;
    bool readEncryption = FALSE;

    if (track_reader == NULL || senc == NULL)
        BAILWITHERROR(MP4BadParamErr)

    sampleCount = 0;

    err = senc->getSampleCount(senc, &sampleCount);
    if (err)
        BAILWITHERROR(MP4BadParamErr)

    // IV_size only support 0,8,16
    if (track_reader->default_IV_size != 0 && track_reader->default_IV_size != 8 &&
        track_reader->default_IV_size != 16)
        BAILWITHERROR(MP4BadParamErr)

    defaultIVSize = track_reader->default_IV_size;

    err = MP4GetQueueSize(track_reader->queue, &sampleQueueSize);
    if (err)
        BAILWITHERROR(MP4BadParamErr)

    sencDataPtr = senc->data;

    if (senc->bSubsampleEncryption && senc->dataSize > 0)
        readEncryption = TRUE;

    MP4MSG("SampleEncryption sampleCount=%d,defaultIVSize=%d,readEncryption=%d", sampleCount,
           defaultIVSize, readEncryption);

    for (i = 0; i < sampleCount; i++) {
        if (i >= sampleQueueSize) {
            MP4MSG("BREAK sampleQueueSize=%d", sampleQueueSize);
            break;
        }
        memset(&sample, 0, sizeof(MP4TrackFragmentSample));

        if (senc->dataSize > 0 && offset + track_reader->default_IV_size >= senc->dataSize) {
            MP4MSG("BREAK offset=%d,dataSize=%d", offset, senc->dataSize);
            break;
        }

        if (defaultIVSize > 0) {
            memcpy((void*)&sample.iv, (void*)&sencDataPtr[offset], defaultIVSize);

            offset += track_reader->default_IV_size;
        }

        if (senc->dataSize > 0 && offset >= senc->dataSize) {
            MP4MSG("BREAK offset=%d,dataSize=%d", offset, senc->dataSize);
            break;
        }

        // readSubsamples
        if (readEncryption) {
            memcpy((void*)&subSampleCount, (void*)&sencDataPtr[offset], sizeof(u16));
            reverse_endian_u16(&subSampleCount, 1);
            offset += 2;

            if (senc->dataSize > 0 && offset + subSampleCount * 6 > senc->dataSize) {
                MP4MSG("BREAK offset=%d,dataSize=%d", offset, senc->dataSize);
                break;
            }

            MP4MSG("get subSampleCount=%d", subSampleCount);

            if (subSampleCount >= FRAGMENT_SAMPLE_DRM_MAX_SUBSAMPLE) {
                MP4MSG("subSampleCount > 16");
            }

            for (j = 0; j < subSampleCount; j++) {
                u16 clear;
                u32 encrypted;

                if (j >= FRAGMENT_SAMPLE_DRM_MAX_SUBSAMPLE)
                    break;

                memcpy((void*)&clear, (void*)&sencDataPtr[offset], sizeof(u16));
                reverse_endian_u16(&clear, 1);
                offset += 2;

                sample.clearArray[j] = clear;
                sample.clearsSize++;

                memcpy((void*)&encrypted, (void*)&sencDataPtr[offset], sizeof(u32));
                reverse_endian_u32(&encrypted, 1);
                offset += 4;

                sample.encryptedArray[j] = encrypted;
                sample.encryptedSize++;
                MP4MSG("clear=%d,encrypted=%d", clear, encrypted);
            }

            sample.drm_flags = FRAGMENT_SAMPLE_DRM_FLAG_SUB;
        } else
            sample.drm_flags = FRAGMENT_SAMPLE_DRM_FLAG_FULL;

        err = MP4UpdateSampleDRMInfo(track_reader->queue, i, &sample);
        if (err) {
            BAILWITHERROR(MP4BadParamErr)
        }
    }

bail:
    MP4MSG("[%p]SampleEncryption END i=%d err=%d", track_reader, i, err);
    return err;
}

static MP4Err parseMoofAtom(FragmentedReaderStruct* reader, MP4AtomPtr anAtomPtr) {
    MP4Err err = PARSER_SUCCESS;

    u32 default_size = 0, default_duration = 0, default_flags = 0;
    u64 base_offset, running_offset;
    s64 moof_offset = 0;
    u32 track_count = 0;
    u32 i, j;
    MP4AtomPtr atom = NULL;
    MP4MovieFragmentAtomPtr moof = (MP4MovieFragmentAtomPtr)anAtomPtr;
    MP4TrackFragmentAtomPtr traf = NULL;
    MP4TrackFragmentHeaderAtomPtr tfhd = NULL;
    MP4TrackFragmentDecodeTimeAtomPtr tfdt = NULL;

    MP4TrackFragmentRunAtomPtr trun = NULL;
    FragmentedTrackReaderStruct* track_reader = NULL;

    base_offset = running_offset = -1;
    track_count = moof->getTrackCount(moof);
    err = moof->getStartOffset(moof, &reader->curr_moof_offset);
    if (err)
        goto bail;
    moof_offset = reader->curr_moof_offset;

    for (i = 1; i <= track_count; i++) {
        u32 trun_count = 0;
        err = moof->getTrack(moof, i, &atom);
        traf = (MP4TrackFragmentAtomPtr)atom;
        if (err)
            goto bail;

        tfhd = (MP4TrackFragmentHeaderAtomPtr)traf->tfhd;
        tfdt = (MP4TrackFragmentDecodeTimeAtomPtr)traf->tfdt;

        if (tfhd == NULL)
            continue;

        err = tfhd->getInfo(tfhd, &base_offset, &default_duration, &default_size, &default_flags);
        if (err)
            goto bail;

        trun_count = traf->getTrunCount(traf);

        if (MP4NoErr != getFragmentedTrack(reader, tfhd->track_ID, &track_reader))
            continue;

        MP4MSG("track %d %s parseMoofAtom offset=%lld\n", i, TYPE(track_reader->mediaType),
               moof_offset);

        track_reader->moof_offset = moof_offset;
        track_reader->moof_length = (u32)moof->size;
        track_reader->base_offset = base_offset;
        track_reader->running_offset = running_offset;
        if (default_duration > 0)
            track_reader->default_duration = default_duration;
        if (default_size > 0)
            track_reader->default_size = default_size;
        if (default_flags > 0)
            track_reader->default_flags = default_flags;

        if (tfdt) {
            track_reader->ts = tfdt->time;
            track_reader->startTime = tfdt->time;
        } else
            track_reader->startTime = -1;

        for (j = 1; j <= trun_count; j++) {
            err = traf->getTrun(traf, j, &atom);
            if (err)
                goto bail;
            trun = (MP4TrackFragmentRunAtomPtr)atom;
            err = parseTrunAtom(track_reader, trun);
            if (err)
                goto bail;
        }

        if (traf->senc != NULL) {
            MP4MSG("[%p]parseMoofAtom parseSampleEncryption\n", track_reader);
            err = parseSampleEncryption(track_reader, (MP4SampleEncryptionAtomPtr)traf->senc);
        } else {
            MP4MSG("parseMoofAtom did not find senc\n");
        }

        if (track_reader->bEntrypted && traf->saiz != NULL && traf->saio != NULL) {
            err = parseCencAuxiliaryInformation(track_reader,
                                                (MP4SampleAuxiliaryInfoSizeAtomPtr)traf->saiz,
                                                (MP4SampleAuxiliaryInfoOffsetsAtomPtr)traf->saio);
        }

        running_offset = track_reader->running_offset;
        base_offset = running_offset;
        running_offset = -1;
    }
    reader->curr_moof_offset = base_offset;

    if (moof_offset == reader->curr_moof_offset) {
        reader->curr_moof_offset += moof->size;
    }
    MP4MSG("parseMoofAtom end new moof offset=%lld\n", reader->curr_moof_offset);

bail:
    return err;
}

// get timescale and default duration from movie atom
static MP4Err updateTrackInfo(FragmentedReaderStruct* reader, MP4AtomPtr atomPtr) {
    MP4Err err = MP4NoErr;
    FragmentedTrackReaderStruct* track = NULL;
    MP4AtomPtr aTrack;
    MP4MediaAtomPtr media;
    MP4TrackAtomPtr trackAtom;
    MP4MovieAtomPtr movieAtom;
    u32 i;
    MP4MediaHeaderAtomPtr mediaHeader;
    MP4MovieExtendsAtomPtr mvex;
    MP4TrackExtendsAtomPtr trex;

    MP4MSG("updateTrackInfo\n");

    for (i = 0; i < reader->track_count; i++) {
        if (reader->track_reader[i]->mediaType == MEDIA_VIDEO) {
            track = reader->track_reader[i];
            break;
        }
    }
    if (track == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);

    movieAtom = (MP4MovieAtomPtr)atomPtr;
    err = movieAtom->getIndTrack(movieAtom, i + 1 /*trackIndex*/, &aTrack);
    if (err)
        goto bail;
    if (err)
        goto bail;

    trackAtom = (MP4TrackAtomPtr)aTrack;
    media = (MP4MediaAtomPtr)trackAtom->trackMedia;
    mediaHeader = (MP4MediaHeaderAtomPtr)media->mediaHeader;
    MP4MSG("update timescale from %d to %d\n", track->timescale, mediaHeader->timeScale);
    track->timescale = mediaHeader->timeScale;

    mvex = (MP4MovieExtendsAtomPtr)movieAtom->mvex;
    err = mvex->getTrex(mvex, 0 /*track_id*/, &trex);
    if (err)
        goto bail;

    MP4MSG("udpate default duration from %d to %d\n", track->default_duration,
           trex->default_sample_duration);
    track->default_duration = trex->default_sample_duration;

bail:
    return err;
}

static MP4Err saveCodecData(FragmentedReaderStruct* reader, MP4AtomPtr atomPtr) {
    MP4Err err = MP4NoErr;
    FragmentedTrackReaderStruct* track = NULL;
    MP4AtomPtr aTrack;
    MP4Media media;
    MP4TrackAtomPtr trackAtom;
    MP4MovieAtomPtr movieAtom;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4SampleDescriptionAtomPtr stsd;
    GenericSampleEntryAtomPtr entry;
    MP4AVCCAtomPtr AVCC = NULL;
    MP4HVCCAtomPtr HVCC = NULL;
    u32 mediaType;
    u32 decoderType;
    u32 decoderSubtype;
    u32 i;
    char* data = NULL;
    u32 dataSize = 0;

    MP4MSG("saveCodecData\n");

    for (i = 0; i < reader->track_count; i++) {
        if (reader->track_reader[i]->mediaType == MEDIA_VIDEO) {
            track = reader->track_reader[i];
            break;
        }
    }
    if (track == NULL)
        return MP4InvalidMediaErr;

    movieAtom = (MP4MovieAtomPtr)atomPtr;
    err = movieAtom->getIndTrack(movieAtom, i + 1 /*trackIndex*/, &aTrack);
    if (err)
        goto bail;
    trackAtom = (MP4TrackAtomPtr)aTrack;
    media = (MP4Media)trackAtom->trackMedia;

    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)media)->information;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    if (stbl == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    stsd = (MP4SampleDescriptionAtomPtr)stbl->SampleDescription;
    if (stsd == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    err = stsd->getEntry(stsd, 1 /*sampleDescIndex*/, &entry);
    if (err)
        goto bail;
    if (entry == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    GetDecoderType(entry->type, &mediaType, &decoderType, &decoderSubtype);

    if (mediaType == MEDIA_VIDEO) {
        AVCC = (MP4AVCCAtomPtr)entry->AVCCAtomPtr;
        HVCC = (MP4HVCCAtomPtr)entry->HVCCAtomPtr;
        if (AVCC != NULL && AVCC->data != NULL && AVCC->dataSize > 0) {
            data = (char*)AVCC->data;
            dataSize = AVCC->dataSize;
        } else if (HVCC != NULL && HVCC->data != NULL && HVCC->dataSize > 0) {
            data = (char*)HVCC->data;
            dataSize = HVCC->dataSize;
        }
        if (data != NULL && dataSize > 0) {
            MP4MSG("codecData size %d\n", dataSize);
            track->codecData = MP4LocalCalloc(1, dataSize);
            memcpy(track->codecData, data, dataSize);
            track->codecDataSize = dataSize;
        }
    }
bail:
    return err;
}

static MP4Err findMoofAtom(FragmentedReaderStruct* reader, MP4InputStreamPtr inputStream,
                           char* headerBuf) {
    MP4Err err = PARSER_SUCCESS;
    u32 distance = 0;
    bool possibleHeader = FALSE;
    u32 i = 0;
    u32 readNum = 0;
    clock_t t;

    err = inputStream->readData(inputStream, 8, headerBuf, NULL);
    if (err)
        goto bail;

    MP4MSG("%s find moof atom begin %02x%02x%02x%02x %02x%02x%02x%02x\n",
           TYPE(reader->track_reader[0]->mediaType), headerBuf[0], headerBuf[1], headerBuf[2],
           headerBuf[3], headerBuf[4], headerBuf[5], headerBuf[6], headerBuf[7]);

    t = clock();

    do {
        if (!memcmp(headerBuf + 4, "moof", 4)) {
            break;
        }
        if (!memcmp(headerBuf + 4, "moov", 4)) {
            MP4AtomPtr anAtomPtr = NULL;
            MP4MSG("get new moov atom !!!\n");
            err = MP4ParseAtomInBuf(inputStream, &anAtomPtr, headerBuf);
            if (err == MP4EOF) {
                MP4MSG("%s MP4ParseAtom fail for EOF\n", type);
                goto bail;
            }
            saveCodecData(reader, anAtomPtr);
            updateTrackInfo(reader, anAtomPtr);
            anAtomPtr->destroy(anAtomPtr);
        }

        possibleHeader = FALSE;
        for (i = 4; i < 8; i++) {
            if (headerBuf[i] == 'm') {
                possibleHeader = TRUE;
                break;
            }
        }

        // when 'm' is found, read data 1 by 1, most of time read 4 bytes each time to speed up
        // searching for a range of 200KB, searching consumes about 1.5 seconds
        readNum = possibleHeader ? 1 : 4;
        memmove(headerBuf, headerBuf + readNum, 8 - readNum);
        err = inputStream->readData(inputStream, readNum, headerBuf + 8 - readNum, NULL);
        if (err)
            goto bail;
        distance += readNum;
    } while (1);

    t = clock() - t;
    MP4MSG("%s moof atom found at distance %d, consume %f seconds\n", type, distance,
           (float)t / CLOCKS_PER_SEC);

bail:
    return err;
}

static MP4Err findAndParseMoofAtom(FragmentedReaderStruct* reader) {
    MP4Err err = PARSER_SUCCESS;
    bool find = FALSE;
    MP4InputStreamPtr inputStream = reader->inputStream;
    MP4AtomPtr anAtomPtr = NULL;
    MP4PrivateMovieRecordPtr moov = reader->moov;

    MP4MSG("%s findAndParseMoofAtom\n", TYPE(reader->track_reader[0]->mediaType));

    // first moof is read when initializing whole stream
    if (moov->first_moof) {
        MP4MSG("%s findAndParseMoofAtom got first moof\n",
               TYPE(reader->track_reader[0]->mediaType));
        parseMoofAtom(reader, moov->first_moof);
        moov->first_moof->destroy(moov->first_moof);
        moov->first_moof = NULL;
        goto bail;
    }

    if (inputStream->stream_flags & live_flag) {
        char headerBuf[8];
        err = findMoofAtom(reader, inputStream, headerBuf);
        if (err) {
            MP4MSG("%s findMoofAtom fail for %d\n", type, err);
            goto bail;
        }

        MP4MSG("%s findAndParseMoofAtom MP4ParseAtom\n", type);

        err = MP4ParseAtomInBuf(inputStream, &anAtomPtr, headerBuf);
        if (err == MP4EOF) {
            MP4MSG("%s MP4ParseAtom fail for EOF\n", type);
            goto bail;
        }

        parseMoofAtom(reader, anAtomPtr);
        anAtomPtr->destroy(anAtomPtr);

        goto bail;
    }

    MP4MSG("%s findAndParseMoofAtom seek to curr_moof_offset %lld\n", type,
           reader->curr_moof_offset);
    err = inputStream->seekTo(inputStream, reader->curr_moof_offset, SEEK_SET, "seek to offset");
    if (err)
        goto bail;

    do {
        err = MP4ParseAtom(inputStream, &anAtomPtr);
        if (err == MP4EOF) {
            MP4MSG("%s MP4ParseAtom fail for EOF\n", type);
            goto bail;
        }

        if ((err && (MP4EOF != err)) || (!anAtomPtr)) {
            MP4MSG("%s MP4ParseAtom fail err %x, atomPtr %p\n", type, err, anAtomPtr);
            goto bail;
        }

        if (anAtomPtr->type == MP4MovieFragmentAtomType) {
            MP4MSG("%s findAndParseMoofAtom moof found!\n", type);

            // parse moof
            parseMoofAtom(reader, anAtomPtr);

            find = TRUE;
            anAtomPtr->destroy(anAtomPtr);

        } else if (anAtomPtr->type == MP4MovieFragmentRandomAccessAtomType) {
            MP4MSG("%s findAndParseMoofAtom mfra found!\n", type);
            err = MP4EOF;
            find = TRUE;
            anAtomPtr->destroy(anAtomPtr);

        } else {
            // MP4UnknownAtomPtr self = (MP4UnknownAtomPtr) anAtomPtr;
            anAtomPtr->destroy(anAtomPtr);
        }

    } while (!find);

bail:
    return err;
}

static MP4Err clearAllQueues(FragmentedReaderStruct* self) {
    MP4Err err = MP4NoErr;
    u32 i;
    FragmentedTrackReaderStruct* track_reader = NULL;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr);

    for (i = 0; i < self->track_count; i++) {
        track_reader = self->track_reader[i];

        if (track_reader && track_reader->queue) {
            MP4ClearQueue(track_reader->queue);
        }
    }

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err ResetAllTrackTS(FragmentedReaderStruct* self, u64 ts) {
    MP4Err err = MP4NoErr;
    u32 i;
    FragmentedTrackReaderStruct* track_reader = NULL;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr);

    for (i = 0; i < self->track_count; i++) {
        track_reader = self->track_reader[i];
        track_reader->ts = ts * track_reader->timescale / 1000000;
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err getFragmentedTrackNextSyncSample(FragmentedReaderStruct* reader, uint32 direction,
                                        u32 trackNum, uint8** outBuffer, void** bufferContext,
                                        uint32* outSize, uint64* outTime, uint64* outDuration,
                                        uint32* sampleFlags) {
    MP4Err err = MP4NoErr;
    FragmentedTrackReaderStruct* track_reader;
    u64 seekTime;
    u64 offset;
    u64 usStartTime;
    u64 usDuration;
    u64 currentMoofOffset;
    if (reader == NULL || outSize == NULL || outTime == NULL || outDuration == NULL ||
        sampleFlags == NULL)
        BAILWITHERROR(MP4BadParamErr)

    track_reader = reader->track_reader[trackNum];
    usStartTime = *outTime;
    usDuration = *outDuration;

    if (reader->sidx == NULL && reader->mfra == NULL) {
        BAILWITHERROR(PARSER_ERR_NOT_SEEKABLE)
    }

    if (track_reader->sampleBytesLeft) {
        err = getFragmentedTrackNextSample(reader, trackNum, outBuffer, bufferContext, outSize,
                                           &usStartTime, &usDuration, sampleFlags);
        *outTime = usStartTime;     // * 1000000 / track_reader->timescale;
        *outDuration = usDuration;  // * 1000000/track_reader->timescale;

        goto bail;
    }

    err = clearAllQueues(reader);
    if (err)
        goto bail;

    if (reader->mfra) {
        MP4MovieFragmentRandomAccessAtomPtr mfra =
                (MP4MovieFragmentRandomAccessAtomPtr)reader->mfra;
        MP4TrackFragmentRandomAccessAtomPtr tfra = NULL;
        FragmentedTrackReaderStruct* track_reader = NULL;
        MP4AtomPtr atom = NULL;
        u64 targetTime = 0;
        u32 track_count = mfra->getTrackCount(mfra);

        if (track_count == 0)
            BAILWITHERROR(PARSER_ERR_NOT_SEEKABLE);

        // use the first track to seek
        err = mfra->getTrack(mfra, 1, &atom);

        if (err) {
            BAILWITHERROR(PARSER_ERR_NOT_SEEKABLE);
        }
        tfra = (MP4TrackFragmentRandomAccessAtomPtr)atom;

        err = getFragmentedTrack(reader, tfra->id, &track_reader);
        if (err)
            BAILWITHERROR(PARSER_ERR_NOT_SEEKABLE);

        offset = reader->curr_moof_offset;
        if (direction == FLAG_FORWARD) {
            err = tfra->nextIndex(tfra, &seekTime, &offset);
        } else {
            err = tfra->previousIndex(tfra, &seekTime, &offset);
        }
        if (err)
            goto bail;

        currentMoofOffset = reader->curr_moof_offset = offset;
        targetTime = seekTime * 1000000 / track_reader->timescale;
        ResetAllTrackTS(reader, targetTime);
        err = getFragmentedTrackNextSample(reader, trackNum, outBuffer, bufferContext, outSize,
                                           &usStartTime, &usDuration, sampleFlags);
        reader->curr_moof_offset = currentMoofOffset;
    } else if (reader->sidx) {
        MP4SegmentIndexAtomPtr sidx = (MP4SegmentIndexAtomPtr)reader->sidx;
        offset = reader->curr_moof_offset - reader->first_moof_offset;
        if (direction == FLAG_FORWARD) {
            err = sidx->nextIndex(sidx, &seekTime, &offset);
        } else {
            err = sidx->previousIndex(sidx, &seekTime, &offset);
        }
        if (err)
            goto bail;
        currentMoofOffset = reader->curr_moof_offset = reader->first_moof_offset + offset;
        ResetAllTrackTS(reader, seekTime);
        err = getFragmentedTrackNextSample(reader, trackNum, outBuffer, bufferContext, outSize,
                                           &usStartTime, &usDuration, sampleFlags);
        reader->curr_moof_offset = currentMoofOffset;
    }

    if (0 == track_reader->sampleBytesLeft)
        clearAllQueues(reader);

    *outTime = usStartTime;     // * 1000000 / track_reader->timescale;
    *outDuration = usDuration;  // * 1000000/track_reader->timescale;

    MP4MSG("getTrackNextSyncSample "
           "trackNum=%d,outSize=%d,usStartTime=%lld,usDuration=%lld,flag=%x\n",
           trackNum, *outSize, *outTime, *outDuration, *sampleFlags);

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err getFragmentedRemainingBytes(FragmentedTrackReaderStruct* track_reader,
                                          uint32 trackNum, uint8** outBuffer, void** bufferContext,
                                          uint32* outSize, uint64* usStartTime, uint64* usDuration,
                                          uint32* sampleFlags, uint32 parserFlags) {
    int32 err = PARSER_SUCCESS;

    MP4MediaInformationAtomPtr minf;
    MP4DataHandlerPtr dhlr;

    uint8* sampleBuffer = NULL;

    u32 bufferSize;
    u32 bufferSizeRequested;
    u32 dataSize;
    u64 fileOffset;
    bool isH264;
    bool isNewNAL = FALSE;
    uint32 nalLengthFieldSize = track_reader->nalLengthFieldSize;
    uint32 nalLength;

    *outSize = 0;
    *outBuffer = NULL;

    minf = track_reader->minf;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    dhlr = (MP4DataHandlerPtr)minf->dataHandler; /* use original handler */
    if (dhlr == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    fileOffset = track_reader->sampleFileOffset;

    isH264 = nalLengthFieldSize ? TRUE : FALSE;
    if (isH264) {
        bufferSizeRequested = track_reader->nalBytesLeft;
        // MP4MSG("avc sample %d not finished, bytes left %d, nal bytes left %ld\n", sampleNumber,
        // stream->sampleBytesLeft, stream->nalBytesLeft);

        if (!bufferSizeRequested) /* read a new NAL */
        {
            isNewNAL = TRUE;
            err = getNalLength(dhlr, nalLengthFieldSize, fileOffset, &nalLength);
            if (err)
                goto bail;
            fileOffset += nalLengthFieldSize;
            track_reader->sampleBytesLeft -= nalLengthFieldSize;
            if ((0 == nalLength) || /* zero-size NALU */
                (nalLength >
                 track_reader->sampleBytesLeft) || /* bytes left is less than NALU size */
                (nalLengthFieldSize >= track_reader->sampleBytesLeft -
                                               nalLength)) /* NALU length size is larger than bytes
                                                              left after reading current NALU */
            {
                /* nalLength == 0 is illegal, handle this error inside core
                 * parser, and return the whole sample left */
                nalLength = track_reader->sampleBytesLeft;
            }
            track_reader->nalBytesLeft = nalLength;
            bufferSizeRequested = nalLength + NAL_START_CODE_SIZE;
        }
    } else
        bufferSizeRequested = track_reader->sampleBytesLeft;

    bufferSize = bufferSizeRequested;
    sampleBuffer =
            MP4LocalRequestBuffer(trackNum, &bufferSize, bufferContext, track_reader->appContext);

    if (isNewNAL) {
        FILL_NAL_START_CODE(sampleBuffer)
        sampleBuffer += NAL_START_CODE_SIZE;
        bufferSizeRequested -= NAL_START_CODE_SIZE;
        bufferSize -= NAL_START_CODE_SIZE;
        *outSize = NAL_START_CODE_SIZE;
    }

    dataSize = bufferSizeRequested <= bufferSize ? bufferSizeRequested : bufferSize;

    err = dhlr->copyData(dhlr, fileOffset, (char*)sampleBuffer, dataSize);

    // MA-13712 testStagefright_cve_2015_3873_b_21814993: do assignment anyway
    // so that this buffer can be released correctly when meet error
    *outBuffer = sampleBuffer;

    if (err)
        goto bail;

    *outSize += dataSize;

    fileOffset += dataSize;
    track_reader->sampleBytesLeft -= dataSize;
    if (isH264) {
        track_reader->nalBytesLeft -= dataSize;
    }

    if (track_reader->sampleBytesLeft) /* still not finished */
    {
        track_reader->sampleFileOffset = fileOffset;
    }

    if (track_reader->streamSample.ts >= 0)
        *usStartTime = track_reader->streamSample.ts;
    else
        *usStartTime = track_reader->ts;
    if (parserFlags & FLAG_OUTPUT_PTS) {
        *usStartTime += (u64)track_reader->streamSample.compositionOffset;
    }
    *usDuration = track_reader->streamSample.duration;
    *sampleFlags = track_reader->streamSample.flags;

bail:

    return err;
}

MP4Err getFragmentedTrackNextSample(FragmentedReaderStruct* reader, u32 trackNum, uint8** outBuffer,
                                    void** bufferContext, uint32* outSize, uint64* usStartTime,
                                    uint64* usDuration, uint32* sampleFlags) {
    MP4Err err = MP4NoErr;
    FragmentedTrackReaderStruct* track_reader;
    u32 sample_num;
    MP4MediaInformationAtomPtr minf;
    MP4DataHandlerPtr dhlr;

    bool isH264;
    uint32 nalLengthFieldSize;
    uint32 nalLength;
    u32 sampleSize = 0;
    u8* sampleBuffer = NULL;
    u32 bufferSizeRequested;
    u32 bufferSize;
    u64 fileOffset;
    u32 dataSize;
    u32 cnt = 0;

    MP4MSG("getFragmentedTrackNextSample\n");

    if (reader == NULL || outSize == NULL || usStartTime == NULL || usDuration == NULL ||
        sampleFlags == NULL)
        BAILWITHERROR(MP4BadParamErr)

    track_reader = reader->track_reader[trackNum];
    nalLengthFieldSize = track_reader->nalLengthFieldSize;
    err = MP4GetQueueSize(track_reader->queue, &sample_num);

    if (err)
        goto bail;

    while (cnt < MAX_GET_QUEUE_SIZE_CNT) {
        if (sample_num == 0) {
            // find and parse moof atom
            findAndParseMoofAtom(reader);
            err = MP4GetQueueSize(track_reader->queue, &sample_num);
            if (err)
                goto bail;
        }
        if (sample_num > 0)
            break;
        else
            cnt++;
    }

    if (sample_num == 0) {
        reader->lastSeekTs = -1;
        MP4MSG("%s getTrackNextSample sample_num 0\n", TYPE(track_reader->mediaType));
        BAILWITHERROR(MP4EOF)
    }

    if (track_reader->sampleBytesLeft) {
        err = getFragmentedRemainingBytes(track_reader, trackNum, outBuffer, bufferContext, outSize,
                                          usStartTime, usDuration, sampleFlags, reader->flags);
        fileOffset = track_reader->sampleFileOffset;
        goto bail;
    } else if (track_reader->codecData) {
        bufferSize = track_reader->codecDataSize;
        sampleBuffer = MP4LocalRequestBuffer(trackNum, &bufferSize, bufferContext,
                                             track_reader->appContext);
        if (bufferSize >= track_reader->codecDataSize) {
            memcpy(sampleBuffer, track_reader->codecData, track_reader->codecDataSize);
            MP4LocalFree(track_reader->codecData);
            track_reader->codecData = NULL;
            *sampleFlags |= FLAG_SAMPLE_CODEC_DATA;
            *outSize = track_reader->codecDataSize;
            //*usStartTime = track_reader->ts;
            MP4MSG("send codec data %d\n", track_reader->codecDataSize);
        }
    } else {
        MP4TrackFragmentSample* sample;

        err = MP4GetOneSample(track_reader->queue, &track_reader->streamSample);
        if (err)
            goto bail;

        sample = &track_reader->streamSample;

        if (sample->ts >= 0)
            *usStartTime = sample->ts;
        else
            *usStartTime = track_reader->ts;

        if (reader->flags & FLAG_OUTPUT_PTS) {
            *usStartTime += (u64)sample->compositionOffset;
        }

        *usDuration = sample->duration;
        *outSize = 0;
        *sampleFlags = sample->flags;
        if (sample->drm_flags)
            *sampleFlags |= FLAG_SAMPLE_COMPRESSED_SAMPLE;

        sampleSize = sample->size;
        minf = track_reader->minf;
        err = minf->openDataHandler((MP4AtomPtr)minf, track_reader->dataReferenceIndex);
        if (err)
            goto bail;
        dhlr = (MP4DataHandlerPtr)minf->dataHandler;
        if (dhlr == NULL) {
            BAILWITHERROR(MP4InvalidMediaErr);
        }

        fileOffset = sample->offset;

        /* get sample data */
        isH264 = nalLengthFieldSize ? TRUE : FALSE;

        if (isH264) {
            bufferSizeRequested = sampleSize + NALLENFIELD_INCERASE;
        } else
            bufferSizeRequested = sampleSize;

        bufferSize = bufferSizeRequested;

        sampleBuffer = MP4LocalRequestBuffer(trackNum, &bufferSize, bufferContext,
                                             track_reader->appContext);

        if (isH264) {
            // enough to put a whole h264 frame
            if (bufferSize >= bufferSizeRequested) {
                err = NalStartCodeModifyAndMerge(track_reader, dhlr, fileOffset, sampleBuffer,
                                                 bufferSize, sample->size, outSize);
                if (err)
                    goto bail;
                track_reader->sampleBytesLeft = 0;
                goto postproc;
            } else {
                u32 bytesLeft = sampleSize;

                if (bufferSize < NAL_START_CODE_SIZE) {
                    err = PARSER_INSUFFICIENT_MEMORY;
                    goto bail;
                }

                err = getNalLength(dhlr, nalLengthFieldSize, fileOffset, &nalLength);
                if (err)
                    goto bail;
                fileOffset += nalLengthFieldSize;
                bytesLeft -= nalLengthFieldSize;
                if ((0 == nalLength) ||
                    (nalLength > sampleSize) || /* bytes left is less than NALU size */
                    (nalLengthFieldSize >=
                     bytesLeft - nalLength)) /* NALU length size is larger than sample size after
                                                reading current NALU */
                {
                    /* nalLength == 0 is illegal, handle this error inside core
                     * parser, and return the whole sample left */
                    nalLength = bytesLeft;
                }

                FILL_NAL_START_CODE(sampleBuffer)
                // stream->bufferOffset += NAL_START_CODE_SIZE;
                sampleBuffer += NAL_START_CODE_SIZE;
                bufferSize -= NAL_START_CODE_SIZE;
            }
        }
        dataSize = bufferSizeRequested <= bufferSize ? bufferSizeRequested : bufferSize;

        // if not enough a frame, we process only one nal at most.
        if (isH264) {
            dataSize = nalLength <= bufferSize ? nalLength : bufferSize;
        }

        if (dataSize > 0) {
            err = dhlr->copyData(dhlr, fileOffset, (char*)sampleBuffer, dataSize);
            if (err)
                goto bail;
            DUMP_STREAM_DATA
        }

        fileOffset += dataSize;

        if (isH264) {
            track_reader->sampleBytesLeft = sampleSize - dataSize - nalLengthFieldSize;
            track_reader->nalBytesLeft = nalLength - dataSize;
            *outSize = dataSize + NAL_START_CODE_SIZE;
        } else {
            track_reader->sampleBytesLeft = sampleSize - dataSize;
            *outSize = dataSize;
        }

    postproc:

        if (track_reader->sampleBytesLeft) /* sample not finished */
        {
            *sampleFlags |= FLAG_SAMPLE_NOT_FINISHED;
            track_reader->sampleFileOffset = fileOffset;
        } else {
            track_reader->ts += (u64)track_reader->streamSample.duration;
        }
    }

    *outBuffer = sampleBuffer;
    *usStartTime = *usStartTime * 1000000 / track_reader->timescale;
    *usDuration = *usDuration * 1000000 / track_reader->timescale;

    // if(track_reader->mediaType == MEDIA_VIDEO)
    MP4MSG("%s getFragmentedTrackNextSample trackNum=%d,outSize=%d,StartTime=%lld "
           "ms,Duration=%lld,flag=%x\n",
           TYPE(track_reader->mediaType), trackNum, *outSize, *usStartTime / 1000,
           *usDuration / 1000, *sampleFlags);
bail:
    if (err && sampleBuffer)
        MP4LocalReleaseBuffer(trackNum, sampleBuffer, *bufferContext, track_reader->appContext);

    TEST_RETURN(err);

    return err;
}

MP4Err getFragmentedTrackQueueOffset(FragmentedReaderStruct* reader, u32 trackNum, uint64* offset) {
    MP4Err err = MP4NoErr;
    FragmentedTrackReaderStruct* track_reader;
    u32 sample_num;
    if (reader == NULL || trackNum > reader->track_count || offset == NULL) {
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER);
    }
    track_reader = reader->track_reader[trackNum];
    err = MP4GetQueueSize(track_reader->queue, &sample_num);
    if (err)
        goto bail;
    if (sample_num == 0) {
        // find and parse moof atom
        MP4MSG("%s getQueueOffset no sample\n", TYPE(reader->track_reader[0]->mediaType));
        findAndParseMoofAtom(reader);
        err = MP4GetQueueSize(track_reader->queue, &sample_num);
        if (err)
            goto bail;
    }
    if (sample_num == 0)
        BAILWITHERROR(MP4EOF);
    err = MP4GetHeadSampleOffset(track_reader->queue, offset);
bail:
    TEST_RETURN(err);
    return err;
}

MP4Err seekFragmentedTrack(FragmentedReaderStruct* reader, u32 trackNum, u32 flags,
                           uint64* targetTime) {
    MP4Err err = MP4NoErr;
    u64 offset;

    if (reader == NULL || trackNum > reader->track_count || targetTime == NULL) {
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER);
    }
    MP4MSG("seekFragmentedTrack BEGIN trackNum=%d,ts=%lld\n", trackNum, *targetTime);

    if (reader->lastSeekTs == (s64)*targetTime && reader->lastSeekTrack != trackNum) {
        reader->lastSeekTrack = trackNum;
        // MP4MSG("seekFragmentedTrack return 0");
        return err;
    }

    if (*targetTime == 0) {
        err = clearAllQueues(reader);
        reader->curr_moof_offset = reader->first_moof_offset;
        ResetAllTrackTS(reader, *targetTime);
        reader->lastSeekTrack = trackNum;
        reader->lastSeekTs = *targetTime;
        // MP4MSG("seekFragmentedTrack targetTime = 0 ");
        goto bail;
    }

    if (reader->sidx == NULL && reader->mfra == NULL) {
        // MP4MSG("seekFragmentedTrack PARSER_ERR_NOT_SEEKABLE");
        BAILWITHERROR(PARSER_ERR_NOT_SEEKABLE);
    }

    offset = reader->curr_moof_offset;

    err = clearAllQueues(reader);

    if (reader->mfra) {
        MP4MovieFragmentRandomAccessAtomPtr mfra =
                (MP4MovieFragmentRandomAccessAtomPtr)reader->mfra;
        MP4TrackFragmentRandomAccessAtomPtr tfra = NULL;
        FragmentedTrackReaderStruct* track_reader = NULL;
        MP4AtomPtr atom;
        u64 seekTime = 0;
        u32 track_count = mfra->getTrackCount(mfra);

        if (track_count == 0)
            BAILWITHERROR(PARSER_ERR_NOT_SEEKABLE);

        // use the first track to seek
        err = mfra->getTrack(mfra, 1, &atom);

        if (err) {
            BAILWITHERROR(PARSER_ERR_NOT_SEEKABLE);
        }
        tfra = (MP4TrackFragmentRandomAccessAtomPtr)atom;

        err = getFragmentedTrack(reader, tfra->id, &track_reader);
        if (err)
            BAILWITHERROR(PARSER_ERR_NOT_SEEKABLE);

        seekTime = *targetTime * track_reader->timescale / 1000000;

        if (tfra->sample_count > 0 && tfra->sample) {
            err = tfra->seek(tfra, flags, &seekTime, &offset);
            if (err)
                goto bail;
        }

        reader->curr_moof_offset = offset;
        *targetTime = seekTime * 1000000 / track_reader->timescale;
    } else if (reader->sidx) {
        MP4SegmentIndexAtomPtr sidx = (MP4SegmentIndexAtomPtr)reader->sidx;
        err = sidx->seek(sidx, flags, targetTime, &offset);
        if (err)
            goto bail;
        reader->curr_moof_offset = reader->first_moof_offset + offset;
    }

    ResetAllTrackTS(reader, *targetTime);
    reader->lastSeekTs = *targetTime;
    reader->lastSeekTrack = trackNum;

    MP4MSG("seekFragmentedTrack trackNum=%d,*targetTime=%lld\n", trackNum, *targetTime);

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err isSeekable(FragmentedReaderStruct* reader, bool* seekable) {
    MP4Err err = MP4NoErr;

    if (reader == NULL || seekable == NULL) {
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER);
    }

    *seekable = FALSE;

    if (reader->sidx || reader->mfra) {
        *seekable = TRUE;
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err getFragmentedDuration(FragmentedReaderStruct* reader, u64* usDuration) {
    MP4Err err = MP4NoErr;
    bool seekable;
    bool originTrackState = TRUE;

    if (reader == NULL || usDuration == NULL) {
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER);
    }

    err = isSeekable(reader, &seekable);
    if (err)
        goto bail;

    if (!seekable) {
        *usDuration = 0;
        goto bail;
    }

    if (reader->duration > 0) {
        *usDuration = reader->duration;
        goto bail;
    }
    originTrackState = reader->track_reader[0]->bEnable;
    if (!originTrackState)
        err = enableFragmentedTrack(reader, 0, TRUE);
    if (err)
        goto bail;

    err = clearAllQueues(reader);

    if (reader->sidx) {
        MP4SegmentIndexAtomPtr sidx = (MP4SegmentIndexAtomPtr)reader->sidx;
        reader->duration = sidx->duration;
        MP4MSG("sidx last time =%lld,\n", reader->duration);
    } else if (reader->mfra) {
        MP4MovieFragmentRandomAccessAtomPtr mfra =
                (MP4MovieFragmentRandomAccessAtomPtr)reader->mfra;
        MP4TrackFragmentRandomAccessAtomPtr tfra = NULL;
        FragmentedTrackReaderStruct* track_reader = NULL;
        MP4AtomPtr atom;
        u64 total_duration = 0;
        u32 sample_num;
        MP4TrackFragmentSample sample;

        // use the first track
        err = mfra->getTrack(mfra, 1, &atom);

        if (err)
            goto bail;
        tfra = (MP4TrackFragmentRandomAccessAtomPtr)atom;

        err = getFragmentedTrack(reader, tfra->id, &track_reader);

        if (err)
            BAILWITHERROR(PARSER_ERR_INVALID_MEDIA);

        total_duration = (u64)tfra->last_ts * 1000000 / track_reader->timescale;
        reader->curr_moof_offset = tfra->last_offset;
        MP4MSG("mfra last time =%lld, curr moof=%lld \n", total_duration, reader->curr_moof_offset);

        // seek to the last index then get all samples to calculate the duration
        ResetAllTrackTS(reader, total_duration);
        findAndParseMoofAtom(reader);

        // use the first track,the track may be not equal to mfra's first track
        track_reader = reader->track_reader[0];
        err = MP4GetQueueSize(track_reader->queue, &sample_num);
        if (sample_num == 0)
            BAILWITHERROR(PARSER_ERR_INVALID_MEDIA);

        while (sample_num > 0) {
            err = MP4GetOneSample(track_reader->queue, &sample);
            if (err)
                goto bail;
            sample_num--;
            track_reader->ts += sample.duration;
        }

        reader->duration = track_reader->ts * 1000000 / track_reader->timescale;
    }

    *usDuration = reader->duration;
    reader->curr_moof_offset = reader->first_moof_offset;

    ResetAllTrackTS(reader, 0);

bail:
    if (!originTrackState)
        err = enableFragmentedTrack(reader, 0, FALSE);

    MP4MSG("%s =%lld ms\n", __FUNCTION__, reader->duration / 1000);

    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateFragmentedReader(MP4PrivateMovieRecordPtr theMovie, u32 flag, void* appContext,
                                 FragmentedReaderStruct** outReader) {
    MP4Err err;
    FragmentedReaderStruct* self;
    MP4PrivateMovieRecordPtr moov;

    err = MP4NoErr;
    self = (void*)MP4LocalCalloc(1, sizeof(FragmentedReaderStruct));
    TESTMALLOC(self)
    self->moov = (MP4PrivateMovieRecordPtr)theMovie;
    self->sidx = NULL;
    self->mfra = NULL;
    self->first_moof_offset = 0;
    self->curr_moof_offset = 0;
    self->track_count = 0;
    self->appContext = appContext;
    self->fpDump = NULL;

    moov = self->moov;
    self->first_moof_offset = moov->moofOffset;
    self->curr_moof_offset = self->first_moof_offset;
    self->sidx = moov->sidx;
    self->mfra = moov->mfra;
    self->inputStream = moov->inputStream;
    self->duration = 0;
    self->flags = flag;
    self->lastSeekTs = -1;
    self->lastSeekTrack = 0;
    *outReader = self;

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4DeleteFragmentedReader(FragmentedReaderStruct* self) {
    MP4Err err = MP4NoErr;
    FragmentedTrackReaderStruct* track_reader = NULL;
    u32 i;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr);

    for (i = 0; i < self->track_count; i++) {
        track_reader = self->track_reader[i];

        if (track_reader) {
            track_reader->default_key_crypto_iv = NULL;
            MP4ClearQueue(track_reader->queue);
            MP4LocalFree(track_reader->queue);
            if (track_reader->codecData)
                MP4LocalFree(track_reader->codecData);
            MP4LocalFree(track_reader);
        }
    }

    self->moov = NULL;
    self->inputStream = NULL;

    if (self->fpDump)
        fclose(self->fpDump);

    MP4LocalFree(self);

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err enableFragmentedTrack(FragmentedReaderStruct* self, u32 trackNum, bool enable) {
    MP4Err err = MP4NoErr;
    FragmentedTrackReaderStruct* track_reader = NULL;

    if (!self)
        BAILWITHERROR(MP4BadParamErr)

    track_reader = self->track_reader[trackNum];

    if (!track_reader)
        BAILWITHERROR(MP4BadParamErr)

    MP4MSG("enableFragmentedTrack track %d enable %d\n", trackNum, enable);

    track_reader->bEnable = enable;
    self->lastSeekTs = -1;

bail:
    return err;
}

MP4Err CheckIsEncryptedTrack(FragmentedTrackReaderStruct* self) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf = NULL;
    MP4SampleTableAtomPtr stbl = NULL;
    MP4SampleDescriptionAtomPtr stsd = NULL;
    GenericSampleEntryAtomPtr entry = NULL;
    MP4ProtectedVideoSampleAtomPtr encv = NULL;
    MP4ProtectedAudioSampleAtomPtr enca = NULL;
    MP4ProtectionSchemeInfoAtomPtr sinf = NULL;
    MP4SchemeInfoAtomPtr schi = NULL;
    MP4SchemeTypeAtomPtr schm = NULL;
    MP4TrackEncryptionAtomPtr tenc = NULL;

    uint32 sampleDescIndex = 1;  // start from 1

    if (!self || !self->minf)
        BAILWITHERROR(MP4BadParamErr)

    minf = self->minf;

    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    if (stbl == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    stsd = (MP4SampleDescriptionAtomPtr)stbl->SampleDescription;
    if (stsd == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    do {
        if (sampleDescIndex > stsd->getEntryCount(stsd)) {
            BAILWITHERROR(MP4BadParamErr);
        }
        err = stsd->getEntry(stsd, sampleDescIndex, &entry);
        if (err)
            goto bail;
        if (entry == NULL) {
            BAILWITHERROR(MP4InvalidMediaErr);
        }

        if (entry->type == MP4ProtectedVideoSampleEntryAtomType) {
            encv = (MP4ProtectedVideoSampleAtomPtr)entry;
            break;
        } else if (entry->type == MP4ProtectedAudioSampleEntryAtomType) {
            enca = (MP4ProtectedAudioSampleAtomPtr)entry;
            break;
        }

        sampleDescIndex++;
    } while (sampleDescIndex <= stsd->getEntryCount(stsd));

    if (encv == NULL && enca == NULL)
        return err;

    if (encv)
        sinf = (MP4ProtectionSchemeInfoAtomPtr)encv->sinf;
    else if (enca)
        sinf = (MP4ProtectionSchemeInfoAtomPtr)enca->sinf;

    if (sinf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    schi = (MP4SchemeInfoAtomPtr)sinf->schi;
    if (schi == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    schm = (MP4SchemeTypeAtomPtr)sinf->schm;
    if (schm != NULL) {
        self->default_IsEncrypted = schm->mode;
        self->bSubsampleEncryption = schm->bSubsampleEncryption;
        MP4MSG("CheckIsEncryptedTrack default_IsEncrypted=%d,bSubsampleEncryption=%d\n", schm->mode,
               schm->bSubsampleEncryption);
    }

    tenc = (MP4TrackEncryptionAtomPtr)schi->tenc;
    if (tenc == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    if (0 == self->default_IsEncrypted)
        self->default_IsEncrypted = tenc->default_IsEncrypted;

    MP4MSG("CheckIsEncryptedTrack tenc->default_IsEncrypted=%d,origin_default_IsEncrypted=%d\n",
           tenc->default_IsEncrypted, tenc->origin_default_IsEncrypted);

    self->default_IV_size = (u32)tenc->default_IV_size;
    memcpy(self->default_KID, tenc->default_KID, 16);

    self->default_EnctyptedByteBlock = (u32)tenc->default_EnctyptedByteBlock;
    self->default_SkipByteBlock = (u32)tenc->default_SkipByteBlock;
    self->default_key_iv_Length = (u32)tenc->default_key_iv_Length;
    self->default_key_crypto_iv = tenc->default_key_constant_iv;

    self->bEntrypted = TRUE;
    MP4MSG("CheckIsEncryptedTrack TRUE default_EnctyptedByteBlock=%d,default_SkipByteBlock=%d\n",
           self->default_EnctyptedByteBlock, self->default_SkipByteBlock);

bail:
    return err;
}

MP4Err addExtTrackTags(FragmentedReaderStruct* reader, u32 trackNum, TrackExtTagList* list) {
    MP4Err err = MP4NoErr;
    FragmentedTrackReaderStruct* track_reader = NULL;

    if (reader == NULL || list == NULL)
        BAILWITHERROR(MP4BadParamErr)

    err = getFragmentedTrack(reader, trackNum, &track_reader);
    if (err) {
        BAILWITHERROR(MP4BadParamErr)
    }

    if (track_reader == NULL)
        BAILWITHERROR(MP4InvalidMediaErr)

    if (track_reader->bEntrypted == FALSE)
        BAILWITHERROR(MP4NoErr)

    err = MP4AddTag(list, FSL_PARSER_TRACKEXTTAG_CRPYTOMODE, USER_DATA_FORMAT_INT_LE,
                    sizeof(uint32), (u8*)&track_reader->default_IsEncrypted);

    if (err)
        BAILWITHERROR(MP4BadParamErr)

    err = MP4AddTag(list, FSL_PARSER_TRACKEXTTAG_CRPYTODEFAULTIVSIZE, USER_DATA_FORMAT_INT_LE,
                    sizeof(uint32), (u8*)&track_reader->default_IV_size);

    if (err)
        BAILWITHERROR(MP4BadParamErr)

    err = MP4AddTag(list, FSL_PARSER_TRACKEXTTAG_CRPYTOKEY, USER_DATA_FORMAT_UTF8, 16,
                    (u8*)&track_reader->default_KID);

    if (err)
        BAILWITHERROR(MP4BadParamErr)

    err = MP4AddTag(list, FSL_PARSER_TRACKEXTTAG_CRYPTO_ENCRYPTED_BYTE_BLOCK,
                    USER_DATA_FORMAT_INT_LE, sizeof(uint32),
                    (u8*)&track_reader->default_EnctyptedByteBlock);
    if (err)
        BAILWITHERROR(MP4BadParamErr)

    err = MP4AddTag(list, FSL_PARSER_TRACKEXTTAG_CRYPTO_SKIP_BYTE_BLOCK, USER_DATA_FORMAT_INT_LE,
                    sizeof(uint32), (u8*)&track_reader->default_SkipByteBlock);
    if (err)
        BAILWITHERROR(MP4BadParamErr)

    if (track_reader->default_key_iv_Length > 0 && track_reader->default_key_crypto_iv != NULL) {
        err = MP4AddTag(list, FSL_PARSER_TRACKEXTTAG_CRYPTO_IV, USER_DATA_FORMAT_UTF8,
                        track_reader->default_key_iv_Length, track_reader->default_key_crypto_iv);
    }

bail:
    return err;
}

MP4Err getFragmentedTrackSampleCryptoInfo(FragmentedReaderStruct* reader, u32 trackNum, uint8** iv,
                                          uint32* ivSize, uint8** clearBuffer, uint32* clearSize,
                                          uint8** encryptedBuffer, uint32* encryptedSize) {
    MP4Err err = MP4NoErr;
    FragmentedTrackReaderStruct* track_reader;

    if (reader == NULL)
        BAILWITHERROR(MP4BadParamErr)

    track_reader = reader->track_reader[trackNum];

    if (track_reader == NULL || !track_reader->bEnable || !track_reader->bEntrypted) {
        BAILWITHERROR(MP4BadParamErr)
    }

    if (track_reader->default_key_iv_Length > 0 && track_reader->default_key_crypto_iv != NULL) {
        *iv = track_reader->default_key_crypto_iv;
        *ivSize = track_reader->default_key_iv_Length;
    } else {
        *iv = (uint8*)&track_reader->streamSample.iv;
        *ivSize = track_reader->default_IV_size;
    }

    if (track_reader->streamSample.drm_flags & FRAGMENT_SAMPLE_DRM_FLAG_FULL) {
        *clearBuffer = (uint8*)&track_reader->streamSample.clearsSize;
        *clearSize = sizeof(u32);
        *encryptedBuffer = (uint8*)&track_reader->streamSample.encryptedSize;
        *encryptedSize = sizeof(u32);
        MP4MSG("FRAGMENT_SAMPLE_DRM_FLAG_FULL trackNum=%d,clearsSize=%d,encryptedSize=%d", trackNum,
               track_reader->streamSample.clearsSize, track_reader->streamSample.encryptedSize);
    } else if (track_reader->streamSample.drm_flags & FRAGMENT_SAMPLE_DRM_FLAG_SUB) {
        *clearBuffer = (uint8*)&track_reader->streamSample.clearArray;
        *clearSize = track_reader->streamSample.clearsSize * sizeof(u32);
        *encryptedBuffer = (uint8*)&track_reader->streamSample.encryptedArray;
        *encryptedSize = track_reader->streamSample.encryptedSize * sizeof(u32);
        MP4MSG("FRAGMENT_SAMPLE_DRM_FLAG_SUB trackNum=%d,clearsSize=%d,encryptedSize=%d", trackNum,
               track_reader->streamSample.clearArray[0],
               track_reader->streamSample.encryptedArray[0]);
    } else {
        BAILWITHERROR(MP4BadParamErr)
    }
    MP4MSG("getFragmentedTrackSampleCryptoInfo trackNum=%d "
           "*ivSize=%d,*clearSize=%d,*encryptedSize=%d",
           trackNum, *ivSize, *clearSize, *encryptedSize);
    track_reader->streamSample.drm_flags = 0;

bail:
    return err;
}

MP4Err flushFragmentedTrack(FragmentedReaderStruct* reader, u32 trackNum) {
    MP4Err err = MP4NoErr;
    FragmentedTrackReaderStruct* track_reader = NULL;

    if (reader == NULL || trackNum > reader->track_count)
        BAILWITHERROR(MP4BadParamErr)

    track_reader = reader->track_reader[trackNum];

    if (track_reader && track_reader->queue) {
        MP4MSG("%s flushFragmentedTrack\n", TYPE(reader->track_reader[0]->mediaType));
        MP4ClearQueue(track_reader->queue);
    }

bail:
    return err;
}
