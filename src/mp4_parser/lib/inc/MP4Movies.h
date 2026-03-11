
/************************************************************************
 * Copyright (c) 2005-2014, Freescale Semiconductor, Inc.
 * Copyright 2017-2018, 2021, 2026 NXP
 ************************************************************************/

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
    $Id: MP4Movies.h,v 1.59 2000/03/04 07:58:05 mc Exp $
*/
#ifndef INCLUDED_MP4MOVIE_H
#define INCLUDED_MP4MOVIE_H

#include "fsl_types.h"
#include "file_stream.h"
#include "fsl_media_types.h"
#include "fsl_parser.h"


#include "MP4ParserAPI.h"
#include "MP4ParserPriv.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MP4NewTrackIsVisual = (1 << 1),
    MP4NewTrackIsAudio = (1 << 2),
    MP4NewTrackIsPrivate = (1 << 8)
};

enum {
    MP4ObjectDescriptorHandlerType = MP4_FOUR_CHAR_CODE('o', 'd', 's', 'm'),
    MP4ClockReferenceHandlerType = MP4_FOUR_CHAR_CODE('c', 'r', 's', 'm'),
    MP4SceneDescriptionHandlerType = MP4_FOUR_CHAR_CODE('s', 'd', 's', 'm'),
    MP4VisualHandlerType = MP4_FOUR_CHAR_CODE('v', 'i', 'd', 'e'),
    MP4AudioHandlerType = MP4_FOUR_CHAR_CODE('s', 'o', 'u', 'n'),
    MP4MPEG7HandlerType = MP4_FOUR_CHAR_CODE('m', '7', 's', 'm'),
    MP4OCIHandlerType = MP4_FOUR_CHAR_CODE('o', 'c', 's', 'm'),
    MP4IPMPHandlerType = MP4_FOUR_CHAR_CODE('i', 'p', 's', 'm'),
    MP4MPEGJHandlerType = MP4_FOUR_CHAR_CODE('m', 'j', 's', 'm'),
    MP4HintHandlerType = MP4_FOUR_CHAR_CODE('h', 'i', 'n', 't')
};

#define GETMOOV(arg)                  \
    MP4PrivateMovieRecordPtr moov;    \
    MP4Err err;                       \
    err = MP4NoErr;                   \
    if (arg == NULL)                  \
        BAILWITHERROR(MP4BadParamErr) \
    moov = (MP4PrivateMovieRecordPtr)arg

#define GETMOVIEATOM(arg)      \
    MP4MovieAtomPtr movieAtom; \
    GETMOOV(arg);              \
    movieAtom = (MP4MovieAtomPtr)moov->moovAtomPtr

#define GETMOVIEHEADERATOM(arg)            \
    MP4MovieHeaderAtomPtr movieHeaderAtom; \
    GETMOVIEATOM(arg);                     \
    movieHeaderAtom = (MP4MovieHeaderAtomPtr)movieAtom->mvhd

#define GETIODATOM(arg)                 \
    MP4ObjectDescriptorAtomPtr iodAtom; \
    GETMOVIEATOM(arg);                  \
    iodAtom = (MP4ObjectDescriptorAtomPtr)movieAtom->iods;

struct MP4UserDataRecord {
    void* data;
};
typedef struct MP4UserDataRecord MP4UserDataRecord;
typedef MP4UserDataRecord* MP4UserData;

struct MP4SLConfigRecord {
    void* data;
};
typedef struct MP4SLConfigRecord MP4SLConfigRecord;
typedef MP4SLConfigRecord* MP4SLConfig;

#ifdef PRAGMA_EXPORT
#pragma export on
#endif

/* MP4Handle related */

MP4Err MP4NewHandle(u32 handleSize, MP4Handle* outHandle);

MP4Err MP4SetHandleSizeLocal(MP4Handle h, u32 handleSize);

MP4Err MP4SetHandleSize(MP4Handle h, u32 handleSize);

MP4Err MP4DisposeHandle(MP4Handle h);

MP4Err MP4GetHandleSize(MP4Handle h, u32* outHandleSize);

/* Movie related */

MP4Err MP4DisposeMovie(MP4Movie theMovie);

MP4Err MP4GetMovieDuration(MP4Movie theMovie, u64* outDuration);

MP4Err MP4GetMovieInitialObjectDescriptor(MP4Movie theMovie, MP4Handle outDescriptorH);

MP4Err MP4GetMovieInitialObjectDescriptorUsingSLConfig(MP4Movie theMovie, MP4SLConfig slconfig,
                                                       MP4Handle outDescriptorH);

MP4Err MP4GetMovieIODInlineProfileFlag(MP4Movie theMovie, u8* outFlag);

MP4Err MP4GetMovieProfilesAndLevels(MP4Movie theMovie, u8* outOD, u8* outScene, u8* outAudio,
                                    u8* outVisual, u8* outGraphics);

MP4Err MP4GetMovieTimeScale(MP4Movie theMovie, u32* outTimeScale);

MP4Err MP4GetMovieTrack(MP4Movie theMovie, u32 trackID, MP4Track* outTrack);

MP4Err MP4GetMovieUserData(MP4Movie theMovie, MP4UserData* outUserData);

MP4Err MP4GetMovieCreationTime(MP4Movie theMovie, u64* outTime);

/* Dealing with Movie files */

MP4Err MP4OpenMovieFile(MP4Movie* theMovie, void* fileMappingObject, uint32 openMovieFlags,
                        void* demuxer);

/* Track related */

enum {
    MP4HintTrackReferenceType = MP4_FOUR_CHAR_CODE('h', 'i', 'n', 't'),
    MP4StreamDependencyReferenceType = MP4_FOUR_CHAR_CODE('d', 'p', 'n', 'd'),
    MP4ODTrackReferenceType = MP4_FOUR_CHAR_CODE('m', 'p', 'o', 'd'),
    /* JLF 12/00: added "sync" type for OCR_ES_ID (was broken before) */
    MP4SyncTrackReferenceType = MP4_FOUR_CHAR_CODE('s', 'y', 'n', 'c')
};

MP4_EXTERN(MP4Err)
MP4GetMovieIndTrack(MP4Movie theMovie, u32 trackIndex, MP4Track* outTrack);

/*
MP4_EXTERN ( MP4Err )
MP4GetMovieInitialBIFSTrack( MP4Movie theMovie, MP4Track *outBIFSTrack );
*/

MP4_EXTERN(MP4Err)
MP4GetMovieTrackCount(MP4Movie theMovie, u32* outTrackCount);

MP4_EXTERN(MP4Err)
MP4GetTrackDuration(MP4Track theTrack, long double* outDuration);

MP4_EXTERN(MP4Err)
MP4GetTrackEnabled(MP4Track theTrack, u32* outEnabled);

MP4_EXTERN(MP4Err)
MP4GetTrackID(MP4Track theTrack, u32* outTrackID);

MP4_EXTERN(MP4Err)
MP4GetTrackMedia(MP4Track theTrack, MP4Media* outMedia);

MP4_EXTERN(MP4Err)
MP4GetTrackMovie(MP4Track theTrack, MP4Movie* outMovie);

MP4_EXTERN(MP4Err)
MP4GetTrackOffset(MP4Track track, u32* outMovieOffsetTime);

MP4_EXTERN(MP4Err)
MP4GetTrackReference(MP4Track theTrack, u32 referenceType, u32 referenceIndex,
                     MP4Track* outReferencedTrack);

MP4_EXTERN(MP4Err)
MP4GetTrackReferenceCount(MP4Track theTrack, u32 referenceType, u32* outReferenceCount);

MP4_EXTERN(MP4Err)
MP4GetTrackUserData(MP4Track theTrack, MP4UserData* outUserData);

MP4_EXTERN(MP4Err)
MP4GetTrackRotationDegree(MP4Track theTrack, u32* rotationDegrees);

MP4_EXTERN(MP4Err)
MP4TrackTimeToMediaTime(MP4Track theTrack, u64 inTrackTime, s64* outMediaTime);

MP4_EXTERN(MP4Err)
MP4GetTrackEditListInfo(MP4Track theTrack, u64* segment_duration, s64* media_time);

MP4_EXTERN(MP4Err)
MP4GetTrackDimensions(MP4Track theTrack, u32* outWidth, u32* outHeight);

/* Media related */

MP4_EXTERN(MP4Err)
MP4CheckMediaDataReferences(MP4Media theMedia);

MP4_EXTERN(MP4Err)
MP4GetIndMediaSample(MP4Media mediaAtom, u32* nextSampleNumber,
                     // MP4Handle outSample,
                     u32* outSize, u64* outDTS, s32* outCTSOffset, u64* outDuration,
                     u32* outSampleFlags, u32* outSampleDescIndex, u32 odsm_offset,
                     u32 sdsm_offset);

MP4Err MP4GetCachedMediaSamples(MP4Media mediaAtom, /*engr59629*/
                                u32* nextSampleNumber,
                                // MP4Handle outSample,
                                u32* outSize, u64* outDTS, s32* outCTSOffset, u64* outDuration,
                                u32* outSampleFlags, u32* outSampleDescIndex, u32 odsm_offset,
                                u32 sdsm_offset);

MP4_EXTERN(MP4Err)
MP4GetIndMediaSampleReference(MP4Media theMedia, u32 sampleNumber, u32* outOffset, u32* outSize,
                              u32* outDuration, u32* outSampleFlags, u32* outSampleDescIndex,
                              MP4Handle sampleDesc);

MP4_EXTERN(MP4Err)
MP4GetMediaDataRefCount(MP4Media theMedia, u32* outCount);

/* data reference attributes */
enum {
    MP4DataRefSelfReferenceMask = (1 << 0)
};

enum {
    MP4URLDataReferenceType = MP4_FOUR_CHAR_CODE('u', 'r', 'l', ' '),
    MP4URNDataReferenceType = MP4_FOUR_CHAR_CODE('u', 'r', 'n', ' ')
};

MP4_EXTERN(MP4Err)
MP4GetMediaDataReference(MP4Media theMedia, u32 index, MP4Handle referenceURL,
                         MP4Handle referenceURN, u32* outReferenceType,
                         u32* outReferenceAttributes);

MP4_EXTERN(MP4Err)
MP4GetMediaDuration(MP4Media theMedia, u64* outDuration);

MP4_EXTERN(MP4Err)
MP4GetMediaHandlerDescription(MP4Media theMedia, u32* outType, MP4Handle* outName);

MP4_EXTERN(MP4Err)
MP4GetMediaLanguage(MP4Media theMedia, char* outThreeCharCode);

/* flags for NextInterestingTime */
enum {
    MP4NextTimeSearchForward = 0,
    MP4NextTimeSearchBackward = -1,
    MP4NextTimeMediaSample = (1 << 0),
    MP4NextTimeMediaEdit = (1 << 1),
    MP4NextTimeTrackEdit = (1 << 2),
    MP4NextTimeSyncSample = (1 << 3),
    MP4NextTimeEdgeOK = (1 << 4)
};

/* NB: This ignores any edit list present in the Media's Track */
MP4_EXTERN(MP4Err)
MP4GetMediaNextInterestingTime(MP4Media theMedia,
                               u32 interestingTimeFlags,   /* eg: MP4NextTimeMediaSample */
                               u64 searchFromTime,         /* in Media time scale */
                               u32 searchDirection,        /* eg: MP4NextTimeSearchForward */
                               u64* outInterestingTime,    /* in Media time scale */
                               u64* outInterestingDuration /* in Media's time coordinate system */
);

/* MP4 private sample flags */
enum {
    MP4MediaSampleHasCTSOffset = (1 << 16) /* low 16-bit is reserved for common flags */
};

MP4_EXTERN(MP4Err)
MP4GetMediaSample(MP4Media theMedia, MP4Handle outSample, u32* outSize, u64 desiredDecodingTime,
                  u64* outDecodingTime, u64* outCompositionTime, u64* outDuration,
                  MP4Handle outSampleDescription, u32* outSampleDescriptionIndex,
                  u32* outSampleFlags);

MP4_EXTERN(MP4Err)
MP4GetMediaSampleCount(MP4Media theMedia, u32* outCount);

MP4Err MP4GetMediaTotalBytes(MP4Media theMedia, u64* outTotalBytes);

MP4_EXTERN(MP4Err)
MP4GetMediaSampleDescription(MP4Media theMedia, u32 index, MP4Handle outDescriptionH,
                             u32* outDataReferenceIndex);

MP4_EXTERN(MP4Err)
MP4GetMediaTimeScale(MP4Media theMedia, u32* outTimeScale);

MP4_EXTERN(MP4Err)
MP4GetMediaTrack(MP4Media theMedia, MP4Track* outTrack);

MP4_EXTERN(MP4Err)
MP4MediaTimeToSampleNum(MP4Media theMedia, u64 mediaTime, u32* outSampleNum, u64* outSampleCTS,
                        u64* outSampleDTS, s32* outSampleDuration);

MP4_EXTERN(MP4Err)
MP4SampleNumToMediaTime(MP4Media theMedia, u32 sampleNum, u64* outSampleCTS, u64* outSampleDTS,
                        s32* outSampleDuration);

MP4Err MP4GetVideoProperties(MP4Media media, u32 sampleDescIndex, u32* width, u32* height,
                             u32* frameRateNumerator, u32* frameRateDenominator);

MP4Err MP4GetAudioProperties(MP4Media media, u32 sampleDescIndex, sAudioTrackInfo* info,
                             u32* audioObjectType);

MP4_EXTERN(MP4Err)
MP4GetMediaDecoderConfig(MP4Media theMedia, u32 sampleDescIndex, MP4Handle decoderConfigH);

MP4Err MP4GetMediaDecoderInformation(MP4Media media, u32 sampleDescIndex, u32 flags, u32* mediaType,
                                     u32* decoderType, u32* decoderSubtype, u32* maxBitRate,
                                     u32* avgBitRate, u32* NALLengthFieldSize,
                                     MP4Handle specificInfoH);

MP4Err MP4GetMediaColorInfo(MP4Media media, u32 sampleDescIndex, u32 decoderType, bool* hasColr,
                            int32* primaries, int32* transfer, int32* coeff,
                            int32* full_range_flag);

MP4Err MP4GetVideoThumbnailSampleTime(MP4Media media, u64* outInterestingTime);
/* TrackReader stuff */

MP4_EXTERN(MP4Err)
MP4CreateTrackReader(MP4Track theTrack, MP4TrackReader* outReader);

MP4_EXTERN(MP4Err)
MP4DisposeTrackReader(MP4TrackReader theReader);

MP4_EXTERN(MP4Err)
MP4TrackReaderGetCurrentDecoderConfig(MP4TrackReader theReader, MP4Handle decoderConfigH);

MP4_EXTERN(MP4Err)
MP4TrackReaderGetNextAccessUnit(MP4TrackReader theReader, MP4Handle outAccessUnit, u32* outSize,
                                u32* outSampleFlags, u64* outCTS, u64* outDTS,
                                u32* max_chunk_offset, u32 odsm_offset, u32 sdsm_offset);

MP4_EXTERN(MP4Err)
MP4TrackReaderGetNextAccessUnitWithDuration(MP4TrackReader theReader, MP4Handle outAccessUnit,
                                            u32* outSize, u32* outSampleFlags, u64* outCTS,
                                            u64* outDTS, u32* outDuration, u32* max_chunk_offset,
                                            u32 odsm_offset, u32 sdsm_offset);

MP4_EXTERN(MP4Err)
MP4TrackReaderGetNextPacket(MP4TrackReader theReader, MP4Handle outPacket, u32* outSize);

MP4_EXTERN(MP4Err)
MP4TrackReaderSetSLConfig(MP4TrackReader theReader, MP4SLConfig slConfig);

/* User Data */

MP4_EXTERN(MP4Err)
MP4AddUserData(MP4UserData theUserData, MP4Handle dataH, u32 userDataType, u32* outIndex);

MP4_EXTERN(MP4Err)
MP4GetIndUserDataType(MP4UserData theUserData, u32 typeIndex, u32* outType);

MP4_EXTERN(MP4Err)
MP4GetUserDataEntryCount(MP4UserData theUserData, u32 userDataType, u32* outCount);

MP4_EXTERN(MP4Err)
MP4GetUserDataItem(MP4UserData theUserData, MP4Handle dataH, u32 userDataType, u32 itemIndex);

MP4_EXTERN(MP4Err)
MP4GetUserDataTypeCount(MP4UserData theUserData, u32* outCount);

MP4_EXTERN(MP4Err)
MP4NewUserData(MP4UserData* outUserData);

/* SLConfig */

typedef struct MP4SLConfigSettingsRecord {
    u32 predefined;
    u32 useAccessUnitStartFlag;
    u32 useAccessUnitEndFlag;
    u32 useRandomAccessPointFlag;
    u32 useRandomAccessUnitsOnlyFlag;
    u32 usePaddingFlag;
    u32 useTimestampsFlag;
    u32 useIdleFlag;
    u32 durationFlag;
    u32 timestampResolution;
    u32 OCRResolution;
    u32 timestampLength;
    u32 OCRLength;
    u32 AULength;
    u32 instantBitrateLength;
    u32 degradationPriorityLength;
    u32 AUSeqNumLength;
    u32 packetSeqNumLength;
    u32 timeScale;
    u32 AUDuration;
    u32 CUDuration;
    u64 startDTS;
    u64 startCTS;
    u32 OCRESID;
} MP4SLConfigSettings, *MP4SLConfigSettingsPtr;

MP4_EXTERN(MP4Err)
MP4NewSLConfig(MP4SLConfigSettingsPtr settings, MP4SLConfig* outSLConfig);

MP4_EXTERN(MP4Err)
MP4GetSLConfigSettings(MP4SLConfig config, MP4SLConfigSettingsPtr outSettings);

MP4_EXTERN(MP4Err)
MP4SetSLConfigSettings(MP4SLConfig config, MP4SLConfigSettingsPtr settings);

#ifndef INCLUDED_ISOMOVIE_H
#include "ISOMovies.h"
#endif

/* JLF 12/00: checking of a specific data entry. */
MP4_EXTERN(MP4Err)
MP4CheckMediaDataRef(MP4Media theMedia, u32 dataEntryIndex);

bool isTrackEmpty(MP4Media mdia);
MP4Err MP4GetSampleOffset(MP4Media mediaAtom, u32 sampleNumber, u64* offset);

MP4Err MP4GetMettMime(MP4Media theMedia, u8** mime, u32* mime_size);
MP4Err MP4GetMediaTextFormatData(MP4Media theMedia, u32* outSize, MP4Handle outHandle);

void GetDecoderType(u32 entryType, u32* mediaType, u32* decoderType, u32* decoderSubtype);

#ifdef __cplusplus
}
#endif
#ifdef PRAGMA_EXPORT
#pragma export off
#endif

#endif
