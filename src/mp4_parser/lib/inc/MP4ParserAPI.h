
/*
 ***********************************************************************
 * Copyright (c) 2009-2016, Freescale Semiconductor, Inc.
 * Copyright 2017-2018, 2021, 2024, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

/**
 * @file MP4Parser.h
 * @MP4 core parser API header file
 */

#ifndef FSL_MP4_PARSER_API_HEADER_INCLUDED
#define FSL_MP4_PARSER_API_HEADER_INCLUDED

#include "fsl_types.h"
#include "file_stream.h"
#include "fsl_media_types.h"
#include "fsl_parser.h"


/***************************************************************************************
 *                  MP4 parser specific error codes
 *
 *  Note:
 *  (a)For parsers' common error codes and argument types, please refer to "fsl_parser.h".
 *  (b)API functions do not use enumeration types but int32 as return values.
 *     It's because different compilers can treat enum types as different basic data types
 *     (eg.int or unsigned char).
 ***************************************************************************************/

enum {
    MP4HasRootOD = 102,

    MP4EOF = PARSER_EOS,
    MP4NoErr = PARSER_SUCCESS,
    MP4FileNotFoundErr = PARSER_FILE_OPEN_ERROR,
    MP4BadParamErr = PARSER_ERR_INVALID_PARAMETER,
    MP4NoMemoryErr = PARSER_INSUFFICIENT_MEMORY,
    MP4IOErr = PARSER_READ_ERROR,
    MP4InvalidMediaErr = PARSER_ERR_INVALID_MEDIA,
    MP4NotImplementedErr = PARSER_NOT_IMPLEMENTED,

    MP4NoLargeAtomSupportErr = -105,
    MP4BadDataErr = -106,
    MP4VersionNotSupportedErr = -107,
    MP4DataEntryTypeNotSupportedErr = -108,
    MP4NoQTAtomErr = -120,
    MP4_ERR_WRONG_SAMPLE_SIZE = -200,
    MP4_ERR_EMTRY_TRACK =
            -210 /* all stbl child atoms are empty (entry count is 0), can not seek or read */
};

/***************************************************************************************
 *                  Data Structures & Constants
 ***************************************************************************************/
#define MAX_MP4_TRACKS 64

/*
 * Handle of the MP4 core parser created.
 */
// typedef void * MP4ParserHandle;

/***************************************************************************************
 *                  API Funtions
 * For calling sequence, please refer to the end of this file.
 ***************************************************************************************/
#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN
#endif

/*
 * function to get the MP4 core parser version.
 *
 * @return Version string.
 */
EXTERN const char* MP4ParserVersionInfo();

/**
 * Function to create the MP4 core parser.
 * It will parse the MP4 file header and probe whether the movie can be handled by this MP4 parser.
 *
 * @param fileName [in] File name or URL.
 *                      Actually, it can be an abstracted file object from the application!
 *                      The core parser just take it as the file name in the file I/O callback.
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
                             FslParserHandle* parserHandle);
/**
 * Function to delete the MP4 core parser.
 *
 * @param parserHandle Handle of the MP4 core parser.
 * @return
 */
EXTERN int32 MP4DeleteParser(FslParserHandle parserHandle);

/**
 * Function to create the MP4 core parser.
 * It will parse the MP4 file header and probe whether the movie can be handled by this MP4 parser.
 *
 * @param flags [uint32] flags for parser creation, now 3 flags are used.
 *
 *  a. READ_FALG_H264_ALIGNMENT_AU
 *      output original codec data and frame buffers. No convert function will be used for h264
 * stream. this will have same behaviors with opensource demux.
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
EXTERN int32 MP4CreateParser2(uint32 flags, FslFileStream* streamOps, ParserMemoryOps* memOps,
                              ParserOutputBufferOps* outputBufferOps, void* context,
                              FslParserHandle* parserHandle);
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
EXTERN int32 MP4IsSeekable(FslParserHandle parserHandle, bool* seekable);

/**
 * Function to tell how many tracks in the movie.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param numTracks [out] Number of tracks.
 * @return
 */
EXTERN int32 MP4GetNumTracks(FslParserHandle parserHandle, uint32* numTracks);

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
                            uint32* stringLength);

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
                            uint32* userDataLength);

/**
 * Function to tell the movie duration.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param usDuration [out] Duration in us.
 * @return
 */
EXTERN int32 MP4GetTheMovieDuration(FslParserHandle parserHandle, uint64* usDuration);

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
                                    uint64* usDuration);

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
                             uint32* decoderType, uint32* decoderSubtype);

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
                                       uint32* size);

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
EXTERN int32 MP4GetMaxSampleSize(FslParserHandle parserHandle, uint32 trackNum, uint32* size);

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
EXTERN int32 MP4GetLanguage(FslParserHandle parserHandle, uint32 trackNum, uint8* threeCharCode);

/**
 * Function to tell the average bitrate of a track.
 * If the average bitrate is not available in the file header, 0 is given.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based.
 * @param bitrate [out] Average bitrate, in bits per second.
 * @return
 */
EXTERN int32 MP4GetBitRate(FslParserHandle parserHandle, uint32 trackNum, uint32* bitrate);

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
                                  uint64* usDuration);

/**
 * Function to tell the width in pixels of a video track.
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a video track.
 * @param width [out] Width in pixels.
 * @return
 */
EXTERN int32 MP4GetVideoFrameWidth(FslParserHandle parserHandle, uint32 trackNum, uint32* width);

/**
 * Function to tell the height in pixels of a video track.
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a video track.
 * @param height [out] Height in pixels.
 * @return
 */
EXTERN int32 MP4GetVideoFrameHeight(FslParserHandle parserHandle, uint32 trackNum, uint32* height);

/**
 * Function to tell the frame rate of a video track. *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a video track.
 * @param rate [out] Numerator of the frame rate. The frame rate is equals to rate/scale.
 * @param scale [out] Denominator of the frame rate.
 * @return
 */
EXTERN int32 MP4GetVideoFrameRate(FslParserHandle parserHandle, uint32 trackNum, uint32* rate,
                                  uint32* scale);

/**
 * Function to tell the frame rotation of a video track.
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a video track.
 * @param height [out] rotation degree (0,90,180,270).
 * @return
 */
EXTERN int32 MP4GetVideoFrameRotation(FslParserHandle parserHandle, uint32 trackNum,
                                      uint32* rotation);

/**
 * Function to tell the color infomation of a video track
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a video track.
 * @param primaries [out] colour primaties
 * @param transfer [out] transfer characteristics
 * @param coeff [out] matrix coefficients
 * @param fullRange [out] full range flag
 * @return
 */
EXTERN int32 MP4GetVideoColorInfo(FslParserHandle parserHandle, uint32 trackNum, int32* primaries,
                                  int32* transfer, int32* coeff, int32* fullRange);

/**
 * Function to tell the display width in pixels of a video track.
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a video track.
 * @param width [out] Width in pixels.
 * @return
 */
EXTERN int32 MP4GetVideoDisplayWidth(FslParserHandle parserHandle, uint32 trackNum, uint32* width);

/**
 * Function to tell the display height in pixels of a video track.
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a video track.
 * @param height [out] Height in pixels.
 * @return
 */
EXTERN int32 MP4GetVideoDisplayHeight(FslParserHandle parserHandle, uint32 trackNum,
                                      uint32* height);

/**
 * Function to tell frame count of a video track.
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a video track.
 * @param count [out] Frame count.
 * @return
 */
EXTERN int32 MP4GetVideoFrameCount(FslParserHandle parserHandle, uint32 trackNum, uint32* count);

/**
 * Function to tell thumbnail time of a video track.
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a video track.
 * @param outTs [out] thumbnail time.
 * @return
 */
EXTERN int32 MP4GetVideoThumbnailTime(FslParserHandle parserHandle, uint32 trackNum, uint64* outTs);

/**
 * Function to tell scan type of a video track.
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a video track.
 * @param scanType [out] scan type.
 * @return
 */
EXTERN int32 MP4GetVideoScanType(FslParserHandle parserHandle, uint32 trackNum, uint32* scanType);

/**
 * Function to tell how many channels in an audio track.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to an audio track.
 * @param numchannels [out] Number of the channels. 1 mono, 2 stereo, or more for multiple channels.
 * @return
 */
EXTERN int32 MP4GetAudioNumChannels(FslParserHandle parserHandle, uint32 trackNum,
                                    uint32* numChannels);

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
                                   uint32* sampleRate);

/**
 * Function to tell the bits per sample for an audio track.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to an audio track.
 * @param bitsPerSample [out] Bits per PCM sample.
 * @return
 */
EXTERN int32 MP4GetAudioBitsPerSample(FslParserHandle parserHandle, uint32 trackNum,
                                      uint32* bitsPerSample);

/**
 * Function to tell the block align for an adpcm audio track.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to an audio track.
 * @param blockAlign [out] block align for adpcm audio
 * @return
 */
EXTERN int32 MP4GetAudioBlockAlign(FslParserHandle parserHandle, uint32 trackNum,
                                   uint32* blockAlign);

/**
 * Function to get information of MPEG-H audio track.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to an audio track.
 * @param profileLevelIndication [out]
 * @param referenceChannelLayout [out]
 * @param compatibleSetsSize [out]
 * @param compatibleSets [out]
 * @return
 */
EXTERN int32 MP4GetAudioMpeghInfo(FslParserHandle parserHandle, uint32 trackNum,
                                  uint32* profileLevelIndication, uint32* referenceChannelLayout,
                                  uint32* compatibleSetsSize, uint8** compatibleSets);

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
EXTERN int32 MP4GetTextTrackWidth(FslParserHandle parserHandle, uint32 trackNum, uint32* width);

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
EXTERN int32 MP4GetTextTrackHeight(FslParserHandle parserHandle, uint32 trackNum, uint32* height);

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
                                 uint32* data_len);
/**
 * Function to set the mode to read media samples, file-based or track-based.
 *  a. File-based sample reading.
 *      The reading order is same as that of track interleaving in the file.
 *      Mainly for streaming application.
 *
 *  b. Track-based sample reading.
 *      Each track can be read and seeked independently from each other.
 *
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
EXTERN int32 MP4SetReadMode(FslParserHandle parserHandle, uint32 readMode);

/**
 * Function to get the mode to read media samples, file-based or track-based. *
 * And the parser has a default read mode.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to a text track.
 * @param readMode [out] Current Sample reading mode.
 *
 *                      READ_MODE_FILE_BASED
 *                      Default mode.Linear sample reading. The reading order is same as that of
 *                      track interleaving in the file.
 *
 *                      READ_MODE_TRACK_BASED
 *                      Track-based sample reading. Each track can be read independently from each
 * other.
 * @return
 */
EXTERN int32 MP4GetReadMode(FslParserHandle parserHandle, uint32* readMode);

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
EXTERN int32 MP4EnableTrack(FslParserHandle parserHandle, uint32 trackNum, bool enable);

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
                              uint64* usDuration, uint32* sampleFlags);

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
                                  uint64* usStartTime, uint64* usDuration, uint32* sampleFlags);

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
EXTERN int32 MP4Seek(FslParserHandle parserHandle, uint32 trackNum, uint64* usTime, uint32 flag);

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
                                  uint64* usStartTime, uint64* usDuration, uint32* flags);

EXTERN int32 MP4GetFileNextSyncSample(FslParserHandle parserHandle, uint32 direction,
                                      uint32* trackNum, uint8** sampleBuffer, void** bufferContext,
                                      uint32* dataSize, uint64* usStartTime, uint64* usDuration,
                                      uint32* sampleFlags);

EXTERN int32 MP4GetTrackExtTag(FslParserHandle parserHandle, uint32 trackNum,
                               TrackExtTagList** pList);
/**
 * Function to read the drm sample crypto info from a track, only for track-based reading mode.
 *
 * In this function, the parser may use callback to request buffers to output the sample.
 * And it will tell which buffer is output on returning. But if the buffer is not available or too
 small,
 * this function will fail.
 *
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track to read, 0-based.
 * @param iv [out]   Buffer to hold the sample data.
 * @param ivContext [out] Buffer context from application, got on requesting the buffer.
 *
 * @param ivSize [out]    If a sample or part of sample is output successfully (return value is
 PARSER_SUCCESS ),
 *                          it's the size of the data actually got.
 *
 * @param clearBuffer [out]   Buffer to hold the sample data.
 * @param clearContext [out] Buffer context from application, got on requesting the buffer.
 *
 * @param clearSize [out]    If a sample or part of sample is output successfully (return value is
 PARSER_SUCCESS ),
 *                          it's the size of the data actually got.

 * @param encryptedBuffer [out]   Buffer to hold the sample data.
 * @param encryptedContext [out] Buffer context from application, got on requesting the buffer.
 *
 * @param encryptedSize [out]    If a sample or part of sample is output successfully (return value
 is PARSER_SUCCESS ),
 *                          it's the size of the data actually got.

 * @return  PARSER_SUCCESS     An entire sample or part of it is got successfully.
 */
EXTERN int32 MP4GetSampleCryptoInfo(FslParserHandle parserHandle, uint32 trackNum, uint8** iv,
                                    uint32* ivSize, uint8** clearBuffer, uint32* clearSize,
                                    uint8** encryptedBuffer, uint32* encryptedSize);
/**
 * Function to get the info of media samples just read.
 *
 * @param parserHandle [in] Handle of the MP4 core parser.
 * @param trackNum [in] Number of the track to read, 0-based.
 * @param sampleFileOffset [out] file offset of just read sample.
 * @param lastSampleIndexInChunk [out] if current read sample is the last sample in chunk,
 *                                     this parameter is assigned with sample index 0-based.
 *
 * @return  PARSER_SUCCESS     An entire sample or part of it is got successfully.
 */
EXTERN int32 MP4GetSampleInfo(FslParserHandle parserHandle, uint32 trackNum,
                              uint64* sampleFileOffset, uint64* lastSampleIndexInChunk);

/**
 * Function to tell audio presentation num for an audio track.
 *
 * @param parserHandle [in] Handle of the Mp4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to an audio track.
 * @param presentationNum [out] audio presentation num.
 * @return
 */
EXTERN int32 MP4GetAudioPresentationNum(FslParserHandle parserHandle, uint32 trackNum,
                                        int32* presentationNum);

/**
 * Function to tell audio presentation info for an audio track.
 *
 * @param parserHandle [in] Handle of the Mp4 core parser.
 * @param trackNum [in] Number of the track, 0-based. It must point to an audio track.
 * @param presentationNum [in] audio presentation num.
 * @return
 */
EXTERN int32 MP4GetAudioPresentationInfo(FslParserHandle parserHandle, uint32 trackNum,
                                         int32 presentationNum, int32* presentationId,
                                         char** language, uint32* masteringIndication,
                                         uint32* audioDescriptionAvailable,
                                         uint32* spokenSubtitlesAvailable,
                                         uint32* dialogueEnhancementAvailable);

/**
 * Function to flush track. This function will reset output buffers and stream info
 *
 * @param parserHandle [in] Handle of the Mp4 core parser.
 * @param trackNum [in] Number of the track, 0-based.
 * @return  PARSER_SUCCESS Track is flushed successfully.
 */
EXTERN int32 MP4FlushTrack(FslParserHandle parserHandle, uint32 trackNum);

EXTERN int32 MP4GetImageInfo(FslParserHandle parserHandle, uint32 trackNum, ImageInfo** ppInfo);

#endif  // FSL_MP4_PARSER_API_HEADER_INCLUDED
