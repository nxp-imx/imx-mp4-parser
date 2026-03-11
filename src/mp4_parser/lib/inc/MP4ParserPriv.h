
/************************************************************************
 * Copyright (c) 2005-2014, Freescale Semiconductor, Inc.
 * Copyright 2017-2018, 2021, 2023, 2024, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ************************************************************************/

#ifndef MP4PARSER_PRIV_HEADER_INCLUDED
#define MP4PARSER_PRIV_HEADER_INCLUDED

#include "MP4LinkedList.h"
#include "h264parser.h"

#if defined(__WINCE) || defined(WIN32)
#include <windows.h>  //Required for definition on LONGLONG
#endif

/*=============================================================================
                             CONSTANTS / VARIABLES
=============================================================================*/
#define MP4_PARSER_MAX_STREAM                                                            \
    64 /* maximum media tracks to support for an MP4 movie.                              \
                      If there are more tracks, those with a larger track number will be \
          overlooked */

#define MIN_MP4_FILE_SIZE 24

#define NAL_START_CODE_SIZE 4 /* Length of H264 NAL start code (0x00000001), 4 bytes */

#define MP4_MAX_SAMPLE_SIZE (8 * 1024 * 1024) /* max valid sample size, 8MB */

#define MAX_AV_INTERLEAVE_DEPTH (8 * 1024 * 1024)

/*=============================================================================
                              MACROS
=============================================================================*/

//#define MP4_PARSER_STREAM_MORE_THEN_MAX                                      11
//#define MP4_PARSER_SUCCESS                                                   0
//#define MP4_PARSER_FAILURE_BIT_ALLOCATION                                  100
//#define MP4_PARSER_CROSS_THE_TOTAL_BYTE_STREAM                             101
//#define MP4_PARSER_MALLOC_FAILED                                            -1
//#define MP4_PARSER_FILE_TRACK_NUMBER    1 // Amanda:No longer used! Conflicting with MP4EOF, file
//reading/writing will map to this value.

//#define MP4_H264_NAL_START_CODE_MARGIN 60 /* extra bytes to insert NAL start codes into H265
//sample data.*/

/* Macros for the user data atom list. */
#define MP4_UDTA_LIST_TITLE 100
#define MP4_UDTA_LIST_ARTIST 101
#define MP4_UDTA_LIST_ALBUM 102
#define MP4_UDTA_LIST_COMMENT 103
#define MP4_UDTA_LIST_TOOL 105
#define MP4_UDTA_LIST_ILST 106
#define MP4_UDTA_LIST_HDLR 107
#define MP4_UDTA_LIST_FREE 108
#define MP4_UDTA_LIST_DATA 109
#define MP4_UDTA_LIST_UNKOWN 110
#define MP4_UDTA_LIST_GNRE 111
#define MP4_UDTA_LIST_YEAR 112
#define MP4_UDTA_LIST_META 113

/* Initialise for the sanity check in the code. */
#define MP4_UDTA_LIST_GNRE_SCI 10000

/*---------------------- STRUCTURE/UNION DATA TYPES --------------------------*/

enum /* ObjectTypeIndication value. 14496-1 section8.6.6 "DecoderConfigDescriptor" */
{
    OBJECT_TYPE_FORBIDDEN = 0,

    OBJECT_SYSTEM_MPEG4_A = 0X01, /* all stream types except IPMP stream */
    OBJECT_SYSTEM_MPEG4_B = 0X02,
    OBJECT_VISUAL_MPEG4 = 0X20,
    OBJECT_AUDIO_MPEG4 = 0X40, /* MPEG4 AAC */

    OBJECT_VISUAL_MPEG2_SIMPLE_PROFILE = 0X60,
    OBJECT_VISUAL_MPEG2_MAIN_PROFILE = 0X61,
    OBJECT_VISUAL_MPEG2_SNR_PROFILE = 0X62,
    OBJECT_VISUAL_MPEG2_SPATIAL_PROFILE = 0X63,
    OBJECT_VISUAL_MPEG2_HIGH_PROFILE = 0X634,
    OBJECT_VISUAL_MPEG2_422_PROFILE = 0X65,

    OBJECT_AUDIO_MPEG2_AAC_MAIN_PROFILE = 0X66,
    OBJECT_AUDIO_MPEG2_AAC_LC_PROFILE = 0X67,
    OBJECT_AUDIO_MPEG2_AAC_SSR_PROFILE = 0X68, /* scalable sampling rate profile */

    OBJECT_AUDIO_MPEG2 = 0X69,

    OBJECT_VISUAL_MPEG1 = 0X6A,
    OBJECT_AUDIO_MPEG1 = 0X6B,

    OBJECT_VISUAL_10918 = 0X6C,

    OBJECT_VISUAL_JPEG2000 = 0X6E,

    OBJECT_AUDIO_AC3 = 0XA5,

    OBJECT_TYPE_OGG = 0XDD,
    OBJECT_TYPE_VORBIS = 0XDE, /* vorbis audio of ogg */
    OBJECT_TYPE_THEORA = 0XDF, /* theora video of ogg */

    OBJECT_TYPE_NOT_SPECIFIED = 0XFF
};

/* stream type value. 14496-1 section8.6.6 "DecoderConfigDescriptor" */
enum {
    MP4_STREAMTYPE_FORBIDDEN = 0,
    MP4_ODSM = 1,          /* object description stream */
    MP4_CLK_REFERENCE = 2, /* clock reference stream */
    MP4_SDSM = 3,          /* scene description stream */
    MP4_VISUAL_STREAM = 4,
    MP4_AUDIO_STREAM = 5,
    MP4_MPEG7_STREAM = 6,
    MP4_IPMP_STREAM = 7,
    MP4_OBJECT_CONTENT_INFO_STREAM = 8,
    MP4_MPEGJ_STREAM = 9 /* 0X0A - 0X1F reserved for ISO use */
};

/* Audio object type value, 14496-3 section 1.6 */
enum {
    AUDIO_OBJECT_ER_BSAC = 22
};

enum {
    QT_WELL_KNOWN_DATA_TYPE_UTF_8 = 1,
    QT_WELL_KNOWN_DATA_TYPE_UTF_16BE = 2,
    QT_WELL_KNOWN_DATA_TYPE_S_JIS = 3,
    QT_WELL_KNOWN_DATA_TYPE_UTF_8_SORT = 4,
    QT_WELL_KNOWN_DATA_TYPE_UTF_16BE_SORT = 5,
    QT_WELL_KNOWN_DATA_TYPE_JPEG = 13,
    QT_WELL_KNOWN_DATA_TYPE_PNG = 14,
    QT_WELL_KNOWN_DATA_TYPE_INT_BE = 21,
    QT_WELL_KNOWN_DATA_TYPE_UINT_BE = 22,
    QT_WELL_KNOWN_DATA_TYPE_FLOAT32_BE = 23,
    QT_WELL_KNOWN_DATA_TYPE_FLOAT64_BE = 24,
    QT_WELL_KNOWN_DATA_TYPE_BMP = 27,
    QT_WELL_KNOWN_DATA_TYPE_QT_METADATA_ATOM = 28
};

typedef struct mp4AudioSpecificConfig {
    /* Audio Specific Info */
    unsigned char objectTypeIndex;
    unsigned char samplingFrequencyIndex;
    unsigned long samplingFrequency;
    unsigned char channelsConfiguration;

    /* GA Specific Info */
    unsigned char frameLengthFlag;
    unsigned char dependsOnCoreCoder;
    unsigned short coreCoderDelay;
    unsigned char layerNr;
    unsigned int numOfSubFrame;
    unsigned int layer_length;
    unsigned char extensionFlag;
    unsigned char aacSectionDataResilienceFlag;
    unsigned char aacScalefactorDataResilienceFlag;
    unsigned char aacSpectralDataResilienceFlag;
    unsigned char epConfig;

    char sbr_present_flag;
    char forceUpSampling;
} mp4AudioSpecificConfig;

struct MP4MovieRecord {
    void* data; /* ptr to MP4MovieRecord */
};
typedef struct MP4MovieRecord MP4MovieRecord;
typedef MP4MovieRecord* MP4Movie;

typedef void* MP4Track; /* engr59629: To hide atom detail. To use it as "MP4TrackAtomPtr", defined
                           in "MP4Atoms.h". */

typedef void* MP4Media; /* engr59629: To hide atom detail. To use it as "MP4MediaAtomPtr", defined
                           in "MP4Atoms.h". */

struct MP4TrackReaderRecord {
    void* data;
};
typedef struct MP4TrackReaderRecord MP4TrackReaderRecord;
typedef MP4TrackReaderRecord* MP4TrackReader;
typedef char** MP4Handle;

typedef void* MP4FragmentedReader;

// MasteringIndication
enum {
    MASTERING_NOT_INDICATED,
    MASTERED_FOR_STEREO,
    MASTERED_FOR_SURROUND,
    MASTERED_FOR_3D,
    MASTERED_FOR_HEADPHONE,
};

typedef struct {
    int32 presentationId;
    char* language;
    uint32 masteringIndication;
    uint32 audioDescriptionAvailable;
    uint32 spokenSubtitlesAvailable;
    uint32 dialogueEnhancementAvailable;
} audioPresentation;

typedef struct {
    uint32 frameWidth;
    uint32 frameHeight;
    uint32 frameRateNumerator;
    uint32 frameRateDenominator;
    // uint32 pixelAspectRatioX;
    // uint32 pixelAspectRatioY;
    // bool fContentIsInterlaced;
    uint32 NALLengthFieldSize;
    uint32 frameRotationDegree;
    bool hasColorInfo;
    int32 primaries;
    int32 transfer;
    int32 coeff;
    int32 full_range_flag;
    uint32 displayWidth;
    uint32 displayHeight;
    uint64 thunmbnailTime;
    uint32 scanType;
} sVideoTrackInfo;

typedef struct {
    uint32 sampleRate;
    uint32 numChannels;                     // would be 6 for 5.1
    uint32 numPrimaryChannels;              // would be 5 for 5.1
    uint32 numSecondaryChannels;            // would be 1 for 5.1
    uint32 sampleSize;                      /* bits per PCM audio sample */
    uint32 blockAlign;                      // block align for adpcm
    uint32 profileLevelIndication;          // MPEG-H
    uint32 referenceChannelLayout;          // MPEG-H
    uint32 compatibleSetsSize;              // MPEG-H
    u8* compatibleSets;                     // MPEG-H
    audioPresentation* audioPresentations;  // AC4
    u8 numAudioPresentation;                // AC4
} sAudioTrackInfo;

typedef struct {
    uint32 handleSize;
    uint8* mime;
    uint32 mime_size;
} sTextTrackInfo;

typedef struct {
    uint32 index;
    uint64 pts;
} sSamplePtsEntry;

typedef struct AVStream {
    uint32 streamIndex; /* 0-based stream index. Not assume stream 0 is video ! */
    void* appContext;   /* application context of the parser */

    FslFileHandle fileHandle; /* not used now */

    uint32 mediaType;
    uint32 decoderType;
    uint32 decoderSubtype; /* 0 means unknown or no subtypes */

    bool enabled; /* Only enabled track can output samples */

    bool seekable; /* a track is seekable only if it has random access points */
    bool isEmpty;  /* a track is emtpy if its index tables are empty.
                     Shall not read an empty track. */

    uint32 timeScale;
    // uint32 startDelay; /* start delay of the stream in scale. NON-zero means the stream does not
    // start concurrently with the file */
    long double usDuration; /*duration in us */

    uint32 maxBitRate;
    uint32 avgBitRate;

    uint32 numSamples; /* for debug only. Total number of samples in this track */
    uint64 totalBytes; /* for debug only. Total bytes of samples in this track */

    uint64 firstSampleFileOffset;

    union {
        sVideoTrackInfo videoInfo;
        sAudioTrackInfo audioInfo;
        sTextTrackInfo textInfo;
    };

    /* only for BSAC audio tracks, to handle track reference */
    uint32 isBSAC;
    uint32 nextBSACTrackNum; /* for last BSAC audio track, this value is invalid (-1) */
    uint32 prevBSACTrackNum; /* for 1st BSAC audio track, this value is invalid (-1) */

    MP4Track trak; /* track atom */
    MP4Media mdia; /* media atom */
    MP4TrackReader reader;
    MP4Handle decoderSpecificInfoH; /* handle of decoder specific info */
    MP4Handle sampleH;              /* Handle of sample */
    MP4Handle tx3gHandle;           /* Handle of tx3g atom */
    /* reading control ---- START */
    uint32 readMode;

    uint32 nextSampleNumber; /* next sample number, 1-based */
    uint32 chunkNumber; /* current chunk number, 1-based. 0 means not-decided and need lookup, eg.
                            reading 1st sample in a chunk, after seek, or trick mode.*/
    uint32 firstSampleNumberInChunk;
    uint32 lastSampleNumberInChunk;
    uint32 sampleDescriptionIndex;

    uint8* sampleBuffer; /* current sample buffer, from application, not released or output yet. */
    void* bufferContext; /* buffer context, from application on requesting this buffer */
    uint32 bufferSize;
    uint32 bufferOffset; /* offset in bytes. A buffer may contain more than one samples, for PCM
                            audio. */
    uint32 copyOffsetInChunk;  // data offset in a chunk.
    uint32 nalLengthFieldSize; /* bytes of NAL length field, in bytes. Only for H.264 video track. 0
                                  for other media types. */
    bool internalSample;

    /* partial output of large samples */
    uint32 sampleBytesLeft; /* bytes of current sample data not output yet, with the possible
                            padding byte. If this value is non-zero, current sample output is not
                            finished, and need backup its flag, size, start time & end time*/

    uint32 nalBytesLeft; /* bytes of current NAL data not output yet.Only for H.264 video track. */

    /* backup of current sample info*/
    uint64 sampleFileOffset; /* file offset of sample data not output yet */
    uint32 sampleFlags;      /* flag backup */
    uint64 sampleDTS;        /* start time backup, in us */
    int32 sampleCTSOffset;
    uint64 sampleDuration;  /* sample duration backup, in us */
    uint32 sampleDescIndex; /* description index */

    /* for file-based trick mode */
    // bool bos; /* whether meet BOS when rewinding. seeking will clear this flag */
    bool isOneSampleGotAfterSkip; /* Whether just read a sample after seekig or skipping to a sync
                                    sample. to avoid sample counter decrease without sample reading
                                    when rewinding */

    /* sample info for media appender */
    uint64 curReadingSampleFileOffset;

    /* for error concealment */
    uint32 corruptChunk;

    /* reading control ---- END */

    MP4LinkedList userDataList;
    H264ParserHandle pH264Parser;
    bool bGetSecondField;
    uint32 dwFirstFieldSize;
    u64 qwFirstFieldOutDTS;
    u64 qwFirstFieldOutDuration;

    bool bPrevSampleNotFinished;
    TrackExtTagList* extTagList;
    uint32 audioObjectType;

    sSamplePtsEntry* samplePtsTable;

    /* HEIF format */
    ImageInfo imageInfo;
    s32 currentReadingImageIndex;
} AVStream, *AVStreamPtr;

/* Structure to keep the information of the userdata list. */
typedef struct {
    u16* unicodeString;
    u32 stringLength;

} sUserDataEntry, *sUserDataEntryPtr;

typedef struct {
    sUserDataEntry FileName;   /* Holds name of the file.     */
    sUserDataEntry ArtistName; /* Holds name of the artist.   */
    sUserDataEntry AlbumName;  /* Holds name of the album.    */

    sUserDataEntry Comment; /* Holds comment of the file.  */
    sUserDataEntry Tool;    /* Holds tool used in file.    */
    sUserDataEntry Year;    /* Holds the release year. Creation date.    */

    sUserDataEntry Genre; /* Holds Books and Spoken, or  */
                          /* Spoken Word, or Audio book. */

    sUserDataEntry CopyRight;

} sMP4ParserUdtaList, *sMP4ParserUdtaListPtr;

// USER_DATA_RATING writes 64-bit pointer into asciBuf, whose max length is 20, plus null ending is
// 21
#define ASCILEN 21

typedef struct {
    void* appContext;

    const uint8* fileName; /* file name or url */
    struct FileMappingObjectRecord* fileMappingObject;
    bool isLive;
    uint32 flags;

    uint64 fileSize; /* actual file size in bytes */

    MP4Movie movie; /* private movie record */

    uint32 numTracks;
    AVStreamPtr streams[MP4_PARSER_MAX_STREAM]; /* tracks */

    uint32 primaryTrackNumber; /* 0-based, 1st video track, or 1st audio track is video is not
                                  present */
    bool seekable;

    bool isDeepInterleaving; /* deep interleaved or non-interleaved */

    uint32 readMode;        /* file-based or track-based */
    AVStreamPtr nextStream; /* next stream to read, only for file-based mode */

    sMP4ParserUdtaList udtaList;
    MP4LinkedList userDataList;

    /* for BSAC audio tracks */
    uint32 numBSACTracks; /* total number of BSAC tracks */
    uint32 firstBSACTrackNum;
    uint32 nextBSACTrackNum; /* next BSAC track to read */

    /* for USER_DATA_TRACKNUMBER, USER_DATA_TOTALTRACKNUMBER, get UTF-8 format */
    uint8 asciTrackNumber[ASCILEN];
    uint8 asciTotalTrackNumber[ASCILEN];
    int32 audioEncoderDelay;
    int32 audioEncoderPadding;

    uint8 asciUserRating[ASCILEN];

    bool fragmented;
    u64 firstMoof;
    u64 currentMoof;
    MP4FragmentedReader fragmented_reader;
    uint32 psshLen;
    void* pssh;
    u64 creationTime;

    bool isImageFile;
} sMP4ParserObject, *sMP4ParserObjectPtr;

enum {
    MONO = 1,
    STEREO = 2
};

#define PREROLL_MARGIN_IN_US (1000 * 1000)

/*---------------------------- GLOBAL DATA ----------------------------------*/
/*------------------------ FUNCTION PROTOTYPE(S) ----------------------------*/
/* To get the size of the header of the ADTS in terms of bytes. */
#define ADTS_HEADER_SIZE(p) (7 + (p->protection_abs ? 0 : ((p->num_of_rdb + 1) * 2)))

enum {
    AOT_USAC = 42,
};

#endif /* MP4PARSER_PRIV_HEADER_INCLUDED */
