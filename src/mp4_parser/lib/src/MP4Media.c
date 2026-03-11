/*
 ***********************************************************************
 * Copyright (c) 2005-2014, Freescale Semiconductor, Inc.
 * Copyright 2017-2018, 2020-2022, 2024-2026 NXP
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
derivative works. Copyright (c) 1999, 2000.
*/
/*
$Id: MP4Media.c,v 1.44 2001/06/09 16:30:50 rtm Exp $
*/
//#define DEBUG

#include <stdlib.h>
#include <string.h>
#include "AC4InfoParser.h"
#include "BitReader.h"
#include "MP4Atoms.h"
#include "MP4DataHandler.h"
#include "MP4Descriptors.h"
#include "MP4Movies.h"

extern ParserOutputBufferOps g_outputBufferOps;

extern MP4Err AudioSpecificConfig(unsigned char* decoder_info, mp4AudioSpecificConfig* mp4ASC);

static MP4Err parseAvcDecoderSpecificInfo(MP4AVCCAtomPtr AVCC, uint32* NALLengthFieldSize,
                                          MP4Handle specificInfoH);
static MP4Err parseHevcDecoderSpecificInfo(MP4HVCCAtomPtr HVCC, uint32* NALLengthFieldSize,
                                           MP4Handle specificInfoH);

static MP4Err getNextSample(AVStreamPtr stream, MP4Media mediaAtom, u32* nextSampleNumber,
                            u32* outSize, u64* outDTS, s32* outCTSOffset, u64* outDuration,
                            u32* outSampleFlags, u32* outSampleDescIndex, u32 odsm_offset,
                            u32 sdsm_offset);

static int32 getSampleRemainingBytes(AVStreamPtr stream, MP4Media mediaAtom, u32* nextSampleNumber,
                                     u32* outSize, u64* outDTS, s32* outCTSOffset, u64* outDuration,
                                     u32* outSampleFlags, u32* outSampleDescIndex);

static int32 getNalLength(MP4DataHandlerPtr dhlr, u32 nalLengthFieldSize, u64 fileOffset,
                          u32* nalLength);

static MP4Err NalStartCodeModifyAndMerge(AVStreamPtr stream, MP4DataHandlerPtr dhlr,
                                         uint64 fileOffset, uint8* sampleBuffer,
                                         u32 sampleBufferLen, u32 sampleSize, u32* outSampleSize);

#define FILL_NAL_START_CODE(buffer) \
    {                               \
        buffer[0] = 0;              \
        buffer[1] = 0;              \
        buffer[2] = 0;              \
        buffer[3] = 1;              \
    }

//{memcpy(buffer, 0, NAL_START_CODE_SIZE); *(buffer + 3) = 0x01; }

static MP4Err requestSampleBuffer(AVStreamPtr stream, uint32 sizeRequested) {
    MP4Err err = MP4NoErr;

    if (stream->sampleBuffer)
        return PARSER_ERR_UNKNOWN;

    stream->bufferOffset = 0;
    stream->bufferSize = sizeRequested;
    if (stream->internalSample)
        stream->sampleBuffer = MP4LocalMalloc(stream->bufferSize);
    else
        stream->sampleBuffer = MP4LocalRequestBuffer(stream->streamIndex, &stream->bufferSize,
                                                     &stream->bufferContext, stream->appContext);
    // MP4MSG("trk %d, got buffer %p, context \n", stream->sampleBuffer, stream->bufferContext);
    if (!stream->sampleBuffer)
        err = PARSER_ERR_NO_OUTPUT_BUFFER;

    return err;
}

MP4Err MP4GetMediaLanguage(MP4Media theMedia, char* outThreeCharCode) {
    MP4Err err = MP4NoErr;
    u32 packedLanguage;
    MP4MediaHeaderAtomPtr mediaHeader;

    if ((theMedia == NULL) || (outThreeCharCode == NULL)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    mediaHeader = (MP4MediaHeaderAtomPtr)((MP4MediaAtomPtr)theMedia)->mediaHeader;
    if (mediaHeader == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    packedLanguage = mediaHeader->packedLanguage;

    /* for standard MP4,
    bit(1) pad = 0;
    unsined int(5)[3] language;
    Each character is packed as the difference between its ASCII value and 0x60.
    ISO 639-2/T, confined to being three lower-case characters. */
    if (packedLanguage) {
        *outThreeCharCode++ = ((packedLanguage >> 10) & 0x1F) + 0x60;
        *outThreeCharCode++ = ((packedLanguage >> 5) & 0x1F) + 0x60;
        *outThreeCharCode = (packedLanguage & 0x1F) + 0x60;
    } else /* undetermined language */
    {
        *outThreeCharCode++ = 'u';
        *outThreeCharCode++ = 'n';
        *outThreeCharCode = 'd';
    }

    /*QuickTime also defines its own languge code, not found now. */
bail:
    TEST_RETURN(err);

    return err;
}

ISOErr MJ2GetMediaGraphicsMode(ISOMedia theMedia, u32* outMode, ISORGBColor* outOpColor) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4VideoMediaHeaderAtomPtr vmhd;
    u32 handlerType;

    if ((theMedia == NULL) || (outOpColor == NULL)) {
        BAILWITHERROR(MP4BadParamErr);
    }

    err = MP4GetMediaHandlerDescription(theMedia, &handlerType, NULL);
    if (err)
        goto bail;
    if (handlerType != MP4VisualHandlerType)
        BAILWITHERROR(MP4NotImplementedErr)

    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
    if (minf == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);

    vmhd = (MP4VideoMediaHeaderAtomPtr)minf->mediaHeader;
    if (vmhd == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);

    *outMode = vmhd->graphicsMode;
    outOpColor->red = (u16)vmhd->opColorRed;
    outOpColor->green = (u16)vmhd->opColorGreen;
    outOpColor->blue = (u16)vmhd->opColorBlue;
bail:
    TEST_RETURN(err);

    return err;
}

ISOErr MJ2SetMediaSoundBalance(ISOMedia theMedia, s16 balance) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SoundMediaHeaderAtomPtr smhd;
    u32 handlerType;

    if (theMedia == NULL) {
        BAILWITHERROR(MP4BadParamErr);
    }

    err = MP4GetMediaHandlerDescription(theMedia, &handlerType, NULL);
    if (err)
        goto bail;
    if (handlerType != MP4AudioHandlerType)
        BAILWITHERROR(MP4NotImplementedErr)

    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
    if (minf == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);

    smhd = (MP4SoundMediaHeaderAtomPtr)minf->mediaHeader;
    if (smhd == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);

    smhd->balance = (u32)balance;
bail:
    TEST_RETURN(err);

    return err;
}

ISOErr MJ2GetMediaSoundBalance(ISOMedia theMedia, s16* outBalance) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SoundMediaHeaderAtomPtr smhd;
    u32 handlerType;

    if ((theMedia == NULL) || (outBalance == NULL)) {
        BAILWITHERROR(MP4BadParamErr);
    }

    err = MP4GetMediaHandlerDescription(theMedia, &handlerType, NULL);
    if (err)
        goto bail;
    if (handlerType != MP4AudioHandlerType)
        BAILWITHERROR(MP4NotImplementedErr)

    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
    if (minf == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);

    smhd = (MP4SoundMediaHeaderAtomPtr)minf->mediaHeader;
    if (smhd == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);

    *outBalance = (s16)smhd->balance;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMediaTimeScale(MP4Media theMedia, u32* outTimeScale) {
    MP4Err err = MP4NoErr;
    MP4MediaHeaderAtomPtr mediaHeader;

    if ((theMedia == NULL) || (outTimeScale == NULL)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    mediaHeader = (MP4MediaHeaderAtomPtr)((MP4MediaAtomPtr)theMedia)->mediaHeader;
    if (mediaHeader == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    *outTimeScale = mediaHeader->timeScale;

    // theMedia->media_duration = (unsigned int)mediaHeader->duration; /* engr59629: don't blend
    // operation, there is MP4GetMediaDuration()*/
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMediaDuration(MP4Media theMedia, u64* outDuration) {
    MP4Err err = MP4NoErr;
    MP4MediaHeaderAtomPtr mediaHeader;
    MP4MediaAtomPtr mdia;

    if ((theMedia == NULL) || (outDuration == NULL)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    mdia = (MP4MediaAtomPtr)theMedia;
    mediaHeader = (MP4MediaHeaderAtomPtr)(mdia)->mediaHeader;
    if (mediaHeader == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

#ifndef ANDROID
    //#pragma message ("get media duration from sample duration")
    // return duration from hdhd header for android.
    // for others, add all sample duration together and make it the track duration.
    err = mdia->calculateDuration(mdia);
    if (err)
        goto bail;
#endif
    *outDuration = mediaHeader->duration;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMediaTrack(MP4Media theMedia, MP4Track* outTrack) {
    MP4Err err = MP4NoErr;

    if ((theMedia == NULL) || (outTrack == NULL)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    *outTrack = (MP4Track)((MP4MediaAtomPtr)theMedia)->mediaTrack;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMediaSampleCount(MP4Media theMedia, u32* outCount) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4SampleSizeAtomPtr stsz;
    MP4CompactSampleSizeAtomPtr stz2;

    if ((theMedia == NULL) || (outCount == NULL)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    if (stbl == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stsz = (MP4SampleSizeAtomPtr)stbl->SampleSize;
    stz2 = (MP4CompactSampleSizeAtomPtr)stbl->CompactSampleSize;
    if (stsz == NULL && stz2 == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    *outCount = (stsz != NULL) ? stsz->sampleCount : stz2->sampleCount;

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMediaTotalBytes(MP4Media theMedia, u64* outTotalBytes) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4SampleSizeAtomPtr stsz;
    MP4CompactSampleSizeAtomPtr stz2;

    if ((theMedia == NULL) || (outTotalBytes == NULL)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    if (stbl == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stsz = (MP4SampleSizeAtomPtr)stbl->SampleSize;
    stz2 = (MP4CompactSampleSizeAtomPtr)stbl->CompactSampleSize;
    if (stsz == NULL && stz2 == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    /* since this value is not necessary for playback, it can be ZERO
    if the whole stsz table is not loaded in parsing phase. */
    *outTotalBytes = 0;
    if (stsz != NULL && stsz->scanFinished)
        *outTotalBytes = (u64)stsz->totalBytes;
    else if (stz2 != NULL && stz2->scanFinished)
        *outTotalBytes = (u64)stz2->totalBytes;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMediaSampleDescription(MP4Media theMedia, u32 index, MP4Handle outDescriptionH,
                                    u32* outDataReferenceIndex) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4SampleDescriptionAtomPtr stsd;
    GenericSampleEntryAtomPtr entry;

    if ((theMedia == NULL) || (index == 0)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
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
    if (index > stsd->getEntryCount(stsd)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    err = stsd->getEntry(stsd, index, &entry);
    if (err)
        goto bail;
    if (entry == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    if (outDescriptionH) {
#if 0 /* use another method to directly get size and its content! */
        err = entry->calculateSize( (MP4AtomPtr) entry ); if (err) goto bail;
        err = MP4SetHandleSize( outDescriptionH, entry->size ); if (err) goto bail;
        err = entry->serialize( (MP4AtomPtr) entry, *outDescriptionH ); if (err) goto bail;
#endif
    }
    if (outDataReferenceIndex) {
        *outDataReferenceIndex = entry->dataReferenceIndex;
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMediaESD(MP4Media theMedia, u32 index, MP4ES_DescriptorPtr* outESD,
                      u32* outDataReferenceIndex) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4SampleDescriptionAtomPtr stsd;
    GenericSampleEntryAtomPtr entry;
    MP4ESDAtomPtr ESD;

    if ((theMedia == NULL) || (index == 0)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
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
    if (index > stsd->getEntryCount(stsd)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    err = stsd->getEntry(stsd, index, &entry);
    if (err)
        goto bail;
    if (entry == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    ESD = (MP4ESDAtomPtr)entry->ESDAtomPtr;
    if (ESD == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    if (outESD) {
        MP4ES_DescriptorPtr esdDescriptor;
        esdDescriptor = (MP4ES_DescriptorPtr)ESD->descriptor;
        *outESD = esdDescriptor;
    }
    if (outDataReferenceIndex) {
        *outDataReferenceIndex = entry->dataReferenceIndex;
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMediaDecoderConfig(MP4Media theMedia, u32 sampleDescIndex, MP4Handle decoderConfigH) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4SampleDescriptionAtomPtr stsd;
    GenericSampleEntryAtomPtr entry;
    MP4ES_DescriptorPtr esdDescriptor;
    MP4DescriptorPtr decoderConfig;
    MP4ESDAtomPtr ESD;

    if ((theMedia == NULL) || (sampleDescIndex == 0)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
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
    if (sampleDescIndex > stsd->getEntryCount(stsd)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    err = stsd->getEntry(stsd, sampleDescIndex, &entry);
    if (err)
        goto bail;
    if (entry == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    ESD = (MP4ESDAtomPtr)entry->ESDAtomPtr;
    if (ESD == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    esdDescriptor = (MP4ES_DescriptorPtr)ESD->descriptor;
    if (esdDescriptor == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    decoderConfig = esdDescriptor->decoderConfig;
    if (decoderConfig) {
        err = decoderConfig->calculateSize(decoderConfig);
        if (err)
            goto bail;
        err = MP4SetHandleSize(decoderConfigH, decoderConfig->size);
        if (err)
            goto bail;
        if (decoderConfig->size) {
            err = decoderConfig->serialize(decoderConfig, *decoderConfigH);
            if (err)
                goto bail;
        }
    } else {
        err = MP4SetHandleSize(decoderConfigH, 0);
    }

bail:
    TEST_RETURN(err);

    return err;
}

void GetDecoderType(u32 entryType, u32* mediaType, u32* decoderType, u32* decoderSubtype) {
    *mediaType = MEDIA_TYPE_UNKNOWN;
    *decoderType = UNKNOWN_CODEC_TYPE;
    *decoderSubtype = UNKNOWN_CODEC_SUBTYPE; /* assume no subtypes */

    /* get stream & decoder type */
    switch (entryType) {
        /*************** Text/subtile *********************************************/
        case QTTimeCodeSampleEntryAtomType:
            /* engr94385:
            for quicktime time code sample entry, none of the following child atoms (D263, AVCC, ESD
            ...)is contained.*/
            *mediaType = MEDIA_TYPE_UNKNOWN;
            break;

        case QTTextEntryAtomType:
            *mediaType = MEDIA_TEXT;
            *decoderType = TXT_QT_TEXT;
            break;

        case TimedTextEntryType:
            /* engr109101:
             for 3gp timed text entry, none of the following child atoms (D263, AVCC, ESD ...)is
             contained.*/
            *mediaType = MEDIA_TEXT;
            *decoderType = TXT_3GP_STREAMING_TEXT;
            break;

        case MP4MetadataSampleEntryAtomType:
            *mediaType = MEDIA_TEXT;
            *decoderType = TXT_METADATA;
            break;

        /*************** Audio **********************************************
         * warning: 'mp4a' can wrap various audio types, and if 'esds' is present,
         * there will be detailed auido types.
         *************** Audio ***********************************************/
        case MP4AudioSampleEntryAtomType:
            *mediaType = MEDIA_AUDIO;
            *decoderType = AUDIO_TYPE_UNKNOWN; /* can not decide audio decoder type yet! */
            break;

        case MP4Mp3SampleEntryAtomType:
        case MP4Mp3CbrSampleEntryAtomType:
            *mediaType = MEDIA_AUDIO;
            *decoderType = AUDIO_MP3;
            break;

        case AmrNBSampleEntryAtomType:
        case AmrWBSampleEntryAtomType:
            *mediaType = MEDIA_AUDIO;
            *decoderType = AUDIO_AMR;
            if (AmrNBSampleEntryAtomType == entryType)
                *decoderSubtype = AUDIO_AMR_NB;
            else
                *decoderSubtype = AUDIO_AMR_WB;
            break;

        case QTSowtSampleEntryAtomType:
        case QTTwosSampleEntryAtomType:
        case RawAudioSampleEntryAtomType:
            *mediaType = MEDIA_AUDIO;
            *decoderType = AUDIO_PCM;
            if (QTSowtSampleEntryAtomType == entryType)
                *decoderSubtype = AUDIO_PCM_S16LE;
            else if (QTTwosSampleEntryAtomType == entryType)
                *decoderSubtype = AUDIO_PCM_S16BE;
            else if (RawAudioSampleEntryAtomType == entryType)
                *decoderSubtype = AUDIO_PCM_U8;
            break;

        case MP4MSAdpcmEntryAtomType:
            *mediaType = MEDIA_AUDIO;
            *decoderType = AUDIO_ADPCM;
            *decoderSubtype = AUDIO_IMA_ADPCM;
            break;

        case ImaAdpcmSampleEntryAtomType:
            *mediaType = MEDIA_AUDIO;
            *decoderType = AUDIO_ADPCM;
            *decoderSubtype = AUDIO_ADPCM_QT;
            break;

        case MuLawAudioSampleEntryAtomType:
            *mediaType = MEDIA_AUDIO;
            *decoderType = AUDIO_PCM_MULAW;
            break;

        case MP4AC3SampleEntryAtomType:
            *mediaType = MEDIA_AUDIO;
            *decoderType = AUDIO_AC3;
            break;
        case MP4EC3SampleEntryAtomType:
            *mediaType = MEDIA_AUDIO;
            *decoderType = AUDIO_EC3;
            break;
        case MP4AC4SampleEntryAtomType:
            *mediaType = MEDIA_AUDIO;
            *decoderType = AUDIO_AC4;
            break;

        case MP4ALACSampleEntryAtomType:
            *mediaType = MEDIA_AUDIO;
            *decoderType = AUDIO_ALAC;
            break;

        case MP4OpusSampleEntryAtomType:
            *mediaType = MEDIA_AUDIO;
            *decoderType = AUDIO_OPUS;
            break;

        case MP4FlacSampleEntryAtomType:
            *mediaType = MEDIA_AUDIO;
            *decoderType = AUDIO_FLAC;
            break;

        case MPEGHA1SampleEntryAtomType:
            *mediaType = MEDIA_AUDIO;
            *decoderType = AUDIO_MPEGH_MHA1;
            break;

        case MPEGHM1SampleEntryAtomType:
            *mediaType = MEDIA_AUDIO;
            *decoderType = AUDIO_MPEGH_MHM1;
            break;

            /*************** Video **********************************************/

        case MP4VisualSampleEntryAtomType:
            *mediaType = MEDIA_VIDEO;
            *decoderType = VIDEO_MPEG4;
            break;

        case MP4AvcSampleEntryAtomType:
            *mediaType = MEDIA_VIDEO;
            *decoderType = VIDEO_H264;
            break;
        case MP4Av1SampleEntryAtomType:
            *mediaType = MEDIA_VIDEO;
            *decoderType = VIDEO_AV1;
            break;
        case MP4HevcSampleEntryAtomType:
        case MP4Hev1SampleEntryAtomType:
            *mediaType = MEDIA_VIDEO;
            *decoderType = VIDEO_HEVC;
            break;

        case QTH263SampleEntryAtomType:
        case MP4H263SampleEntryAtomType:
        case H263SampleEntryAtomType:
            *mediaType = MEDIA_VIDEO;
            *decoderType = VIDEO_H263;
            break;

        case JPEGSampleEntryAtomType:
            *mediaType = MEDIA_VIDEO;
            *decoderType = VIDEO_JPEG;
            break;

        case MJPEGFormatASampleEntryAtomType:
        case MJPEGFormatBSampleEntryAtomType:
        case MJPEG2000SampleEntryAtomType:
            *mediaType = MEDIA_VIDEO;
            *decoderType = VIDEO_MJPG;
            if (MJPEGFormatASampleEntryAtomType == entryType)
                *decoderSubtype = VIDEO_MJPEG_FORMAT_A;
            else if (MJPEGFormatBSampleEntryAtomType == entryType)
                *decoderSubtype = VIDEO_MJPEG_FORMAT_B;
            else if (MJPEG2000SampleEntryAtomType == entryType)
                *decoderSubtype = VIDEO_MJPEG_2000;
            break;

        case SVQ3SampleEntryAtomType:
            *mediaType = MEDIA_VIDEO;
            *decoderType = VIDEO_SORENSON;
            *decoderSubtype = VIDEO_SVQ3;
            break;

        case MP4DivxSampleEntryAtomType:
            *mediaType = MEDIA_VIDEO;
            *decoderType = VIDEO_DIVX;
            *decoderSubtype = VIDEO_DIVX5_6;
            break;
        case DVAVSampleEntryAtomType:
        case DVA1SampleEntryAtomType:
        case DVHESampleEntryAtomType:
        case DVH1SampleEntryAtomType:
        case DAV1SampleEntryAtomType:
            *mediaType = MEDIA_VIDEO;
            *decoderType = VIDEO_DOLBY_VISION;
            break;
        case MP4Vp9SampleEntryAtomType:
            *mediaType = MEDIA_VIDEO;
            *decoderType = VIDEO_ON2_VP;
            *decoderSubtype = VIDEO_VP9;
            break;
        case MP4ApvSampleEntryAtomType:
            *mediaType = MEDIA_VIDEO;
            *decoderType = VIDEO_APV;
            break;
        case MP4MPEGSampleEntryAtomType:
            /* need check object indication, normal to be dscription stream/subtitle */
        default:

            MP4MSG("Unknown media/decoder type\n");
    }
}

/* For definition of DecoderSpecificInfo:
        ES_Descriptor--->DecoderConfigDescriptor-->DecoderSpecificInfo
            -->AudioSpecificConfig/videoSepcificInfo (depend on the media type)
        Refer to 14496-1 p42
        8.6.6 DecoderConfigDescriptor
        8.6.6.1 Syntax

note: mp3 audio can be described in ".mp3" entry (QuickTime) or "mp4a" entry (standard MPEG-4)
*/
MP4Err MP4GetMediaDecoderInformation(MP4Media media, u32 sampleDescIndex, u32 flags, u32* mediaType,
                                     u32* decoderType, u32* decoderSubtype, u32* maxBitRate,
                                     u32* avgBitRate, u32* NALLengthFieldSize,
                                     MP4Handle specificInfoH) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4SampleDescriptionAtomPtr stsd;
    GenericSampleEntryAtomPtr entry;
    MP4ES_DescriptorPtr esdDescriptor = NULL;
    MP4DecoderConfigDescriptorPtr decoderConfig = NULL;
    MP4ESDAtomPtr ESD;
    MP4AVCCAtomPtr AVCC = NULL;
    QTsiDecompressParaAtomPtr WAVE;
    MP4HVCCAtomPtr HVCC = NULL;
    MP4VPCCAtomPtr VPCC = NULL;
    MP4APVCAtomPtr APVC = NULL;
    MP4BitrateAtomPtr btrt = NULL;

    u32 objectType; /* objectTypeIndication in DecoderConfigDescriptor, 14496-1 sec 8.6.6 11*/
    u32 streamType;

    // bool unknownMpegStream = FALSE; /* for mp4s  */

    if ((media == NULL) || (sampleDescIndex == 0) || (NULL == NALLengthFieldSize) ||
        (NULL == specificInfoH)) {
        BAILWITHERROR(MP4BadParamErr);
    }

    /* engr59629: this is the only func in which " theMedia" is of type "MP4MediaRecordPtr", not
     * media atom ptr  */
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
    if (sampleDescIndex > stsd->getEntryCount(stsd)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    err = stsd->getEntry(stsd, sampleDescIndex, &entry);
    if (err)
        goto bail;
    if (entry == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    /* default properties */
    if (maxBitRate)
        *maxBitRate = 0;

    if (avgBitRate)
        *avgBitRate = 0;

    if (NALLengthFieldSize)
        *NALLengthFieldSize = 0;

    if (specificInfoH) {
        err = MP4SetHandleSize(specificInfoH, 0);
        if (err)
            goto bail;
    }

    GetDecoderType(entry->type, mediaType, decoderType, decoderSubtype);

    if (entry->type == MP4ProtectedVideoSampleEntryAtomType) {
        u32 type = 0;
        MP4ProtectedVideoSampleAtomPtr videoPtr = (MP4ProtectedVideoSampleAtomPtr)entry;
        if (MP4NoErr == videoPtr->getOriginFormat(videoPtr, &type))
            GetDecoderType(type, mediaType, decoderType, decoderSubtype);

        if (videoPtr->AVCCAtomPtr != NULL && *decoderType == VIDEO_H264)
            AVCC = (MP4AVCCAtomPtr)videoPtr->AVCCAtomPtr;
        if (videoPtr->HVCCAtomPtr != NULL && *decoderType == VIDEO_HEVC)
            HVCC = (MP4HVCCAtomPtr)videoPtr->HVCCAtomPtr;

    } else if (entry->type == MP4ProtectedAudioSampleEntryAtomType) {
        u32 type = 0;
        MP4ProtectedAudioSampleAtomPtr audioPtr = (MP4ProtectedAudioSampleAtomPtr)entry;
        if (MP4NoErr == audioPtr->getOriginFormat(audioPtr, &type)) {
            GetDecoderType(type, mediaType, decoderType, decoderSubtype);
            // some android files have 2 entries in stsd box: one is enca and the other stores codec
            // information.
            MP4ESDAtomPtr esds = (MP4ESDAtomPtr)entry->ESDAtomPtr;
            if (esds == NULL) {
                u32 entryCount = stsd->getEntryCount(stsd);
                if (entryCount > 1 && sampleDescIndex < entryCount) {
                    sampleDescIndex++;
                    err = stsd->getEntry(stsd, sampleDescIndex, &entry);
                    if (err)
                        goto bail;
                }
            }
        }
    }

    if (entry->type == MP4MetadataSampleEntryAtomType)
        return err;

    if (entry->type == MP4Av1SampleEntryAtomType) {
        MP4Av1SampleEntryAtomPtr av01 = (MP4Av1SampleEntryAtomPtr)entry;
        btrt = (MP4BitrateAtomPtr)av01->BitrateAtomPtr;
    } else if (entry->type == MP4Vp9SampleEntryAtomType) {
        MP4Vp9SampleEntryAtomPtr vp9 = (MP4Vp9SampleEntryAtomPtr)entry;
        btrt = (MP4BitrateAtomPtr)vp9->BitrateAtomPtr;
    }

    if (btrt != NULL) {
        if (maxBitRate)
            *maxBitRate = btrt->maxBitrate;
        if (avgBitRate)
            *avgBitRate = btrt->avgBitrate;
    }

    if (AUDIO_PCM == *decoderType) {
        PcmAudioSampleEntryAtomPtr pcmPtr = (PcmAudioSampleEntryAtomPtr)entry;
        switch (pcmPtr->bitsPerSample) {
            case 8:
                if (*decoderSubtype != AUDIO_PCM_U8) {
                    *decoderSubtype = AUDIO_PCM_S8;
                }
                break;
            case 16:
                if (*decoderSubtype == AUDIO_PCM_U8 || *decoderSubtype == AUDIO_PCM_S8) {
                    *decoderSubtype = AUDIO_PCM_S16BE;
                }
                break;
            case 24:
                if (*decoderSubtype == AUDIO_PCM_S16BE) {
                    *decoderSubtype = AUDIO_PCM_S24BE;
                } else if (*decoderSubtype == AUDIO_PCM_S16LE) {
                    *decoderSubtype = AUDIO_PCM_S24LE;
                }
                break;
            default:
                break;
        }
    }

    /*****************************************************************
    get decoder specific info:
    - only mpeg4 video & audio has this info in file.
    - make this info for AMR audio
    - mp3, sowt/twos, h263 has no specific info
    *****************************************************************/
    if (AVCC == NULL)
        AVCC = (MP4AVCCAtomPtr)entry->AVCCAtomPtr;
    if (HVCC == NULL)
        HVCC = (MP4HVCCAtomPtr)entry->HVCCAtomPtr;
    if (VPCC == NULL)
        VPCC = (MP4VPCCAtomPtr)entry->VPCCAtomPtr;
    if (APVC == NULL)
        APVC = (MP4APVCAtomPtr)entry->APVCAtomPtr;
    ESD = (MP4ESDAtomPtr)entry->ESDAtomPtr;
    WAVE = (QTsiDecompressParaAtomPtr)entry->WaveAtomPtr;

    /* MPEG4 video & audio (AAC) */
    if ((NULL == ESD) && WAVE) {
        /* engr94385: for some quicktime AAC audio track (entry version 1),
        'esds' wrapped in 'wave'.Not comply to mp4 spec. */
        ESD = (MP4ESDAtomPtr)WAVE->ESDAtomPtr;
    }

    if (ESD) {
        esdDescriptor = (MP4ES_DescriptorPtr)ESD->descriptor;
        if (esdDescriptor == NULL) {
            BAILWITHERROR(MP4InvalidMediaErr);
        }
        decoderConfig = (MP4DecoderConfigDescriptorPtr)esdDescriptor->decoderConfig;
        if (decoderConfig) {
            MP4DescriptorPtr specificInfo;
            objectType = decoderConfig->objectTypeIndication;
            streamType = decoderConfig->streamType;
            MP4MSG("object type indication %d (0x%x), stream type %d (0x%x)\n", objectType,
                   objectType, streamType, streamType);

            if (OBJECT_AUDIO_MPEG4 == objectType) {
                *decoderType = AUDIO_AAC;
                *decoderSubtype = AUDIO_AAC_RAW;
            }

            else if ((OBJECT_AUDIO_MPEG2_AAC_MAIN_PROFILE == objectType) ||
                     (OBJECT_AUDIO_MPEG2_AAC_LC_PROFILE == objectType) ||
                     (OBJECT_AUDIO_MPEG2_AAC_SSR_PROFILE == objectType)) {
                /* this MPEG-2 AAC, revise audio decoder type (ENGR114907)*/
                *decoderType = AUDIO_MPEG2_AAC;
                *decoderSubtype = AUDIO_AAC_RAW;
            } else if ((OBJECT_AUDIO_MPEG2 == objectType) || (OBJECT_AUDIO_MPEG1 == objectType)) {
                /* this MP3 audiio, revise audio decoder type */
                *decoderType = AUDIO_MP3;
            } else if (((OBJECT_TYPE_OGG == objectType) || (OBJECT_TYPE_VORBIS == objectType)) &&
                       (MP4_AUDIO_STREAM == streamType))
                *decoderType = AUDIO_VORBIS;
            else if ((OBJECT_VISUAL_10918 == objectType) && (MP4_VISUAL_STREAM == streamType)) {
                *decoderType = VIDEO_JPEG;
            } else if ((OBJECT_VISUAL_JPEG2000 == objectType) &&
                       (MP4_VISUAL_STREAM == streamType)) {
                *decoderType = VIDEO_MJPG;
                *decoderSubtype = VIDEO_MJPEG_2000;
            } else if ((OBJECT_AUDIO_AC3 == objectType) && (MP4_AUDIO_STREAM == streamType)) {
                *decoderType = AUDIO_AC3;
            } else if ((objectType >= OBJECT_VISUAL_MPEG2_SIMPLE_PROFILE) &&
                       (objectType <= OBJECT_VISUAL_MPEG2_422_PROFILE)) {
                // check object type and set type to mpeg2
                *decoderType = VIDEO_MPEG2;
            }

            if (maxBitRate) {
                *maxBitRate = decoderConfig->maxBitrate;
            }
            if (avgBitRate) {
                *avgBitRate = decoderConfig->avgBitrate;
            }
            if (specificInfoH) {
                specificInfo = decoderConfig->decoderSpecificInfo;
                if (specificInfo) {
                    err = specificInfo->calculateSize(specificInfo);
                    if (err)
                        goto bail;
                    err = MP4SetHandleSize(specificInfoH, specificInfo->size);
                    if (err)
                        goto bail;
                    if (specificInfo->size) {
                        u32 bytesSkipped;
                        char* cp = *specificInfoH;
                        err = specificInfo->serialize(specificInfo, *specificInfoH);
                        if (err)
                            goto bail;
                        cp++; /* skip tag & size field */
                        for (bytesSkipped = 1; *cp & 0x80;) {
                            cp++;
                            bytesSkipped++;
                            if (bytesSkipped == specificInfo->size) {
                                BAILWITHERROR(MP4InvalidMediaErr);
                            }
                        }
                        bytesSkipped++;
                        /* rtm: changed memcpy to memmove, since memory may overlap */
                        memmove(*specificInfoH, *specificInfoH + bytesSkipped,
                                specificInfo->size - bytesSkipped);
                        err = MP4SetHandleSize(specificInfoH, specificInfo->size - bytesSkipped);
                        if (err)
                            goto bail;
                        MP4MSG("decoder specific info size: %d, ptr 0x%p\n",
                               specificInfo->size - bytesSkipped, *specificInfoH);

                        if (OBJECT_AUDIO_MPEG4 == objectType) {
                            mp4AudioSpecificConfig mp4ASC;
                            AudioSpecificConfig((unsigned char*)(*specificInfoH), &mp4ASC);
                            MP4MSG("AAC audio object type: %d\n", mp4ASC.objectTypeIndex);
                            if (AUDIO_OBJECT_ER_BSAC == mp4ASC.objectTypeIndex)
                                *decoderSubtype = AUDIO_ER_BSAC;
                        }
                    }
                }
            }
        }
    } else if (AUDIO_AMR == *decoderType) /* AMR audio */
    {
#if 0 /* shall not create this info */
        u32 info_size=6;
        char amr_header[6]={0x23,0x21,0x41,0x4d,0x52,0x0a};

        if(specificInfoH)
        {
            err = MP4SetHandleSize( specificInfoH, 6); if (err) goto bail;
            //*specificInfoH = (char *)MP4LocalMalloc( info_size );
            //TESTMALLOC(*specificInfoH)
            memcpy(*specificInfoH, amr_header, 6);
            MP4MSG("decoder specific info size: %d, ptr 0x%p\n", 6, *specificInfoH);
        }
#endif
    } else if (AVCC != NULL) {
#ifdef CORE_PARSER_PARSE_H264_DEC_SPECIFIC_INFO
        if (!(flags & FLAG_H264_NO_CONVERT)) {
            err = parseAvcDecoderSpecificInfo(AVCC, NALLengthFieldSize, specificInfoH);
        } else {
            uint8* data;
            err = MP4SetHandleSize(specificInfoH, AVCC->dataSize);
            if (err)
                goto bail;
            MP4MSG("AVC atom data size: %d, ptr 0x%p\n", AVCC->dataSize, *specificInfoH);
            data = (uint8*)*specificInfoH;
            memcpy(data, AVCC->data, AVCC->dataSize);
        }
#else
        uint8* data;
        /* by default, use AVC atom data as decoder specific info */
        err = MP4SetHandleSize(specificInfoH, AVCC->dataSize);
        if (err)
            goto bail;
        MP4MSG("AVC atom data size: %d, ptr 0x%p\n", AVCC->dataSize, *specificInfoH);
        data = (uint8*)*specificInfoH;
        memcpy(data, AVCC->data, AVCC->dataSize);

#endif
    } else if (HVCC) {
        // Todo: not support byte stream since we need a nal parser for hevc frame.
        if (0) {
            err = parseHevcDecoderSpecificInfo(HVCC, NALLengthFieldSize, specificInfoH);
        } else {
            uint8* data;
            err = MP4SetHandleSize(specificInfoH, HVCC->dataSize);
            if (err)
                goto bail;
            MP4MSG("HEVC atom data size: %d, ptr 0x%p\n", HVCC->dataSize, *specificInfoH);
            data = (uint8*)*specificInfoH;
            memcpy(data, HVCC->data, HVCC->dataSize);
        }
    } else if (VPCC) {
        uint8* data;
        err = MP4SetHandleSize(specificInfoH, VPCC->dataSize);
        if (err)
            goto bail;
        MP4MSG("VPCC atom data size: %d, data ptr %p, target ptr %p\n", VPCC->dataSize, VPCC->data,
               *specificInfoH);
        data = (uint8*)*specificInfoH;
        memcpy(data, VPCC->data, VPCC->dataSize);
        MP4MSG("VPCC copied\n");
    } else if (APVC) {
        uint8* data;
        err = MP4SetHandleSize(specificInfoH, APVC->dataSize);
        if (err)
            goto bail;
        MP4MSG("APVC atom data size: %d, data ptr %p, target ptr %p\n", APVC->dataSize, APVC->data,
               *specificInfoH);
        data = (uint8*)*specificInfoH;
        memcpy(data, APVC->data, APVC->dataSize);
    }

    if (MEDIA_AUDIO == *mediaType) {
        if (AUDIO_ALAC == *decoderType) {
            uint8* data;
            QTsiDecompressParaAtomPtr wave;
            MP4ALACSampleEntryAtomPtr ptr = (MP4ALACSampleEntryAtomPtr)entry;
            err = MP4SetHandleSize(specificInfoH, ALAC_SPECIFIC_INFO_SIZE);
            if (err)
                goto bail;
            data = (uint8*)*specificInfoH;
            wave = (QTsiDecompressParaAtomPtr)ptr->WaveAtomPtr;
            if (wave != NULL)
                memcpy(data, wave->AlacInfo, ALAC_SPECIFIC_INFO_SIZE);
            else if (ptr->AlacInfoAvailable)
                memcpy(data, ptr->AlacInfo, ALAC_SPECIFIC_INFO_SIZE);
        } else if (AUDIO_OPUS == *decoderType) {
            uint8* data;
            MP4OpusSampleEntryAtomPtr ptr = (MP4OpusSampleEntryAtomPtr)entry;
            err = MP4SetHandleSize(specificInfoH, ptr->csdSize);
            if (err)
                goto bail;
            data = (uint8*)*specificInfoH;
            memcpy(data, ptr->csd, ptr->csdSize);
        } else if (AUDIO_FLAC == *decoderType) {
            uint8* data;
            MP4FlacSampleEntryAtomPtr ptr = (MP4FlacSampleEntryAtomPtr)entry;
            err = MP4SetHandleSize(specificInfoH, ptr->csdSize);
            if (err)
                goto bail;
            data = (uint8*)*specificInfoH;
            memcpy(data, ptr->csd, ptr->csdSize);
        } else if (AUDIO_MPEGH_MHA1 == *decoderType || AUDIO_MPEGH_MHM1 == *decoderType) {
            uint8* data;
            MPEGHSampleEntryAtomPtr ptr = (MPEGHSampleEntryAtomPtr)entry;
            if (ptr->MhacAtom) {
                MhacAtomPtr mhac = (MhacAtomPtr)ptr->MhacAtom;
                if (mhac->csdSize > 0 && mhac->csd != NULL) {
                    err = MP4SetHandleSize(specificInfoH, mhac->csdSize);
                    if (err)
                        goto bail;
                    data = (uint8*)*specificInfoH;
                    memcpy(data, mhac->csd, mhac->csdSize);
                }
            }
        }
    } else if (MEDIA_VIDEO == *mediaType) {
        if (VIDEO_HEVC == *decoderType || VIDEO_H264 == *decoderType) {
            GeneralVideoSampleEntryAtomPtr ptr = (GeneralVideoSampleEntryAtomPtr)entry;
            MP4AVCCAtomPtr dvvc = (MP4AVCCAtomPtr)ptr->DVVCAtomPtr;
            if (dvvc != NULL) {
                MP4MSG("SampleEntryAtom with dvvc, convert to dolby vision, size %d",
                       dvvc->dataSize);
                *decoderType = VIDEO_DOLBY_VISION;
            }
        }

        if (VIDEO_DOLBY_VISION == *decoderType) {
            uint32 totalSize = 16;  // 4 uint32 sizes: csd0, csd2, csd_hevc, csd_avc
            DolbyVisionSampleEntryAtomPtr ptr = (DolbyVisionSampleEntryAtomPtr)entry;
            MP4AVCCAtomPtr av1c = (MP4AVCCAtomPtr)ptr->AV1CAtomPtr;
            MP4AVCCAtomPtr dvvc = (MP4AVCCAtomPtr)ptr->DVVCAtomPtr;
            if (av1c && av1c->dataSize > 0)
                totalSize += av1c->dataSize;
            if (dvvc && dvvc->dataSize > 0)
                totalSize += dvvc->dataSize;
            if (HVCC && HVCC->dataSize > 0)
                totalSize += HVCC->dataSize;
            if (AVCC && AVCC->dataSize > 0)
                totalSize += AVCC->dataSize;
            if (totalSize > 16) {
                uint8* data;
                err = MP4SetHandleSize(specificInfoH, totalSize);
                if (err)
                    goto bail;
                data = (uint8*)*specificInfoH;
                *((uint32*)data) = av1c ? av1c->dataSize : 0;
                data += 4;
                *((uint32*)data) = dvvc ? dvvc->dataSize : 0;
                data += 4;
                *((uint32*)data) = HVCC ? HVCC->dataSize : 0;
                data += 4;
                *((uint32*)data) = AVCC ? AVCC->dataSize : 0;
                data += 4;
                if (av1c && av1c->dataSize > 0) {
                    memcpy(data, av1c->data, av1c->dataSize);
                    data += av1c->dataSize;
                }
                if (dvvc && dvvc->dataSize > 0) {
                    memcpy(data, dvvc->data, dvvc->dataSize);
                    data += dvvc->dataSize;
                }
                if (HVCC && HVCC->dataSize > 0) {
                    memcpy(data, HVCC->data, HVCC->dataSize);
                    data += HVCC->dataSize;
                }
                if (AVCC && AVCC->dataSize > 0) {
                    memcpy(data, AVCC->data, AVCC->dataSize);
                    data += AVCC->dataSize;
                }
            }
        }
    }

bail:
    return err;
}

MP4Err MP4GetMediaDataRefCount(MP4Media theMedia, u32* outCount) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4DataInformationAtomPtr dinf;
    MP4DataReferenceAtomPtr dref;

    if ((theMedia == NULL) || (outCount == NULL)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    dinf = (MP4DataInformationAtomPtr)minf->dataInformation;
    if (dinf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    dref = (MP4DataReferenceAtomPtr)dinf->dataReference;
    if (dref == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    *outCount = dref->getEntryCount(dref);
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CheckMediaDataReferences(MP4Media theMedia) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4DataInformationAtomPtr dinf;
    MP4DataReferenceAtomPtr dref;
    u32 count;
    u32 dataEntryIndex;

    if (theMedia == NULL) {
        BAILWITHERROR(MP4BadParamErr);
    }
    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    dinf = (MP4DataInformationAtomPtr)minf->dataInformation;
    if (dinf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    dref = (MP4DataReferenceAtomPtr)dinf->dataReference;
    if (dref == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    count = dref->getEntryCount(dref);
    for (dataEntryIndex = 1; dataEntryIndex <= count; dataEntryIndex++) {
        err = minf->testDataEntry(minf, dataEntryIndex);
        if (err)
            goto bail;
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMediaDataReference(MP4Media theMedia, u32 index, MP4Handle referenceH, MP4Handle urnH,
                                u32* outReferenceType, u32* outReferenceAttributes) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4DataInformationAtomPtr dinf;
    MP4DataReferenceAtomPtr dref;
    MP4DataEntryAtomPtr referenceAtom;
    MP4DataEntryURLAtomPtr url;
    MP4DataEntryURNAtomPtr urn;
    u32 referenceType;
    u32 referenceFlags;

    if ((theMedia == NULL) || (index == 0)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    dinf = (MP4DataInformationAtomPtr)minf->dataInformation;
    if (dinf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    dref = (MP4DataReferenceAtomPtr)dinf->dataReference;
    if (dref == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    if (index > dref->getEntryCount(dref)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    err = dref->getEntry(dref, index, &referenceAtom);
    if (err)
        goto bail;
    if (referenceAtom == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    referenceType = referenceAtom->type;
    referenceFlags = referenceAtom->flags;
    if (outReferenceType) {
        *outReferenceType = referenceType;
    }
    if (outReferenceAttributes) {
        *outReferenceAttributes = referenceFlags;
    }
    if (referenceH || urnH) {
        switch (referenceType) {
            case MP4DataEntryURLAtomType:
                if (referenceH) {
                    url = (MP4DataEntryURLAtomPtr)referenceAtom;
                    err = MP4SetHandleSize(referenceH, url->locationLength);
                    if (err)
                        goto bail;
                    if (url->locationLength) {
                        memcpy(*referenceH, url->location, url->locationLength);
                    }
                }
                break;

            case MP4DataEntryURNAtomType:
                if (referenceH) {
                    url = (MP4DataEntryURLAtomPtr)referenceAtom;
                    err = MP4SetHandleSize(referenceH, url->locationLength);
                    if (err)
                        goto bail;
                    if (url->locationLength) {
                        memcpy(*referenceH, url->location, url->locationLength);
                    }
                }
                if (urnH) {
                    urn = (MP4DataEntryURNAtomPtr)referenceAtom;
                    err = MP4SetHandleSize(referenceH, urn->nameLength);
                    if (err)
                        goto bail;
                    if (urn->nameLength) {
                        memcpy(*referenceH, urn->location, urn->nameLength);
                    }
                }
                break;

            default:
                BAILWITHERROR(MP4InvalidMediaErr);
                break;
        }
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMediaHandlerDescription(MP4Media theMedia, u32* outType, MP4Handle* outName) {
    MP4Err err = MP4NoErr;
    MP4HandlerAtomPtr handler;

    if ((theMedia == NULL) || ((outType == NULL) && (outName == 0))) {
        BAILWITHERROR(MP4BadParamErr);
    }
    handler = (MP4HandlerAtomPtr)((MP4MediaAtomPtr)theMedia)->handler;
    if (handler == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    if (outType) {
        *outType = handler->handlerType;
    }
    if (outName) {
        MP4Handle h;
        err = MP4NewHandle(handler->nameLength, &h);
        if (err)
            goto bail;
        memcpy(*h, handler->nameUTF8, handler->nameLength);
        *outName = h;
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMediaNextInterestingTime(
        MP4Media theMedia, u32 interestingTimeFlags, /* eg: MP4NextTimeMediaSample */
        u64 searchFromTime,                          /* in Media time scale */
        u32 searchDirection,                         /* eg: MP4NextTimeSearchForward */
        u64* outInterestingTime,                     /* in Media time scale */
        u64* outInterestingDuration                  /* in Media's time coordinate system */
) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4TimeToSampleAtomPtr stts;
    s64 priorSample;
    s64 exactSample;
    s64 nextSample;
    u32 sampleNumber;
    s32 sampleDuration;

    if ((theMedia == NULL) || (interestingTimeFlags == 0)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    if (stbl == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stts = (MP4TimeToSampleAtomPtr)stbl->TimeToSample;
    if (stts == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    err = stts->findSamples(stbl->TimeToSample, searchFromTime, &priorSample, &exactSample,
                            &nextSample, &sampleNumber, &sampleDuration);
    if (err)
        goto bail;
    if (outInterestingTime) {
        if (searchDirection == MP4NextTimeSearchForward) {
            if ((exactSample >= 0) && (interestingTimeFlags & MP4NextTimeEdgeOK)) {
                *outInterestingTime = exactSample;
            } else {
                *outInterestingTime = nextSample;
            }

        } else {
            if ((exactSample >= 0) && (interestingTimeFlags & MP4NextTimeEdgeOK)) {
                *outInterestingTime = exactSample;
            } else {
                *outInterestingTime = priorSample;
            }
        }
    }
    if (outInterestingDuration) {
        *outInterestingDuration = sampleDuration;
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMediaSampleDescIndex(MP4Media theMedia, u64 desiredTime,
                                  u32* outSampleDescriptionIndex) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4TimeToSampleAtomPtr stts;
    MP4SampleSizeAtomPtr stsz;
    MP4CompactSampleSizeAtomPtr stz2;
    MP4SampleToChunkAtomPtr stsc;

    s64 priorSample;
    s64 exactSample;
    s64 nextSample;
    u32 sampleNumber;
    s32 sampleDuration;
    u32 chunkNumber;
    u32 sampleDescriptionIndex;
    u32 firstSampleNumberInChunk;
    u32 lastSampleNumberInChunk;

    if ((theMedia == NULL) || (outSampleDescriptionIndex == 0)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    if (stbl == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stts = (MP4TimeToSampleAtomPtr)stbl->TimeToSample;
    if (stts == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stsz = (MP4SampleSizeAtomPtr)stbl->SampleSize;
    stz2 = (MP4CompactSampleSizeAtomPtr)stbl->CompactSampleSize;
    if (stsz == NULL && stz2 == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stsc = (MP4SampleToChunkAtomPtr)stbl->SampleToChunk;
    if (stsc == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    /* get sample number and time */
    err = stts->findSamples(stbl->TimeToSample, desiredTime, &priorSample, &exactSample,
                            &nextSample, &sampleNumber, &sampleDuration);
    if (err)
        goto bail;

    /* get sample description */
    err = stsc->lookupSample(stbl->SampleToChunk, sampleNumber, &chunkNumber,
                             &sampleDescriptionIndex, &firstSampleNumberInChunk,
                             &lastSampleNumberInChunk);
    if (err)
        goto bail;

    *outSampleDescriptionIndex = sampleDescriptionIndex;
bail:
    TEST_RETURN(err);

    return err;
}

/* get a sample of the desired DTS. No longer used. */
MP4Err MP4GetMediaSample(MP4Media theMedia, MP4Handle outSample, u32* outSize,
                         u64 desiredDecodingTime, u64* outDecodingTime, u64* outCompositionTime,
                         u64* outDuration, MP4Handle outSampleDescription,
                         u32* outSampleDescriptionIndex, u32* outSampleFlags) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4TimeToSampleAtomPtr stts;
    MP4CompositionOffsetAtomPtr dtts;
    MP4SyncSampleAtomPtr stss;
    MP4SampleSizeAtomPtr stsz;
    MP4CompactSampleSizeAtomPtr stz2;
    MP4SampleToChunkAtomPtr stsc;
    MP4ChunkOffsetAtomPtr stco;
    MP4DataHandlerPtr dhlr;

    s64 priorSample;
    s64 exactSample;
    s64 nextSample;
    u32 sampleNumber;
    s32 sampleDuration;
    u32 sampleSize;
    u32 chunkNumber;
    u32 sampleDescriptionIndex;
    u64 chunkOffset;
    u32 dataReferenceIndex;
    u32 firstSampleNumberInChunk;
    u32 sampleOffsetWithinChunk;
    u32 lastSampleNumberInChunk;

    if ((theMedia == NULL) || (outSample == 0)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    if (stbl == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stts = (MP4TimeToSampleAtomPtr)stbl->TimeToSample;
    if (stts == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    dtts = (MP4CompositionOffsetAtomPtr)stbl->CompositionOffset;
    stss = (MP4SyncSampleAtomPtr)stbl->SyncSample;
    stsz = (MP4SampleSizeAtomPtr)stbl->SampleSize;
    stz2 = (MP4CompactSampleSizeAtomPtr)stbl->CompactSampleSize;
    if (stsz == NULL && stz2 == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stsc = (MP4SampleToChunkAtomPtr)stbl->SampleToChunk;
    if (stsc == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stco = (MP4ChunkOffsetAtomPtr)stbl->ChunkOffset;
    if (stco == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    /* get sample number and time */
    if (outSampleFlags) {
        *outSampleFlags = 0;
    }
    err = stts->findSamples(stbl->TimeToSample, desiredDecodingTime, &priorSample, &exactSample,
                            &nextSample, &sampleNumber, &sampleDuration);
    if (err)
        goto bail;

    if (dtts) {
        if (outSampleFlags) {
            *outSampleFlags |= MP4MediaSampleHasCTSOffset;
        }

        if (outCompositionTime) {
            s32 compositionTimeOffset;
            err = dtts->getOffsetForSampleNumber((MP4AtomPtr)dtts, sampleNumber,
                                                 &compositionTimeOffset);
            if (err)
                goto bail;
            *outCompositionTime = exactSample + compositionTimeOffset;
        }
    }

    /* get sample description */
    err = stsc->lookupSample(stbl->SampleToChunk, sampleNumber, &chunkNumber,
                             &sampleDescriptionIndex, &firstSampleNumberInChunk,
                             &lastSampleNumberInChunk);
    if (err)
        goto bail;

    if (outSampleDescriptionIndex) {
        *outSampleDescriptionIndex = sampleDescriptionIndex;
    }

    /* get sample size */
    if (stsz != NULL)
        err = stsz->getSampleSizeAndOffset(stbl->SampleSize, sampleNumber, &sampleSize,
                                           firstSampleNumberInChunk, &sampleOffsetWithinChunk);
    else
        err = stz2->getSampleSizeAndOffset(stbl->CompactSampleSize, sampleNumber, &sampleSize,
                                           firstSampleNumberInChunk, &sampleOffsetWithinChunk);
    if (err)
        goto bail;
    *outSize = sampleSize;

    /* get the chunk offset */
    err = stco->getChunkOffset(stbl->ChunkOffset, chunkNumber, &chunkOffset);
    if (err)
        goto bail;

    /* get the sample */
    err = MP4GetMediaSampleDescription(theMedia, sampleDescriptionIndex, outSampleDescription,
                                       &dataReferenceIndex);
    if (err)
        goto bail;
    err = minf->openDataHandler((MP4AtomPtr)minf, dataReferenceIndex);
    if (err)
        goto bail;
    dhlr = (MP4DataHandlerPtr)minf->dataHandler;
    if (dhlr == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    /* finally we can get the sample */
    err = MP4SetHandleSize(outSample, sampleSize);
    if (err)
        goto bail;
    err = dhlr->copyData(dhlr, (chunkOffset + sampleOffsetWithinChunk), *outSample, sampleSize);
    if (err)
        goto bail;

    /* sync sample test */
    if (outSampleFlags) {
        if (stss) {
            u32 sync;
            err = stss->isSyncSample(stbl->SyncSample, sampleNumber, &sync);
            if (err)
                goto bail;
            *outSampleFlags |= (sync == sampleNumber) ? FLAG_SYNC_SAMPLE : 0;
        } else
            *outSampleFlags |= FLAG_SYNC_SAMPLE; /* ENGR54535: every sample is sync */
    }

    if (outDecodingTime) {
        *outDecodingTime = exactSample ? exactSample : nextSample;
    }
    if (outDuration) {
        *outDuration = sampleDuration;
    }

bail:
    TEST_RETURN(err);

    return err;
}

/***********************************************************************************************
Arguments:
Get a new sample by sample number, or output the remaining data of current sample (partial outpt).
outSample [in] sample buffer handle, no longer used.
outSize [out] sample data size.
************************************************************************************************/
MP4Err MP4GetIndMediaSample(MP4Media mediaAtom, /*engr59629*/
                            u32* nextSampleNumber, u32* outSize, u64* outDTS, s32* outCTSOffset,
                            u64* outDuration, u32* outSampleFlags, u32* outSampleDescIndex,
                            u32 odsm_offset, u32 sdsm_offset) {
    // Ranjeet
    MP4Err err = MP4NoErr;
    AVStreamPtr stream;
    uint32 is_sync_frame = 0;
    if (mediaAtom == NULL) {
        BAILWITHERROR(MP4BadParamErr);
    }

    stream = (AVStreamPtr)((MP4MediaAtomPtr)mediaAtom)->parserStream;
    if (NULL == stream)
        BAILWITHERROR(MP4InvalidMediaErr);

    if (stream->sampleBuffer) { /* output buffer shall always been output or released, shall be NULL
                                 */
        MP4MSG("ERR! trk %d Invalid sample buffer left, size %u, ptr %p\n", stream->streamIndex,
               stream->bufferSize, stream->sampleBuffer);
        BAILWITHERROR(PARSER_ERR_UNKNOWN);
    }

#ifdef MERGE_FIELDS
    stream->bGetSecondField = FALSE;
#endif

    if (stream->sampleBytesLeft) /* output the remaining data of current sample */
    {
        err = getSampleRemainingBytes(stream, mediaAtom, nextSampleNumber, outSize, outDTS,
                                      outCTSOffset, outDuration, outSampleFlags,
                                      outSampleDescIndex);

    } else /* output a new sample */
    {
        err = getNextSample(stream, mediaAtom, nextSampleNumber, outSize, outDTS, outCTSOffset,
                            outDuration, outSampleFlags, outSampleDescIndex, odsm_offset,
                            sdsm_offset);

        if ((PARSER_SUCCESS == err) && (stream->mediaType == MEDIA_VIDEO) &&
            (stream->decoderType == VIDEO_H264) && stream->pH264Parser &&
            (stream->sampleBytesLeft == 0)) {
            H264ParserRetCode retCode = H264PARSER_SUCCESS;
#ifndef MERGE_FIELDS
            retCode = ParseH264Field(stream->pH264Parser, stream->sampleBuffer, *outSize,
                                     &is_sync_frame);
            if (H264PARSER_SUCCESS == retCode)  // one sample is one field
            {
                *outSampleFlags |= FLAG_SAMPLE_NOT_FINISHED;
                stream->videoInfo.scanType = VIDEO_SCAN_INTERLACED;
            } else
                stream->videoInfo.scanType = VIDEO_SCAN_PROGRESSIVE;
            MP4MSG("scanType from ParseH264Field %d\n", stream->videoInfo.scanType);
#else
            if (H264PARSER_HAS_ONE_FRAME == retCode) {
                goto bail;
            }

            stream->bGetSecondField = TRUE;
            stream->dwFirstFieldSize = *outSize;
            stream->qwFirstFieldOutDTS = *outDTS;
            stream->qwFirstFieldOutDuration = *outDuration;

            /* current chunk ends ? */
            if (*nextSampleNumber > stream->lastSampleNumberInChunk) {
                stream->chunkNumber = 0; /* shall find a new chunk */
            }

            err = getNextSample(stream, mediaAtom, nextSampleNumber, outSize, outDTS, outCTSOffset,
                                outDuration, outSampleFlags, outSampleDescIndex, odsm_offset,
                                sdsm_offset);

            if (err != 0) {
                goto bail;
            }

            // expect a second filed, frame is finished.
            retCode = ParseH264Field(stream->pH264Parser, stream->sampleBuffer, *outSize,
                                     &is_sync_frame);
            if (H264PARSER_SUCCESS != retCode && H264PARSER_HAS_ONE_FRAME != retCode) {
                MP4MSG("ERR! trk %d, unexpect status, not a second filed\n", stream->streamIndex);
                err = PARSER_ERR_UNKNOWN;
                goto bail;
            }

            // now we get a frame of two filed
            stream->sampleBuffer -= stream->dwFirstFieldSize;
            stream->bufferSize += stream->dwFirstFieldSize;
            *outSize += stream->dwFirstFieldSize;
            *outDTS = stream->qwFirstFieldOutDTS;
            *outDuration += stream->qwFirstFieldOutDuration;
#endif
        }
    }

bail:
    TEST_RETURN(err);

    return err;
}

/********************************************************************************
Arguments:
Get a new "sample" by sample number.
If the buffer is not enough to read an PCM sample, this function will fail "INSUFFICIENT_MEMORY".
No need to handle partial output, because the sample is too small.

Only for PCM audio track. A cache is needed to cache samples to speed up reading.
It reads as much as samples in current chunk until the cache is full or the chunk ends.
All the samples in cache are output as "one sample" to the user.
*********************************************************************************/
MP4Err MP4GetCachedMediaSamples(MP4Media mediaAtom, /*engr59629*/
                                u32* nextSampleNumber, u32* outSize, u64* outDTS, s32* outCTSOffset,
                                u64* outDuration, u32* outSampleFlags, u32* outSampleDescIndex,
                                u32 odsm_offset, u32 sdsm_offset) {
    MP4Err err = MP4NoErr;
    AVStreamPtr stream;

    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4TimeToSampleAtomPtr stts;
    MP4CompositionOffsetAtomPtr dtts;
    MP4SyncSampleAtomPtr stss;
    MP4SampleSizeAtomPtr stsz;
    MP4CompactSampleSizeAtomPtr stz2;
    MP4SampleToChunkAtomPtr stsc;
    MP4ChunkOffsetAtomPtr stco;
    MP4DataHandlerPtr dhlr;
    (void)odsm_offset;
    (void)sdsm_offset;

    u32 sampleNumber = *nextSampleNumber;
    uint8* sampleBuffer;                  /* sample data buffer */
    u32 bufferSize; /* size available */  //= *outSize;
    u32 bufferOffset; /* relative data offset within the buffer, in this reading. same as "total
                         size" */
    u32 sampleCached = 0;
    u32 totalSize = 0; /* data size got in this reading */
    u64 totalDuration = 0;
    u32 firstSampleOffsetWithinChunk; /* offset within the chunk of 1st sample read */

    s64 sampleDTS;
    s32 sampleDuration;
    u32 sampleSize = 0;
    u32 chunkNumber;
    u32 sampleDescriptionIndex;
    u64 chunkOffset = 0;
    u32 dataReferenceIndex;
    u32 firstSampleNumberInChunk;
    u32 lastSampleNumberInChunk;
    u32 sampleOffsetWithinChunk = 0;
    u32 samples_per_packet = 0;
    u32 bytes_per_frame = 0;

    *outSize = 0;

    if (mediaAtom == NULL) {
        BAILWITHERROR(MP4BadParamErr);
    }

    stream = (AVStreamPtr)((MP4MediaAtomPtr)mediaAtom)->parserStream;
    if (NULL == stream)
        BAILWITHERROR(MP4InvalidMediaErr);

    /* output a new sample, cache as much samples as much in one chunk */

    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)mediaAtom)->information;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    if (stbl == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stts = (MP4TimeToSampleAtomPtr)stbl->TimeToSample;
    if (stts == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    dtts = (MP4CompositionOffsetAtomPtr)stbl->CompositionOffset;
    // MP4MSG("dtts = %ld", dtts);
    stss = (MP4SyncSampleAtomPtr)stbl->SyncSample;
    // MP4MSG("stss = %ld", stss);
    stsz = (MP4SampleSizeAtomPtr)stbl->SampleSize;
    stz2 = (MP4CompactSampleSizeAtomPtr)stbl->CompactSampleSize;
    if (stsz == NULL && stz2 == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stsc = (MP4SampleToChunkAtomPtr)stbl->SampleToChunk;
    if (stsc == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stco = (MP4ChunkOffsetAtomPtr)stbl->ChunkOffset;
    if (stco == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    if (outSampleFlags) {
        *outSampleFlags = 0;
    }

    samples_per_packet = ((MP4MediaAtomPtr)mediaAtom)->samples_per_packet;
    bytes_per_frame = ((MP4MediaAtomPtr)mediaAtom)->bytes_per_frame;

    /* get sample DTS,
    When cache is used, use 1st sample's DTS */
    err = stts->getTimeForSampleNumber(stbl->TimeToSample, sampleNumber, (u64*)&sampleDTS,
                                       &sampleDuration);
    if (err)
        goto bail;
    if (dtts) {
        if (outSampleFlags) {
            *outSampleFlags |= MP4MediaSampleHasCTSOffset;
        }
        if (outCTSOffset) {
            s32 decodingOffset;
            err = dtts->getOffsetForSampleNumber((MP4AtomPtr)dtts, sampleNumber, &decodingOffset);
            if (err)
                goto bail;
            *outCTSOffset = decodingOffset;
        }
    }
    if (outDTS) { /* cached samples use 1st sample's DTS */
        *outDTS = sampleDTS;
    }

    /* get sample description & chunk number */
    if (stream->chunkNumber) {
        chunkNumber = stream->chunkNumber;
        sampleDescriptionIndex = stream->sampleDescriptionIndex;
        firstSampleNumberInChunk = stream->firstSampleNumberInChunk;
        lastSampleNumberInChunk = stream->lastSampleNumberInChunk;
    } else { /* first sample in a new chunk */
        err = stsc->lookupSample((MP4AtomPtr)stsc, sampleNumber, &chunkNumber,
                                 &sampleDescriptionIndex, &firstSampleNumberInChunk,
                                 &lastSampleNumberInChunk);
        if (err)
            goto bail;

        stream->chunkNumber = chunkNumber;
        stream->sampleDescriptionIndex = sampleDescriptionIndex;
        stream->firstSampleNumberInChunk = firstSampleNumberInChunk;
        stream->lastSampleNumberInChunk = lastSampleNumberInChunk;
        stream->copyOffsetInChunk = 0;
    }
    // MP4MSG("sample %d, chunk %d, 1st sample %d, last sample %d\n", sampleNumber, chunkNumber,
    // firstSampleNumberInChunk, lastSampleNumberInChunk);

    if (outSampleDescIndex)
        *outSampleDescIndex = sampleDescriptionIndex;

    /* get the chunk offset (shared)*/
    err = stco->getChunkOffset(stbl->ChunkOffset, chunkNumber, &chunkOffset);
    if (err)
        goto bail;

    /* get the sample data handler (shared)*/
    err = MP4GetMediaSampleDescription(mediaAtom, sampleDescriptionIndex, NULL,
                                       &dataReferenceIndex);
    if (err)
        goto bail;
    err = minf->openDataHandler((MP4AtomPtr)minf, dataReferenceIndex);
    if (err)
        goto bail;
    dhlr = (MP4DataHandlerPtr)minf->dataHandler;
    if (dhlr == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    // err = MP4SetHandleSizeLocal( outSample, cacheSize ); if (err) goto bail;

    /* prepare buffer.
    In fact, for PCM audio, we shall always request a new big buffer and output it in once reading!
  */
    if (stream->sampleBuffer) {
        sampleBuffer = stream->sampleBuffer + stream->bufferOffset;
        bufferSize = stream->bufferSize - stream->bufferOffset;
        if (0 >= (s32)bufferSize) {
            MP4MSG("ERR! Invalid sample buffer size %u, ptr %p\n", bufferSize, sampleBuffer);
            BAILWITHERROR(PARSER_ERR_UNKNOWN);
        }
    } else {
        // MP4MSG("Suggested cache size %d\n", ((MP4MediaAtomPtr) mediaAtom)->suggestedCacheSize);
        err = requestSampleBuffer(stream, ((MP4MediaAtomPtr)mediaAtom)->suggestedCacheSize);
        if (PARSER_SUCCESS != err)
            goto bail;
        sampleBuffer = stream->sampleBuffer;
        bufferSize = stream->bufferSize;
        // MP4MSG("buffer got %p, size %ld\n", sampleBuffer, bufferSize );
    }
    bufferOffset = 0;

    // use the samples_per_packet to calculate frame duration and chuck offset.
    if (samples_per_packet > 0)
        bufferSize = samples_per_packet;

    /* read a samples, or if cache is used, read samples until cache is full.
    All cached sample must in the same chunk, so sharing the same data reference & data handler */
    while (bufferOffset < bufferSize) {
        if (sampleNumber > lastSampleNumberInChunk) /* 1st sample in the cache */
        {
            /* chunk number must NOT change ! */
            MP4MSG("sample %d (chunk %d last sample %d): chunk changed, stop caching\n",
                   sampleNumber, chunkNumber, lastSampleNumberInChunk);
            break;
        }

        /* get sample size */
        if (stsz != NULL)
            err = stsz->getSampleSizeAndOffset(stbl->SampleSize, sampleNumber, &sampleSize,
                                               firstSampleNumberInChunk, &sampleOffsetWithinChunk);
        else
            err = stz2->getSampleSizeAndOffset(stbl->CompactSampleSize, sampleNumber, &sampleSize,
                                               firstSampleNumberInChunk, &sampleOffsetWithinChunk);
        if (err)
            goto bail;
        if ((bufferOffset + sampleSize) > bufferSize) {
            MP4MSG("cache is full, stop caching\n");
            break;
        }

        if (0 == sampleCached) /* First sample got */
        {                      /* 1st sample in this reading */
            firstSampleOffsetWithinChunk = sampleOffsetWithinChunk;

            /* sync sample test. Fortunately, video track never cache samples */
            if (outSampleFlags) {
                if (stss) {
                    u32 sync;
                    err = stss->isSyncSample(stbl->SyncSample, sampleNumber, &sync);
                    if (err)
                        goto bail;
                    *outSampleFlags |= (sync == sampleNumber) ? FLAG_SYNC_SAMPLE : 0;
                } else
                    *outSampleFlags |= FLAG_SYNC_SAMPLE; /* ENGR54535 */
            }
        }

        sampleNumber++;
        sampleCached++;
        bufferOffset += sampleSize;
        totalSize += sampleSize;
        totalDuration += sampleDuration;

    } /* read 1 sample or samples */

    // copy and read one buffer according to length of bytes_per_frame when version of sound sample
    // description is 1.
    if (bytes_per_frame > 0 && samples_per_packet > 0) {
        totalSize = bytes_per_frame;
        firstSampleOffsetWithinChunk = stream->copyOffsetInChunk;
        stream->copyOffsetInChunk += totalSize;
    }
    stream->bufferOffset += totalSize;

    // MP4MSG("sample %d: size %d, DTS %d, duration %d\n", *nextSampleNumber, totalSize,
    // (u32)sampleDTS, (u32)totalDuration);
    if (0 == totalSize) {
        *outSize = ((MP4MediaAtomPtr)mediaAtom)->suggestedCacheSize; /* suggest the cache size */
        BAILWITHERROR(PARSER_INSUFFICIENT_MEMORY)
    }

    *nextSampleNumber = sampleNumber;
    stream->nextSampleNumber = sampleNumber;

    /* finally we can get data of all the samples */
    err = dhlr->copyData(dhlr, (chunkOffset + firstSampleOffsetWithinChunk), (char*)sampleBuffer,
                         totalSize);
    if (err)
        goto bail;
    *outSize = totalSize;

    if (outDuration) {
        *outDuration = totalDuration;
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetIndMediaSampleReference(MP4Media theMedia, u32 sampleNumber, u32* outOffset,
                                     u32* outSize, u32* outDuration, u32* outSampleFlags,
                                     u32* outSampleDescIndex, MP4Handle sampleDesc) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4TimeToSampleAtomPtr stts;
    MP4CompositionOffsetAtomPtr dtts;
    MP4SyncSampleAtomPtr stss;
    MP4SampleSizeAtomPtr stsz;
    MP4CompactSampleSizeAtomPtr stz2;
    MP4SampleToChunkAtomPtr stsc;
    MP4ChunkOffsetAtomPtr stco;

    s64 sampleCTS;

    s32 sampleDuration;
    u32 sampleSize;
    u32 chunkNumber;
    u32 sampleDescriptionIndex;
    u64 chunkOffset;
    u32 dataReferenceIndex;
    u32 firstSampleNumberInChunk;
    u32 lastSampleNumberInChunk;
    u32 sampleOffsetWithinChunk;

    if ((theMedia == NULL) || (sampleNumber == 0)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    if (stbl == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stts = (MP4TimeToSampleAtomPtr)stbl->TimeToSample;
    if (stts == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    dtts = (MP4CompositionOffsetAtomPtr)stbl->CompositionOffset;
    stss = (MP4SyncSampleAtomPtr)stbl->SyncSample;
    stsz = (MP4SampleSizeAtomPtr)stbl->SampleSize;
    stz2 = (MP4CompactSampleSizeAtomPtr)stbl->CompactSampleSize;
    if (stsz == NULL && stz2 == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stsc = (MP4SampleToChunkAtomPtr)stbl->SampleToChunk;
    if (stsc == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stco = (MP4ChunkOffsetAtomPtr)stbl->ChunkOffset;
    if (stco == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    if (outSampleFlags) {
        *outSampleFlags = 0;
    }
    /* get sample CTS */
    err = stts->getTimeForSampleNumber(stbl->TimeToSample, sampleNumber, (u64*)&sampleCTS,
                                       &sampleDuration);
    if (err)
        goto bail;
    if (dtts) {
        if (outSampleFlags) {
            *outSampleFlags |= MP4MediaSampleHasCTSOffset;
        }
    }

    /* get sample description */
    err = stsc->lookupSample(stbl->SampleToChunk, sampleNumber, &chunkNumber,
                             &sampleDescriptionIndex, &firstSampleNumberInChunk,
                             &lastSampleNumberInChunk);
    if (err)
        goto bail;

    /* get sample size */
    if (stsz != NULL)
        err = stsz->getSampleSizeAndOffset(stbl->SampleSize, sampleNumber, &sampleSize,
                                           firstSampleNumberInChunk, &sampleOffsetWithinChunk);
    else
        err = stz2->getSampleSizeAndOffset(stbl->CompactSampleSize, sampleNumber, &sampleSize,
                                           firstSampleNumberInChunk, &sampleOffsetWithinChunk);
    if (err)
        goto bail;
    *outSize = sampleSize;

    /* get the chunk offset */
    err = stco->getChunkOffset(stbl->ChunkOffset, chunkNumber, &chunkOffset);
    if (err)
        goto bail;

    /* get the sample description */
    err = MP4GetMediaSampleDescription(theMedia, sampleDescriptionIndex, sampleDesc,
                                       &dataReferenceIndex);
    if (err)
        goto bail;

    if (outOffset) {
        *outOffset = (u32)chunkOffset + sampleOffsetWithinChunk;
    }
    if (outSampleDescIndex) {
        *outSampleDescIndex = sampleDescriptionIndex;
    }
    /* sync sample test */
    if (outSampleFlags) {
        if (stss) {
            u32 sync;
            err = stss->isSyncSample(stbl->SyncSample, sampleNumber, &sync);
            if (err)
                goto bail;
            *outSampleFlags |= (sync == sampleNumber) ? FLAG_SYNC_SAMPLE : 0;
        } else
            *outSampleFlags |= FLAG_SYNC_SAMPLE; /* ENGR54535 */
    }

    if (outDuration) {
        *outDuration = sampleDuration;
    }

bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4MediaTimeToSampleNum(MP4Media theMedia, u64 mediaTime, u32* outSampleNum,
                               u64* outSampleCTS, u64* outSampleDTS, s32* outSampleDuration) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4TimeToSampleAtomPtr stts;
    MP4CompositionOffsetAtomPtr dtts;

    s64 priorSample;
    s64 exactSample;
    s64 nextSample;
    u32 sampleNumber;
    s32 sampleDuration;

    if (theMedia == NULL) {
        BAILWITHERROR(MP4BadParamErr);
    }
    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    if (stbl == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stts = (MP4TimeToSampleAtomPtr)stbl->TimeToSample;
    if (stts == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    dtts = (MP4CompositionOffsetAtomPtr)stbl->CompositionOffset;
    /* get sample number and time */
    err = stts->findSamples(stbl->TimeToSample, mediaTime, &priorSample, &exactSample, &nextSample,
                            &sampleNumber, &sampleDuration);
    if (err)
        goto bail;

    if (dtts) {
        if (outSampleDTS) {
            s32 decodingOffset;
            err = dtts->getOffsetForSampleNumber((MP4AtomPtr)dtts, sampleNumber, &decodingOffset);
            if (err)
                goto bail;
            *outSampleDTS = exactSample - decodingOffset;
        }
    }
    if (outSampleNum) {
        *outSampleNum = sampleNumber;
    }
    if (outSampleCTS) {
        *outSampleCTS = exactSample;
    }
    if (outSampleDuration) {
        *outSampleDuration = sampleDuration;
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4SampleNumToMediaTime(MP4Media theMedia, u32 sampleNum, u64* outSampleCTS,
                               u64* outSampleDTS, s32* outSampleDuration) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4TimeToSampleAtomPtr stts;
    MP4CompositionOffsetAtomPtr dtts;
    s32 sampleDuration;
    s64 sampleCTS;

    if ((theMedia == NULL) || (sampleNum == 0)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    if (stbl == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stts = (MP4TimeToSampleAtomPtr)stbl->TimeToSample;
    if (stts == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    dtts = (MP4CompositionOffsetAtomPtr)stbl->CompositionOffset;
    /* get sample time and duration */
    err = stts->getTimeForSampleNumber(stbl->TimeToSample, sampleNum, (u64*)&sampleCTS,
                                       &sampleDuration);
    if (err)
        goto bail;

    if (dtts) {
        if (outSampleDTS) {
            s32 decodingOffset;
            err = dtts->getOffsetForSampleNumber((MP4AtomPtr)dtts, sampleNum, &decodingOffset);
            if (err)
                goto bail;
            *outSampleDTS = sampleCTS - decodingOffset;
        }
    }
    if (outSampleCTS) {
        *outSampleCTS = sampleCTS;
    }
    if (outSampleDuration) {
        *outSampleDuration = sampleDuration;
    }
bail:
    TEST_RETURN(err);

    return err;
}

/* jlf 12/00: check a specific data ref */
MP4Err MP4CheckMediaDataRef(MP4Media theMedia, u32 dataEntryIndex) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4DataInformationAtomPtr dinf;
    MP4DataReferenceAtomPtr dref;

    if (theMedia == NULL)
        BAILWITHERROR(MP4BadParamErr);
    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
    if (minf == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);
    dinf = (MP4DataInformationAtomPtr)minf->dataInformation;
    if (dinf == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);
    dref = (MP4DataReferenceAtomPtr)dinf->dataReference;
    if (dref == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);

    return minf->testDataEntry(minf, dataEntryIndex);

bail:
    return err;
}

MP4Err MP4GetVideoProperties(MP4Media media, u32 sampleDescIndex, u32* width, u32* height,
                             u32* frameRateNumerator, u32* frameRateDenominator) {
    int32 err = PARSER_SUCCESS;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4SampleDescriptionAtomPtr stsd;
    GenericSampleEntryAtomPtr entry;
    // MP4VisualSampleEntryAtomPtr visualEntry;
    MP4TimeToSampleAtomPtr stts;
    MP4SampleSizeAtomPtr stsz;
    MP4CompactSampleSizeAtomPtr stz2;
    uint64 cts;
    uint32 duration = 0;
    uint32 timeScale;
    uint32 sampleCount;
    uint64 totalDuration = 0;

    uint32 i;

    if ((media == NULL) || (sampleDescIndex == 0)) {
        BAILWITHERROR(MP4BadParamErr);
    }

    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)(media))->information;
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
    if (sampleDescIndex > stsd->getEntryCount(stsd)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    err = stsd->getEntry(stsd, sampleDescIndex, &entry);
    if (err)
        goto bail;
    if (entry == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    // visualEntry = (MP4VisualSampleEntryAtomPtr)entry;
    *width = ((MP4VisualSampleEntryAtomPtr)entry)->video_width;
    *height = ((MP4VisualSampleEntryAtomPtr)entry)->video_height;

    stsz = (MP4SampleSizeAtomPtr)stbl->SampleSize;
    stz2 = (MP4CompactSampleSizeAtomPtr)stbl->CompactSampleSize;
    if (stsz != NULL)
        sampleCount = stsz->sampleCount;
    else if (stz2 != NULL)
        sampleCount = stz2->sampleCount;
    else
        sampleCount = 0;
    MP4MSG("video track has %d samples\n", sampleCount);

    err = MP4GetMediaTimeScale(media, &timeScale);
    if (err)
        goto bail;

    stts = (MP4TimeToSampleAtomPtr)stbl->TimeToSample;
    err = stts->getTotalDuration(stts, &totalDuration);

    if (sampleCount > 0)
        duration = (uint32)(totalDuration / sampleCount);

    // use average frame duration first.
    for (i = 1; i <= sampleCount; i++) {
        if (duration)
            break;
        /* the 1st stts entry may give a ZERO duraiton, need to check more. ENGR105337 */
        err = stts->getTimeForSampleNumber((MP4AtomPtr)stts, i, &cts, (s32*)&duration);
        if (err)
            goto bail;

        MP4MSG("video sample %d 's duration is %d\n", i, duration);
    }

    *frameRateNumerator = timeScale;
    *frameRateDenominator = duration;

    MP4MSG("video width %u, height %u, frame rate %f\n", *width, *height,
           (double)timeScale / duration);

bail:
    return err;
}

MP4Err MP4GetAudioProperties(MP4Media media, u32 sampleDescIndex, sAudioTrackInfo* info,
                             u32* audioObjectType) {
    int32 err = PARSER_SUCCESS;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4SampleDescriptionAtomPtr stsd;
    GenericSampleEntryAtomPtr entry;
    MP4SampleSizeAtomPtr stsz;
    MP4CompactSampleSizeAtomPtr stz2;
    int32 stblSampleSize;

    if ((media == NULL) || (sampleDescIndex == 0)) {
        BAILWITHERROR(MP4BadParamErr);
    }

    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)(media))->information;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    if (stbl == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stsz = (MP4SampleSizeAtomPtr)stbl->SampleSize;
    stz2 = (MP4CompactSampleSizeAtomPtr)stbl->CompactSampleSize;
    if (stsz == NULL && stz2 == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stblSampleSize = stsz != NULL ? stsz->sampleSize : 0;

    stsd = (MP4SampleDescriptionAtomPtr)stbl->SampleDescription;
    if (stsd == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    if (sampleDescIndex > stsd->getEntryCount(stsd)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    err = stsd->getEntry(stsd, sampleDescIndex, &entry);
    if (err)
        goto bail;
    if (entry == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    /* common fields are same for all kinds of audio sample entry */
    info->numChannels =
            ((MP4AudioSampleEntryAtomPtr)entry)->channels; /* ENGR114877: can be 6 channels */
    info->sampleRate = ((MP4AudioSampleEntryAtomPtr)entry)->timeScale;
    info->sampleSize = ((MP4AudioSampleEntryAtomPtr)entry)->sampleSize;
    info->blockAlign = 0;

    /* if sample rate is an invalid value, set it to the time scale value in 'mdhd' */
    if ((u32)-1 == info->sampleRate) {
        if (MP4NoErr != MP4GetMediaTimeScale(media, &info->sampleRate))
            info->sampleRate = 0;
    }

    /* for PCM/ADPCM auido, suggest using cache to speed up sample reading.
    They usually have very small samples of the same size.*/
    if ((QTSowtSampleEntryAtomType == entry->type) || (QTTwosSampleEntryAtomType == entry->type) ||
        (RawAudioSampleEntryAtomType == entry->type) ||
        (ImaAdpcmSampleEntryAtomType == entry->type) ||
        (MuLawAudioSampleEntryAtomType == entry->type)) {
        if ((QTSowtSampleEntryAtomType == entry->type) ||
            (QTTwosSampleEntryAtomType == entry->type) ||
            (RawAudioSampleEntryAtomType == entry->type)) {
            /*ENGR116684: QT PCM auido may has wrong sample size, double check sample size */
            if (1 == stblSampleSize) { /* All samples have the same size (thus same duration).
                                       MP4 standard requires sample size in bytes,
                                       but clips may fill number of audio sample point here.
                                       Here it means 1 sample by mistake.
                                       Need to calculate the real sample size from bits per sample
                                       (8 or 16 bits per sample)& number of channels*/
                stsz->sampleSize = (info->sampleSize) * (info->numChannels) / 8;
                stsz->maxSampleSize = stsz->sampleSize;

                MP4MSG("\nWarning!PCM audio sample size shall be %d (%d bits per sample, channel "
                       "count %d)\n",
                       stsz->sampleSize, info->sampleSize, info->numChannels);
                stblSampleSize = stsz->sampleSize;
            }
        }

        if (stblSampleSize && (stblSampleSize < (PCM_AUDIO_CACHE_SIZE / 2))) {
            ((MP4MediaAtomPtr)media)->suggestCache = TRUE;
            ((MP4MediaAtomPtr)media)->suggestedCacheSize = PCM_AUDIO_CACHE_SIZE;
            MP4MSG("\nPCM/ADPCM audio suggest cache size %d\n",
                   ((MP4MediaAtomPtr)media)->suggestedCacheSize);
        } else {
            MP4MSG("\nPCM/ADPCM audio NOT suggest cache\n");
        }
    } else if (MP4MSAdpcmEntryAtomType == entry->type) {
        MP4SampleDescriptionAtomPtr stsd = NULL;
        GenericSampleEntryAtomPtr atomPtr = NULL;
        AdpcmSampleEntryAtomPtr adpcmPtr = NULL;

        stsd = (MP4SampleDescriptionAtomPtr)stbl->SampleDescription;

        if (stblSampleSize != 1 || stsd == NULL)
            BAILWITHERROR(MP4NoErr)

        if (stsd->getEntryCount(stsd) > 0 && MP4NoErr == stsd->getEntry(stsd, 1, &atomPtr)) {
            if (atomPtr->type == MP4MSAdpcmEntryAtomType) {
                adpcmPtr = (AdpcmSampleEntryAtomPtr)atomPtr;
                if (adpcmPtr->version == 1 && adpcmPtr->bytes_per_frame > 0) {
                    ((MP4MediaAtomPtr)media)->suggestCache = TRUE;
                    ((MP4MediaAtomPtr)media)->suggestedCacheSize = adpcmPtr->bytes_per_frame;
                    // for sound sample description version 1.
                    ((MP4MediaAtomPtr)media)->samples_per_packet = adpcmPtr->samples_per_packet;
                    ((MP4MediaAtomPtr)media)->bytes_per_frame = adpcmPtr->bytes_per_frame;
                    info->blockAlign = ((MP4MediaAtomPtr)media)->bytes_per_frame;
                }
            }
        }
    } else if (MP4Mp3CbrSampleEntryAtomType == entry->type) {
        MP4SampleDescriptionAtomPtr stsd = NULL;
        GenericSampleEntryAtomPtr atomPtr = NULL;
        MP4Mp3SampleEntryAtomPtr mp3Ptr = NULL;

        stsd = (MP4SampleDescriptionAtomPtr)stbl->SampleDescription;

        if (stblSampleSize != 1 || stsd == NULL)
            BAILWITHERROR(MP4NoErr)

        if (stsd->getEntryCount(stsd) > 0 && MP4NoErr == stsd->getEntry(stsd, 1, &atomPtr)) {
            if (atomPtr->type == MP4Mp3CbrSampleEntryAtomType) {
                mp3Ptr = (MP4Mp3SampleEntryAtomPtr)atomPtr;
                if (mp3Ptr->version == 1 && mp3Ptr->bytes_per_frame > 0) {
                    ((MP4MediaAtomPtr)media)->suggestCache = TRUE;
                    ((MP4MediaAtomPtr)media)->suggestedCacheSize = mp3Ptr->bytes_per_frame;
                    // for sound sample description version 1.
                    ((MP4MediaAtomPtr)media)->samples_per_packet = mp3Ptr->samples_per_packet;
                    ((MP4MediaAtomPtr)media)->bytes_per_frame = mp3Ptr->bytes_per_frame;
                }
            }
        }

    } else if (MP4AudioSampleEntryAtomType == entry->type) {
        static uint32 kSamplingRate[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000,
                                         22050, 16000, 12000, 11025, 8000,  7350};

        uint32 type = 0;
        uint32 freqIndex = 0;
        MP4ESDAtomPtr ESD = NULL;
        MP4ES_DescriptorPtr esdDescriptor = NULL;
        MP4DecoderConfigDescriptorPtr decoderConfig = NULL;
        MP4DefaultDescriptorPtr ptr = NULL;

        ESD = (MP4ESDAtomPtr)entry->ESDAtomPtr;
        if (ESD == NULL)
            BAILWITHERROR(MP4NoErr)

        esdDescriptor = (MP4ES_DescriptorPtr)ESD->descriptor;
        if (esdDescriptor == NULL)
            BAILWITHERROR(MP4NoErr)

        decoderConfig = (MP4DecoderConfigDescriptorPtr)esdDescriptor->decoderConfig;
        if (decoderConfig == NULL)
            BAILWITHERROR(MP4NoErr)

        ptr = (MP4DefaultDescriptorPtr)decoderConfig->decoderSpecificInfo;

        if (ptr == NULL || ptr->data == NULL || ptr->dataLength < 2)
            BAILWITHERROR(MP4NoErr)

        type = (ptr->data[0] >> 3) & 0x1f;
        if (type != 31) {
            freqIndex = (ptr->data[0] & 0x07) << 1;
            freqIndex += (ptr->data[1] >> 7) & 0x01;
            if (freqIndex != 15) {
                uint32 sample_rate = 0;
                uint32 numChannels = 0;
                numChannels = (ptr->data[1] >> 3) & 0x0f;
                sample_rate = kSamplingRate[freqIndex];
                if (11 == numChannels)
                    numChannels = 7;
                else if (7 == numChannels || 12 == numChannels || 14 == numChannels)
                    numChannels = 8;
                info->sampleRate = sample_rate;
                if (numChannels != 0)
                    info->numChannels = numChannels;
            }
        } else {
            u16 temp = ptr->data[0] << 8 | ptr->data[1];
            *audioObjectType = 32 + (temp >> 5 & 0x3F);

            // skip 6 bits when type is 31
            freqIndex = (ptr->data[1] >> 1) & 0x0f;
            if (freqIndex != 15) {
                uint32 sample_rate = 0;
                uint32 numChannels = 0;
                numChannels = (ptr->data[1] & 0x01) << 3;
                numChannels += (ptr->data[2] >> 5) & 0x07;
                sample_rate = kSamplingRate[freqIndex];
                if (11 == numChannels)
                    numChannels = 7;
                else if (7 == numChannels || 12 == numChannels || 14 == numChannels)
                    numChannels = 8;
                info->sampleRate = sample_rate;
                if (numChannels != 0)
                    info->numChannels = numChannels;
            }
        }
    } else if (MP4ALACSampleEntryAtomType == entry->type) {
        MP4ALACSampleEntryAtomPtr ptr = (MP4ALACSampleEntryAtomPtr)entry;
        QTsiDecompressParaAtomPtr wave = (QTsiDecompressParaAtomPtr)ptr->WaveAtomPtr;
        if (wave != NULL) {
            uint8* data = (uint8*)wave->AlacInfo;
            data += 12;  // skip headers
            info->sampleSize = data[5];
            info->numChannels = data[9];
            info->sampleRate = data[20] << 24 | data[21] << 16 | data[22] << 8 | data[23];
        } else if (ptr->AlacInfoAvailable) {
            uint8* data = (uint8*)ptr->AlacInfo;
            data += 12;  // skip headers
            info->sampleSize = data[5];
            info->numChannels = data[9];
            info->sampleRate = data[20] << 24 | data[21] << 16 | data[22] << 8 | data[23];
        }
    } else if (MPEGHA1SampleEntryAtomType == entry->type ||
               MPEGHM1SampleEntryAtomType == entry->type) {
        MPEGHSampleEntryAtomPtr ptr = (MPEGHSampleEntryAtomPtr)entry;
        info->profileLevelIndication = 0;
        info->referenceChannelLayout = 0;
        info->compatibleSets = NULL;
        info->compatibleSetsSize = 0;
        if (ptr->MhacAtom != NULL) {
            MhacAtomPtr mhac = (MhacAtomPtr)ptr->MhacAtom;
            info->profileLevelIndication = mhac->profileLevelIndication;
            info->referenceChannelLayout = mhac->referenceChannelLayout;
        }
        if (ptr->MhapAtom != NULL) {
            MhapAtomPtr mhap = (MhapAtomPtr)ptr->MhapAtom;
            if (mhap->compatibleSetsSize > 0 && mhap->compatibleSets != NULL) {
                info->compatibleSets = mhap->compatibleSets;
                info->compatibleSetsSize = mhap->compatibleSetsSize;
            }
        }
    } else if (MP4AC4SampleEntryAtomType == entry->type) {
        // parse dsi info and save in audioTrackInfo
        AC4SampleEntryAtomPtr ptr = (AC4SampleEntryAtomPtr)entry;
        uint32 i = 0, j = 0;

        if (ptr->dsiData != NULL && ptr->dsiLength > 0) {
            BitReader br;
            AC4InfoParser parser;
            BitReaderInit(&br, ptr->dsiData, ptr->dsiLength);
            if (!AC4Parse(&parser, &br)) {
                MP4MSG("AC4InfoParser fail to parse data");
            } else if (parser.mPresentations != NULL && parser.mNumPresentation > 0) {
                audioPresentation* pDst;
                AC4Presentation* pSrc = parser.mPresentations;
                info->audioPresentations =
                        MP4LocalCalloc(parser.mNumPresentation, sizeof(audioPresentation));
                pDst = info->audioPresentations;

                MP4MSG("mNumPresentation %d ", parser.mNumPresentation);
                for (i = 0, j = 0; i < parser.mNumPresentation; i++) {
                    if (!pSrc[i].mEnabled) {
                        MP4MSG("not enabled, skip");
                        continue;
                    }
                    pDst[j].presentationId = pSrc[i].mGroupIndex;
                    // pDst[j].programId = pSrc[i].mProgramID;
                    pDst[j].language = strdup(pSrc[i].mLanguage);
                    if (pSrc[i].mPreVirtualized) {
                        pDst[j].masteringIndication = MASTERED_FOR_HEADPHONE;
                    } else {
                        switch (pSrc[i].mChannelMode) {
                            case CHANNEL_MODE_Mono:
                            case CHANNEL_MODE_Stereo:
                                pDst[j].masteringIndication = MASTERED_FOR_STEREO;
                                break;
                            case CHANNEL_MODE_3_0:
                            case CHANNEL_MODE_5_0:
                            case CHANNEL_MODE_5_1:
                            case CHANNEL_MODE_7_0_34:
                            case CHANNEL_MODE_7_1_34:
                            case CHANNEL_MODE_7_0_52:
                            case CHANNEL_MODE_7_1_52:
                                pDst[j].masteringIndication = MASTERED_FOR_SURROUND;
                                break;
                            case CHANNEL_MODE_7_0_322:
                            case CHANNEL_MODE_7_1_322:
                            case CHANNEL_MODE_7_0_4:
                            case CHANNEL_MODE_7_1_4:
                            case CHANNEL_MODE_9_0_4:
                            case CHANNEL_MODE_9_1_4:
                            case CHANNEL_MODE_22_2:
                                pDst[j].masteringIndication = MASTERED_FOR_3D;
                                break;
                            default:
                                MP4MSG("Invalid channel mode in AC4 presentation");
                        }
                    }
                    pDst[j].audioDescriptionAvailable =
                            (pSrc[i].mContentClassifier == kVisuallyImpaired);
                    pDst[j].spokenSubtitlesAvailable = (pSrc[i].mContentClassifier == kVoiceOver);
                    pDst[j].dialogueEnhancementAvailable = pSrc[i].mHasDialogEnhancements;
                    j++;
                }
                info->numAudioPresentation = j;
                MP4MSG("numAudioPresentation %d", info->numAudioPresentation);
            }

            AC4Free(&parser);
        }
    }

    MP4MSG("audio channel count %u, sample rate %u, sample size %d\n", info->numChannels,
           info->sampleRate, info->sampleSize);

bail:
    return err;
}

/* parse the NAL length field size, SPS and PPS info, wrap SPS/PPS in NALs */
static MP4Err parseAvcDecoderSpecificInfo(MP4AVCCAtomPtr AVCC, uint32* NALLengthFieldSize,
                                          MP4Handle specificInfoH) {
    MP4Err err = MP4NoErr;
    /* H264 video, wrap decoder info in NAL units. The parameter NAL length field size
    is always 2 bytes long, different from that of data NAL units (1, 2 or 4 bytes)*/
    uint32 i, j, k;
    u32 info_size = 0;
    char* data;
    u8 lengthSizeMinusOne;
    u8 numOfSequenceParameterSets;
    u8 numOfPictureParameterSets;
    u16 NALLength;

    if ((NULL == AVCC->data) || (0 == AVCC->dataSize)) {
        *NALLengthFieldSize = 0;
        err = MP4SetHandleSize(specificInfoH, info_size);
        goto bail;
    }

    lengthSizeMinusOne = AVCC->data[4];
    lengthSizeMinusOne &= 0x03;
    *NALLengthFieldSize = lengthSizeMinusOne + 1; /* lengthSizeMinusOne = 0x11, 0b1111 1111 */
    MP4MSG("NAL length size %d bytes\n", *NALLengthFieldSize);

    numOfSequenceParameterSets = (AVCC->data[5]) & 0x1f;
    MP4MSG("number of sequence parameter sets: %d\n", numOfSequenceParameterSets);

    k = 6;
    for (i = 0; i < numOfSequenceParameterSets; i++) {
        if (k >= AVCC->dataSize) {
            MP4MSG("Invalid Sequence parameter NAL length: %d\n", NALLength);
            BAILWITHERROR(MP4InvalidMediaErr);
        }
        NALLength = AVCC->data[k];
        NALLength = (NALLength << 8) + AVCC->data[k + 1];
        // MP4MSG("Sequence parameter NAL length: %d\n",  NALLength);
        k += (NALLength + 2);
        info_size += (NALLength + NAL_START_CODE_SIZE);
    }
    numOfPictureParameterSets = AVCC->data[k];
    k++;
    MP4MSG("number of picture parameter sets: %d\n", numOfPictureParameterSets);

    for (i = 0; i < numOfPictureParameterSets; i++) {
        if (k >= AVCC->dataSize) {
            MP4MSG("Invalid picture parameter NAL length: %d\n", NALLength);
            BAILWITHERROR(MP4InvalidMediaErr);
        }
        NALLength = AVCC->data[k];
        NALLength = (NALLength << 8) + AVCC->data[k + 1];
        // MP4MSG("Picture parameter NAL length: %d\n",  NALLength);
        k += (NALLength + 2);
        info_size += (NALLength + NAL_START_CODE_SIZE);
    }

    err = MP4SetHandleSize(specificInfoH, info_size);
    if (err)
        goto bail;
    MP4MSG("decoder specific info size: %d, ptr 0x%p\n", info_size, *specificInfoH);
    //*specificInfoH = (char *)MP4LocalMalloc( info_size );
    data = *specificInfoH;
    // BAILWITHERROR( MP4InvalidMediaErr );

    k = 6;
    j = 0;
    for (i = 0; i < numOfSequenceParameterSets; i++) {
        NALLength = AVCC->data[k];
        NALLength = (NALLength << 8) + AVCC->data[k + 1];
        // MP4MSG("Sequence parameter NAL length: %d\n",  NALLength);
        *(data + j) = 0;
        *(data + j + 1) = 0;
        *(data + j + 2) = 0;
        *(data + j + 3) = 1;
        j += 4;

        memmove(data + j, AVCC->data + k + 2, NALLength);
        k += (NALLength + 2);
        j += NALLength;
    }
    k++; /* number of picture parameter sets */
    for (i = 0; i < numOfPictureParameterSets; i++) {
        NALLength = AVCC->data[k];
        NALLength = (NALLength << 8) + AVCC->data[k + 1];
        // MP4MSG("Picture parameter NAL length: %d\n",  NALLength);
        *(data + j) = 0;
        *(data + j + 1) = 0;
        *(data + j + 2) = 0;
        *(data + j + 3) = 1;
        j += 4;

        memmove(data + j, AVCC->data + k + 2, NALLength);
        k += (NALLength + 2);
        j += NALLength;
    }

bail:
    return err;
}

static MP4Err parseHevcDecoderSpecificInfo(MP4HVCCAtomPtr HVCC, uint32* NALLengthFieldSize,
                                           MP4Handle specificInfoH) {
    MP4Err err = MP4NoErr;
    uint32 i, j;
    u32 info_size = 0;
    uint8* data;
    uint8* ptr;
    u32 size;
    u8 lengthSizeMinusOne;

    u32 blockNum;

    u32 nalNum;
    u16 NALLength;

    if ((NULL == HVCC->data) || (0 == HVCC->dataSize) || HVCC->dataSize < 23) {
        *NALLengthFieldSize = 0;
        err = MP4SetHandleSize(specificInfoH, info_size);
        goto bail;
    }

    lengthSizeMinusOne = HVCC->data[21];
    lengthSizeMinusOne &= 0x03;
    *NALLengthFieldSize = lengthSizeMinusOne + 1; /* lengthSizeMinusOne = 0x11, 0b1111 1111 */
    MP4MSG("NAL length size %d bytes\n", *NALLengthFieldSize);

    blockNum = HVCC->data[22];

    data = HVCC->data + 23;
    size = HVCC->dataSize - 23;

    // get target codec data len
    for (i = 0; i < blockNum; i++) {
        data += 1;
        size -= 1;

        // Num of nals
        nalNum = (data[0] << 8) + data[1];
        data += 2;
        size -= 2;

        for (j = 0; j < nalNum; j++) {
            if (size < 2) {
                break;
            }

            NALLength = (data[0] << 8) + data[1];
            data += 2;
            size -= 2;

            if (size < NALLength) {
                break;
            }

            // start code needs 4 bytes
            info_size += 4 + NALLength;

            data += NALLength;
            size -= NALLength;
        }
    }
    err = MP4SetHandleSize(specificInfoH, info_size);
    if (err)
        goto bail;
    data = (uint8*)*specificInfoH;

    ptr = HVCC->data + 23;
    size = HVCC->dataSize - 23;
    for (i = 0; i < blockNum; i++) {
        ptr += 1;
        size -= 1;

        // Num of nals
        nalNum = (ptr[0] << 8) + ptr[1];
        ptr += 2;
        size -= 2;

        for (j = 0; j < nalNum; j++) {
            if (size < 2) {
                break;
            }

            NALLength = (ptr[0] << 8) + ptr[1];
            ptr += 2;
            size -= 2;

            if (size < NALLength) {
                break;
            }

            // add start code
            *data = 0;
            *(data + 1) = 0;
            *(data + 2) = 0;
            *(data + 3) = 1;

            // as start code needs 4 bytes, move 2 bytes for start code, the other 2 bytes uses the
            // nal length
            memcpy(data + 4, ptr, NALLength);

            data += 4 + NALLength;

            ptr += NALLength;
            size -= NALLength;
        }
    }

bail:
    return err;
}

bool isTrackEmpty(MP4Media mdia) {
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4SampleSizeAtomPtr stsz;
    MP4CompactSampleSizeAtomPtr stz2;
    MP4TimeToSampleAtomPtr stts;

    if (!mdia)
        goto bail;

    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)mdia)->information;
    if (!minf)
        goto bail;

    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    if (!stbl)
        goto bail;

    stsz = (MP4SampleSizeAtomPtr)stbl->SampleSize;
    stz2 = (MP4CompactSampleSizeAtomPtr)stbl->CompactSampleSize;
    if ((!stsz || (!stsz->sampleSize && !stsz->sampleCount)) && (!stz2 || !stz2->sampleCount)) {
        MP4MSG("stsz table is empty!\n");
        goto bail;
    }

    stts = (MP4TimeToSampleAtomPtr)stbl->TimeToSample;
    if (!stts || !stts->entryCount) {
        MP4MSG("stts table is empty!\n");
        goto bail;
    }

    return FALSE;

bail:
    return TRUE; /* empty track is found */
}

MP4Err MP4GetSampleOffset(MP4Media mediaAtom, u32 sampleNumber, u64* offset) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4SampleToChunkAtomPtr stsc;
    MP4SampleSizeAtomPtr stsz;
    MP4CompactSampleSizeAtomPtr stz2;
    MP4ChunkOffsetAtomPtr stco;
    u32 chunkNumber;
    u32 sampleDescriptionIndex;
    u32 firstSampleNumberInChunk;
    u32 lastSampleNumberInChunk;
    u32 sampleSize;
    u32 sampleOffsetWithinChunk;
    u64 chunkOffset;

    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)mediaAtom)->information;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    if (stbl == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    stsc = (MP4SampleToChunkAtomPtr)stbl->SampleToChunk;
    if (stsc == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    stsz = (MP4SampleSizeAtomPtr)stbl->SampleSize;
    stz2 = (MP4CompactSampleSizeAtomPtr)stbl->CompactSampleSize;
    if (stsz == NULL && stz2 == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    stco = (MP4ChunkOffsetAtomPtr)stbl->ChunkOffset;
    if (stco == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    err = stsc->lookupSample((MP4AtomPtr)stsc, sampleNumber, &chunkNumber, &sampleDescriptionIndex,
                             &firstSampleNumberInChunk, &lastSampleNumberInChunk);
    if (err)
        goto bail;

    if (stsz != NULL)
        err = stsz->getSampleSizeAndOffset((MP4AtomPtr)stsz, sampleNumber, &sampleSize,
                                           firstSampleNumberInChunk, &sampleOffsetWithinChunk);
    else
        err = stz2->getSampleSizeAndOffset((MP4AtomPtr)stz2, sampleNumber, &sampleSize,
                                           firstSampleNumberInChunk, &sampleOffsetWithinChunk);
    if (err)
        goto bail;

    err = stco->getChunkOffset((MP4AtomPtr)stco, chunkNumber, &chunkOffset);
    if (err)
        goto bail;

    *offset = chunkOffset + sampleOffsetWithinChunk;
    // MP4MSG("sample %ld, chunk number %ld, 1st sample number %ld, last sample number %ld, chunk
    // offset %lld\n", sampleNumber, chunkNumber, firstSampleNumberInChunk, lastSampleNumberInChunk,
    // chunkOffset);

bail:
    return err;
}

/* get a new sample */
static MP4Err getNextSample(AVStreamPtr stream, MP4Media mediaAtom, u32* nextSampleNumber,
                            u32* outSize, u64* outDTS, s32* outCTSOffset, u64* outDuration,
                            u32* outSampleFlags, u32* outSampleDescIndex, u32 odsm_offset,
                            u32 sdsm_offset) {
    MP4Err err = MP4NoErr;

    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4TimeToSampleAtomPtr stts;
    MP4CompositionOffsetAtomPtr dtts;
    MP4SyncSampleAtomPtr stss;
    MP4SampleSizeAtomPtr stsz;
    MP4CompactSampleSizeAtomPtr stz2;
    MP4SampleToChunkAtomPtr stsc;
    MP4ChunkOffsetAtomPtr stco;
    MP4DataHandlerPtr dhlr;

    s64 sampleDTS;
    s32 decodingOffset = 0;
    s32 sampleDuration;
    u32 sampleSize = 0;
    u32 chunkNumber;
    u32 sampleDescriptionIndex;
    u64 chunkOffset = 0;
    u32 dataReferenceIndex;
    u32 firstSampleNumberInChunk;
    u32 lastSampleNumberInChunk;
    u32 sampleOffsetWithinChunk = 0;

    u32 sampleNumber = *nextSampleNumber;

    uint8* sampleBuffer;
    u32 bufferSizeRequested;
    u32 bufferSize;
    u32 dataSize;
    uint64 fileOffset;

    bool isH264;
    bool isAC4 = stream->decoderType == AUDIO_AC4;
    uint32 nalLengthFieldSize = stream->nalLengthFieldSize;
    uint32 nalLength = 0;

    (void)odsm_offset;
    (void)sdsm_offset;

    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)mediaAtom)->information;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    if (stbl == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stts = (MP4TimeToSampleAtomPtr)stbl->TimeToSample;
    if (stts == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    dtts = (MP4CompositionOffsetAtomPtr)stbl->CompositionOffset;
    // MP4MSG("dtts = %ld", dtts);
    stss = (MP4SyncSampleAtomPtr)stbl->SyncSample;
    // MP4MSG("stss = %ld", stss);
    stsz = (MP4SampleSizeAtomPtr)stbl->SampleSize;
    // MP4MSG("stsz = %ld", stsz);
    stz2 = (MP4CompactSampleSizeAtomPtr)stbl->CompactSampleSize;
    // MP4MSG("stz2 = %ld", stz2);
    if (stsz == NULL && stz2 == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stsc = (MP4SampleToChunkAtomPtr)stbl->SampleToChunk;
    // MP4MSG("stsc %ld \n", stsc);  // Ranjeet.
    if (stsc == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stco = (MP4ChunkOffsetAtomPtr)stbl->ChunkOffset;
    // MP4MSG("stco %ld \n", stco);  // Ranjeet.
    if (stco == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    if (outSampleFlags) {
        *outSampleFlags = 0;
    }

    /* get sample DTS */
    err = stts->getTimeForSampleNumber((MP4AtomPtr)stts, sampleNumber, (u64*)&sampleDTS,
                                       &sampleDuration);
    if (err)
        goto bail;
    if (dtts) {
        if (outSampleFlags) {
            *outSampleFlags |= MP4MediaSampleHasCTSOffset;
        }
        if (outCTSOffset) {
            err = dtts->getOffsetForSampleNumber((MP4AtomPtr)dtts, sampleNumber, &decodingOffset);
            if (err)
                goto bail;
            *outCTSOffset = decodingOffset;
        }
    }

    /* get sample description */
    if (stream->chunkNumber) {
        chunkNumber = stream->chunkNumber;
        sampleDescriptionIndex = stream->sampleDescriptionIndex;
        firstSampleNumberInChunk = stream->firstSampleNumberInChunk;
        lastSampleNumberInChunk = stream->lastSampleNumberInChunk;
    } else { /* first sample in a new chunk */
        err = stsc->lookupSample((MP4AtomPtr)stsc, sampleNumber, &chunkNumber,
                                 &sampleDescriptionIndex, &firstSampleNumberInChunk,
                                 &lastSampleNumberInChunk);
        if (err)
            goto bail;

        stream->chunkNumber = chunkNumber;
        stream->sampleDescriptionIndex = sampleDescriptionIndex;
        stream->firstSampleNumberInChunk = firstSampleNumberInChunk;
        stream->lastSampleNumberInChunk = lastSampleNumberInChunk;
    }
    // MP4MSG("sample %d, chunk %d, 1st sample %d, last sample %d\n", sampleNumber, chunkNumber,
    // firstSampleNumberInChunk, lastSampleNumberInChunk);

    /* get sample size */
    if (stsz != NULL)
        err = stsz->getSampleSizeAndOffset((MP4AtomPtr)stsz, sampleNumber, &sampleSize,
                                           firstSampleNumberInChunk, &sampleOffsetWithinChunk);
    else
        err = stz2->getSampleSizeAndOffset((MP4AtomPtr)stz2, sampleNumber, &sampleSize,
                                           firstSampleNumberInChunk, &sampleOffsetWithinChunk);
    if (err)
        goto bail;
    if ((MP4_MAX_SAMPLE_SIZE < sampleSize) || (stream->corruptChunk == chunkNumber)) {
        sampleSize = 0;
        stream->sampleBytesLeft = 0;
        stream->corruptChunk = chunkNumber;
        // MP4MSG("Err! sample %d, invalid size %ld\n", sampleNumber, sampleSize);
        // BAILWITHERROR( MP4_ERR_WRONG_SAMPLE_SIZE);
    }

    /* get the chunk offset */
    err = stco->getChunkOffset((MP4AtomPtr)stco, chunkNumber, &chunkOffset);
    if (err)
        goto bail;
    // MP4MSG("new sample %d, size %ld, offset within chunk %ld, chunk offset %lld -> sample offset
    // %lld\n", sampleNumber, sampleSize, sampleOffsetWithinChunk, chunkOffset, chunkOffset +
    // sampleOffsetWithinChunk);

    err = MP4GetMediaSampleDescription(mediaAtom, sampleDescriptionIndex, NULL,
                                       &dataReferenceIndex);
    if (err)
        goto bail;
    err = minf->openDataHandler((MP4AtomPtr)minf, dataReferenceIndex);
    if (err)
        goto bail;
    dhlr = (MP4DataHandlerPtr)minf->dataHandler;
    if (dhlr == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    fileOffset = chunkOffset + sampleOffsetWithinChunk;
    stream->curReadingSampleFileOffset = fileOffset;

    if (sampleSize) {
        /* get sample data */
        isH264 = nalLengthFieldSize ? TRUE : FALSE;
        // MP4MSG("NAL length field size %ld, is h264? %d\n", nalLengthFieldSize, isH264);
        if (isH264) {
            bufferSizeRequested = sampleSize + NALLENFIELD_INCERASE;

#ifdef MERGE_FIELDS  // To avoid seek back, don't merge two field, just set sample unfinished. Let
                     // the user to cope with it.
            // fix ENGR00174607
            if ((stream->bGetSecondField == FALSE) &&
                (sampleNumber < ((stsz != NULL) ? stsz->sampleCount : stz2->sampleCount))) {
                u32 dwNextSampleSize;
                if (stsz != NULL)
                    stsz->getSampleSize((MP4AtomPtr)stsz, sampleNumber + 1, &dwNextSampleSize);
                else
                    stz2->getSampleSize((MP4AtomPtr)stz2, sampleNumber + 1, &dwNextSampleSize);
                bufferSizeRequested += dwNextSampleSize + NALLENFIELD_INCERASE;
            }
#endif
        } else if (isAC4) {
            bufferSizeRequested = sampleSize + AC4_SYNC_HEADER_SIZE;  // space for sync header
        } else
            bufferSizeRequested = sampleSize;

        err = requestSampleBuffer(stream, bufferSizeRequested);

#ifdef MERGE_FIELDS
        /* prepare buffer */
        if (stream->bGetSecondField == FALSE) {
            err = requestSampleBuffer(stream, bufferSizeRequested);
        } else  // get second filed buffer, since we request two max sample size, so no need to
                // alloc again.
        {
            stream->bufferSize -= stream->dwFirstFieldSize;
            stream->sampleBuffer += stream->dwFirstFieldSize;
        }

#endif
        if (PARSER_SUCCESS != err)
            goto bail;
        sampleBuffer = stream->sampleBuffer;
        bufferSize = stream->bufferSize;
        // MP4MSG("new buffer got %p, size %ld (B)\n", stream->sampleBuffer, stream->bufferSize);

        if (isH264) {
            // enough to put a whole h264 frame
            if (bufferSize >= bufferSizeRequested) {
                err = NalStartCodeModifyAndMerge(stream, dhlr, fileOffset, sampleBuffer, bufferSize,
                                                 sampleSize, outSize);
                if (err)
                    goto bail;

                stream->sampleBytesLeft = 0;
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

        // fill sync header
        if (isAC4) {
            sampleBuffer[0] = 0xAC;
            sampleBuffer[1] = 0x40;
            sampleBuffer[2] = 0xFF;
            sampleBuffer[3] = 0xFF;
            sampleBuffer[4] = (uint8)((sampleSize >> 16) &
                                      0xFF);  // use sampleSize because user shall combine
                                              // un-finished samples into one sample
            sampleBuffer[5] = (uint8)((sampleSize >> 8) & 0xFF);
            sampleBuffer[6] = (uint8)((sampleSize >> 0) & 0xFF);
            sampleBuffer += AC4_SYNC_HEADER_SIZE;
            dataSize -= AC4_SYNC_HEADER_SIZE;
        }

        // MP4MSG("trk %d, sample %d, sample size %d, to output %d\n", stream->streamIndex,
        // sampleNumber, sampleSize, dataSize);
        if (dataSize > 0) {
            err = dhlr->copyData(dhlr, fileOffset, (char*)sampleBuffer, dataSize);
            // MP4MSG("Offset number = %ld \n", chunkOffset + sampleOffsetWithinChunk); // Ranjeet
            if (err)
                goto bail;
        }
        // stream->bufferOffset += dataSize;
        fileOffset += dataSize;

        if (isH264) {
            stream->sampleBytesLeft = sampleSize - dataSize - nalLengthFieldSize;
            stream->nalBytesLeft = nalLength - dataSize;
            *outSize = dataSize + NAL_START_CODE_SIZE;
        } else if (isAC4) {
            stream->sampleBytesLeft = sampleSize - dataSize;
            *outSize = dataSize + AC4_SYNC_HEADER_SIZE;
        } else {
            stream->sampleBytesLeft = sampleSize - dataSize;
            *outSize = dataSize;
        }
    } else { /* zero-size sample, shall output an empty buffer */
        MP4MSG("Sample size is 0, bytes left %u\n", stream->sampleBytesLeft);
        if (stream->sampleBuffer)
            BAILWITHERROR(PARSER_ERR_UNKNOWN)
        stream->bufferContext = NULL;
    }

postproc:
    /* sample non-data info */
    if (outSampleDescIndex) { /* all cached samples share the same description index */
        *outSampleDescIndex = sampleDescriptionIndex;
    }

    /* sync sample test. Fortunately, video track never cache samples */
    if (outSampleFlags) {
        if (stss) {
            u32 sync;
            err = stss->isSyncSample((MP4AtomPtr)stss, sampleNumber, &sync);
            if (err)
                goto bail;
            *outSampleFlags |= (sync == sampleNumber) ? FLAG_SYNC_SAMPLE : 0;
        } else
            *outSampleFlags |= FLAG_SYNC_SAMPLE; /* ENGR54535 */
    }

    if (outDTS) { /* cached samples use 1st sample's DTS */
        *outDTS = sampleDTS;
    }

    if (outDuration) {
        *outDuration = sampleDuration;
    }

    if (stream->sampleBytesLeft) /* sample not finished */
    {
        *outSampleFlags |= FLAG_SAMPLE_NOT_FINISHED;
        /* backup sample info */
        stream->sampleFileOffset = fileOffset;
        stream->sampleFlags = *outSampleFlags;
        stream->sampleDTS = sampleDTS;
        stream->sampleCTSOffset = decodingOffset;
        stream->sampleDuration = sampleDuration;
        stream->sampleDescIndex = sampleDescriptionIndex;
        // MP4MSG("sample %d not finished, bytes left %d, nal bytes left %ld\n", sampleNumber,
        // stream->sampleBytesLeft, stream->nalBytesLeft);
    } else {
        sampleNumber++;
        *nextSampleNumber = sampleNumber;
    }

#if 0
    //fix ct39395571
    if(sampleDuration == 0)
    {
        *outSampleFlags |= FLAG_SAMPLE_NOT_FINISHED;
    }
#endif

    stream->nextSampleNumber = sampleNumber;

bail:

    return err;
}

static MP4Err NalStartCodeModifyAndMerge(AVStreamPtr stream, MP4DataHandlerPtr dhlr,
                                         uint64 fileOffset, uint8* sampleBuffer,
                                         u32 sampleBufferLen, u32 sampleSize, u32* outSampleSize) {
    MP4Err err = PARSER_SUCCESS;
    uint32 nalLengthFieldSize = stream->nalLengthFieldSize;
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

static int32 getSampleRemainingBytes(AVStreamPtr stream, MP4Media mediaAtom, u32* nextSampleNumber,
                                     u32* outSize, u64* outDTS, s32* outCTSOffset, u64* outDuration,
                                     u32* outSampleFlags, u32* outSampleDescIndex) {
    int32 err = PARSER_SUCCESS;

    MP4MediaInformationAtomPtr minf;
    MP4DataHandlerPtr dhlr;

    u32 sampleNumber = *nextSampleNumber;

    uint8* sampleBuffer;

    u32 bufferSize;
    u32 bufferSizeRequested;
    u32 dataSize;
    u64 fileOffset;
    bool isH264;
    bool isNewNAL = FALSE;
    uint32 nalLengthFieldSize = stream->nalLengthFieldSize;
    uint32 nalLength;

    *outSize = 0;

    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)mediaAtom)->information;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    dhlr = (MP4DataHandlerPtr)minf->dataHandler; /* use original handler */
    if (dhlr == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    fileOffset = stream->sampleFileOffset;
    stream->curReadingSampleFileOffset = fileOffset;

    isH264 = nalLengthFieldSize ? TRUE : FALSE;
    if (isH264) {
        bufferSizeRequested = stream->nalBytesLeft;
        // MP4MSG("avc sample %d not finished, bytes left %d, nal bytes left %ld\n", sampleNumber,
        // stream->sampleBytesLeft, stream->nalBytesLeft);

        if (!bufferSizeRequested) /* read a new NAL */
        {
            isNewNAL = TRUE;
            err = getNalLength(dhlr, nalLengthFieldSize, fileOffset, &nalLength);
            if (err)
                goto bail;
            fileOffset += nalLengthFieldSize;
            stream->sampleBytesLeft -= nalLengthFieldSize;
            if ((0 == nalLength) ||                      /* zero-size NALU */
                (nalLength > stream->sampleBytesLeft) || /* bytes left is less than NALU size */
                (nalLengthFieldSize >=
                 stream->sampleBytesLeft - nalLength)) /* NALU length size is larger than bytes left
                                                          after reading current NALU */
            {
                /* nalLength == 0 is illegal, handle this error inside core
                 * parser, and return the whole sample left */
                nalLength = stream->sampleBytesLeft;
            }
            stream->nalBytesLeft = nalLength;
            bufferSizeRequested = nalLength + NAL_START_CODE_SIZE;
        }
    } else
        bufferSizeRequested = stream->sampleBytesLeft;

    err = requestSampleBuffer(stream, bufferSizeRequested);
    if (PARSER_SUCCESS != err)
        goto bail;
    sampleBuffer = stream->sampleBuffer;
    bufferSize = stream->bufferSize;
    // MP4MSG("new buffer got %p, size %ld (A)\n", stream->sampleBuffer, stream->bufferSize);

    if (isNewNAL) {
        FILL_NAL_START_CODE(sampleBuffer)
        // stream->bufferOffset += NAL_START_CODE_SIZE;
        sampleBuffer += NAL_START_CODE_SIZE;
        bufferSizeRequested -= NAL_START_CODE_SIZE;
        bufferSize -= NAL_START_CODE_SIZE;
        *outSize = NAL_START_CODE_SIZE;
    }

    dataSize = bufferSizeRequested <= bufferSize ? bufferSizeRequested : bufferSize;
    // MP4MSG("sample %d, bytes left %d, to output %d\n", sampleNumber, stream->sampleBytesLeft,
    // dataSize);

    err = dhlr->copyData(dhlr, fileOffset, (char*)sampleBuffer, dataSize);
    if (err)
        goto bail;

    *outSize += dataSize;

    if (outDTS)
        *outDTS = stream->sampleDTS;
    if (outCTSOffset)
        *outCTSOffset = stream->sampleCTSOffset;
    if (outDuration)
        *outDuration = stream->sampleDuration;
    if (outSampleDescIndex)
        *outSampleDescIndex = stream->sampleDescIndex;

    // stream->bufferOffset += dataSize;
    fileOffset += dataSize;
    stream->sampleBytesLeft -= dataSize;
    if (isH264) {
        stream->nalBytesLeft -= dataSize;
    }

    if (stream->sampleBytesLeft) /* still not finished */
    {
        if (outSampleFlags)
            *outSampleFlags = stream->sampleFlags;
        stream->sampleFileOffset = fileOffset;
        // MP4MSG("sample %d still not finished, bytes left %d\n", sampleNumber,
        // stream->sampleBytesLeft);
    } else {
        if (outSampleFlags)
            *outSampleFlags = stream->sampleFlags & (~FLAG_SAMPLE_NOT_FINISHED);
        sampleNumber++;
        *nextSampleNumber = sampleNumber;
        // MP4MSG("sample %d finished, next sample number %d\n", sampleNumber -1,
        // *nextSampleNumber);
    }

#if 0
    //fix ct39395571
    if(stream->sampleDuration == 0)
    {
        *outSampleFlags |= FLAG_SAMPLE_NOT_FINISHED;
    }
#endif

    stream->nextSampleNumber = sampleNumber;

bail:

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

MP4Err MP4GetMediaTextFormatData(MP4Media theMedia, u32* outSize, MP4Handle outHandle) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4SampleDescriptionAtomPtr stsd;
    TimedTextSampleEntryAtomPtr entry;
    u32 entryCnt = 0;
    u32 entryIndex = 1;
    u32 handleSize = 0;

    if (theMedia == NULL || outSize == NULL || outHandle == NULL) {
        BAILWITHERROR(MP4BadParamErr);
    }
    *outSize = 0;
    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
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

    entryCnt = stsd->getEntryCount(stsd);
    if (entryCnt < 1) {
        BAILWITHERROR(MP4BadParamErr);
    }

    while (entryIndex <= entryCnt) {
        u32 atomSize = 0;
        u16 dataIndex = 0;
        u8* tempPtr = NULL;
        char* data = NULL;
        err = stsd->getEntry(stsd, entryIndex, (GenericSampleEntryAtom**)&entry);
        if (err)
            goto bail;
        if (entry == NULL) {
            BAILWITHERROR(MP4InvalidMediaErr);
        }

        if (entry->size < 16 || entry->dataSize > entry->size)
            break;

        atomSize = (u32)entry->size;
        dataIndex = entry->dataReferenceIndex;

        err = MP4SetHandleSize(outHandle, handleSize + atomSize);
        if (err)
            BAILWITHERROR(MP4NoMemoryErr);

        data = (char*)*outHandle;
        // for little endian
        tempPtr = (u8*)data + handleSize;
        *tempPtr = (atomSize >> 24) & 0xFF;
        tempPtr++;
        *tempPtr = (atomSize >> 16) & 0xFF;
        tempPtr++;
        *tempPtr = (atomSize >> 8) & 0xFF;
        tempPtr++;
        *tempPtr = atomSize & 0xFF;

        tempPtr++;
        *tempPtr = 't';
        tempPtr++;
        *tempPtr = 'x';
        tempPtr++;
        *tempPtr = '3';
        tempPtr++;
        *tempPtr = 'g';
        memcpy((u8*)data + handleSize + 8, &entry->reserved[0], 6);
        tempPtr = (u8*)data + handleSize + 14;
        *tempPtr = (dataIndex >> 8) & 0xFF;
        tempPtr++;
        *tempPtr = dataIndex & 0xFF;

        memcpy(data + handleSize + 16, entry->data, entry->dataSize);
        handleSize += atomSize;
        entryIndex++;
    }
    *outSize = handleSize;

bail:
    return err;
}

MP4Err MP4GetMettMime(MP4Media theMedia, u8** mime, u32* mime_size) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4SampleDescriptionAtomPtr stsd;
    GenericSampleEntryAtomPtr entry;
    MetadataSampleEntryAtomPtr metadata_entry;
    u32 entryCnt = 0;
    u32 entryIndex = 1;

    if (theMedia == NULL || mime == NULL || mime_size == NULL) {
        BAILWITHERROR(MP4BadParamErr);
    }
    *mime_size = 0;
    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
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

    entryCnt = stsd->getEntryCount(stsd);
    if (entryCnt < 1) {
        BAILWITHERROR(MP4BadParamErr);
    }

    while (entryIndex <= entryCnt) {
        err = stsd->getEntry(stsd, entryIndex, (GenericSampleEntryAtom**)&entry);
        if (err)
            goto bail;
        if (entry == NULL) {
            BAILWITHERROR(MP4InvalidMediaErr);
        }
        if (entry->type == MP4MetadataSampleEntryAtomType) {
            u32 null_pos = 0;
            u8* str;
            metadata_entry = (MetadataSampleEntryAtomPtr)entry;
            str = (u8*)metadata_entry->data;

            while (null_pos < metadata_entry->dataSize) {
                if (*(str + null_pos) == '\0') {
                    break;
                }
                ++null_pos;
            }
            if (null_pos == metadata_entry->dataSize - 1) {
                // non-standard compliant metadata track.
                *mime = (u8*)metadata_entry->data;
                *mime_size = metadata_entry->dataSize;
            } else {
                // standard compliant metadata track.
                null_pos = 8;
                // skip encoding string
                while (null_pos < metadata_entry->dataSize) {
                    if (*(str + null_pos) == '\0') {
                        break;
                    }
                    ++null_pos;
                }
                null_pos++;  // skip \0
                if (null_pos < metadata_entry->dataSize) {
                    *mime = (u8*)(str + null_pos);
                    *mime_size = metadata_entry->dataSize - null_pos;
                }
            }
            break;
        }

        entryIndex++;
    }

bail:
    return err;
}

MP4Err MP4GetMediaColorInfo(MP4Media media, u32 sampleDescIndex, u32 decoderType, bool* hasColr,
                            int32* primaries, int32* transfer, int32* coeff,
                            int32* full_range_flag) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4SampleDescriptionAtomPtr stsd;
    GenericSampleEntryAtomPtr entry;
    QTColorParameterAtomPtr colr;

    if ((media == NULL) || (sampleDescIndex == 0) || (NULL == primaries) || (NULL == transfer) ||
        (coeff == NULL) || (full_range_flag == NULL)) {
        BAILWITHERROR(MP4BadParamErr);
    }

    if (decoderType != VIDEO_H264 && decoderType != VIDEO_HEVC && decoderType != VIDEO_MPEG2 &&
        decoderType != VIDEO_AV1 && decoderType != VIDEO_MPEG4)
        BAILWITHERROR(MP4DataEntryTypeNotSupportedErr);

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
    if (sampleDescIndex > stsd->getEntryCount(stsd)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    err = stsd->getEntry(stsd, sampleDescIndex, &entry);
    if (err)
        goto bail;
    if (entry == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    colr = (QTColorParameterAtomPtr)((GenericSampleEntryAtomPtr)entry)->colrAtomPtr;
    if (colr == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);

    *hasColr = 1;
    *primaries = (int32)colr->primariesIndex;
    *transfer = (int32)colr->transferFuncIndex;
    *coeff = (int32)colr->matrixIndex;

    if (colr->full_range_flag & 0x80)  // check the first bit
        *full_range_flag = (int32)1;
    else
        *full_range_flag = (int32)0;
bail:
    return err;
}

MP4Err MP4GetVideoThumbnailSampleTime(MP4Media media, u64* outTime) {
    MP4Err err = MP4NoErr;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4TimeToSampleAtomPtr stts;
    MP4SyncSampleAtomPtr stss;
    MP4SampleSizeAtomPtr stsz;
    MP4CompactSampleSizeAtomPtr stz2;
    const int scan_sample_count = 20;
    u32 start_scan_index = 1;
    u32 sample_index = 0;
    u32 sample_size = 0;
    u32 max_sample_size = 0;
    u32 max_sample_index = 1;
    s32 outDuration = 0;
    int i = 0;

    if ((media == NULL) || outTime == NULL)
        BAILWITHERROR(MP4BadParamErr);

    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)media)->information;
    if (minf == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    if (stbl == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    stts = (MP4TimeToSampleAtomPtr)stbl->TimeToSample;
    if (stts == NULL) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }

    stss = (MP4SyncSampleAtomPtr)stbl->SyncSample;
    stsz = (MP4SampleSizeAtomPtr)stbl->SampleSize;
    stz2 = (MP4CompactSampleSizeAtomPtr)stbl->CompactSampleSize;

    if (stss == NULL) {
        *outTime = 0;
        MP4MSG("stss empty\n");
        return err;
    }

    if (stsz == NULL && stz2 == NULL) {
        *outTime = 0;
        MP4MSG("no stsz");
        return err;
    }

    for (i = 0; i < scan_sample_count; i++) {
        err = stss->nextSyncSample(stbl->SyncSample, start_scan_index, &sample_index, FLAG_FORWARD);

        if (err != MP4NoErr) {
            MP4MSG("nextSyncSample err=%d index=%d,outTime=%lld", err, max_sample_index, *outTime);
            break;
        }

        start_scan_index = sample_index + 1;

        if (stsz != NULL)
            err = stsz->getSampleSize(stbl->SampleSize, sample_index, &sample_size);
        else
            err = stz2->getSampleSize(stbl->CompactSampleSize, sample_index, &sample_size);

        if (err != MP4NoErr) {
            break;
        }

        if (sample_size > max_sample_size) {
            max_sample_size = sample_size;
            max_sample_index = sample_index;
        }
    }

    err = stts->getTimeForSampleNumber(stbl->TimeToSample, max_sample_index, outTime, &outDuration);
    MP4MSG("getTimeForSampleNumber index=%d,outTime=%lld", max_sample_index, *outTime);

    if (err)
        goto bail;

bail:
    return err;
}
