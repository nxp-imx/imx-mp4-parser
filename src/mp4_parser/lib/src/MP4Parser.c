/*
***********************************************************************
* Copyright (c) 2007-2016, Freescale Semiconductor, Inc.
* Copyright 2017-2018, 2020-2026 NXP
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************
*/

//#define DEBUG
#include "fsl_types.h"
#include "file_stream.h"
#include "fsl_media_types.h"
#include "fsl_parser.h"

#include "MP4ParserAPI.h"
#include "W32FileMappingObject.h"

#include "MP4TrackReader.h"
#include "ID3GenreTable.h"
#include "MP4Atoms.h"
#include "MP4DataHandler.h"
#include "MP4Impl.h"

#include <string.h>
#include "FragmentedReader.h"
#include "ItemTable.h"
#include "MP4TagList.h"

/*--------------------------------- Version Infomation --------------------------------*/
#define SEPARATOR " "

#define BASELINE_SHORT_NAME "MPEG4PARSER_07.00.00"

#if defined(__WINCE)
#define OS_NAME "_WINCE"
#elif defined(WIN32)
#define OS_NAME "_WIN32"
#else
#define OS_NAME ""
#endif

#ifdef DEMO_VERSION
#define CODEC_RELEASE_TYPE "_DEMO"
#else
#define CODEC_RELEASE_TYPE ""
#endif

/* user define suffix */
#define VERSION_STR_SUFFIX ""

#define CODEC_VERSION_STR                                                                  \
    (BASELINE_SHORT_NAME OS_NAME CODEC_RELEASE_TYPE SEPARATOR VERSION_STR_SUFFIX SEPARATOR \
     "build on" SEPARATOR __DATE__ SEPARATOR __TIME__)

#ifdef DEBUG
#ifdef ANDROID
#include <android/log.h>
#define LOG_BUF_SIZE 1024

void LogOutput(char* fmt, ...) {
    va_list ap;
    char buf[LOG_BUF_SIZE];

    va_start(ap, fmt);
    vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
    va_end(ap);

    __android_log_write(ANDROID_LOG_DEBUG, "MP4_PARSER", buf);

    return;
}

#pragma message("enable debug message")
#define LOG_PRINTF LogOutput
#endif
#endif

#ifndef INT32_MAX
#define INT32_MAX 0x7fffffff
#endif

#ifndef INT32_MIN
#define INT32_MIN (-0x7fffffff - 1)
#endif

#define ABS(a, b) (a > b ? a - b : b - a)

/***************************************************************************************
 *                  Global variables
 ***************************************************************************************/
FslFileStream g_streamOps;
ParserMemoryOps g_memOps;
ParserOutputBufferOps g_outputBufferOps;

static uint32 g_userDataTypeTable[USER_DATA_MAX][4] = {
        {UdtaTitleType, Udta3GppTitleType, -1},                 /* USER_DATA_TITLE */
        {-1},                                                   /* USER_DATA_LANGUAGE */
        {UdtaGenreType, UdtaGenreType2, Udta3GppGenreType, -1}, /* USER_DATA_GENRE */
        {UdtaArtistType, Udta3GppArtistType, -1},               /* USER_DATA_ARTIST */
        {UdtaCopyrightType, -1},                                /* USER_DATA_COPYRIGHT */
        {UdtaCommentType, -1},                                  /* USER_DATA_COMMENTS */
        {UdtaDateType, -1},                                     /* USER_DATA_CREATION_DATE */
        {UdtaRateType, -1},                                     /* USER_DATA_RATING */
        {UdtaAlbumType, Udta3GppAlbumType, -1},                 /* USER_DATA_ALBUM */
        {-1},                                                   /* USER_DATA_VCODECNAME */
        {-1},                                                   /* USER_DATA_ACODECNAME */
        {UdtaCoverType, -1},                                    /* USER_DATA_ARTWORK */
        {UdtaComposerType, -1},                                 /* USER_DATA_COMPOSER */
        {UdtaDirectorType, -1},                                 /* USER_DATA_DIRECTOR */
        {UdtaInformationType, -1},                              /* USER_DATA_INFORMATION */
        {UdtaCreatorType, -1},                                  /* USER_DATA_CREATOR */
        {UdtaProducerType, -1},                                 /* USER_DATA_PRODUCER */
        {UdtaPerformerType, -1},                                /* USER_DATA_PERFORMER */
        {UdtaRequirementType, -1},                              /* USER_DATA_REQUIREMENTS */
        {UdtaSongWritorType, -1},                               /* USER_DATA_SONGWRITER */
        {UdtaMovieWritorType, Udta3GppAuthType, -1},            /* USER_DATA_MOVIEWRITER */
        {UdtaToolType, -1},                                     /* USER_DATA_TOOL */
        {UdtaDescriptionType, UdtaDescType2, -1},               /* USER_DATA_DESCRIPTION */
        {UdtaTrackNumberType, UdtaCDTrackNumType, -1},          /* USER_DATA_TRACKNUMBER */
        {UdtaTrackNumberType, -1},                              /* USER_DATA_TOTALTRACKNUMBER */
        {UdtaLocationType, -1},                                 /* USER_DATA_GEOGRAPH */
        {-1},                                                   // USER_DATA_CHAPTER_MENU
        {-1},                                                   // USER_DATA_FORMATVERSION
        {-1},                                                   // USER_DATA_PROFILENAME
        {-1},                                                   // USER_DATA_PROGRAMINFO
        {-1},                                                   // USER_DATA_PMT
        {FSLiTunSMPBAtomType, -1},                              // USER_DATA_AUD_ENC_DELAY 31
        {FSLiTunSMPBAtomType, -1},                              // USER_DATA_AUD_ENC_PADDING
        {-1},                                                   // USER_DATA_DISCNUMBER
        {UdtaAuthorType, -1},                                   // USER_DATA_AUTHOR
        {UdtaCollectionType, -1},                               // USER_DATA_COLLECTION
        {UdtaPublisherType, -1},                                // USER_DATA_PUBLISHER
        {UdtaSoftwareType, -1},                                 // USER_DATA_SOFTWARE
        {UdtaYearType, Udta3GppYearType, -1},                   // USER_DATA_YEAR
        {UdtaKeywordType, -1},                                  // USER_DATA_KEYWORDS
        {UdtaAlbumArtistType, -1},
        {UdtaCompilationType, -1},
        {UdtaAndroidVersionType, -1},
        {UdtaCaptureFpsType, -1},
        {-1},
        {-1, -1},
};

#if 1
static uint32 g_userDataFormatTable[] = {
        -1,                          /* 0 - reserved */
        USER_DATA_FORMAT_UTF8,       /* 1 - UTF-8 */
        -1,                          /* 2 - UTF-16BE */
        -1,                          /* 3 - S/JIS */
        -1,                          /* 4 - UTF-8 sort */
        -1,                          /* 5 - UTF-16BE sort */
        -1,                          /* 6 - reserved */
        -1,                          /* 7 - reserved */
        -1,                          /* 8 - reserved */
        -1,                          /* 9 - reserved */
        -1,                          /* 10 - reserved */
        -1,                          /* 11 - reserved */
        -1,                          /* 12 - reserved */
        USER_DATA_FORMAT_JPEG,       /* 13 - JPEG */
        USER_DATA_FORMAT_PNG,        /* 14 - PNG */
        -1,                          /* 15 - reserved */
        -1,                          /* 16 - reserved */
        -1,                          /* 17 - reserved */
        -1,                          /* 18 - reserved */
        -1,                          /* 19 - reserved */
        -1,                          /* 20 - reserved */
        USER_DATA_FORMAT_INT_BE,     /* 21 - BE Signed Integer */
        USER_DATA_FORMAT_UINT_BE,    /* 22 - BE Unsigned Integer */
        USER_DATA_FORMAT_FLOAT32_BE, /* 23 - BE Float32 */
        USER_DATA_FORMAT_FLOAT64_BE, /* 24 - BE Float64 */
        -1,                          /* 25 - reserved */
        -1,                          /* 26 - reserved */
        USER_DATA_FORMAT_BMP,        /* 27 - BMP */
        -1,                          /* 28 - QuickTime Metadata atom */
        -1,
        -1,
};
#else
static uint32 g_userDataFormatTable[] = {
        WELL_KNOWN_DATA_TYPE_UTF_8,      /* USER_DATA_FORMAT_UTF8 */
        WELL_KNOWN_DATA_TYPE_INT_BE,     /* USER_DATA_FORMAT_INT_BE */
        WELL_KNOWN_DATA_TYPE_UINT_BE,    /* USER_DATA_FORMAT_UINT_BE */
        WELL_KNOWN_DATA_TYPE_FLOAT32_BE, /* USER_DATA_FORMAT_FLOAT32_BE */
        WELL_KNOWN_DATA_TYPE_FLOAT64_BE, /* USER_DATA_FORMAT_FLOAT64_BE */
        WELL_KNOWN_DATA_TYPE_JPEG,       /* USER_DATA_FORMAT_JPEG */
        WELL_KNOWN_DATA_TYPE_PNG,        /* USER_DATA_FORMAT_PNG */
        WELL_KNOWN_DATA_TYPE_BMP,        /* USER_DATA_FORMAT_BMP */
        -1,                              /* USER_DATA_FORMAT_GIF undefined in MP4 */
};
#endif

/***************************************************************************************
 *                  static functions
 ***************************************************************************************/
static int32 parseFileHeader(FslParserHandle parserHandle, uint32 flag);
static int32 getTrackNextSample(FslParserHandle parserHandle, uint32 trackNum,
                                // uint8 * sampleData,
                                uint32* dataSize, uint64* usStartTime, uint64* usEndTime,
                                uint32* sampleFlags);

static int32 findTrackNextSyncSample(AVStreamPtr stream, uint32 direction,
                                     uint32* syncSampleNumber);

static int32 resetTrackReadingStatus(FslParserHandle parserHandle, uint32 trackNum);

static MP4Err getFileUserDataList(FslParserHandle parserHandle);
static MP4Err getStreamUserDataList(FslParserHandle parserHandle);
static MP4Err getUserDataList(FslParserHandle parserHandle);
static void freeUserDataList(FslParserHandle parserHandle);
static void freeMetaDataList(MP4LinkedList* list);
static MP4Err getUserDataField(MP4UserDataAtomPtr udta, MP4Handle handle, u32 fieldType,
                               u16** unicodeString, u32* stringLength);

static int32 getDefaultReadMode(FslParserHandle parserHandle);
static int32 isMovieSeekable(FslParserHandle parserHandle);

static int32 getNextTrackToRead(FslParserHandle parserHandle);
static int32 getNextTrackToReadTrickMode(FslParserHandle parserHandle, uint32 direction);

static int32 seekTrack(FslParserHandle parserHandle, uint32 trackNum, uint32 flag, uint64* usTime);

static int32 setupTrackReader(sMP4ParserObjectPtr self, AVStreamPtr stream, u32 CounterFrame);

static void checkInterleavingDepth(sMP4ParserObjectPtr self);

static int32 comparePtsEntry(const void* _a, const void* _b);

static int32 buildSamplePtsTable(MP4SampleTableAtomPtr stbl, sSamplePtsEntry** samplePtsTable,
                                 uint32 sampleCount);

static void findSampleAtTime(sSamplePtsEntry* ptsTable, uint32 size, uint64 req_time,
                             uint32* sample_index, uint32 flag);

static MP4Err getScanTypeFromSample(sMP4ParserObjectPtr self);

/***************************************************************************************
 *                 DLL entry point
 ***************************************************************************************/
EXTERN int32 FslParserQueryInterface(uint32 id, void** func) {
    int32 err = PARSER_SUCCESS;

    // MP4MSG("enter %s,%d.\n",__FUNCTION__,__LINE__);
    if (!func)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    // MP4MSG("query API, id %ld\n", id);
    *func = NULL;

    switch (id) {
        /* creation & deletion */
        case PARSER_API_GET_VERSION_INFO:
            *func = MP4ParserVersionInfo;
            break;

        case PARSER_API_CREATE_PARSER:
            *func = MP4CreateParser;
            break;

        case PARSER_API_DELETE_PARSER:
            *func = MP4DeleteParser;
            break;
        case PARSER_API_CREATE_PARSER2:
            *func = MP4CreateParser2;
            break;

        /* movie properties */
        case PARSER_API_IS_MOVIE_SEEKABLE:
            *func = MP4IsSeekable;
            break;

        case PARSER_API_GET_MOVIE_DURATION:
            *func = MP4GetTheMovieDuration;
            break;

        case PARSER_API_GET_USER_DATA:
            *func = MP4GetUserData;
            break;

        case PARSER_API_GET_META_DATA:
            *func = MP4GetMetaData;
            break;

        case PARSER_API_GET_NUM_TRACKS:
            *func = MP4GetNumTracks;
            break;

        /* generic track properties */
        case PARSER_API_GET_TRACK_TYPE:
            *func = MP4GetTrackType;
            break;

        case PARSER_API_GET_DECODER_SPECIFIC_INFO:
            *func = MP4GetDecoderSpecificInfo;
            break;

        case PARSER_API_GET_LANGUAGE:
            *func = MP4GetLanguage;
            break;

        case PARSER_API_GET_TRACK_DURATION:
            *func = MP4GetTheTrackDuration;
            break;

        case PARSER_API_GET_BITRATE:
            *func = MP4GetBitRate;
            break;

        case PARSER_API_GET_TRACK_EXT_TAG:
            *func = MP4GetTrackExtTag;
            break;

        /* video properties */
        case PARSER_API_GET_VIDEO_FRAME_WIDTH:
            *func = MP4GetVideoFrameWidth;
            break;

        case PARSER_API_GET_VIDEO_FRAME_HEIGHT:
            *func = MP4GetVideoFrameHeight;
            break;

        case PARSER_API_GET_VIDEO_FRAME_RATE:
            *func = MP4GetVideoFrameRate;
            break;

        case PARSER_API_GET_VIDEO_FRAME_ROTATION:
            *func = MP4GetVideoFrameRotation;
            break;

        case PARSER_API_GET_VIDEO_COLOR_INFO:
            *func = MP4GetVideoColorInfo;
            break;

        case PARSER_API_GET_VIDEO_DISPLAY_WIDTH:
            *func = MP4GetVideoDisplayWidth;
            break;

        case PARSER_API_GET_VIDEO_DISPLAY_HEIGHT:
            *func = MP4GetVideoDisplayHeight;
            break;
        case PARSER_API_GET_VIDEO_FRAME_COUNT:
            *func = MP4GetVideoFrameCount;
            break;

        case PARSER_API_GET_VIDEO_FRAME_THUMBNAIL_TIME:
            *func = MP4GetVideoThumbnailTime;
            break;
        case PARSER_API_GET_VIDEO_SCAN_TYPE:
            *func = MP4GetVideoScanType;
            break;

        /* audio properties */
        case PARSER_API_GET_AUDIO_NUM_CHANNELS:
            *func = MP4GetAudioNumChannels;
            break;

        case PARSER_API_GET_AUDIO_SAMPLE_RATE:
            *func = MP4GetAudioSampleRate;
            break;

        case PARSER_API_GET_AUDIO_BITS_PER_SAMPLE:
            *func = MP4GetAudioBitsPerSample;
            break;
        case PARSER_API_GET_AUDIO_BLOCK_ALIGN:
            *func = MP4GetAudioBlockAlign;
            break;
        case PARSER_API_GET_AUDIO_MPEGH_INFO:
            *func = MP4GetAudioMpeghInfo;
            break;

        /* text/subtitle properties */
        case PARSER_API_GET_TEXT_TRACK_WIDTH:
            *func = MP4GetTextTrackWidth;
            break;

        case PARSER_API_GET_TEXT_TRACK_HEIGHT:
            *func = MP4GetTextTrackHeight;
            break;

        case PARSER_API_GET_TEXT_TRACK_MIME:
            *func = MP4GetTextTrackMime;
            break;

        /* sample reading, seek & trick mode */
        case PARSER_API_GET_READ_MODE:
            *func = MP4GetReadMode;
            break;

        case PARSER_API_SET_READ_MODE:
            *func = MP4SetReadMode;
            break;

        case PARSER_API_ENABLE_TRACK:
            *func = MP4EnableTrack;
            break;

        case PARSER_API_GET_NEXT_SAMPLE:
            *func = MP4GetNextSample;
            break;

        case PARSER_API_GET_NEXT_SYNC_SAMPLE:
            *func = MP4GetNextSyncSample;
            break;

        case PARSER_API_GET_FILE_NEXT_SAMPLE:
            *func = MP4GetFileNextSample;
            break;

        case PARSER_API_GET_FILE_NEXT_SYNC_SAMPLE:
            *func = MP4GetFileNextSyncSample;
            break;

        case PARSER_API_GET_SAMPLE_CRYPTO_INFO:
            *func = MP4GetSampleCryptoInfo;
            break;

        case PARSER_API_GET_SAMPLE_INFO:
            *func = MP4GetSampleInfo;
            break;

        case PARSER_API_SEEK:
            *func = MP4Seek;
            break;
        case PARSER_API_GET_AUDIO_PRESENTATION_NUM:
            *func = (void*)MP4GetAudioPresentationNum;
            break;
        case PARSER_API_GET_AUDIO_PRESENTATION_INFO:
            *func = (void*)MP4GetAudioPresentationInfo;
            break;
        case PARSER_API_FLUSH_TRACK:
            *func = (void*)MP4FlushTrack;
            break;
        case PARSER_API_GET_IMAGE_INFO:
            *func = MP4GetImageInfo;
        default:
            break; /* no support for other API */
    }

bail:
    // MP4MSG("leave %s,%d.\n",__FUNCTION__,__LINE__);
    return err;
}

/***************************************************************************************
 *                  API functions
 ***************************************************************************************/

EXTERN const char* MP4ParserVersionInfo() {
    return (const char*)CODEC_VERSION_STR;
}

/**
 * Function to create the MP4 core parser.
 * It will parse the MP4 file header and probe whether the movie can be handled by this MP4 parser.
 *
 * @param isLive [in] True for a live source, and FALSE for a local file.
 *
 * @param streamOps [in]  File I/O functions.
 *                        It implements functions to open, close, tell, read and seek a file.
 *
 * @param memOps [in]   Memory operation callback table.
 *                      It implements the functions to malloc, calloc, realloc and free memory.
 *
 * @param outputBufferOps [in] Callback to get/release a buffer for output.
 *
 * @param context [in]  Application context for the callback functions. MP4 parser never modify it.
 *
 * @param parserHandle [out] Handle of the MP4 core parser if succeeds. NULL for failure.
 *
 * @return
 */
EXTERN int32 MP4CreateParser(bool isLive, FslFileStream* streamOps, ParserMemoryOps* memOps,
                             ParserOutputBufferOps* outputBufferOps, void* context,
                             FslParserHandle* parserHandle) {
    uint32 flags = 0;
    if (isLive) {
        flags |= FILE_FLAG_NON_SEEKABLE;
        flags |= FILE_FLAG_READ_IN_SEQUENCE;
    }
    return MP4CreateParser2(flags, streamOps, memOps, outputBufferOps, context, parserHandle);
}

EXTERN int32 MP4CreateParser2(uint32 flags, FslFileStream* streamOps, ParserMemoryOps* memOps,
                              ParserOutputBufferOps* outputBufferOps, void* context,
                              FslParserHandle* parserHandle) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = NULL;
    MP4MSG("%s flags 0x%x\n", __FUNCTION__, flags);

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

    if ((NULL == streamOps) || (NULL == memOps) || (NULL == parserHandle)) {
        return PARSER_ERR_INVALID_PARAMETER;
    }

    *parserHandle = NULL;

    g_streamOps.Open = streamOps->Open;
    g_streamOps.Read = streamOps->Read;
    g_streamOps.Seek = streamOps->Seek;
    g_streamOps.Tell = streamOps->Tell;
    g_streamOps.Size = streamOps->Size;
    g_streamOps.Close = streamOps->Close;
    g_streamOps.CheckAvailableBytes = streamOps->CheckAvailableBytes;
    if (streamOps->GetFlag) {
        g_streamOps.GetFlag = streamOps->GetFlag;
    } else {
        g_streamOps.GetFlag = NULL;
    }

    g_memOps.Calloc = memOps->Calloc;
    g_memOps.Malloc = memOps->Malloc;
    g_memOps.Free = memOps->Free;
    g_memOps.ReAlloc = memOps->ReAlloc;

    g_outputBufferOps.RequestBuffer = outputBufferOps->RequestBuffer;
    g_outputBufferOps.ReleaseBuffer = outputBufferOps->ReleaseBuffer;

#ifdef MP4_MEM_DEBUG_SELF
    mm_mm_init();
#endif

    self = MP4LocalCalloc(1, sizeof(sMP4ParserObject));

    TESTMALLOC(self)
    memset(self, 0, sizeof(sMP4ParserObject));
    self->appContext = context;
    self->isLive = FALSE;
    if ((flags & FILE_FLAG_NON_SEEKABLE) && (flags & FILE_FLAG_READ_IN_SEQUENCE)) {
        self->isLive = TRUE;
    }
    MP4MSG("isLive %d\n", self->isLive);

    /* try to open the source stream */
    self->fileName = NULL;
    self->flags = flags;
    err = MP4CreateFileMappingObject((char*)self->fileName, &self->fileMappingObject,
                                     self->appContext);
    if (err) {
        MP4MSG("error: can not open source file %p\n", self->fileName);
        goto bail;
    }

    /* NOTE: when file size is larger than 4GB, large chunk offset table, not supported yet,
    must be used to locate the chunks ! */
    self->fileSize = self->fileMappingObject->size;
    if (MIN_MP4_FILE_SIZE >= self->fileSize) {
        MP4MSG("error: file size %lld is illegal or exceeds parser's capacity!\n", self->fileSize);
        BAILWITHERROR(PARSER_ILLEAGAL_FILE_SIZE)
    }
    MP4MSG("file content size %lld, flag %x\n", self->fileSize, flags);

    self->flags |= MP4LocalGetFlag((FslParserHandle)self, self->appContext);
    if (self->isLive) {
        self->flags |= FILE_FLAG_NON_SEEKABLE;
        self->flags |= FILE_FLAG_READ_IN_SEQUENCE;
    }
    self->fragmented = FALSE;
    err = parseFileHeader((FslParserHandle)self, self->flags);
    if (PARSER_SUCCESS != err) {
        MP4MSG("fail to parse file header\n");
        goto bail;
    }

    if (!self->isImageFile && !self->isLive)
        err = getScanTypeFromSample(self);

bail:
    if (PARSER_SUCCESS != err) {
        if (self) {
            MP4DeleteParser((FslParserHandle)self);
        }

#ifdef MP4_MEM_DEBUG_SELF
        mm_mm_exit();
#endif
    } else {
        *parserHandle = (FslParserHandle*)self;
        MP4MSG("MP4 parser created successfully\n");
    }

#ifdef FSL_MP4_PARSER_DBG_TIME
#if defined(__WINCE) || defined(WIN32)
    end_time = timeGetTime();
    MP4DbgLog("\nMP4 loading time : %ld ms\n\n", end_time - start_time);
#else
    gettimeofday(&end_time, &time_zone);
    if (end_time.tv_usec >= start_time.tv_usec) {
        MP4DbgLog("\nMP4 loading time: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec,
                  end_time.tv_usec - start_time.tv_usec);
    } else {
        MP4DbgLog("\nMP4 loading time: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec - 1,
                  end_time.tv_usec + 1000000000 - start_time.tv_usec);
    }

    MP4DbgLog("start %ld sec, %ld us\n\n", start_time.tv_sec, start_time.tv_usec);
#endif
#endif

    return err;
}

/**
 * Function to delete the MP4 core parser.
 *
 * @param parserHandle Handle of the MP4 core parser.
 * @return
 */
EXTERN int32 MP4DeleteParser(FslParserHandle parserHandle) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;

    uint32 i;
    AVStreamPtr stream;

    MP4MSG("MP4DeleteParser\n");

    if (NULL == self) {
        MP4MSG("error: invalid parser handle\n");
        return PARSER_ERR_INVALID_PARAMETER;
    }

    if (self->fragmented) {
        MP4DeleteFragmentedReader(self->fragmented_reader);
        MP4MSG("MP4DeleteFragmentedReader END\n");
    }

    if (self->fileMappingObject) {
        // MP4MSG("fileMappingObject %p\n", moov->fileMappingObject);
        self->fileMappingObject->close(self->fileMappingObject);
        self->fileMappingObject->destroy(self->fileMappingObject);
        self->fileMappingObject = NULL;
    }

    /* TODO: free track reader */
    for (i = 0; i < MP4_PARSER_MAX_STREAM; i++) {
        /* engr59629:
        track record shall no overlap on the track atom. Allocate memory for track record - Amanda*/
        stream = self->streams[i];
        if (stream) {
            if (stream->pH264Parser) {
                DeleteH264Parser(stream->pH264Parser);
                stream->pH264Parser = NULL;
            }

            if (stream->decoderSpecificInfoH) {
                MP4DisposeHandle(stream->decoderSpecificInfoH);
                stream->decoderSpecificInfoH = NULL;
            }

            if (stream->sampleH) {
                /* NEVER call MP4DisposeHandle(stream->sampleH).
                Otherwise "bad free" because this function will try to free the data buffer but
                internal data buffer of the handle is from outside.
                Only free the handle itself!*/
                MP4LocalFree(stream->sampleH);
                stream->sampleH = NULL;
            }

            if (stream->tx3gHandle) {
                MP4DisposeHandle(stream->tx3gHandle);
                stream->tx3gHandle = NULL;
            }

            if (stream->reader) {
                err = MP4DisposeTrackReader(stream->reader);
                // MP4MSG("trk %d, delete reader %p, err %d\n", i, stream->reader, err);
                stream->reader = NULL;
            }

            if (stream->userDataList) {
                freeMetaDataList(&(stream->userDataList));
            }

            if (stream->extTagList)
                MP4DeleteTagList(stream->extTagList);

            if (MEDIA_AUDIO == stream->mediaType) {
                if (stream->decoderType == AUDIO_AC4 && stream->audioInfo.audioPresentations) {
                    MP4LocalFree(stream->audioInfo.audioPresentations);
                    stream->audioInfo.audioPresentations = NULL;
                }
            }

            if (stream->samplePtsTable) {
                MP4LocalFree(stream->samplePtsTable);
                stream->samplePtsTable = NULL;
            }

            MP4LocalFree(stream);
            self->streams[i] = NULL;
        }
    }

    /* Amanda: not free data handler here, they are created by 'minf' atom, so let 'minf' free them
     */
    if (self->movie) {
        MP4DisposeMovie(self->movie);
        self->movie = NULL;
    }

    freeUserDataList(self);
    freeMetaDataList(&(self->userDataList));

    MP4LocalFree(self);
    MP4MSG("parser deleted\n");

#ifdef MP4_MEM_DEBUG_SELF
    mm_mm_exit();
#endif

    return err;
}

/**
 * Function to tell whether the movie is seekable. A seekable MP4 movie must have the index table.
 * If the file's index table is loaded from file or imported successfully, it's seekable.
 * Seeking and trick mode can be performed on a seekable file.
 * If the index table is corrupted, the file is NOT seekable. This function will fail and return
 * value can tell the error type.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param seekable [out] true for seekable and false for non-seekable
 * @return
 */
EXTERN int32 MP4IsSeekable(FslParserHandle parserHandle, bool* seekable) {
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    *seekable = self->seekable;

    return PARSER_SUCCESS;
}

/**
 * function to tell how many tracks in the movie.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param numTracks [out] Number of tracks.
 * @return
 */
EXTERN int32 MP4GetNumTracks(FslParserHandle parserHandle, uint32* numTracks) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;

    if (NULL == self) {
        MP4MSG("error: invalid parser handle\n");
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)
    }

    *numTracks = self->numTracks;

bail:
    return err;
}

/**
 * Function to tell the user data information (title, artist, genre etc) of the movie. User data
 * API. The information is usually a null-terminated ASCII string.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param userDataId [in] User data ID. Type of the user data.
 * @param buffer [out] Buffer containing the information. The core parser manages this buffer and
 * the user shall NOT free it. If no such info is availabe, this value will be set to NULL.
 * @param size [out] Length of the information in bytes.
 *                               If no such info is available, this value will be set to 0.
 * @return
 */
EXTERN int32 MP4GetUserData(FslParserHandle parserHandle, uint32 userDataId, uint16** unicodeString,
                            uint32* stringLength) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    sMP4ParserUdtaListPtr udtaList;
    sUserDataEntryPtr entry = NULL;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    *unicodeString = NULL;
    *stringLength = 0;

    udtaList = &self->udtaList;
    switch (userDataId) {
        case USER_DATA_TITLE:
            entry = &udtaList->FileName;
            break;

        case USER_DATA_GENRE:
            entry = &udtaList->Genre;
            break;

        case USER_DATA_ARTIST:
            entry = &udtaList->ArtistName;
            break;

        case USER_DATA_COMMENTS:
            entry = &udtaList->Comment;
            break;

        case USER_DATA_CREATION_DATE:
            entry = &udtaList->Year;
            break;

        case USER_DATA_COPYRIGHT:
            entry = &udtaList->CopyRight;
            break;

        case USER_DATA_ALBUM:
            entry = &udtaList->AlbumName;
            break;

        case USER_DATA_LANGUAGE:
        case USER_DATA_RATING:

        default:
            // MP4MSG("No such user data available, user data id %d\n",id);
            break; /* not a failure */
    }

    if (entry && entry->unicodeString) {
        MP4MSG("string length %u\n", entry->stringLength);
        *unicodeString = entry->unicodeString;
        *stringLength = entry->stringLength;
    }
bail:
    return err;
}

/**
 * function to tell the meta data information (title, artist, genre etc) of the
 * movie. User data API.
 * this new API is defined to substitute MP4GetUserData().
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param userDataId [in] User data ID. Type of the user data. Value can be from enum
 * FSL_PARSER_USER_DATA_TYPE
 * @param userDataFormat [in/out] format of user data. Value can be from enum
 * FSL_PARSER_USER_DATA_FORMAT
 * @param userData [out] Buffer containing the information. The core parser manages this buffer and
 * the user shall NOT free it. If no such info is availabe, this value will be set to NULL.
 * @param userDataLength [out] Length of the information in bytes. The informaiton is usually a
 * null-terminated ASCII string. If no such info is available, this value will be set to 0.
 * @return
 */
EXTERN int32 MP4GetMetaData(FslParserHandle parserHandle, UserDataID userDataId,
                            UserDataFormat* userDataFormat, uint8** userData,
                            uint32* userDataLength) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    MP4LinkedList userDataList;
    uint32* userDataTypes = NULL;
    int32 recordCnt = 0, recordIdx = 0;
    uint32 i = 0;
    uint32 recordType = 0;

    if ((NULL == self) || ((uint32)userDataId >= USER_DATA_MAX))
        return PARSER_ERR_INVALID_PARAMETER;

    if ((userDataFormat == NULL) || ((uint32)(*userDataFormat) >= USER_DATA_FORMAT_MAX))
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    if ((userData == NULL) || (userDataLength == NULL))
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    *userData = NULL;
    *userDataLength = 0;

    /* check if our parser support such user data type */
    userDataTypes = g_userDataTypeTable[userDataId];
    if ((uint32)-1 == userDataTypes[0])
        goto bail;

    userDataList = self->userDataList;
    err = MP4GetListEntryCount(userDataList, (u32*)&recordCnt);
    if (err)
        goto bail;

    // encoder delay and padding values are stored with utf8 formats in the atom
    if ((USER_DATA_AUD_ENC_DELAY == userDataId) || (USER_DATA_AUD_ENC_PADDING == userDataId)) {
        *userDataFormat = USER_DATA_FORMAT_UTF8;
    }
    if (USER_DATA_RATING == userDataId) {
        *userDataFormat = USER_DATA_FORMAT_FLOAT32_BE;
    }

    for (recordIdx = 0; recordIdx < recordCnt; recordIdx++) {
        UserDataRecordPtr record;
        err = MP4GetListEntry(userDataList, recordIdx, (char**)&record);
        if (err)
            goto bail;

        if (record == NULL)
            continue;

        i = 0;
        while (userDataTypes[i] != (uint32)-1) {
            if (record->type == userDataTypes[i]) {
                MP4LinkedList itemList;
                UserDataItemPtr item;
                int32 itemCnt = 0, itemIdx = 0;

                itemList = record->list;
                err = MP4GetListEntryCount(itemList, (u32*)&itemCnt);
                if (err)
                    goto bail;

                for (itemIdx = 0; itemIdx < itemCnt; itemIdx++) {
                    err = MP4GetListEntry(itemList, itemIdx, (char**)&item);
                    if (err)
                        goto bail;

                    if (item->format >= sizeof(g_userDataFormatTable)) {
                        /* illegal format index */
                        continue;
                    }

                    if (g_userDataFormatTable[item->format] == *userDataFormat) {
                        *userData = (uint8*)item->data;
                        *userDataLength = item->size;
                        recordType = record->type;
                        break;
                    }
                }

                /* if no matched format, output the default one */
                if ((NULL == *userData) || (0 == *userDataLength)) {
                    for (itemIdx = 0; itemIdx < itemCnt; itemIdx++) {
                        err = MP4GetListEntry(itemList, itemIdx, (char**)&item);
                        if (err)
                            goto bail;

                        if (item->format >= sizeof(g_userDataFormatTable)) {
                            /* illegal format index */
                            continue;
                        }

                        *userData = (uint8*)item->data;
                        *userDataLength = item->size;
                        *userDataFormat = g_userDataFormatTable[item->format];
                    }
                }

                break;
            }

            i++;
        }
    }

#define OFFSET_TRACKNUMBER 3
#define OFFSET_TOTALTRACKNUMBER 5
    if (recordType == (uint32)UdtaCDTrackNumType && (*userData) && (*userDataLength > (uint32)0) &&
        (USER_DATA_TRACKNUMBER == userDataId)) {
        *userDataFormat = USER_DATA_FORMAT_UTF8;
    } else if ((*userData) && (*userDataLength > (uint32)0) &&
               ((USER_DATA_TRACKNUMBER == userDataId) ||
                (USER_DATA_TOTALTRACKNUMBER == userDataId))) {
        int len;
        uint8* pbyData = *userData;

        uint8 offset = (USER_DATA_TRACKNUMBER == userDataId) ? OFFSET_TRACKNUMBER
                                                             : OFFSET_TOTALTRACKNUMBER;
        uint8* asciBuf = (USER_DATA_TRACKNUMBER == userDataId) ? self->asciTrackNumber
                                                               : self->asciTotalTrackNumber;

        len = sprintf((char*)asciBuf, "%d", pbyData[offset]);
        if (len < 0) {
            len = ASCILEN;
        }

        *userData = asciBuf;
        *userDataLength = len;
        *userDataFormat = USER_DATA_FORMAT_UTF8;
    }
    if ((*userData) && (*userDataLength > (uint32)0) && (*userDataLength <= sizeof(uint16)) &&
        (USER_DATA_GENRE == userDataId)) {
        MP4PrivateMovieRecord* movie;
        movie = (MP4PrivateMovieRecord*)self->movie;
        if (!(movie->inputStream->stream_flags & flag_3gp)) {
            uint8* data = *userData;
            uint16 index = 0;
            if (*userDataLength == sizeof(uint16)) {
                index = data[0] << 8 | data[1];
            } else {
                index = data[0];
            }
            if (index > 0 && index <= ID3_GENRE_MAX) {
                char* asciBuf = genre_table[index - 1];
                int len = strlen(asciBuf);
                *userData = (uint8*)asciBuf;
                *userDataLength = len;
                *userDataFormat = USER_DATA_FORMAT_UTF8;
            }
        }
    }

#define OFFSET_ENCODER_DELAY 4
#define OFFSET_ENCODER_PADDING 8
    //#define INT_LEN 4
    if ((USER_DATA_AUD_ENC_DELAY == userDataId) || (USER_DATA_AUD_ENC_PADDING == userDataId)) {
        if ((*userData) && (*userDataLength > (uint32)0)) {
            uint8* pbyData = *userData;
            int32 delay = 0;
            int32 padding = 0;
            if (2 == sscanf((char*)pbyData, " %*x %x %x %*x", &delay, &padding)) {
                if (delay > 0 || padding > 0) {
                    self->audioEncoderDelay = delay;
                    self->audioEncoderPadding = padding;
                }
            }
        }
        if (self->audioEncoderDelay > 0 || self->audioEncoderPadding > 0) {
            *userData = (USER_DATA_AUD_ENC_DELAY == userDataId)
                                ? (uint8*)&self->audioEncoderDelay
                                : (uint8*)&self->audioEncoderPadding;
            *userDataLength = sizeof(int32);
            *userDataFormat = USER_DATA_FORMAT_INT_LE;
        } else {
            *userData = NULL;
            *userDataLength = 0;
            *userDataFormat = USER_DATA_FORMAT_INT_LE;
        }
    }

    if ((*userData) && (*userDataLength > (uint32)1) && (USER_DATA_LOCATION == userDataId)) {
        // add this according to android MPEG4Extractor to pass cts test
        //  Add a trailing slash if there wasn't one.
        if ((*userData)[*userDataLength - 1] != '/') {
            (*userData)[*userDataLength - 1] = '/';
        }
    }

    if ((*userData) && (*userDataLength > (uint32)1) && (USER_DATA_RATING == userDataId)) {
        int len = 0;
        uint8* pbyData = *userData;
        uint8* asciBuf = self->asciUserRating;

#ifdef __LP64__
        len = sprintf((char*)asciBuf, "%llu", (long long)pbyData);
#else
        len = sprintf((char*)asciBuf, "%u", (uint32)pbyData);
#endif
        if (len < 0) {
            len = ASCILEN;
        }
        *userData = asciBuf;
        *userDataLength = len;
        *userDataFormat = USER_DATA_FORMAT_UTF8;
    }

    if ((USER_DATA_COMPILATION == userDataId) && (*userData) && (*userDataLength == (uint32)1)) {
        *userDataLength = sizeof(int32);
        *userDataFormat = USER_DATA_FORMAT_INT_LE;
    }

bail:
    if (USER_DATA_PSSH == userDataId && self->psshLen > 0) {
        *userData = self->pssh;
        *userDataLength = self->psshLen;
        *userDataFormat = USER_DATA_FORMAT_UTF8;
    }
    if (USER_DATA_MP4_CREATION_TIME == userDataId) {
        *userData = (uint8*)&self->creationTime;
        *userDataLength = sizeof(uint64);
        *userDataFormat = USER_DATA_FORMAT_UINT_LE;
    }
    return err;
}

/**
 * Function to tell the movie duration.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param usDuration [out] Duration in us.
 * @return
 */
EXTERN int32 MP4GetTheMovieDuration(FslParserHandle parserHandle, uint64* usDuration) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    MP4PrivateMovieRecord* movie;
    MP4MovieAtomPtr moov;
    MP4MovieHeaderAtomPtr mvhd;
    MP4MovieExtendsAtomPtr mvex = NULL;
    MP4MovieExtendsHeaderAtomPtr mehd = NULL;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    if (self->isImageFile) {
        *usDuration = 0;
        goto bail;
    }

    movie = (MP4PrivateMovieRecord*)self->movie;
    moov = (MP4MovieAtomPtr)movie->moovAtomPtr;
    mvhd = (MP4MovieHeaderAtomPtr)moov->mvhd;
    mvex = (MP4MovieExtendsAtomPtr)moov->mvex;

    if (mvex)
        mehd = (MP4MovieExtendsHeaderAtomPtr)mvex->mehd;
    /* core parser select the longest track's duration as the movie duration */
    *usDuration = (uint64)((((float)mvhd->duration) / mvhd->timeScale) * 1000 * 1000);
    if (self->fragmented) {
        if (mehd) {
            *usDuration =
                    (uint64)((((float)mehd->fragment_duration) / mvhd->timeScale) * 1000 * 1000);
        } else {
            u64 duration = 0;
            err = getFragmentedDuration(self->fragmented_reader, &duration);
            if (err == PARSER_SUCCESS)
                *usDuration = duration;
        }
    }

    MP4MSG("%s duration=%lld ms\n", __FUNCTION__, *usDuration / 1000);
bail:
    return err;
}

/**
 * Function to tell a track's duration.
 * The tracks may have different durations.
 * And the movie's duration is usually the video track's duration (maybe not the longest one).
 *
 * If a track's duration is 0, this track is empty! Application can read nothing from an empty
 * track.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based.
 * @param usDuration [out] Duration in us.
 * @return
 */
EXTERN int32 MP4GetTheTrackDuration(FslParserHandle parserHandle, uint32 trackNum,
                                    uint64* usDuration) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)
    if (self->fragmented) {
        err = MP4GetTheMovieDuration(parserHandle, usDuration);
        goto bail;
    }

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (stream->isEmpty)
        *usDuration = 0; /* prevent application from reading this track */
    else
        *usDuration = stream->usDuration;

bail:
    return err;
}

/**
 * Function to tell the type of a track.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based.
 *
 * @param mediaType [out] Media type of the track. (video, audio, subtitle...)
 *                        "MEDIA_TYPE_UNKNOWN" means the media type is unknown.
 *
 * @param decoderType [out] Decoder type of the track if available. (eg. MPEG-4, H264, AAC, MP3, AMR
 * ...) "UNKNOWN_CODEC_TYPE" means the decoder type is unknown.
 *
 * @param decoderSubtype [out] Decoder Subtype type of the track if available. (eg. AMR-NB, AMR-WB
 * ...) "UNKNOWN_CODEC_SUBTYPE" means the decoder subtype is unknown.
 * @return
 */
EXTERN int32 MP4GetTrackType(FslParserHandle parserHandle, uint32 trackNum, uint32* mediaType,
                             uint32* decoderType, uint32* decoderSubtype) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (trackNum >= self->numTracks)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];

    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    *mediaType = (uint32)stream->mediaType;
    *decoderType = stream->decoderType;
    *decoderSubtype = stream->decoderSubtype;

    // if(MEDIA_VIDEO == stream->mediaType)
    //*mediaType = MEDIA_TYPE_UNKNOWN;

bail:
    return err;
}

/**
 * Function to tell the decoder specific information of a track.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based.
 * @param data [out] Buffer holding the decoder specific information. The user shall never free this
 * buffer.
 * @param size [out] Size of the codec specific information, in bytes.
 * @return
 */
EXTERN int32 MP4GetDecoderSpecificInfo(FslParserHandle parserHandle, uint32 trackNum, uint8** data,
                                       uint32* size) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self || NULL == size)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    *size = 0;
    *data = NULL;

    err = MP4GetHandleSize(stream->decoderSpecificInfoH, size);
    if (err)
        goto bail;

    if (*size > 0)
        *data = (uint8*)(*stream->decoderSpecificInfoH);

    /* Some media types USUALLY have decoder specific info, but not a MUST!
    ENGR120706: some MPEG-4 may have decoder specific info with frame data, not in the file header.
  */
    if (((MEDIA_AUDIO == stream->mediaType) &&
         ((AUDIO_AAC == stream->decoderType) || (AUDIO_MPEG2_AAC == stream->decoderType))) ||
        ((MEDIA_VIDEO == stream->mediaType) &&
         ((VIDEO_MPEG4 == stream->decoderType) || (VIDEO_H264 == stream->decoderType)))) {
        if ((0 == *size) || (NULL == *data)) {
            MP4MSG("Warning!trk %d, invalid decoder specific info %p, size %d\n", trackNum, *data,
                   *size);
            *size = 0;
            *data = NULL;
            BAILWITHERROR(PARSER_SUCCESS)
        }
    }

    if ((PARSER_SUCCESS == err) && (stream->mediaType == MEDIA_VIDEO) &&
        (stream->decoderType == VIDEO_H264) && stream->pH264Parser) {
        uint32 is_sync_frame = 0;

        (void)ParseH264Field(stream->pH264Parser, *data, *size, &is_sync_frame);
    }

bail:
    return err;
}

/**
 * Optional API. Function to tell the maximum sample size of a track.
 * MP4 parser read A/V tracks sample by sample. The max sample size can help the user to prepare a
 * big enough buffer. Warning: [1]The "max sample size" is not available if the index table is
 * invalid. And for MP4 file, if the index table is invalid, playback will be affected. [2]Core
 * parser need to scan the whole index table to get the max sample size. So this API is
 *    time-consuming for long movies that has big index tables. For application that requests a
 * quick movie loading, it's better not call this API. Anyway, the parser can output a sample piece
 * by piece if output buffer is not big enough.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based.
 * @param size [out] Max sample size of the track, in bytes.
 *
 * @return:
 * PARSER_SUCCESS
 * PARSER_ERR_INVALID_MEDIA  Index table is invalid so max sample size is not available.
 */
EXTERN int32 MP4GetMaxSampleSize(FslParserHandle parserHandle, uint32 trackNum, uint32* size) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;
    MP4MediaAtomPtr media;
    MP4MediaInformationAtomPtr minf;
    MP4SampleTableAtomPtr stbl;
    MP4SampleSizeAtomPtr stsz;
    MP4CompactSampleSizeAtomPtr stz2;
    u32 maxSampleSize;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if ((NULL == stream) || (NULL == stream->mdia))
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (stream->isEmpty) {
        *size = 0;
        return err;
    }

    media = (MP4MediaAtomPtr)stream->mdia;
    minf = (MP4MediaInformationAtomPtr)media->information;
    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    stsz = (MP4SampleSizeAtomPtr)stbl->SampleSize;
    stz2 = (MP4CompactSampleSizeAtomPtr)stbl->CompactSampleSize;

    if (stsz != NULL && !stsz->scanFinished) {
        MP4MSG("trk %d, finish scan stsz ...\n", trackNum);
        stsz->finishScan((MP4AtomPtr)stsz);
    } else if (stz2 != NULL && !stz2->scanFinished) {
        MP4MSG("trk %d, finish scan stz2 ...\n", trackNum);
        stz2->finishScan((MP4AtomPtr)stz2);
    }

    err = MP4GetMediaTotalBytes(stream->mdia, &stream->totalBytes);
    if (err)
        goto bail;
    // MP4DbgLog("\t total bytes: %lld\n", stream->totalBytes);

    maxSampleSize = stsz != NULL ? stsz->maxSampleSize : (stz2 != NULL ? stz2->maxSampleSize : 0);

    if (MP4_MAX_SAMPLE_SIZE > maxSampleSize)
        *size = maxSampleSize;
    else {
        /* TODO: let parserHeader() fails and clearup all resources */
        MP4MSG("Invalid max sample size %d\n", maxSampleSize);
        *size = 0;
        // err = MP4_ERR_WRONG_SAMPLE_SIZE;
    }

    // fix ENGR00174607
    if (stream->nalLengthFieldSize) {
#ifdef MERGE_FIELDS
        *size = *size * 2 + NALLENFIELD_INCERASE;
#else
        *size += NALLENFIELD_INCERASE;
#endif
    }

bail:
    return err;
}

/**
 * Function to tell the language of a track used.
 * This is helpful to select a video/audio/subtitle track or menu pages.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based.
 * @param threeCharCode [out] Three character language code.
 *                  See ISO 639-2/T for the set of three character codes.Eg. 'eng' for English.
 *                  Four special cases:
 *                  mis- "uncoded languages"
 *                  mul- "multiple languages"
 *                  und- "undetermined language"
 *                  zxx- "no linguistic content"
 * @return
 */
EXTERN int32 MP4GetLanguage(FslParserHandle parserHandle, uint32 trackNum, uint8* threeCharCode) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    if (self->numTracks <= trackNum)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (NULL == stream->mdia)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    err = MP4GetMediaLanguage(stream->mdia, (char*)threeCharCode);

bail:
    return err;
}

/**
 * Function to tell the average bitrate of a track.
 * If the average bitrate is not available in the file header, 0 is given.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based.
 * @param bitrate [out] Average bitrate, in bits per second.
 * @return
 */
EXTERN int32 MP4GetBitRate(FslParserHandle parserHandle, uint32 trackNum, uint32* bitrate) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    if (self->numTracks <= trackNum)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    // MP4MSG("trk %d, max bitrate %d, avg bit rate %d\n", trackNum, stream->maxBitRate,
    // stream->avgBitRate);
    *bitrate = stream->avgBitRate;

bail:
    return err;
}

/**
 * Function to tell the sample duration in us of a track.
 * If the sample duration is not a constant (eg. audio, subtitle), 0 is given.
 * For a video track, the frame rate can be calculated from this information.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based.
 * @param usDuration [out] Sample duration in us. If sample duration is not a constant, this value
 * is 0.
 * @return
 */
EXTERN int32 MP4GetSampleDuration(FslParserHandle parserHandle, uint32 trackNum,
                                  uint64* usDuration) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_VIDEO == stream->mediaType) {
        *usDuration = (uint64)stream->videoInfo.frameRateDenominator * 1000 * 1000 /
                      stream->videoInfo.frameRateNumerator;
    } else
        *usDuration = 0;

bail:
    return err;
}

/**
 * Function to tell the width in pixels of a video track.
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a video track.
 * @param width [out] Width in pixels.
 * @return
 */
EXTERN int32 MP4GetVideoFrameWidth(FslParserHandle parserHandle, uint32 trackNum, uint32* width) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_VIDEO != stream->mediaType && MEDIA_IMAGE != stream->mediaType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    *width = stream->videoInfo.frameWidth;

bail:
    return err;
}

/**
 * Function to tell the height in pixels of a video track.
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a video track.
 * @param height [out] Height in pixels.
 * @return
 */
EXTERN int32 MP4GetVideoFrameHeight(FslParserHandle parserHandle, uint32 trackNum, uint32* height) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_VIDEO != stream->mediaType && MEDIA_IMAGE != stream->mediaType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    *height = stream->videoInfo.frameHeight;

bail:
    return err;
}

EXTERN int32 MP4GetVideoFrameRate(FslParserHandle parserHandle, uint32 trackNum, uint32* rate,
                                  uint32* scale) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_VIDEO != stream->mediaType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    *rate = stream->videoInfo.frameRateNumerator;
    *scale = stream->videoInfo.frameRateDenominator;

bail:
    return err;
}

EXTERN int32 MP4GetVideoFrameRotation(FslParserHandle parserHandle, uint32 trackNum,
                                      uint32* rotation) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_VIDEO != stream->mediaType && MEDIA_IMAGE != stream->mediaType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    *rotation = stream->videoInfo.frameRotationDegree;

bail:
    return err;
}

EXTERN int32 MP4GetVideoColorInfo(FslParserHandle parserHandle, uint32 trackNum, int32* primaries,
                                  int32* transfer, int32* coeff, int32* fullRange) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream || NULL == primaries || NULL == transfer || NULL == coeff ||
        NULL == fullRange)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_VIDEO != stream->mediaType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    if (!stream->videoInfo.hasColorInfo)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    *primaries = stream->videoInfo.primaries;
    *transfer = stream->videoInfo.transfer;
    *coeff = stream->videoInfo.coeff;
    *fullRange = stream->videoInfo.full_range_flag;
bail:
    return err;
}

/**
 * Function to tell the width in pixels of a video track.
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a video track.
 * @param width [out] Width in pixels.
 * @return
 */
EXTERN int32 MP4GetVideoDisplayWidth(FslParserHandle parserHandle, uint32 trackNum, uint32* width) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_VIDEO != stream->mediaType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    *width = stream->videoInfo.displayWidth;

bail:
    return err;
}

/**
 * Function to tell the width in pixels of a video track.
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a video track.
 * @param width [out] Width in pixels.
 * @return
 */
EXTERN int32 MP4GetVideoDisplayHeight(FslParserHandle parserHandle, uint32 trackNum,
                                      uint32* height) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_VIDEO != stream->mediaType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    *height = stream->videoInfo.displayHeight;

bail:
    return err;
}

/**
 * Function to tell frame count of a video track.
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a video track.
 * @param count [out] Frame count.
 * @return
 */
EXTERN int32 MP4GetVideoFrameCount(FslParserHandle parserHandle, uint32 trackNum, uint32* count) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_VIDEO != stream->mediaType)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    *count = stream->numSamples;

bail:
    return err;
}

EXTERN int32 MP4GetVideoThumbnailTime(FslParserHandle parserHandle, uint32 trackNum,
                                      uint64* outTs) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_VIDEO != stream->mediaType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    if (self->fragmented) {
        uint64 streamDuration;
        err = MP4GetTheTrackDuration(parserHandle, trackNum, &streamDuration);
        *outTs = streamDuration / 4;
    } else
        *outTs = stream->videoInfo.thunmbnailTime;

bail:
    return err;
}

EXTERN int32 MP4GetVideoScanType(FslParserHandle parserHandle, uint32 trackNum, uint32* scanType) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_VIDEO != stream->mediaType || VIDEO_H264 != stream->decoderType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    *scanType = stream->videoInfo.scanType;

bail:
    return err;
}

/**
 * Function to tell how many channels in an audio track.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to an audio track.
 * @param numchannels [out] Number of the channels. 1 mono, 2 stereo, or more for multiple channels.
 * @return
 */
EXTERN int32 MP4GetAudioNumChannels(FslParserHandle parserHandle, uint32 trackNum,
                                    uint32* numChannels) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_AUDIO != stream->mediaType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    *numChannels = stream->audioInfo.numChannels;

bail:
    return err;
}

/**
 * Function to tell the audio sample rate (sampling frequency) of an audio track.
 * Warning:
 * The audio sample rate from the file header may be wrong. And if the audio decoder specific
 * information is available (for AAC), the decoder can double check the audio sample rate from this
 * information.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to an audio track.
 * @param sampleRate [out] Audio integer sample rate (sampling frequency).
 * @return
 */
EXTERN int32 MP4GetAudioSampleRate(FslParserHandle parserHandle, uint32 trackNum,
                                   uint32* sampleRate) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_AUDIO != stream->mediaType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    *sampleRate = stream->audioInfo.sampleRate;

bail:
    return err;
}

/**
 * Function to tell the bits per sample for an audio track.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to an audio track.
 * @param bitsPerSample [out] Bits per PCM sample.
 * @return
 */
EXTERN int32 MP4GetAudioBitsPerSample(FslParserHandle parserHandle, uint32 trackNum,
                                      uint32* bitsPerSample) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_AUDIO != stream->mediaType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    *bitsPerSample = stream->audioInfo.sampleSize;

bail:
    return err;
}

EXTERN int32 MP4GetAudioBlockAlign(FslParserHandle parserHandle, uint32 trackNum,
                                   uint32* blockAlign) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_AUDIO != stream->mediaType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    *blockAlign = stream->audioInfo.blockAlign;

bail:
    return err;
}

EXTERN int32 MP4GetAudioMpeghInfo(FslParserHandle parserHandle, uint32 trackNum,
                                  uint32* profileLevelIndication, uint32* referenceChannelLayout,
                                  uint32* compatibleSetsSize, uint8** compatibleSets) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_AUDIO != stream->mediaType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    *profileLevelIndication = stream->audioInfo.profileLevelIndication;
    *referenceChannelLayout = stream->audioInfo.referenceChannelLayout;
    *compatibleSetsSize = stream->audioInfo.compatibleSetsSize;
    *compatibleSets = stream->audioInfo.compatibleSets;

bail:
    return err;
}

/**
 * Function to tell the width of a text track.
 * The text track defines a window to display the subtitles.
 * This window shall be positioned in the middle of the screen.
 * And the sample is displayed in the window.How to position the sample within the window is defined
 * by the sample data. The origin of window is always (0, 0).
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a text track.
 * @param width [out] Width of the text track, in pixels.
 * @return
 */
EXTERN int32 MP4GetTextTrackWidth(FslParserHandle parserHandle, uint32 trackNum, uint32* width) {
    (void)parserHandle;
    (void)trackNum;
    *width = 0;
    return PARSER_SUCCESS;
}

/**
 * Function to tell the height of a text track.
 * The text track defines a window to display the subtitles.
 * This window shall be positioned in the middle of the screen.
 * And the sample is displayed in the window.How to position the sample within the window is defined
 * by the sample data. The origin of window is always (0, 0).
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a text track.
 * @param height [out] Height of the window, in pixels.
 * @return
 */
EXTERN int32 MP4GetTextTrackHeight(FslParserHandle parserHandle, uint32 trackNum, uint32* height) {
    (void)parserHandle;
    (void)trackNum;
    *height = 0;
    return PARSER_SUCCESS;
}

/**
 * Function to tell the mime type of a text metadata track.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a text track.
 * @param mime [out] mime type strings.
 * @param data_len [out] mime type length.
 * @return
 */
EXTERN int32 MP4GetTextTrackMime(FslParserHandle parserHandle, uint32 trackNum, uint8** mime,
                                 uint32* data_len) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_TEXT != stream->mediaType && TXT_METADATA != stream->decoderType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    *mime = stream->textInfo.mime;
    *data_len = stream->textInfo.mime_size;

bail:
    return err;
}

/**
 * Function to set the mode to read media samples, file-based or track-based. *
 * Warning:
 *  - The parser may support only one reading mode.Setting a not-supported reading mode will fail.
 *  - Once selected, the reading mode can no longer change.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a text track.
 * @param readMode [in] Sample reading mode.
 *
 *                      READ_MODE_FILE_BASED
 *                      Default mode.Linear sample reading. The reading order is same as that of
 *                      track interleaving in the file.
 *
 *                      READ_MODE_TRACK_BASED
 *                      Track-based sample reading. Each track can be read independently from each
 * other.
 * @return
 * PARSER_SUCCESS   The reading mode is set successfully.
 * PARSER_NOT_IMPLEMENTED   The reading mode is not supported.
 *
 */
EXTERN int32 MP4SetReadMode(FslParserHandle parserHandle, uint32 readMode) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;
    uint32 i;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    if (readMode == self->readMode)
        return err;

    if ((PARSER_READ_MODE_FILE_BASED == readMode) && self->numBSACTracks) {
        MP4MSG("ERR!BSAC audio can not support file-based sample reading!\n");
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)
    }

    if (self->isDeepInterleaving && (PARSER_READ_MODE_FILE_BASED == readMode)) {
        MP4MSG("ERR! CAN NOT support file-based reading mode on a deep interleaving clip!\n");
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)
    }

    if (self->isImageFile && (PARSER_READ_MODE_FILE_BASED == readMode)) {
        MP4MSG("ERR! CAN NOT support file-based reading mode for HEIF clip!\n");
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)
    }

    self->readMode = readMode;
    for (i = 0; i < self->numTracks; i++) {
        stream = self->streams[i];
        if (NULL == stream)
            BAILWITHERROR(PARSER_ERR_UNKNOWN)
        stream->readMode = readMode;
    }
    MP4MSG("Change read mode to %s\n",
           readMode == PARSER_READ_MODE_FILE_BASED ? "FILE_BASED" : "TRACK_BASED");

bail:
    return err;
}

/**
 * Function to get the mode to read media samples, file-based or track-based. *
 * And the parser has a default read mode.
 * For streaming application, file-based reading is mainly for streaming application.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a text track.
 * @param readMode [out] Current Sample reading mode.
 * @return
 */
EXTERN int32 MP4GetReadMode(FslParserHandle parserHandle, uint32* readMode) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    *readMode = self->readMode;

bail:
    return err;
}

/**
 * Function to enable or disable track.
 * The parser can only output samples from enabled tracks.
 * To avoid unexpected memory cost or data output from a track, the application can disable the
 * track.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a text track.
 * @param enable [in] TRUE to enable the track and FALSE to disable it.
 * @return
 */
EXTERN int32 MP4EnableTrack(FslParserHandle parserHandle, uint32 trackNum, bool enable) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (enable == stream->enabled) {
        MP4MSG("No need to turn on/off trk %d (current state %d)\n", trackNum, stream->enabled);
        return err; /* no need to change. */
    }

    /* To disable a track, it's partial output status is cleared.
    For BSAC tracks, must enale/disable them all. */
    if (self->fragmented) {
        err = enableFragmentedTrack(self->fragmented_reader, trackNum, enable);
    }

    if (stream->isBSAC) {
        uint32 i;

        for (i = 0; i < self->numTracks; i++) {
            stream = self->streams[i];
            if (NULL == stream)
                BAILWITHERROR(PARSER_ERR_UNKNOWN)

            if (stream->isBSAC) {
                stream->enabled = enable;
                if (!enable)
                    MP4MSG("disable BSAC trk %d\n", i);
                else {
                    MP4MSG("enable BSAC trk %d\n", i);
                }
            }
        }
    } else {
        stream->enabled = enable;
        if (!enable)
            MP4MSG("disable trk %d\n", trackNum);
        else {
            MP4MSG("enable trk %d\n", trackNum);
        }
    }

    /* reset reading status of this track or all BSAC tracks */
    if (!enable) {
        err = resetTrackReadingStatus(parserHandle, trackNum);
        if (PARSER_SUCCESS != err)
            goto bail;
    } else if (PARSER_READ_MODE_FILE_BASED == self->readMode) {
        MP4MSG("need select next trk to read\n");
        self->nextStream = NULL;
    }

bail:
    return err;
}

/**
 * Function to read the next sample from a track, only for track-based reading mode.
 *
 * In this function, the parser may use callback to request buffers to output the sample.
 * And it will tell which buffer is output on returning. But if the buffer is not available or too
 * small, this function will fail.
 *
 * It supports partial output of large samples:
 * If the entire sample can not be output by calling this function once, its remaining data
 * can be got by repetitive calling the same function.
 *
 * BSAC audio track is somewhat special:
 * Parser does not only read this BSAC track. But it will read one sample from each BSAC track
 * in the proper order and make up a "big sample" for the user.
 * Partial output is still supported and the sample flags describe this "big sample".
 * So the user shall take all BSAC tracks as one track and use any BSAC track number to call this
 * function.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track to read, 0-based.
 * @param sampleBuffer [out]   Buffer to hold the sample data.
 *                             If the buffer is not big enough, only part of sample is output.
 * @param bufferContext [out] Buffer context from application, got on requesting the buffer.
 *
 * @param dataSize [out]    If a sample or part of sample is output successfully (return value is
 * PARSER_SUCCESS ), it's the size of the data actually got.
 *
 * @param usStartTime [out] Start time of the sample in us (timestamp)
 * @param usDuration [out] Duration of the sample in us. PARSER_UNKNOWN_DURATION for unknown
 * duration.
 *
 * @param sampleFlags [out] Flags of this sample, if a sample or part of it is got successfully.
 *
 *                            FLAG_SYNC_SAMPLE
 *                                  Whether this sample is a sync sample (video key frame, random
 * access point). For non-video media, the application can take every sample as sync sample.
 *
 *                            FLAG_SAMPLE_NOT_FINISHED
 *                                  Sample data output is not finished because the buffer is not big
 * enough. Need to get the remaining data by repetitive calling this func.
 *
 *
 * @return  PARSER_SUCCESS     An entire sample or part of it is got successfully.
 *          PARSER_EOS     No sample is got because of end of the track.
 *          PARSER_INSUFFICIENT_MEMORY Buffer is not availble, or too small to output any data at
 * all. PARSER_READ_ERROR  File reading error. No need for further error concealment. Others ...
 */
EXTERN int32 MP4GetNextSample(FslParserHandle parserHandle, uint32 trackNum, uint8** sampleBuffer,
                              void** bufferContext, uint32* dataSize, uint64* usStartTime,
                              uint64* usDuration, uint32* sampleFlags) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;
    MP4MSG("%s trackNum %d\n", __FUNCTION__, trackNum);

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (!stream->enabled)
        BAILWITHERROR(PARSER_ERR_TRACK_DISABLED)

    if (PARSER_READ_MODE_TRACK_BASED != stream->readMode)
        BAILWITHERROR(PARSER_ERR_INVALID_READ_MODE)

    if (!stream->isBSAC) /* non-BSAC track, only read this track */
    {
        err = getTrackNextSample(parserHandle, trackNum, dataSize, usStartTime, usDuration,
                                 sampleFlags);

    } else { /* read from current BSAC track to the last one */
        uint32 sampleFlagsOneTrack;

        trackNum = self->nextBSACTrackNum;
        if ((uint32)PARSER_INVALID_TRACK_NUMBER == trackNum)
            BAILWITHERROR(PARSER_ERR_UNKNOWN)
        stream = self->streams[trackNum];

        err = getTrackNextSample(parserHandle, trackNum, dataSize, usStartTime, usDuration,
                                 &sampleFlagsOneTrack);

        if (PARSER_SUCCESS != err) {
            MP4MSG("fail to read BSAC trk %d, err %d\n", trackNum, err);
            goto bail;
        }

        if (!(FLAG_SAMPLE_NOT_FINISHED &
              sampleFlagsOneTrack)) /* Sample of this track is finished */
        {
            // MP4MSG("BSAC trk %d, sample finish, data size %ld, ", trackNum, *dataSize);
            /* move forward to next track */
            trackNum = stream->nextBSACTrackNum;

            if ((uint32)PARSER_INVALID_TRACK_NUMBER == trackNum) {
                self->nextBSACTrackNum = self->firstBSACTrackNum;
                // MP4MSG("rewind to 1st BSAC trk %d\n", self->nextBSACTrackNum);
            } else {
                self->nextBSACTrackNum = stream->nextBSACTrackNum;
                sampleFlagsOneTrack |= FLAG_SAMPLE_NOT_FINISHED; /* not last track, not finished */
                // MP4MSG("switch to next BSAC trk %d\n", self->nextBSACTrackNum);
            }
        }

        *sampleFlags = sampleFlagsOneTrack;
    }

bail:
    if (PARSER_SUCCESS == err) {
        *sampleBuffer = stream->sampleBuffer;
        *bufferContext = stream->bufferContext;

        stream->sampleBuffer = NULL;
    } /* otherwise, the sample buffer is already released by getTrackNextSample() */

    return err;
}

/**
 * Function to read the next sample from the file, only for file-based reading mode.
 * The parser will tell which track is outputting now and disabled track will be skipped.
 *
 * In this function, the parser may use callback to request buffers to output the sample.
 * And it will tell which buffer is output on returning. But if the buffer is not available or too
 * small, this function will fail.
 *
 * It supports partial output of large samples:
 * If the entire sample can not be output by calling this function once, its remaining data
 * can be got by repetitive calling the same function.
 *
 * BSAC audio track is NOT supported for the track dependency consideration.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [out] Number of the track to read, 0-based.
 * @param sampleBuffer [out]   Buffer to hold the sample data.
 *                             If the buffer is not big enough, only part of sample is output.
 * @param bufferContext [out] Buffer context from application, got on requesting the buffer.
 *
 * @param dataSize [out]    If a sample or part of sample is output successfully (return value is
 * PARSER_SUCCESS ), it's the size of the data actually got.
 *
 * @param usStartTime [out] Start time of the sample in us (timestamp)
 * @param usDuration [out] Duration of the sample in us. PARSER_UNKNOWN_DURATION for unknown
 * duration.
 *
 * @param sampleFlags [out] Flags of this sample, if a sample or part of it is got successfully.
 *
 *                            FLAG_SYNC_SAMPLE
 *                                  Whether this sample is a sync sample (video key frame, random
 * access point). For non-video media, the application can take every sample as sync sample.
 *
 *                            FLAG_SAMPLE_NOT_FINISHED
 *                                  Sample data output is not finished because the buffer is not big
 * enough. Need to get the remaining data by repetitive calling this func.
 *
 *
 * @return  PARSER_SUCCESS     An entire sample or part of it is got successfully.
 *          PARSER_EOS     No sample is got because of end of the track.
 *          PARSER_INSUFFICIENT_MEMORY Buffer is not availble, or too small to output any data at
 * all. PARSER_READ_ERROR  File reading error. No need for further error concealment. Others ...
 */

EXTERN int32 MP4GetFileNextSample(FslParserHandle parserHandle, uint32* trackNum,
                                  uint8** sampleBuffer, void** bufferContext, uint32* dataSize,
                                  uint64* usStartTime, uint64* usDuration, uint32* sampleFlags) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    MP4MSG("\n%s trackNum %d\n", __FUNCTION__, *trackNum);

    if (PARSER_READ_MODE_FILE_BASED != self->readMode)
        BAILWITHERROR(PARSER_ERR_INVALID_READ_MODE)

    if (NULL == self->nextStream) {
        err = getNextTrackToRead(parserHandle);
        if (PARSER_SUCCESS != err) {
            MP4MSG("getNextTrackToRead fail\n");
            goto bail;
        }

        if (NULL == self->nextStream) {
            MP4MSG("getNextTrackToRead return NULL, fail\n");
            BAILWITHERROR(PARSER_EOS)
        } /* All tracks are disabled or EOS */

        // MP4MSG("First track to read: trk %d\n", self->nextStream->streamIndex);
    }

    stream = self->nextStream;
    if (PARSER_READ_MODE_FILE_BASED != stream->readMode)
        BAILWITHERROR(PARSER_ERR_INVALID_READ_MODE)

    *trackNum = stream->streamIndex;

    err = getTrackNextSample(parserHandle, stream->streamIndex, dataSize, usStartTime, usDuration,
                             sampleFlags);

bail:
    if (PARSER_SUCCESS == err) {
        *sampleBuffer = stream->sampleBuffer;
        *bufferContext = stream->bufferContext;
        // MP4MSG("%s success ts %lld, size %d\n", __FUNCTION__, *usStartTime, *dataSize);

        stream->sampleBuffer = NULL;
    } /* otherwise, the sample buffer is already released by getTrackNextSample() */

    return err;
}

/**
 * Function to seek a track to a target time, for both track-based mode and file-based mode.
 * The parser handles the mode difference internally.
 *
 * It will seek to a sync sample of the time stamp
 * matching the target time. Due to the scarcity of the video sync samples (key frames),
 * there can be a gap between the target time and the timestamp of the matched sync sample.
 * So this time stamp will be output to as the accurate start time of the following playback
 * segment.
 *
 * BSAC audio track is somewhat special:
 * Parser does not only seek this BSAC track. But it will seek all BSAC tracks to the target time
 * and align their reading position.
 * So the user shall take all BSAC tracks as one track and use any BSAC track number to call this
 * function.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track to seek, 0-based.
 * @param usTime [in/out]  Target time to seek as input, in us.
 *                         Actual seeking time, timestamp of the matched sync sample, as output.
 *
 * @param flag [in]  Control flags to seek.
 *
 *                      SEEK_FLAG_NEAREST
 *                      Default flag.The matched time stamp shall be the nearest one to the target
 * time (may be later or earlier).
 *
 *                      SEEK_FLAG_NO_LATER
 *                      The matched time stamp shall be no later than the target time.
 *
 *                      SEEK_FLAG_NO_EARLIER
 *                      The matched time stamp shall be no earlier than the target time.
 *
 * @return  PARSER_SUCCESS    Seeking succeeds.
 *          PARSER_EOS  Can NOT to seek to the target time because of end of stream.
 *          PARSER_ERR_NOT_SEEKABLE    Seeking fails because the movie is not seekable (index not
 * available or no sync samples) Others      Seeking fails for other reason.
 */
EXTERN int32 MP4Seek(FslParserHandle parserHandle, uint32 trackNum, uint64* usTime, uint32 flag) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    MP4MSG("\n%s trackNum %d\n", __FUNCTION__, trackNum);

    err = seekTrack(parserHandle, trackNum, flag, usTime);

    if (self && (PARSER_READ_MODE_FILE_BASED == self->readMode))
        self->nextStream = NULL; /* need to select next track to read */

    return err;
}

/**
 * Function to get the next or previous sync sample (key frame) from current reading position of a
 * track, only for track-based reading mode.
 *
 * For trick mode FF/RW.
 * Also support partial output of large samples.
 * If not the entire sample is got, its remaining data can be got by repetitive calling the same
 * function.
 *
 * Warning: This function does not support BSAC audio tracks because there is dependency between
 * these tracks.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track to read, 0-based.
 * @param direction [in]  Direction to get the sync sample.
 *                           FLAG_FORWARD   Read the next sync sample from current reading position.
 *                           FLAG_BACKWARD  Read the previous sync sample from current reading
 * position.
 *
 * @param sampleBuffer [out]   Buffer to hold the sample data.
 *                             If the buffer is not big enough, only part of sample is output.
 * @param bufferContext [out] Buffer context from application, got on requesting the buffer.
 *
 * @param dataSize [out]  If a sample or part of sample is output successfully (return value is
 * PARSER_SUCCESS ), it's the size of the data actually got.
 *
 * @param usStartTime [out] Start time of the sample in us (timestamp)
 * @param usDuration [out] Duration of the sample in us. PARSER_UNKNOWN_DURATION for unknown
 * duration.
 *
 * @param flag [out] Flags of this sample, if a sample or part of it is got successfully.
 *                            FLAG_SYNC_SAMPLE
 *                                  Whether this sample is a sync sample (key frame).
 *                                  For non-video media, the application can take every sample as a
 * sync sample. This flag shall always be SET for this API.
 *
 *                            FLAG_SAMPLE_NOT_FINISHED
 *                                  Sample data output is not finished because the buffer is not big
 * enough. Need to get the remaining data by repetitive calling this func.
 *
 *
 * @return  PARSER_SUCCESS     An entire sync sample or part of it is got successfully.
 *          PARSER_ERR_NOT_SEEKABLE    No sync sample is got  because the movie is not seekable
 * (index not available or no sync samples) PARSER_EOS      Reaching the end of the track, no sync
 * sample is got. PARSER_BOS      Reaching the beginning of the track, no sync sample is got.
 *          PARSER_INSUFFICIENT_MEMORY Buffer is too small to hold the sample data.
 *                                  The user can allocate a larger buffer and call this API again.
 *          PARSER_READ_ERROR  File reading error. No need for further error concealment.
 *          PARSER_ERR_CONCEAL_FAIL  There is error in bitstream, and no sample can be found by
 * error concealment. A seeking is helpful. Others ... Reading fails for other reason.
 */
EXTERN int32 MP4GetNextSyncSample(FslParserHandle parserHandle, uint32 direction, uint32 trackNum,
                                  uint8** sampleBuffer, void** bufferContext, uint32* dataSize,
                                  uint64* usStartTime, uint64* usDuration, uint32* flags)

{
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;
    MP4MSG("%s trackNum %d\n", __FUNCTION__, trackNum);

    if ((NULL == self) || (trackNum >= self->numTracks))
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (!stream->enabled)
        BAILWITHERROR(PARSER_ERR_TRACK_DISABLED)

    if (MEDIA_AUDIO == stream->mediaType) {
        if ((AUDIO_AAC == stream->decoderType) && (AUDIO_ER_BSAC == stream->decoderSubtype))
            BAILWITHERROR(PARSER_NOT_IMPLEMENTED)
        else if (AUDIO_PCM ==
                 stream->decoderType) /* PCM audio normal output more than one samples one time */
            BAILWITHERROR(PARSER_NOT_IMPLEMENTED)
    } else if (MEDIA_IMAGE == stream->mediaType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    if (self->fragmented) {
        err = getFragmentedTrackNextSyncSample(self->fragmented_reader, direction, trackNum,
                                               sampleBuffer, bufferContext, dataSize, usStartTime,
                                               usDuration, flags);
        goto bail;
    }
    // stream->sampleBytesLeft is for mp4 container syntax.
    // stream->bSampleNotFinishe is for h264 syntax. interleaved clips, one field one sample.
    if ((0 == stream->sampleBytesLeft) && (FALSE == stream->bPrevSampleNotFinished)) {
        MP4TrackReaderPtr reader;
        uint32 nextSyncSampleNumber;

        /* find the sync sample */
        err = findTrackNextSyncSample(stream, direction, &nextSyncSampleNumber);
        if (PARSER_SUCCESS != err) {
            // MP4MSG("trk %d, can not find next sync sample (direction %d)\n", stream->streamIndex,
            // direction);
            goto bail;
        }

        /* Update the track reader. On EOS or ERROR, reader's sample number does not change, still
         * at the previous postion.*/
        reader = (MP4TrackReaderPtr)stream->reader;
        if (reader) {
            SAFE_DESTROY(reader);
            stream->reader = NULL;
        }
        MP4GetTrackReader(stream->trak, nextSyncSampleNumber, &stream->reader);

    } /* otherwise, directly output the remaining data of the sync sample */

    err = MP4GetNextSample(parserHandle, trackNum, sampleBuffer, bufferContext, dataSize,
                           usStartTime, usDuration, flags);

    stream->bPrevSampleNotFinished = *flags & FLAG_SAMPLE_NOT_FINISHED;

bail:
    return err;
}

EXTERN int32 MP4GetFileNextSyncSample(FslParserHandle parserHandle, uint32 direction,
                                      uint32* trackNum, uint8** sampleBuffer, void** bufferContext,
                                      uint32* dataSize, uint64* usStartTime, uint64* usDuration,
                                      uint32* sampleFlags) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;
    MP4MSG("\n%s trackNum %d direction %s\n", __FUNCTION__, *trackNum,
           direction == FLAG_BACKWARD ? "backward" : "forward");

    if (PARSER_READ_MODE_FILE_BASED != self->readMode)
        BAILWITHERROR(PARSER_ERR_INVALID_READ_MODE)

    stream = self->nextStream;
    if ((stream && (0 == stream->sampleBytesLeft) && (FALSE == stream->bPrevSampleNotFinished)) ||
        !stream) {
        /* find next track with nearest sync sample offset */
        err = getNextTrackToReadTrickMode(parserHandle, direction);
        if (PARSER_SUCCESS != err)
            goto bail;

        if (NULL == self->nextStream)
            BAILWITHERROR(PARSER_EOS) /* All tracks are disabled or EOS */

        stream = self->nextStream;
        MP4MSG("Next track to read (direction %d): trk %d\n", direction,
               self->nextStream->streamIndex);
    } /* otherwise, directly output the remaining data of the sync sample */

    if (self->fragmented) {
        err = getFragmentedTrackNextSyncSample(self->fragmented_reader, direction,
                                               stream->streamIndex, sampleBuffer, bufferContext,
                                               dataSize, usStartTime, usDuration, sampleFlags);
        stream->bPrevSampleNotFinished = *sampleFlags & FLAG_SAMPLE_NOT_FINISHED;
        return err;

    } else {
        err = getTrackNextSample(parserHandle, stream->streamIndex, dataSize, usStartTime,
                                 usDuration, sampleFlags);
    }
    /* find the next sync sample for all enabled tracks */

    stream->bPrevSampleNotFinished = *sampleFlags & FLAG_SAMPLE_NOT_FINISHED;

bail:
    if (PARSER_SUCCESS == err) {
        *trackNum = stream->streamIndex;
        *sampleBuffer = stream->sampleBuffer;
        *bufferContext = stream->bufferContext;

        stream->sampleBuffer = NULL;
    } /* otherwise, the sample buffer is already released by getTrackNextSample() */

    return err;
}

int32 MP4GetSampleCryptoInfo(FslParserHandle parserHandle, uint32 trackNum, uint8** iv,
                             uint32* ivSize, uint8** clearBuffer, uint32* clearSize,
                             uint8** encryptedBuffer, uint32* encryptedSize) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    if (self->fragmented) {
        err = getFragmentedTrackSampleCryptoInfo(self->fragmented_reader, trackNum, iv, ivSize,
                                                 clearBuffer, clearSize, encryptedBuffer,
                                                 encryptedSize);
    } else
        err = PARSER_NOT_IMPLEMENTED;
    return err;
}

/*******************************************************************************
Implementation of the function pointers for all the call back functions.
********************************************************************************/
// #ifndef __WINCE  /* not applicable for wince platform */

#ifndef MP4_MEM_DEBUG

#define PROTECT_SIZE 8

void* MP4LocalCalloc(u32 number, u32 size) {
    return g_memOps.Calloc(number, size + PROTECT_SIZE);
}

void* MP4LocalMalloc(u32 size) {
    return g_memOps.Malloc(size + PROTECT_SIZE);
}

void* MP4LocalReAlloc(void* MemoryBlock, u32 size) {
    return g_memOps.ReAlloc(MemoryBlock, size + PROTECT_SIZE);
}

void MP4LocalFree(void* MemoryBlock) {
    g_memOps.Free(MemoryBlock);
}
#endif

/**
 * function to parse the MP4 file header. It shall be called after MP4 parser is created
 * and it will probe whether the movie can be handled by this MP4 parser.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @return
 */
static int32 parseFileHeader(FslParserHandle parserHandle, uint32 flag) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    MP4Movie movie = NULL;
    MP4MovieAtomPtr moov;
    MP4PrivateMovieRecordPtr moovp = NULL;
    MP4MovieHeaderAtomPtr mvhd;

    AVStreamPtr stream;
    uint32 numTracks;
    uint32 i, j;
    uint64 usSeekTime; /* seek to beginning of tracks */

#ifdef _WINCE_FSL_MP4_PARSER_DBG_TIME
    u32 start_time = timeGetTime();
    u32 end_time;
#endif

    err = MP4OpenMovieFile(&movie, self->fileMappingObject, flag, self->appContext);
    if (err)
        goto bail;

#ifdef _WINCE_FSL_MP4_PARSER_DBG_TIME
    end_time = timeGetTime();
    MP4DbgLog("\ntime for OpenMovieFile: %ld ms\n\n", end_time - start_time);
    MP4DbgLog("start %ld ms, end %ld ms\n\n", start_time, end_time);

#endif

    self->movie = movie;
    moov = (MP4MovieAtomPtr)((MP4PrivateMovieRecordPtr)movie)->moovAtomPtr;
    moovp = (MP4PrivateMovieRecordPtr)movie;
    if (moovp->fragmented) {
        FragmentedReaderStruct* fragmented_reader;
        self->fragmented = moovp->fragmented;
        self->firstMoof = moovp->moofOffset;
        self->currentMoof = moovp->moofOffset;

        err = MP4CreateFragmentedReader(moovp, flag, self->appContext, &fragmented_reader);
        if (err)
            goto bail;
        self->fragmented_reader = (MP4FragmentedReader)fragmented_reader;
        MP4MSG("parseFileHeader fragmented found! \n");
    }

    if (moovp->isHeif || moovp->isAvif) {
        ItemTablePtr itemTable = moovp->itemTable;
        numTracks = countDisplayable(itemTable);
        for (i = 0; i < numTracks; i++) {
            MP4MSG("\n%s trk %d:\n", moovp->isHeif ? "Heif" : "Avif", i);
            stream = (AVStreamPtr)MP4LocalCalloc(1, sizeof(AVStream));
            TESTMALLOC(stream)
            stream->streamIndex = i;
            stream->appContext = self->appContext;
            stream->isEmpty = FALSE;

            err = getDisplayImageInfo(itemTable, i, stream);
            if (err)
                goto bail;
            stream->mediaType = MEDIA_IMAGE;
            stream->decoderType = moovp->isHeif ? VIDEO_HEVC : VIDEO_H264;

            self->streams[i] = stream;
            MP4MSG("track %d isDefault %d isGrid %d\n", i, self->streams[i]->imageInfo.isDefault,
                   self->streams[i]->imageInfo.isGrid);
        }
        self->numTracks = numTracks;
        self->isImageFile = TRUE;

        BAILWITHERROR(PARSER_SUCCESS)
    }

    if (NULL == moov) {
        MP4MSG("Parsing finish but moov not found!\n");
        BAILWITHERROR(PARSER_ERR_UNKNOWN)
    }

    mvhd = (MP4MovieHeaderAtomPtr)moov->mvhd;

    err = MP4GetMovieTrackCount(movie, &numTracks);
    if (err)
        goto bail;
    self->numTracks = numTracks;

    /* scan all tracks */
    self->primaryTrackNumber = PARSER_INVALID_TRACK_NUMBER;

    err = MP4GetMovieCreationTime(movie, &self->creationTime);
    if (err)
        goto bail;

    for (i = 0; i < numTracks; i++) {
        uint32 trackNum;
        uint32 stsdIndex = 1;
        trackNum = i + 1;

        MP4MSG("\ntrk %d:\n", i);
        stream = (AVStreamPtr)MP4LocalCalloc(1, sizeof(AVStream));
        TESTMALLOC(stream)
        self->streams[i] = stream;
        stream->streamIndex = i;
        stream->corruptChunk = -1;
        stream->internalSample = FALSE;

        stream->appContext = self->appContext;

        err = MP4GetMovieIndTrack(movie, trackNum, &(stream->trak));
        if (err)
            goto bail;

        err = MP4GetTrackMedia(stream->trak, &(stream->mdia));
        if (err) {
            stream->isEmpty = TRUE;
            continue;
        }
        stream->isEmpty = isTrackEmpty(stream->mdia);
        if (stream->isEmpty && self->fragmented) {
            stream->isEmpty = FALSE;
        }

        ((MP4MediaAtomPtr)(stream->mdia))->parserStream = stream; /* set the hook */

        err = MP4NewHandle(0, &(stream->decoderSpecificInfoH));
        if (err)
            goto bail;
        err = MP4NewHandle(0, &(stream->sampleH));
        if (err)
            goto bail; /* initial size is 0, not allocate data buffer in the handler */
        stream->extTagList = MP4CreateTagList();

        /* A/V propeties */
        MP4GetTrackDuration(stream->trak, &stream->usDuration);
        MP4MSG("track duration in time scale: %lld\n", (long long)stream->usDuration);

        stream->usDuration = stream->usDuration * 1000 * 1000 / mvhd->timeScale;
        MP4MSG("duration in us: %lld\n", (long long)stream->usDuration);

        err = MP4GetMediaSampleCount(stream->mdia, &stream->numSamples);
        MP4MSG("number of samples: %u\n", stream->numSamples);
        if (err) {
            stream->isEmpty = TRUE;
            continue;
        }
        err = MP4GetMediaTimeScale(stream->mdia, &stream->timeScale);
        if (err)
            goto bail;
        // MP4MSG("time scale: %ld\n", stream->timeScale);

        /* get the decoder type from 'stsd' 1st entry's fourcc.
        Obsolete method:
        Get media decoder type,  objectTypeIndication,
        streamType, profile and level (in initialObjectDescriptor) and
        decoderConfig and instantiate appropriate decoder. */
        err = MP4GetMediaDecoderInformation(
                stream->mdia, stsdIndex, flag, &stream->mediaType, &stream->decoderType,
                &stream->decoderSubtype, &stream->maxBitRate, &stream->avgBitRate,
                &stream->videoInfo.NALLengthFieldSize, stream->decoderSpecificInfoH);
        if (err)
            goto bail;
        if (MEDIA_TYPE_UNKNOWN == stream->mediaType) {
            stsdIndex++;
            err = MP4GetMediaDecoderInformation(
                    stream->mdia, stsdIndex, flag, &stream->mediaType, &stream->decoderType,
                    &stream->decoderSubtype, &stream->maxBitRate, &stream->avgBitRate,
                    &stream->videoInfo.NALLengthFieldSize, stream->decoderSpecificInfoH);
            if (err)
                continue;
        }
        if (MEDIA_VIDEO == stream->mediaType) {
            u64 segment_duration = 0;
            s64 media_time = 0;

            err = MP4GetVideoProperties(stream->mdia, stsdIndex, &stream->videoInfo.frameWidth,
                                        &stream->videoInfo.frameHeight,
                                        &stream->videoInfo.frameRateNumerator,
                                        &stream->videoInfo.frameRateDenominator);
            if (err)
                goto bail;

            err = MP4GetTrackRotationDegree(stream->trak, &stream->videoInfo.frameRotationDegree);
            MP4MSG("***get rotate =%d, err=%d\n", stream->videoInfo.frameRotationDegree, err);
            if (err)
                goto bail;

            err = MP4GetMediaColorInfo(
                    stream->mdia, stsdIndex, stream->decoderType, &stream->videoInfo.hasColorInfo,
                    &stream->videoInfo.primaries, &stream->videoInfo.transfer,
                    &stream->videoInfo.coeff, &stream->videoInfo.full_range_flag);

            MP4MSG("***get color info %d, %d,%d,%d,%d err=%d\n", stream->videoInfo.hasColorInfo,
                   stream->videoInfo.primaries, stream->videoInfo.transfer, stream->videoInfo.coeff,
                   stream->videoInfo.full_range_flag, err);

            // ignore the resule because some format do not has colr atom
            if (err)
                err = PARSER_SUCCESS;

            stream->nalLengthFieldSize = stream->videoInfo.NALLengthFieldSize;
            MP4MSG("\nstream %u, nal length field size %u ****\n", stream->streamIndex,
                   stream->nalLengthFieldSize);

            err = MP4GetTrackDimensions(stream->trak, &stream->videoInfo.displayWidth,
                                        &stream->videoInfo.displayHeight);
            if (err)
                goto bail;

            if ((uint32)PARSER_INVALID_TRACK_NUMBER == self->primaryTrackNumber)
                self->primaryTrackNumber = i;

            MP4GetTrackEditListInfo(stream->trak, &segment_duration, &media_time);

            if (!moovp->fragmented) {
                u64 cts;
                err = MP4GetVideoThumbnailSampleTime(stream->mdia, &cts);
                stream->videoInfo.thunmbnailTime = cts * 1000 * 1000 / stream->timeScale;
                MP4MSG("***get thumbnail time=%lld, err=%d\n", stream->videoInfo.thunmbnailTime,
                       err);
                if (err)
                    goto bail;
            }

        } else if (MEDIA_AUDIO == stream->mediaType) {
            u64 segment_duration = 0;
            s64 media_time = 0;
            err = MP4GetAudioProperties(stream->mdia, stsdIndex, &stream->audioInfo,
                                        &stream->audioObjectType);
            if (err)
                goto bail;

            if ((uint32)PARSER_INVALID_TRACK_NUMBER == self->primaryTrackNumber)
                self->primaryTrackNumber = i;

            if ((AUDIO_AAC == stream->decoderType) && (AUDIO_ER_BSAC == stream->decoderSubtype)) {
                self->numBSACTracks++;
                stream->isBSAC = TRUE;
            }

            if (MP4NoErr == MP4GetTrackEditListInfo(stream->trak, &segment_duration, &media_time)) {
                u32 movie_timescale = 0;
                u64 half_scale = 0;
                s64 enc_delay = 0;
                s64 paddingsamples = 0;

                if (stream->audioInfo.sampleRate > 0 && stream->timeScale > 0 && media_time > 0) {
                    half_scale = stream->timeScale / 2;
                    enc_delay = (media_time * stream->audioInfo.sampleRate + half_scale) /
                                stream->timeScale;
                    if (enc_delay < INT32_MAX && enc_delay > INT32_MIN)
                        self->audioEncoderDelay = (s32)enc_delay;
                }
                if (segment_duration > 0 && media_time >= 0 && stream->audioInfo.sampleRate > 0 &&
                    MP4NoErr == MP4GetMovieTimeScale(movie, &movie_timescale) &&
                    movie_timescale > 0) {
                    s64 segment_end;
                    s64 padding;
                    s64 segment_duration_e6;
                    s64 media_time_scaled_e6;
                    s64 scaled_duration;
                    MP4MSG("segment_duration %lld, media_time %lld, movie_scale %u, samplerate %u",
                           segment_duration, media_time, movie_timescale,
                           stream->audioInfo.sampleRate);
                    scaled_duration = stream->usDuration * (s64)movie_timescale;
                    segment_duration_e6 = segment_duration * 1000000;
                    media_time_scaled_e6 = media_time * movie_timescale * 1000000;
                    media_time_scaled_e6 /= stream->audioInfo.sampleRate;
                    segment_end = segment_duration_e6 + media_time_scaled_e6;
                    padding = scaled_duration - segment_end;
                    if (padding > 0L) {
                        s64 halfscale_mht;
                        s64 halfscale_e6;
                        s64 timescale_e6;

                        halfscale_mht = (s64)movie_timescale / 2;
                        paddingsamples = padding * stream->audioInfo.sampleRate;
                        halfscale_e6 = halfscale_mht * 1000000;
                        timescale_e6 = (s64)movie_timescale * 1000000;
                        paddingsamples += halfscale_e6;
                        paddingsamples /= timescale_e6;
                        if (paddingsamples > INT32_MAX || paddingsamples < 0)
                            paddingsamples = 0;
                    }
                }
                self->audioEncoderPadding = (u32)paddingsamples;
                MP4MSG("audioEncoderPadding %d\n", self->audioEncoderPadding);
            }
        } else if (stream->mediaType == MEDIA_TEXT &&
                   stream->decoderType == TXT_3GP_STREAMING_TEXT) {
            err = MP4NewHandle(0, &(stream->tx3gHandle));
            if (err)
                goto bail;
            err = MP4GetMediaTextFormatData(stream->mdia, &stream->textInfo.handleSize,
                                            stream->tx3gHandle);
            if (err)
                goto bail;
            err = MP4AddTag(stream->extTagList, FSL_PARSER_TRACKEXTTAG_TX3G, USER_DATA_FORMAT_UTF8,
                            stream->textInfo.handleSize, (uint8*)(*stream->tx3gHandle));
            if (err)
                goto bail;
        } else if (stream->mediaType == MEDIA_TEXT && stream->decoderType == TXT_METADATA) {
            err = MP4GetMettMime(stream->mdia, &stream->textInfo.mime, &stream->textInfo.mime_size);
            if (err)
                goto bail;
        }

        if ((stream->mediaType == MEDIA_VIDEO) && (stream->decoderType == VIDEO_H264)) {
            u32 avccSize = 0;
            uint8* avcc = NULL;
            err = CreateH264Parser(&stream->pH264Parser, &g_memOps, 0);
            if (err)
                goto bail;

            err = MP4GetHandleSize(stream->decoderSpecificInfoH, &avccSize);
            if (err)
                goto bail;
            if (avccSize > 0) {
                avcc = (uint8*)(*stream->decoderSpecificInfoH);
                if (avcc) {
                    H264HeaderInfo h264Header;
                    memset(&h264Header, 0, sizeof(H264HeaderInfo));
                    if (ParseH264CodecDataFrame(stream->pH264Parser, avcc, avccSize, &h264Header) ==
                        0) {
                        stream->videoInfo.scanType = h264Header.scanType;
                        MP4MSG("scantype from avcc: %d\n", h264Header.scanType);
                    }
                }
            }
        }

        if (self->fragmented) {
            MP4MediaInformationAtomPtr minf =
                    (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)stream->mdia)->information;

            MP4MovieExtendsAtomPtr mvex = (MP4MovieExtendsAtomPtr)moov->mvex;
            MP4TrackExtendsAtomPtr trex = NULL;

            u32 track_id = 0;
            err = MP4GetTrackID(stream->trak, &track_id);
            err = mvex->getTrex(mvex, track_id, &trex);

            if (MEDIA_VIDEO == stream->mediaType) {
                err = addFragmentedTrack((FragmentedReaderStruct*)self->fragmented_reader, track_id,
                                         minf, 1, stream->videoInfo.NALLengthFieldSize,
                                         stream->timeScale, trex, stream->mediaType);
            } else {
                err = addFragmentedTrack((FragmentedReaderStruct*)self->fragmented_reader, track_id,
                                         minf, 1, 0, stream->timeScale, trex, stream->mediaType);
            }
            if ((uint32)PARSER_INVALID_TRACK_NUMBER == self->primaryTrackNumber)
                self->primaryTrackNumber = i;

            err = addExtTrackTags((FragmentedReaderStruct*)self->fragmented_reader, track_id,
                                  stream->extTagList);
        }
    }

    if (self->numBSACTracks) /* setup relation map of BSAC audio tracks */
    {
        AVStreamPtr refStream;
        MP4TrackAtomPtr refTrak; /* next BSAC track */

        MP4MSG("\nsetup BSAC reference\n");
        moov->SetupReferences(moov);

        for (i = 0; i < numTracks; i++) {
            stream = self->streams[i];
            if (!stream->isBSAC)
                continue;

            stream->prevBSACTrackNum = PARSER_INVALID_TRACK_NUMBER;
            stream->nextBSACTrackNum = PARSER_INVALID_TRACK_NUMBER;

            refTrak = (MP4TrackAtomPtr)(((MP4TrackAtomPtr)stream->trak)->trak_reference);
            if (NULL == refTrak) {
                MP4MSG("trk %d is the last BSAC audio track\n", i);
            } else {
                for (j = 0; j < self->numTracks; j++) {
                    refStream = self->streams[j];
                    if (refTrak == (MP4TrackAtomPtr)refStream->trak) {
                        stream->nextBSACTrackNum = refStream->streamIndex;
                        refStream->prevBSACTrackNum = stream->streamIndex;
                        MP4MSG("trk %d has the next trk %d\n", stream->streamIndex,
                               stream->nextBSACTrackNum);
                        break;
                    }
                }
                if ((uint32)PARSER_INVALID_TRACK_NUMBER == stream->nextBSACTrackNum) {
                    MP4MSG("ERR!trk %d's next track not found\n", i);
                    BAILWITHERROR(PARSER_ERR_UNKNOWN)
                }
            }
        }

        for (i = 0; i < numTracks; i++) {
            stream = self->streams[i];
            if (!stream->isBSAC)
                continue;

            if ((uint32)PARSER_INVALID_TRACK_NUMBER == stream->prevBSACTrackNum) {
                MP4MSG("trk %d is the 1st BSAC audio track\n", i);
                self->firstBSACTrackNum = i;
                self->nextBSACTrackNum = i;
                break;
            }
        }
    }

    /* user data */
    err = getUserDataList(parserHandle);
    if (err)
        goto bail;

    err = getFileUserDataList(parserHandle);
    if (err)
        goto bail;

    err = getStreamUserDataList(parserHandle);
    if (err)
        goto bail;

    if (!self->fragmented)
        checkInterleavingDepth(self);

    err = getDefaultReadMode(parserHandle);
    if (err)
        goto bail;

    /* movie is seekable? */
    err = isMovieSeekable(parserHandle);
    if (err)
        goto bail;

    /* seek to the start of each track (shall always succeed) */
    for (i = 0; i < numTracks; i++) {
        usSeekTime = 0;
        stream = self->streams[i];

        if (stream->isEmpty) { /* application will not output empty tracks to avoid pre-roll failure
                                  under file-based reading mode. */
            stream->mediaType = MEDIA_TYPE_UNKNOWN;
            continue;
        }

        if (stream->isBSAC && (i != self->firstBSACTrackNum))
            continue; /* for BSAC, seek one track will seek all BSAC tracks*/

        err = MP4Seek(parserHandle, i, &usSeekTime, SEEK_FLAG_NO_LATER);
        if (err)
            goto bail;
    }

bail:
    return err;
}

/* get the next sample from a track,
sampleData [in] sample buffer
dataSize [in] size of the sample buffer */
static int32 getTrackNextSample(FslParserHandle parserHandle, uint32 trackNum,
                                // uint8 * sampleData,
                                uint32* dataSize, uint64* usStartTime, uint64* usDuration,
                                uint32* sampleFlags) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    uint64 cts = 0, dts = 0;
#ifdef ANDROID
    int64 sampleTs;
#else
    uint64 sampleTs;
#endif
    u64 elstShiftStartTicks = 0;
    uint32 sampleDuration;
    u32 max_chunk_offset = 0;
    u32 odsm_offset = 0;
    u32 sdsm_offset = 0;

    MP4MSG("%s\n", __FUNCTION__);

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)
    if (stream->isEmpty)
        BAILWITHERROR(PARSER_EOS)

    stream->isOneSampleGotAfterSkip = TRUE;

    /* TODO: set sample handler size only */
    //*stream->sampleH = (char*)sampleData;
    if (self->fragmented) {
        u64 duration = 0;
        err = getFragmentedTrackNextSample(self->fragmented_reader, trackNum, &stream->sampleBuffer,
                                           &stream->bufferContext, dataSize, &dts, &duration,
                                           sampleFlags);
        *usStartTime = dts;
        *usDuration = duration;
        self->nextStream = NULL;
    } else if (self->isImageFile) {
        MP4PrivateMovieRecordPtr moovp = (MP4PrivateMovieRecordPtr)self->movie;
        ItemTablePtr itemTable = moovp->itemTable;
        err = readImageContent(itemTable, stream, dataSize);
    } else {
        /* Get the Access unit from the file. */
        err = MP4TrackReaderGetNextAccessUnitWithDuration(
                stream->reader, stream->sampleH, dataSize, sampleFlags, &cts, &dts, &sampleDuration,
                &max_chunk_offset, odsm_offset, sdsm_offset);

        if (MP4_ERR_WRONG_SAMPLE_SIZE == err)
            err = PARSER_ERR_CONCEAL_FAIL;
        else if (err)
            goto bail;

        sampleTs = (FLAG_OUTPUT_PTS & self->flags) ? cts : dts;
        sampleTs += ((MP4TrackAtomPtr)(stream->trak))->elstInitialEmptyEditTicks;
        elstShiftStartTicks = ((MP4TrackAtomPtr)(stream->trak))->elstShiftStartTicks;

#ifndef ANDROID  // linux doens't support negative ts
        if (elstShiftStartTicks <= sampleTs)
#endif
            sampleTs -= elstShiftStartTicks;

        *usStartTime = sampleTs * 1000 * 1000 / stream->timeScale;

        MP4MSG("getSample: result sampltTime %lld, usSampleTime %lld, dataSize %d\n", sampleTs,
               *usStartTime, *dataSize);

        //*usEndTime = (dts + sampleDuration)*1000 * 1000 / stream->timeScale;
        *usDuration = (uint64)sampleDuration * 1000000 / stream->timeScale;
    }

bail:
    if ((PARSER_SUCCESS != err) && stream &&
        stream->sampleBuffer) { /* release the buffer on err. Never holds a buffer after one output
                                   is finished */
        MP4MSG("trk %d, release buffer %p on err %d\n", trackNum, stream->sampleBuffer, err);
        MP4LocalReleaseBuffer(trackNum, stream->sampleBuffer, stream->bufferContext,
                              stream->appContext);

        stream->sampleBuffer = NULL;
    }

    if (PARSER_SUCCESS == err) {
        /* current chunk ends ? */
        if (stream->nextSampleNumber > stream->lastSampleNumberInChunk) {
            // MP4MSG("trk %d, chunk %d ends, next sample number %d \n", trackNum,
            // stream->chunkNumber, stream->nextSampleNumber);
            if (stream->sampleBytesLeft || (0 == stream->chunkNumber))
                BAILWITHERROR(PARSER_ERR_UNKNOWN)
            stream->chunkNumber = 0; /* shall find a new chunk */

            /* need a track switch? Only for file-based reading mode.*/
            if (PARSER_READ_MODE_FILE_BASED == stream->readMode) {
                err = getNextTrackToRead(
                        parserHandle); /* careful, some track may end while some not! */
                if (err) {
                    MP4MSG("ERR, fail to select next track to read, err %d\n", err);
                }
            }
        }
    }

    return err;
}

/* skip to a track's next sync sample and setup the reader.
If no sync sample can be found, return EOS/BOS and reader not changed.*/
static int32 findTrackNextSyncSample(AVStreamPtr stream, uint32 direction,
                                     uint32* syncSampleNumber) {
    int32 err = PARSER_SUCCESS;

    MP4TrackReaderPtr reader;
    int32 forward;
    u32 CounterFrame = 1; /* Holds the current number of frame.                */
    u32 sync_sample = 0;  /* Holds the nearest I frame                         */

    MP4MediaInformationAtomPtr minf; /* Media Information.            */
    MP4SampleTableAtomPtr stbl;      /* Pointer to stream size table. */
    MP4SyncSampleAtomPtr stss;

    /* find the sync sample */
    reader = (MP4TrackReaderPtr)stream->reader;
    if (reader) {
        CounterFrame =
                reader->nextSampleNumber; /* the reader may not exist because of previous error */
    }

    if (!stream->isOneSampleGotAfterSkip) {
        *syncSampleNumber = CounterFrame; /* not output yet, nothing to do */
        MP4MSG("Trk %u, still stay on sample number %u. No need to skip.\n", stream->streamIndex,
               CounterFrame);
        return PARSER_SUCCESS;
    }

    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)(stream->mdia))->information;
    if (NULL == minf)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    if (NULL == stbl)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    stss = (MP4SyncSampleAtomPtr)stbl->SyncSample;
    forward = (FLAG_FORWARD == direction) ? TRUE : FALSE;

    if (MEDIA_VIDEO == stream->mediaType && stss) {
        /* for video, points to the nearest sync sample table atom before this sample*/
        if (0 == CounterFrame) {
            MP4MSG("Trk %u, invalid read position %d\n", stream->streamIndex, (int32)CounterFrame);
            BAILWITHERROR(PARSER_BOS)
        }

        if (forward)
            err = stss->nextSyncSample(stbl->SyncSample, CounterFrame, &sync_sample, forward);
        else {
            u32 dwStep = stream->videoInfo.scanType == VIDEO_SCAN_INTERLACED ? 3 : 2;
            if (CounterFrame > dwStep)
                CounterFrame -= dwStep;
            else
                BAILWITHERROR(PARSER_BOS)

            err = stss->nextSyncSample(stbl->SyncSample, CounterFrame, &sync_sample, forward);
        }
        if (PARSER_SUCCESS != err)
            goto bail;

        MP4MSG("video CounterFrame %u, matched sync_sample %u, err %d\n", CounterFrame, sync_sample,
               err);
        CounterFrame = sync_sample;
        // printf("video syc sample %ld matchedc\n",CounterFrame);
    } else /* every sample is sync */
    {
        if (forward) {
            if (CounterFrame > stream->numSamples) { /* GetNextSample() will increase the sample
                                                        counter, can be numSamples + 1 */
                // CounterFrame = stream->numSamples; /* not do this, can still setup reader,
                // otherwise reverse reading will skip the last one.*/
                err = PARSER_EOS;
            }
        } else {
            if (2 < CounterFrame)
                CounterFrame -= 2;
            else {
                CounterFrame = 1;
                err = PARSER_BOS;
            }
        }
    }

    *syncSampleNumber = CounterFrame;
    stream->chunkNumber = 0; /* next sync sample may not in the same chunk */

bail:

    if (PARSER_SUCCESS != err) {
        MP4MSG("trk %d, can not find next sync sample (direction %d), next sample number %u, err "
               "%d\n",
               stream->streamIndex, direction, CounterFrame, err);
    }

    return err;
}

/* For MP4 parser, it never holds a buffer after one output is finished */
static int32 resetTrackReadingStatus(FslParserHandle parserHandle, uint32 trackNum) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;
    MP4TrackReaderPtr reader;

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (stream->isBSAC) /* reset all BSAC tracks */
    {
        uint32 i;

        for (i = 0; i < self->numTracks; i++) {
            stream = self->streams[i];
            if (NULL == stream)
                BAILWITHERROR(PARSER_ERR_UNKNOWN)

            if (!stream->isBSAC)
                continue;
            // MP4MSG("reset trk %d reading status\n", i);

            stream->chunkNumber = 0;
            stream->sampleBytesLeft =
                    0; /* clear partial output status, will output a new sample after seeking */
            // stream->bos = FALSE;
            stream->isOneSampleGotAfterSkip = FALSE;
            if (stream->sampleBuffer) {
                MP4MSG("ERR, BSAC trk %d still hold buffer %p\n", stream->streamIndex,
                       stream->sampleBuffer);
                BAILWITHERROR(PARSER_ERR_UNKNOWN)
            }

            reader = (MP4TrackReaderPtr)stream->reader;
            SAFE_DESTROY(reader);
            stream->reader = NULL;
        }
        self->nextBSACTrackNum = self->firstBSACTrackNum;
    } else /* only reset this track */
    {
        stream->chunkNumber = 0;
        stream->sampleBytesLeft =
                0; /* clear partial output status, will output a new sample after seeking */
        // stream->bos = FALSE;
        stream->isOneSampleGotAfterSkip = FALSE;
        if (stream->sampleBuffer) {
            MP4MSG("ERR, trk %d still holds buffer %p\n", stream->streamIndex,
                   stream->sampleBuffer);
            BAILWITHERROR(PARSER_ERR_UNKNOWN)
        }

        reader = (MP4TrackReaderPtr)stream->reader;
        SAFE_DESTROY(reader);
        stream->reader = NULL;
    }

bail:
    return err;
}

static MP4Err getFileUserDataList(FslParserHandle parserHandle) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    MP4LinkedList userDataList;

    MP4PrivateMovieRecordPtr movie;
    MP4UserDataAtomPtr udta;
    MP4MetadataAtomPtr meta;
    MP4MetadataItemListAtomPtr ilst;
    MP4UserDataID3v2AtomPtr id32;
    MP4Handle handle = NULL;
    MP4LinkedList udta2 = NULL;
    MP4PSSHAtomPtr pssh = NULL;
    u32 num = 0;

    movie = (MP4PrivateMovieRecordPtr)self->movie;

    err = MP4MakeLinkedList(&userDataList);
    if (err)
        goto bail;
    self->userDataList = userDataList;

    /* 'udta' may exist in 'moov' or 'trak'. we only check the 'udta' in 'moov'
     * atom since we only want to get the file user data but not track metadata.
     * track metadata list is got in another function getStreamUserDataList () */
    udta = (MP4UserDataAtomPtr)((MP4MovieAtomPtr)movie->moovAtomPtr)->udta;

    err = MP4NewHandle(0, &handle); /*create an empty handle */
    if (MP4NoErr != err) {
        MP4MSG("Fail to get user data. err code is %d\n", err);
        return err;
    }

    if ((udta != NULL) && (MP4NoErr == udta->getItem(udta, handle, Udta3GppAlbumType, 1))) {
        u32 handleSize;
        uint8* buffer;
        u32 outIndex;
        MP4Handle handle2 = NULL;

        MP4GetHandleSize(handle, &handleSize);
        buffer = (uint8*)(*handle);

        if (buffer[handleSize - 1] != '\0') {
            char tmp[4];
            sprintf(tmp, "%u", buffer[handleSize - 1]);
            err = MP4NewHandle(0, &handle2);
            if (MP4NoErr == err) {
                (void)MP4SetHandleSize(handle2, 4);
                memcpy(*handle2, &tmp, 4);
                err = udta->addUserData(udta, handle2, UdtaCDTrackNumType, &outIndex);
                MP4DisposeHandle(handle2);
            }
        }
    }

    if (NULL != udta) {
        udta->appendItemList(udta, userDataList);
    }

    /* 'meta' may exist in file, 'moov', 'trak' or 'udta' atoms. we only find
     * the 'meta' in file, 'moov' and 'moov'->'udta' since we only get file user
     * data here. track metadata list is got in another function
     * getStreamUserDataList () */
    meta = (MP4MetadataAtomPtr)movie->meta;
    if (NULL != meta) {
        ilst = (MP4MetadataItemListAtomPtr)meta->itemList;
        if (NULL != ilst) {
            ilst->appendItemList(ilst, userDataList);
        }
    }

    meta = (MP4MetadataAtomPtr)((MP4MovieAtomPtr)movie->moovAtomPtr)->meta;
    if (NULL != meta) {
        ilst = (MP4MetadataItemListAtomPtr)meta->itemList;
        if (NULL != ilst) {
            ilst->appendItemList(ilst, userDataList);
        }
        id32 = (MP4UserDataID3v2AtomPtr)meta->id32;
        if (NULL != id32) {
            id32->appendItemList(id32, userDataList);
        }
    }

    if (NULL != udta) {
        meta = (MP4MetadataAtomPtr)udta->meta;
        if (NULL != meta) {
            ilst = (MP4MetadataItemListAtomPtr)meta->itemList;
            if (NULL != ilst) {
                ilst->appendItemList(ilst, userDataList);
            }
        }
    }

    udta2 = (MP4LinkedList)((MP4MovieAtomPtr)movie->moovAtomPtr)->udtaList;
    if (udta2 != NULL) {
        if (MP4NoErr != MP4GetListEntryCount(udta2, &num))
            num = 0;

        while (num > 0) {
            num--;
            if (MP4NoErr != MP4GetListEntry(udta2, num, (char**)&udta))
                continue;

            if (NULL != udta) {
                udta->appendItemList(udta, userDataList);

                meta = (MP4MetadataAtomPtr)udta->meta;
                if (NULL != meta) {
                    ilst = (MP4MetadataItemListAtomPtr)meta->itemList;
                    if (NULL != ilst) {
                        ilst->appendItemList(ilst, userDataList);
                    }
                }
            }
        }
    }
    pssh = (MP4PSSHAtomPtr)((MP4MovieAtomPtr)movie->moovAtomPtr)->pssh;
    if (pssh != NULL) {
        self->pssh = pssh->totalData;
        self->psshLen = pssh->totalSize;
    }

    MP4DisposeHandle(handle);

bail:
    return err;
}

static MP4Err getStreamUserDataList(FslParserHandle parserHandle) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    MP4LinkedList userDataList;
    MP4LinkedList trackList;
    u32 trackCount;

    MP4PrivateMovieRecordPtr movie;
    MP4TrackAtomPtr trak;
    MP4UserDataAtomPtr udta;
    MP4MetadataAtomPtr meta;
    MP4MetadataItemListAtomPtr ilst;

    movie = (MP4PrivateMovieRecordPtr)self->movie;
    trackList = ((MP4MovieAtomPtr)movie->moovAtomPtr)->trackList;

    err = MP4GetListEntryCount(trackList, &trackCount);
    if (err)
        goto bail;
    trackList->foundEntryNumber = 0;
    trackList->foundEntry = trackList->head;
    for (; (u32)trackList->foundEntryNumber < trackCount;) {
        err = MP4MakeLinkedList(&userDataList);
        if (err)
            goto bail;
        self->streams[trackList->foundEntryNumber]->userDataList = userDataList;

        trak = (MP4TrackAtomPtr)trackList->foundEntry->data;

        udta = (MP4UserDataAtomPtr)trak->udta;
        if (NULL != udta) {
            udta->appendItemList(udta, userDataList);
        }

        meta = (MP4MetadataAtomPtr)trak->meta;
        if (NULL != meta) {
            ilst = (MP4MetadataItemListAtomPtr)meta->itemList;
            if (NULL != ilst) {
                ilst->appendItemList(ilst, userDataList);
            }
        }

        if (NULL != udta) {
            meta = (MP4MetadataAtomPtr)udta->meta;
            if (NULL != meta) {
                ilst = (MP4MetadataItemListAtomPtr)meta->itemList;
                if (NULL != ilst) {
                    ilst->appendItemList(ilst, userDataList);
                }
            }
        }

        trackList->foundEntryNumber++;
        trackList->foundEntry = trackList->foundEntry->link;
    }

bail:
    return err;
}

static MP4Err getUserDataList(FslParserHandle parserHandle) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    sMP4ParserUdtaListPtr udtaList = &self->udtaList;
    sUserDataEntryPtr userDataEntry;

    MP4PrivateMovieRecordPtr movie;
    MP4UserDataAtomPtr udta;
    MP4Handle handle = NULL;

    movie = (MP4PrivateMovieRecordPtr)self->movie;
    udta = (MP4UserDataAtomPtr)((MP4MovieAtomPtr)movie->moovAtomPtr)->udta;
    if (NULL == udta) {
        MP4MSG("movie has no user data\n");
        return err;
    }

    err = MP4NewHandle(0, &handle); /*create an empty handle */
    if (MP4NoErr != err) {
        MP4MSG("Fail to get user data. err code is %d\n", err);
        return err;
    }

    /*title (file name) */
    userDataEntry = &udtaList->FileName;
    err = getUserDataField(udta, handle, UdtaTitleType, &userDataEntry->unicodeString,
                           &userDataEntry->stringLength);

    /* artist */
    userDataEntry = &udtaList->ArtistName;
    err = getUserDataField(udta, handle, UdtaArtistType, &userDataEntry->unicodeString,
                           &userDataEntry->stringLength);

    /* album */
    userDataEntry = &udtaList->AlbumName;
    err = getUserDataField(udta, handle, UdtaAlbumType, &userDataEntry->unicodeString,
                           &userDataEntry->stringLength);

    /* comment */
    userDataEntry = &udtaList->Comment;
    err = getUserDataField(udta, handle, UdtaCommentType, &userDataEntry->unicodeString,
                           &userDataEntry->stringLength);

    /* genre */
    userDataEntry = &udtaList->Genre;
    err = getUserDataField(udta, handle, UdtaGenreType, &userDataEntry->unicodeString,
                           &userDataEntry->stringLength);
    if (NULL == userDataEntry->unicodeString) {
        err = getUserDataField(udta, handle, UdtaGenreType2, &userDataEntry->unicodeString,
                               &userDataEntry->stringLength);
    }

    /* date/year */
    userDataEntry = &udtaList->Year;
    err = getUserDataField(udta, handle, UdtaDateType, &userDataEntry->unicodeString,
                           &userDataEntry->stringLength);

    /* Tool*/
    userDataEntry = &udtaList->Tool;
    err = getUserDataField(udta, handle, UdtaToolType, &userDataEntry->unicodeString,
                           &userDataEntry->stringLength);

    /* copyright */
    userDataEntry = &udtaList->CopyRight;
    err = getUserDataField(udta, handle, UdtaCopyrightType, &userDataEntry->unicodeString,
                           &userDataEntry->stringLength);

    if (handle) {
        MP4DisposeHandle(handle);
        handle = NULL;
    }

    err = PARSER_SUCCESS;
    // bail:
    return err;
}

static void freeUserDataList(FslParserHandle parserHandle) {
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    sMP4ParserUdtaListPtr udtaList = &self->udtaList;

    if (udtaList) {
        if (udtaList->FileName.unicodeString) {
            MP4LocalFree(udtaList->FileName.unicodeString);
            udtaList->FileName.unicodeString = NULL;
        }

        if (udtaList->ArtistName.unicodeString) {
            MP4LocalFree(udtaList->ArtistName.unicodeString);
            udtaList->ArtistName.unicodeString = NULL;
        }

        if (udtaList->AlbumName.unicodeString) {
            MP4LocalFree(udtaList->AlbumName.unicodeString);
            udtaList->AlbumName.unicodeString = NULL;
        }

        if (udtaList->Genre.unicodeString) {
            MP4LocalFree(udtaList->Genre.unicodeString);
            udtaList->Genre.unicodeString = NULL;
        }

        if (udtaList->Comment.unicodeString) {
            MP4LocalFree(udtaList->Comment.unicodeString);
            udtaList->Comment.unicodeString = NULL;
        }

        if (udtaList->Year.unicodeString) {
            MP4LocalFree(udtaList->Year.unicodeString);
            udtaList->Year.unicodeString = NULL;
        }

        if (udtaList->Tool.unicodeString) {
            MP4LocalFree(udtaList->Tool.unicodeString);
            udtaList->Tool.unicodeString = NULL;
        }

        if (udtaList->CopyRight.unicodeString) {
            MP4LocalFree(udtaList->CopyRight.unicodeString);
            udtaList->CopyRight.unicodeString = NULL;
        }
    }

    return;
}

static void freeMetaDataList(MP4LinkedList* list) {
    int32 err = MP4NoErr;
    MP4LinkedList userDataList;
    int32 recordCnt = 0, recordIdx = 0;
    int32 itemCnt = 0, itemIdx = 0;

    do {
        if (NULL == list)
            break;

        if (NULL == *list)
            break;

        userDataList = *list;

        if (NULL == userDataList)
            break;

        err = MP4GetListEntryCount(userDataList, (u32*)&recordCnt);
        if (MP4NoErr != err)
            break;

        for (recordIdx = 0; recordIdx < recordCnt; recordIdx++) {
            UserDataRecordPtr record;
            err = MP4GetListEntry(userDataList, recordIdx, (char**)&record);
            if (err)
                continue;

            if (record == NULL)
                continue;

            err = MP4GetListEntryCount(record->list, (u32*)&itemCnt);
            if (MP4NoErr != err)
                continue;

            for (itemIdx = 0; itemIdx < itemCnt; itemIdx++) {
                void* item;
                err = MP4GetListEntry(record->list, itemIdx, (char**)&item);
                if (err)
                    continue;

                if (item)
                    MP4LocalFree(item);
            }

            MP4DeleteLinkedList(record->list);
            MP4LocalFree((void*)record);
        }

        err = MP4DeleteLinkedList(userDataList);

        *list = NULL;
    } while (0);

    return;
}

static void asciiToUnicodeString(u16* des, u8* src, u32 strLength) {
    u32 i;

    for (i = 0; i < strLength; i++) {
        des[i] = (u16)src[i];
    }
    des[i] = '\0';
}

static MP4Err getUserDataField(MP4UserDataAtomPtr udta, MP4Handle handle, u32 fieldType,
                               u16** unicodeString, u32* stringLength) {
    u32 handleSize;
    u16* usrData;

    *unicodeString = NULL;
    *stringLength = 0;

    if (MP4NoErr == udta->getItem(udta, handle, fieldType, 1)) {
        MP4GetHandleSize(handle, &handleSize);
        if (handleSize) {
            usrData = MP4LocalCalloc(1, (handleSize + 1) * sizeof(u16));
            if (NULL == usrData) {
                return MP4NoMemoryErr;
            }

            asciiToUnicodeString(usrData, (u8*)(*handle), handleSize);
            *unicodeString = usrData;
            *stringLength = handleSize;
        }
    }

    return MP4NoErr;
}

/*
(1) MP4 is NOT always seekable.
For a track, if stss is not present, every sample is a random access point.
If stss is present but its entry count is zero, there is no random access points and the clip is not
seekable! We only check the primary track here!

(2) a track may be empty, all its index tables are empty, not seekable at all
*/
static int32 isMovieSeekable(FslParserHandle parserHandle) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    bool nonSeekableTrackFound = FALSE;
    uint32 i;
    AVStreamPtr stream;
    MP4MediaInformationAtomPtr minf; /* Media Information.            */
    MP4SampleTableAtomPtr stbl;      /* Pointer to stream size table. */
    MP4SyncSampleAtomPtr stss;

    if ((uint32)PARSER_INVALID_TRACK_NUMBER == self->primaryTrackNumber) {
        self->seekable = FALSE;
        goto bail;
    }

    if (self->flags & FILE_FLAG_NON_SEEKABLE) {
        self->seekable = FALSE;
        goto bail;
    }

    if (self->fragmented) {
        err = isSeekable(self->fragmented_reader, &self->seekable);
        goto bail;
    }
    MP4MSG("\nPrimary track number %d\n", self->primaryTrackNumber);
    for (i = 0; i < self->numTracks; i++) {
        stream = self->streams[i];
        if (NULL == stream)
            BAILWITHERROR(PARSER_ERR_UNKNOWN)

        if (stream->isEmpty) {
            MP4MSG("track %d is empty. Overlook it.\n", i);
            continue;
        }

        if (MEDIA_TYPE_UNKNOWN == stream->mediaType) {
            MP4MSG("track %d is UNKNOWN media\n", i);
            continue;
        }

        if (NULL == stream->mdia) /* still no valid media found for this track */
        {
            MP4MSG("track %d is UNKNOWN media (no valid media atom)\n", i);
            continue;
        }

        minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)(stream->mdia))->information;
        if (NULL == minf)
            BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

        stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
        if (NULL == stbl)
            BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

        stss = (MP4SyncSampleAtomPtr)stbl->SyncSample;
        // MP4MSG("stss %p, entry count %d\n", stss, stss->entryCount);
        if (((NULL == stss) || (stss && stss->entryCount)) && (FALSE == stream->isEmpty)) {
            stream->seekable = TRUE;
            MP4MSG("track %d is seekable\n", i);
        } else if (FALSE == stream->isEmpty) {
            nonSeekableTrackFound = TRUE;
            MP4MSG("track %d is NOT seekable\n", i);
        }
    }

    if (nonSeekableTrackFound) {
        MP4MSG("movie is NOT seekable\n");
        self->seekable = FALSE;
    } else {
        MP4MSG("movie is seekable\n");
        self->seekable = TRUE;
    }

bail:
    return err;
}

static int32 getDefaultReadMode(FslParserHandle parserHandle) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;
    uint32 i;

    if (self->isDeepInterleaving)
        self->readMode = PARSER_READ_MODE_TRACK_BASED;
    else
        self->readMode = PARSER_READ_MODE_FILE_BASED;

    MP4MSG("default read mode %s\n",
           self->readMode == PARSER_READ_MODE_FILE_BASED ? "FILE_BASED" : "TRACK_BASED");
    for (i = 0; i < self->numTracks; i++) {
        stream = self->streams[i];
        if (NULL == stream)
            BAILWITHERROR(PARSER_ERR_UNKNOWN)
        stream->readMode = self->readMode;
    }

bail:
    return err;
}

static int32 getNextTrackToRead(FslParserHandle parserHandle) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream = NULL;
    uint32 i;
    uint32 nextSampleNumber;
    uint64 sampleOffset;
    uint64 minSampleOffset = -1;
    uint32 curTrackNum = PARSER_INVALID_TRACK_NUMBER; /* debug variable */
    uint32 nextTrackNum = PARSER_INVALID_TRACK_NUMBER;

    // MP4MSG("select next trk to read...\n");

    if (PARSER_READ_MODE_FILE_BASED != self->readMode)
        BAILWITHERROR(PARSER_ERR_INVALID_READ_MODE)

    if (self->nextStream)
        curTrackNum = self->nextStream->streamIndex;

    for (i = 0; i < self->numTracks; i++) {
        stream = self->streams[i];
        if (NULL == stream)
            BAILWITHERROR(PARSER_ERR_UNKNOWN)

        if (!stream->enabled || stream->isEmpty) {
            MP4MSG("%s stream %d is not enabled %d or empty %d\n", __FUNCTION__, i, stream->enabled,
                   stream->isEmpty);
            continue;
        }

        if (self->fragmented) {
            err = getFragmentedTrackQueueOffset(self->fragmented_reader, i, &sampleOffset);
        } else {
            nextSampleNumber = stream->nextSampleNumber;
            // MP4MSG("trk %d, check offset\n", i);
            err = MP4GetSampleOffset(stream->mdia, nextSampleNumber, &sampleOffset);
        }
        if (PARSER_EOS == err) {
            MP4MSG("%s trk %d, EOS\n", __FUNCTION__, i);
            err = PARSER_SUCCESS; /* one track EOS is not file EOS */
            continue;
        }
        if (PARSER_SUCCESS != err) {
            MP4MSG("%s sampleOffset fail\n", __FUNCTION__);
            goto bail;
        }

        // MP4MSG("%s trk %d, next sample %d, offset %lld\n",__FUNCTION__, i, nextSampleNumber,
        // sampleOffset);

        if (minSampleOffset > sampleOffset) {
            minSampleOffset = sampleOffset;
            nextTrackNum = i;
            self->nextStream = stream;
        }
    }

    if ((uint32)PARSER_INVALID_TRACK_NUMBER == nextTrackNum) {
        MP4MSG("WARNING: all enabled tracks reach EOS (or no enabled tracks at all),\n");
        if (self->nextStream) {
            MP4MSG("stay on current trk %d\n", self->nextStream->streamIndex);
        } else {
            MP4MSG("current trk not decided yet\n");
        }

        // err = PARSER_EOS; /* still assume success, next reading will fail with EOS */
    } else if (nextTrackNum != curTrackNum) {
        // MP4MSG("switch from trk %d to trk %d\n\n", curTrackNum, self->nextStream->streamIndex);
    }

bail:
    return err;
}

static int32 getNextTrackToReadTrickMode(FslParserHandle parserHandle, uint32 direction) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream = NULL;
    uint32 i;
    uint32 nextSyncSampleNumber;
    uint64 sampleOffset;
    uint64 minSampleOffset = -1;
    int64 maxSampleOffset = -1;
    uint32 curTrackNum = PARSER_INVALID_TRACK_NUMBER; /* debug variable */
    uint32 nextTrackNum = PARSER_INVALID_TRACK_NUMBER;
    uint32 targetTrackSyncSampleNumber = 0;

    // MP4MSG("\nselect next trk to read...\n");

    if (PARSER_READ_MODE_FILE_BASED != self->readMode)
        BAILWITHERROR(PARSER_ERR_INVALID_READ_MODE)

    if (self->nextStream)
        curTrackNum = self->nextStream->streamIndex;

    for (i = 0; i < self->numTracks; i++) {
        stream = self->streams[i];
        if (NULL == stream)
            BAILWITHERROR(PARSER_ERR_UNKNOWN)

        if (!stream->enabled || stream->isEmpty)
            continue;

        if (MEDIA_VIDEO != stream->mediaType)
            continue;

        if (self->fragmented && MEDIA_VIDEO == stream->mediaType) {
            nextTrackNum = i;
            self->nextStream = stream;
            break;
        }
        err = findTrackNextSyncSample(stream, direction, &nextSyncSampleNumber);
        if (PARSER_SUCCESS != err) {
            // MP4MSG("trk %d, can not find next sync sample (direction %d)\n", stream->streamIndex,
            // direction);
            err = PARSER_SUCCESS; /* one track EOS is not file EOS */
            continue;
        }

        err = MP4GetSampleOffset(stream->mdia, nextSyncSampleNumber, &sampleOffset);

        if (PARSER_EOS == err) {
            MP4MSG("trk %d, EOS\n", i);
            err = PARSER_SUCCESS; /* one track EOS is not file EOS */
            continue;
        }
        if (PARSER_SUCCESS != err)
            goto bail;

        MP4MSG("trk %d, next sync sample %d, offset %lld\n", i, nextSyncSampleNumber, sampleOffset);

        if ((FLAG_FORWARD == direction) && (minSampleOffset > sampleOffset)) {
            minSampleOffset = sampleOffset;
            nextTrackNum = i;
            targetTrackSyncSampleNumber = nextSyncSampleNumber;
            self->nextStream = stream;
        } else if ((FLAG_BACKWARD == direction) && (maxSampleOffset < (int64)sampleOffset)) {
            maxSampleOffset = (int64)sampleOffset;
            nextTrackNum = i;
            targetTrackSyncSampleNumber = nextSyncSampleNumber;
            self->nextStream = stream;
        }
    }

    if ((uint32)PARSER_INVALID_TRACK_NUMBER == nextTrackNum) {
        // MP4MSG("WARNING: all enabled tracks reach EOS (or no enabled tracks at all), EOS! stay on
        // current trk %d\n", self->nextStream->streamIndex);
        err = PARSER_EOS; /* Must EOS here,  otherwise, next findTrackNextSyncSample() will locate
                             to this sync frame again! */
    } else {
        MP4TrackReaderPtr reader;

        stream = self->streams[nextTrackNum];
        /* Update the selected track reader. On EOS or ERROR, reader's sample number does not
         * change, still at the previous postion.*/
        reader = (MP4TrackReaderPtr)stream->reader;
        if (reader) {
            SAFE_DESTROY(reader);
            stream->reader = NULL;
        }
        MP4GetTrackReader(stream->trak, targetTrackSyncSampleNumber, &stream->reader);

        if (nextTrackNum != curTrackNum) {
            MP4MSG("switch from trk %d to trk %d\n", curTrackNum, self->nextStream->streamIndex);
        }
    }

bail:
    return err;
}

__attribute__((unused)) static int ts_compare(const void* arg1, const void* arg2) {
    uint64* ts1 = (uint64*)arg1;
    uint64* ts2 = (uint64*)arg2;

    return *ts1 > *ts2 ? 1 : -1;
}

__attribute__((unused)) static const char* showFlag(int flag) {
    switch (flag) {
        case SEEK_FLAG_NEAREST:
            return "NEAREST";
        case SEEK_FLAG_NO_LATER:
            return "NO_LATER";
        case SEEK_FLAG_NO_EARLIER:
            return "NO_EARLIER";
        case SEEK_FLAG_CLOSEST:
            return "CLOSEST";
        case SEEK_FLAG_FRAME_INDEX:
            return "FRAME_INDEX";
        default:
            return "invalid";
    }
}

static int32 seekTrack(FslParserHandle parserHandle, uint32 trackNum, uint32 flag, uint64* usTime) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    MP4MediaInformationAtomPtr minf; /* Media Information.            */
    MP4SampleTableAtomPtr stbl;      /* Pointer to stream size table. */
    MP4SyncSampleAtomPtr stss;
    MP4TimeToSampleAtomPtr stts;
    MP4CompositionOffsetAtomPtr ctts;

    uint64 usTargetTime = *usTime;
    uint64 targetTime = 0; /* seek time in terms of media time scale */

    // internal sample numbers are 1-based
    uint32 syncSample = 0;
    uint32 sampleNumber = 0;
    uint64 sampleTime = 0;
    uint64 usSampleTime = 0;

    uint64 InitialTicks = 0;
    uint64 InitialUs = 0;
    uint64 shiftStartTicks = 0;
    uint64 shiftStartUs = 0;

    if ((NULL == self) || (trackNum >= self->numTracks))
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    MP4MSG("seekTrack num %d, usTargetTime %lld, flag %s\n", trackNum, usTargetTime,
           showFlag(flag));

    err = resetTrackReadingStatus(parserHandle, trackNum);
    if (err) {
        goto bail;
    }

    stream = self->streams[trackNum];
    if (NULL == stream) {
        BAILWITHERROR(PARSER_ERR_UNKNOWN)
    }

    if (stream->isEmpty) {
        return err;
    }

    if (self->isImageFile) {
        MP4PrivateMovieRecordPtr moovp = (MP4PrivateMovieRecordPtr)self->movie;
        ItemTablePtr itemTable = moovp->itemTable;
        u32 displayIndex = trackNum;
        u32 imageIndex;
        int64 us = *(int64*)usTime;

        MP4MSG("seekTrack isImageFile\n");
        if (us >= 0)
            err = findNextImage(itemTable, displayIndex, &imageIndex);
        else
            err = findThumbnailImage(itemTable, displayIndex, &imageIndex);

        if (err)
            goto bail;

        // save for future use in reading data
        stream->currentReadingImageIndex = imageIndex;
        MP4MSG("seekTrack find imageIndex %d", imageIndex);

        BAILWITHERROR(PARSER_SUCCESS)
    }

    if ((stream->mediaType == MEDIA_VIDEO) && (stream->decoderType == VIDEO_H264) &&
        stream->pH264Parser) {
        ResetH264Parser(stream->pH264Parser);
    }

    stream->bPrevSampleNotFinished = FALSE;

    if (self->fragmented) {
        err = seekFragmentedTrack(self->fragmented_reader, trackNum, flag, usTime);
        return err;
    }
    /* if the track is not seekabe but not empty (stss has no entries), can only seek to the
     * beginning (0 us)*/
    if (*usTime && !stream->seekable)
        BAILWITHERROR(PARSER_ERR_NOT_SEEKABLE)

    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)(stream->mdia))->information;
    if (NULL == minf)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
    if (NULL == stbl)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    stts = (MP4TimeToSampleAtomPtr)stbl->TimeToSample;
    ctts = (MP4CompositionOffsetAtomPtr)stbl->CompositionOffset;
    stss = (MP4SyncSampleAtomPtr)stbl->SyncSample;

    MP4MSG("stream scale: %d\n", stream->timeScale);

    if (!stream->timeScale)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    InitialTicks = ((MP4TrackAtomPtr)(stream->trak))->elstInitialEmptyEditTicks;
    InitialUs = InitialTicks * 1000000 / stream->timeScale;
    shiftStartTicks = ((MP4TrackAtomPtr)(stream->trak))->elstShiftStartTicks;
    shiftStartUs = shiftStartTicks * 1000000 / stream->timeScale;

    if (0 == usTargetTime) {
        sampleNumber = syncSample = 1; /* first sample always matches, no need to search*/
    } else                             /* search the sample in time table */
    {
        if (SEEK_FLAG_FRAME_INDEX != flag) {
            if (usTargetTime > InitialUs)
                usTargetTime -= InitialUs;
            else
                usTargetTime = 0;

            usTargetTime += shiftStartUs;
            MP4MSG("shifted usTargetTime %lld, InitialUs %lld, shiftStartUs %lld", usTargetTime,
                   InitialUs, shiftStartUs);
        }

        if (stream->samplePtsTable == NULL)
            err = buildSamplePtsTable(stbl, &stream->samplePtsTable, stream->numSamples);
        if (err)
            goto bail;

        /*
         * search for corresponding sampleNumber according to input time or frame index
         */
        if (SEEK_FLAG_FRAME_INDEX == flag) {
            // input frame index is 0-based
            uint64 inputSampleNumber = usTargetTime;
            if (inputSampleNumber >= stream->numSamples)
                return PARSER_ERR_INVALID_PARAMETER;

            sampleNumber = stream->samplePtsTable[inputSampleNumber].index;
        } else {
            targetTime = usTargetTime * stream->timeScale / (1000 * 1000);
            MP4MSG("targetTime %lld\n", targetTime);
            findSampleAtTime(stream->samplePtsTable, stream->numSamples, targetTime, &sampleNumber,
                             flag);

            if (sampleNumber > stream->numSamples)
                sampleNumber--;
        }
        MP4MSG("sampleNumber %d\n", sampleNumber);

        /*
         *  use sampleNumber and flag to search for sync sample
         */
        syncSample = sampleNumber;  // default: non-USAC audio,non_stss video

        if ((MEDIA_VIDEO == stream->mediaType || AOT_USAC == stream->audioObjectType) && stss) {
            /* find the nearest sync sample */
            err = stss->isSyncSample(stbl->SyncSample, sampleNumber, &syncSample);
            if (err)
                return err;
            MP4MSG("nearest sync sample %d\n", syncSample);

            if ((SEEK_FLAG_NO_EARLIER == flag) && (syncSample < sampleNumber)) {
                err = stss->nextSyncSample((MP4AtomPtr)stss, sampleNumber + 1, &syncSample, TRUE);
                if (err) {
                    MP4MSG("no following sync sample can be found after sample %d\n", sampleNumber);
                    syncSample = stream->numSamples + 1;
                    err = setupTrackReader(self, stream, syncSample);
                    if (err)
                        return err;
                    BAILWITHERROR(PARSER_EOS)
                }
                MP4MSG("no_earlier revised next sync sample %d\n", syncSample);
            } else if ((SEEK_FLAG_NO_LATER == flag || SEEK_FLAG_FRAME_INDEX == flag ||
                        SEEK_FLAG_CLOSEST == flag) &&
                       (sampleNumber > 1) &&
                       (syncSample > sampleNumber) /*(usTimeSyncSample > usTargetTime)*/) {
                err = stss->nextSyncSample((MP4AtomPtr)stss, sampleNumber - 1, &syncSample, FALSE);
                if (err)
                    goto bail;
                MP4MSG("no_later revised prev sync sample %d\n", syncSample);
            }
        }
    }

    MP4MSG("syncSample %d\n", syncSample);

    /*
     * get time of sampleNumber, always return it and let user to determine using it or not
     */

    err = stts->getTimeForSampleNumber((MP4AtomPtr)stts, sampleNumber, (u64*)&sampleTime, NULL);
    if (err)
        goto bail;
    MP4MSG("stts sampleTime %lld\n", sampleTime);

    if (ctts) {
        s32 ptsOffset;
        err = ctts->getOffsetForSampleNumber((MP4AtomPtr)ctts, sampleNumber, &ptsOffset);
        if (err)
            goto bail;
        sampleTime += ptsOffset;
        MP4MSG("ctts ptsOffset %d, updated sampleTime %lld\n", ptsOffset, sampleTime);
    }

    if (InitialTicks > 0)
        sampleTime += InitialTicks;
    if (shiftStartTicks > 0) {
        if (sampleTime > shiftStartTicks)
            sampleTime -= shiftStartTicks;
        else
            sampleTime = 0;
    }
    MP4MSG("elst shifted SampleTime %lld\n", sampleTime);

    usSampleTime = sampleTime * (1000 * 1000) / stream->timeScale;
    MP4MSG("usSampleTime %lld\n", (long long)usSampleTime);

    *usTime = usSampleTime;

    MP4MSG("trk %d, seek result time %lld us (flag %s), matched sample number %d, sync sample %d\n",
           trackNum, usSampleTime, showFlag(flag), sampleNumber, syncSample);

    err = setupTrackReader(self, stream, syncSample);
    if (err)
        return err;

bail:
    return err;
}

static void checkInterleavingDepth(sMP4ParserObjectPtr self) {
    uint32 i;
    AVStreamPtr stream;
    AVStreamPtr firstVideoStream = NULL;
    AVStreamPtr firstAudioStream = NULL;

    MP4MediaInformationAtomPtr minf; /* Media Information.            */
    MP4SampleTableAtomPtr stbl;      /* Pointer to stream size table. */
    MP4ChunkOffsetAtomPtr stco;

    /* back up all track's inital sample offset!
       File-based reading mode is not suitable for large interleaving or non-interleaving file */
    for (i = 0; i < self->numTracks; i++) {
        stream = self->streams[i];
        if (stream->isEmpty)
            continue;

        minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)(stream->mdia))->information;
        if (NULL == minf)
            continue;

        stbl = (MP4SampleTableAtomPtr)minf->sampleTable;
        if (NULL == stbl)
            continue;

        stco = (MP4ChunkOffsetAtomPtr)stbl->ChunkOffset;
        if (NULL == stco)
            continue;

        stream->firstSampleFileOffset = (uint64)stco->firstChunkOffset;

        if ((MEDIA_VIDEO == stream->mediaType) && !firstVideoStream)
            firstVideoStream = stream;

        if ((MEDIA_AUDIO == stream->mediaType) && !firstAudioStream)
            firstAudioStream = stream;
    }

    if (firstVideoStream && firstAudioStream) {
        uint64 firstVideoFileOffset = firstVideoStream->firstSampleFileOffset;
        uint64 firstAudioFileOffset = firstAudioStream->firstSampleFileOffset;
        uint64 interleaveDepthInBytes;

        interleaveDepthInBytes = (firstVideoFileOffset >= firstAudioFileOffset)
                                         ? (firstVideoFileOffset - firstAudioFileOffset)
                                         : (firstAudioFileOffset - firstVideoFileOffset);

        MP4MSG("1st video offset %lld, 1st audio offset %lld -> AV initial offset %lld bytes\n",
               firstVideoFileOffset, firstAudioFileOffset, interleaveDepthInBytes);

        if (MAX_AV_INTERLEAVE_DEPTH < interleaveDepthInBytes) {
            MP4MSG("The AV interleaving is too deep! ");
            self->isDeepInterleaving = TRUE;
        } else if (!firstVideoFileOffset || !firstAudioFileOffset) {
            if (!firstVideoFileOffset)
                MP4MSG("Main video track is empty! ");
            else if (!firstAudioFileOffset) {
                MP4MSG("Main audio track is empty! ");
            }

            self->isDeepInterleaving = TRUE;
        }

        if (self->isDeepInterleaving) {
            MP4MSG("Can not support file-based reading mode!\n");
            self->readMode = PARSER_READ_MODE_TRACK_BASED;
        } else {
            MP4MSG("The AV interleaving is acceptiable!\n");
        }
    }
}

static int32 setupTrackReader(sMP4ParserObjectPtr self, AVStreamPtr stream, u32 CounterFrame) {
    int32 err = PARSER_SUCCESS;
    uint32 trackNum = stream->streamIndex;
    MP4TrackReaderPtr reader; /* Track Reader handle. */

    /* Get the track reader. A non-seekable track still need a reader, as long as it's not empty */
    err = MP4GetTrackReader(stream->trak, CounterFrame, &stream->reader);
    if (err)
        return err;
    reader = (MP4TrackReaderPtr)stream->reader;
    stream->nextSampleNumber = reader->nextSampleNumber;

    if (stream->isBSAC) /* align other BSAC tracks' reader to the same sample */
    {
        AVStreamPtr otherBSACStream;
        uint32 i;

        for (i = 0; i < self->numTracks; i++) {
            otherBSACStream = self->streams[i];
            if (NULL == otherBSACStream)
                BAILWITHERROR(PARSER_ERR_UNKNOWN)

            if ((i == trackNum) || !otherBSACStream->isBSAC)
                continue;

            // MP4MSG("align trk %d\n", i);
            err = MP4GetTrackReader(otherBSACStream->trak, CounterFrame, &otherBSACStream->reader);
            if (err)
                return err;
        }
    }

bail:
    return err;
}

EXTERN int32 MP4GetTrackExtTag(FslParserHandle parserHandle, uint32 trackNum,
                               TrackExtTagList** pList) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    *pList = stream->extTagList;

bail:
    return err;
}

EXTERN int32 MP4GetSampleInfo(FslParserHandle parserHandle, uint32 trackNum,
                              uint64* sampleFileOffset, uint64* lastSampleIndexInChunk) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    if (NULL == self)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    *sampleFileOffset = stream->curReadingSampleFileOffset;
    if (stream->lastSampleNumberInChunk == stream->nextSampleNumber - 1)
        *lastSampleIndexInChunk =
                stream->lastSampleNumberInChunk - 1;  // internal sample index is 1-based
    else
        *lastSampleIndexInChunk = 0;

    // MP4MSG("sample offset %lld, lastSampleInChunk %lld", *sampleFileOffset,
    // *lastSampleIndexInChunk);
bail:
    return err;
}

static int32 comparePtsEntry(const void* _a, const void* _b) {
    const sSamplePtsEntry* a = (const sSamplePtsEntry*)_a;
    const sSamplePtsEntry* b = (const sSamplePtsEntry*)_b;

    if (a->pts < b->pts) {
        return -1;
    } else if (a->pts > b->pts) {
        return 1;
    }

    return 0;
}

static int32 buildSamplePtsTable(MP4SampleTableAtomPtr stbl, sSamplePtsEntry** samplePtsTable,
                                 uint32 sampleCount) {
    int32 err = PARSER_SUCCESS;
    MP4TimeToSampleAtomPtr stts;
    MP4CompositionOffsetAtomPtr ctts;
    sSamplePtsEntry* ptsTable;
    uint32 i = 0;
    uint64 sampleDts = 0;
    int32 ptsOffset = 0;

    MP4MSG("buildSamplePtsTable sampleCount %d", sampleCount);

    stts = (MP4TimeToSampleAtomPtr)stbl->TimeToSample;
    ctts = (MP4CompositionOffsetAtomPtr)stbl->CompositionOffset;

    if (stts == NULL) {
        BAILWITHERROR(PARSER_ERR_UNKNOWN)
    }

    ptsTable = (sSamplePtsEntry*)MP4LocalCalloc(sampleCount, sizeof(sSamplePtsEntry));
    TESTMALLOC(ptsTable)

    memset(ptsTable, 0, sizeof(sSamplePtsEntry) * sampleCount);

    for (i = 0; i < sampleCount; ++i) {
        err = stts->getTimeForSampleNumber((MP4AtomPtr)stts, i + 1, (u64*)&sampleDts, NULL);
        if (err)
            goto bail;

        if (ctts) {
            err = ctts->getOffsetForSampleNumber((MP4AtomPtr)ctts, i + 1, &ptsOffset);
            if (err)
                goto bail;
        }
        ptsTable[i].pts = sampleDts + ptsOffset;
        ptsTable[i].index = i + 1;  // sample number is 1-based
        //    MP4MSG("index %d, dts %lld, offset %d, pts %lld",
        //        i, (long long)sampleDts, ptsOffset, (long long)ptsTable[i].pts);
    }

    qsort(ptsTable, sampleCount, sizeof(sSamplePtsEntry), comparePtsEntry);

    *samplePtsTable = ptsTable;

    /*
    for(int i=70; i< sampleCount && i<80; i++)
        MP4MSG("entry %d: index %d, pts %llu",i, ptsTable[i].index,
            (unsigned long long)ptsTable[i].pts);
    */

    MP4MSG("buildSamplePtsTable end");

bail:
    return err;
}

static void findSampleAtTime(sSamplePtsEntry* ptsTable, uint32 size, uint64 req_time,
                             uint32* sample_index, uint32 flag) {
    uint32 left = 0;
    uint32 right_plus_one = size;
    uint32 closestIndex = 0;

    if (ptsTable == NULL) {
        return;
    }

    while (left < right_plus_one) {
        uint32_t center = left + (right_plus_one - left) / 2;
        uint64_t centerTime = ptsTable[center].pts;
        // MP4MSG("findSample: left %d, center %d, req_time %lld, centerTime %lld",
        //               left, center,(long long)req_time, (long long)centerTime);
        if (req_time < centerTime) {
            right_plus_one = center;
        } else if (req_time > centerTime) {
            left = center + 1;
        } else {
            *sample_index = ptsTable[center].index;
            return;
        }
    }

    closestIndex = left;

    if (closestIndex == size) {
        closestIndex--;
    }

    if (SEEK_FLAG_CLOSEST == flag || SEEK_FLAG_NEAREST == flag) {
        if (closestIndex >= 1 && ABS(ptsTable[closestIndex].pts, req_time) >
                                         ABS(ptsTable[closestIndex - 1].pts, req_time))
            closestIndex--;
    }

    *sample_index = ptsTable[closestIndex].index;

    /*
    MP4MSG("findSampleAtTime: req_time: %lld, prev:%d/%lld, closest:%d/%lld, next:%d/%lld",
        (long long)req_time,
        ptsTable[closestIndex-1].index, ptsTable[closestIndex-1].pts,
        ptsTable[closestIndex].index, ptsTable[closestIndex].pts,
        ptsTable[closestIndex+1].index, ptsTable[closestIndex+1].pts);
        */
    return;
}

int32 MP4GetAudioPresentationNum(FslParserHandle parserHandle, uint32 trackNum,
                                 int32* presentationNum) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_AUDIO != stream->mediaType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    *presentationNum = stream->audioInfo.numAudioPresentation;

bail:
    return err;
}

int32 MP4GetAudioPresentationInfo(FslParserHandle parserHandle, uint32 trackNum,
                                  int32 presentationNum, int32* presentationId, char** language,
                                  uint32* masteringIndication, uint32* audioDescriptionAvailable,
                                  uint32* spokenSubtitlesAvailable,
                                  uint32* dialogueEnhancementAvailable) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;
    audioPresentation* ptr;

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_AUDIO != stream->mediaType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    if (presentationNum >= stream->audioInfo.numAudioPresentation)
        BAILWITHERROR(PARSER_ERR_INVALID_PARAMETER)

    ptr = stream->audioInfo.audioPresentations + presentationNum;
    *presentationId = ptr->presentationId;
    *language = ptr->language;
    *masteringIndication = ptr->masteringIndication;
    *audioDescriptionAvailable = ptr->audioDescriptionAvailable;
    *spokenSubtitlesAvailable = ptr->spokenSubtitlesAvailable;
    *dialogueEnhancementAvailable = ptr->dialogueEnhancementAvailable;

bail:
    return err;
}

int32 MP4FlushTrack(FslParserHandle parserHandle, uint32 trackNum) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    MP4MSG("MP4FlushTrack\n");

    if (self->fragmented) {
        err = flushFragmentedTrack(self->fragmented_reader, trackNum);
        return err;
    }

    return err;
}

EXTERN int32 MP4GetImageInfo(FslParserHandle parserHandle, uint32 trackNum, ImageInfo** ppInfo) {
    int32 err = PARSER_SUCCESS;
    sMP4ParserObjectPtr self = (sMP4ParserObjectPtr)parserHandle;
    AVStreamPtr stream;
    MP4MSG("MP4GetImageInfo track %d\n", trackNum);

    stream = self->streams[trackNum];
    if (NULL == stream)
        BAILWITHERROR(PARSER_ERR_UNKNOWN)

    if (MEDIA_IMAGE != stream->mediaType)
        BAILWITHERROR(PARSER_ERR_INVALID_MEDIA)

    *ppInfo = &stream->imageInfo;

bail:
    return err;
}

static MP4Err getScanTypeFromSample(sMP4ParserObjectPtr self) {
    int32 err = PARSER_SUCCESS;
    uint32 i, numTracks;
    AVStreamPtr stream;

    err = MP4GetMovieTrackCount(self->movie, &numTracks);
    if (err)
        goto bail;

    for (i = 0; i < numTracks; i++) {
        stream = self->streams[i];

        if (NULL == stream || stream->isEmpty)
            continue;

        if ((stream->mediaType == MEDIA_VIDEO) && (stream->decoderType == VIDEO_H264) &&
            stream->pH264Parser) {
            uint32 size;
            MP4MSG("stream %d is H264\n", i);
            err = MP4GetHandleSize(stream->decoderSpecificInfoH, &size);
            if (err)
                goto bail;

            MP4MSG("decoderSpecificInfo size %d\n", size);
            if (size == 0) {
                uint32 dataSize;
                uint64 usStartTime;
                uint64 usDuration;
                uint32 sampleFlags;
                stream->internalSample =
                        TRUE;  // allocate buffer with MP4LocalMalloc instead of memops
                err = getTrackNextSample(self, i, &dataSize, &usStartTime, &usDuration,
                                         &sampleFlags);
                if (PARSER_SUCCESS == err) {
                    H264HeaderInfo h264Header;
                    memset(&h264Header, 0, sizeof(H264HeaderInfo));
                    if (ParseH264CodecDataFrame(stream->pH264Parser, stream->sampleBuffer, dataSize,
                                                &h264Header) == 0) {
                        stream->videoInfo.scanType = h264Header.scanType;
                        MP4MSG("scantype from first sample: %d\n", stream->videoInfo.scanType);
                    }
                    MP4LocalFree(stream->sampleBuffer);
                    stream->sampleBuffer = NULL;
                } else {
                    MP4MSG("read first sample fail %d\n", err);
                }
                setupTrackReader(self, stream, 1);
                stream->internalSample = FALSE;
            }
        }
    }

bail:
    return err;
}
