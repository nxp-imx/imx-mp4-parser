/************************************************************************
 * Copyright (c) 2005-2012, Freescale Semiconductor, Inc.
 * Copyright 2024, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ************************************************************************/

#include <string.h>
#include "MP4ParserAPI.h"
#include "MP4Impl.h"
#include "MP4ParserPriv.h"

#define MAX_CHANNELS 64
#define ER_OBJECT_START 17

typedef struct {
    /* bit input */
    unsigned char* rdbfr;
    unsigned char* rdptr;
    int incnt;
    int bitcnt;
} BitReader;

typedef struct program_config {
    unsigned char element_instance_tag;
    unsigned char object_type;
    unsigned char sf_index;
    unsigned char numFrontChannelElements;
    unsigned char numSideChannelElements;
    unsigned char numBackChannelElements;
    unsigned char numLfeChannelElements;
    unsigned char numAssocDataElements;
    unsigned char numValidCCElements;
    unsigned char monoMixdownPresent;
    unsigned char monoMixdownElementNo;
    unsigned char stereoMixdownPresent;
    unsigned char stereoMixdownElementNo;
    unsigned char matrixMixdownIdxPresent;
    unsigned char pseudoSurroundEnable;
    unsigned char matrixMixdownIdx;
    unsigned char frontElementIsCpe[16];
    unsigned char frontElementTagSelect[16];
    unsigned char sideElementIsCpe[16];
    unsigned char sideElementTagSelect[16];
    unsigned char backElementIsCpe[16];
    unsigned char backElementTagSelect[16];
    unsigned char lfeElementTagSelect[16];
    unsigned char assocDataElementTagSelect[16];
    unsigned char ccElementIsIndSW[16];
    unsigned char validCCElementTagSelect[16];

    unsigned char channels;

    unsigned char commentFieldBytes;
    unsigned char commentFieldData[257];

    /* extra added values */
    unsigned char numFrontChannels;
    unsigned char numSideChannels;
    unsigned char numBackChannels;
    unsigned char numLfeChannels;
    unsigned char sceChannel[16];
    unsigned char cpeChannel[16];
} program_config;

static void initbuffer(unsigned char* buffer, BitReader* bitRd) {
    bitRd->rdptr = buffer;
    bitRd->rdbfr = buffer;
    bitRd->incnt = 0;
    bitRd->bitcnt = 0;
}

static unsigned int showbits(BitReader* bitRd, int n) {
    unsigned char* p = bitRd->rdptr;
    unsigned int a = 0;
    unsigned int c = bitRd->incnt;
    /* load in big-Endian order */
    a = (unsigned int)(p[0] << 24) + (p[1] << 16) + (p[2] << 8) + (p[3]);
    return (a << c) >> (32 - n);
}

static void flushbits(BitReader* bitRd, int n) {
    bitRd->incnt += n;
    bitRd->bitcnt += n;
    bitRd->rdptr += (bitRd->incnt >> 3);
    bitRd->incnt &= 0x07;
}

static unsigned int getbits(BitReader* bitRd, int n) {
    unsigned int val = showbits(bitRd, n);
    flushbits(bitRd, n);
    return val;
}

static unsigned int getbit1(BitReader* bitRd) {
    return getbits(bitRd, 1);
}

static void byte_align(BitReader* bitRd) {
    if (bitRd->incnt) {
        bitRd->bitcnt += (8 - (bitRd->incnt & 0x07));
        bitRd->incnt = 0;
        bitRd->rdptr += 1;
    }
    return;
}

/* Returns the sample rate based on the sample rate index */
static unsigned int get_sample_rate(const unsigned char sr_index) {
    static const unsigned int sample_rates[] = {96000, 88200, 64000, 48000, 44100, 32000,
                                                24000, 22050, 16000, 12000, 11025, 8000};

    if (sr_index < 12) {
        return sample_rates[sr_index];
    }

    return 0;
}

/* Table 4.4.2 */
/* An MPEG-4 Audio decoder is only required to follow the Program
   Configuration Element in GASpecificConfig(). The decoder shall ignore
   any Program Configuration Elements that may occur in raw data blocks.
   PCEs transmitted in raw data blocks cannot be used to convey decoder
   configuration information.
*/
static MP4Err program_config_element(program_config* pce, BitReader* pBitRd) {
    unsigned char i = 0;
    if (NULL == pce || NULL == pBitRd) {
        return MP4BadParamErr;
    }
    memset(pce, 0, sizeof(program_config));
    pce->channels = 0;
    pce->element_instance_tag = getbits(pBitRd, 4);
    pce->object_type = getbits(pBitRd, 2);
    pce->sf_index = getbits(pBitRd, 4);
    pce->numFrontChannelElements = getbits(pBitRd, 4);
    pce->numSideChannelElements = getbits(pBitRd, 4);
    pce->numBackChannelElements = getbits(pBitRd, 4);

    pce->numLfeChannelElements = (unsigned char)getbits(pBitRd, 2);
    pce->numAssocDataElements = (unsigned char)getbits(pBitRd, 3);
    pce->numValidCCElements = (unsigned char)getbits(pBitRd, 4);

    pce->monoMixdownPresent = getbit1(pBitRd);
    if (pce->monoMixdownPresent == 1) {
        pce->monoMixdownElementNo = (unsigned char)getbits(pBitRd, 4);
    }

    pce->stereoMixdownPresent = getbit1(pBitRd);
    if (pce->stereoMixdownPresent == 1) {
        pce->stereoMixdownElementNo = (unsigned char)getbits(pBitRd, 4);
    }

    pce->matrixMixdownIdxPresent = getbit1(pBitRd);
    if (pce->matrixMixdownIdxPresent == 1) {
        pce->matrixMixdownIdx = (unsigned char)getbits(pBitRd, 2);
        pce->pseudoSurroundEnable = getbit1(pBitRd);
    }

    for (i = 0; i < pce->numFrontChannelElements; i++) {
        pce->frontElementIsCpe[i] = getbit1(pBitRd);
        pce->frontElementTagSelect[i] = (unsigned char)getbits(pBitRd, 4);

        if (pce->frontElementIsCpe[i] & 1) {
            pce->cpeChannel[pce->frontElementTagSelect[i]] = pce->channels;
            pce->numFrontChannels += 2;
            pce->channels += 2;
        } else {
            pce->sceChannel[pce->frontElementTagSelect[i]] = pce->channels;
            pce->numFrontChannels++;
            pce->channels++;
        }
    }

    for (i = 0; i < pce->numSideChannelElements; i++) {
        pce->sideElementIsCpe[i] = getbit1(pBitRd);
        pce->sideElementTagSelect[i] = (unsigned char)getbits(pBitRd, 4);

        if (pce->sideElementIsCpe[i] & 1) {
            pce->cpeChannel[pce->sideElementTagSelect[i]] = pce->channels;
            pce->numSideChannels += 2;
            pce->channels += 2;
        } else {
            pce->sceChannel[pce->sideElementTagSelect[i]] = pce->channels;
            pce->numSideChannels++;
            pce->channels++;
        }
    }

    for (i = 0; i < pce->numBackChannelElements; i++) {
        pce->backElementIsCpe[i] = getbit1(pBitRd);
        pce->backElementTagSelect[i] = (unsigned char)getbits(pBitRd, 4);

        if (pce->backElementIsCpe[i] & 1) {
            pce->cpeChannel[pce->backElementTagSelect[i]] = pce->channels;
            pce->channels += 2;
            pce->numBackChannels += 2;
        } else {
            pce->sceChannel[pce->backElementTagSelect[i]] = pce->channels;
            pce->numBackChannels++;
            pce->channels++;
        }
    }

    for (i = 0; i < pce->numLfeChannelElements; i++) {
        pce->lfeElementTagSelect[i] = (unsigned char)getbits(pBitRd, 4);

        pce->sceChannel[pce->lfeElementTagSelect[i]] = pce->channels;
        pce->numLfeChannels++;
        pce->channels++;
    }

    for (i = 0; i < pce->numAssocDataElements; i++)
        pce->assocDataElementTagSelect[i] = (unsigned char)getbits(pBitRd, 4);

    for (i = 0; i < pce->numValidCCElements; i++) {
        pce->ccElementIsIndSW[i] = getbit1(pBitRd);
        pce->validCCElementTagSelect[i] = (unsigned char)getbits(pBitRd, 4);
    }

    byte_align(pBitRd);

    pce->commentFieldBytes = (unsigned char)getbits(pBitRd, 8);

    for (i = 0; i < pce->commentFieldBytes; i++) {
        pce->commentFieldData[i] = (unsigned char)getbits(pBitRd, 8);
    }
    pce->commentFieldData[i] = 0;

    if (pce->channels > MAX_CHANNELS) {
        return MP4BadDataErr;
    }

    return 0;
}

/* Table 4.4.1 */
static MP4Err GASpecificConfig(mp4AudioSpecificConfig* mp4ASC, BitReader* pBitRd) {
    if (NULL == mp4ASC || NULL == pBitRd) {
        return MP4BadParamErr;
    }
    /* 1024 or 960 */
    mp4ASC->frameLengthFlag = getbit1(pBitRd);

#ifndef ALLOW_SMALL_FRAMELENGTH
    if (mp4ASC->frameLengthFlag == 1) {
        MP4MSG("do not support small frame length!\n");
        return MP4BadDataErr;
    }
#endif

    mp4ASC->dependsOnCoreCoder = getbit1(pBitRd);

    if (1 == mp4ASC->dependsOnCoreCoder) {
        mp4ASC->coreCoderDelay = getbits(pBitRd, 14);
    }

    mp4ASC->extensionFlag = getbit1(pBitRd);

    if (0 == mp4ASC->channelsConfiguration) {
        program_config pce;
        if (program_config_element(&pce, pBitRd)) {
            MP4MSG("program_config_element failed!\n");
            return MP4BadDataErr;
        }
    }

    if ((mp4ASC->objectTypeIndex == 6) || (mp4ASC->objectTypeIndex == 20)) {
        mp4ASC->layerNr = getbits(pBitRd, 3);
    }

    if (1 == mp4ASC->extensionFlag) {
        if (mp4ASC->objectTypeIndex == 22)  // BSAC
        {
            mp4ASC->numOfSubFrame = getbits(pBitRd, 5);
            mp4ASC->layer_length = getbits(pBitRd, 11);

            MP4MSG("%d, %d", mp4ASC->numOfSubFrame, mp4ASC->layer_length);
        }
    }
#ifdef ERROR_RESILIENCE
    if (mp4ASC->extensionFlag == 1) {
        /* Error resilience not supported yet */
        if (mp4ASC->objectTypeIndex == 17 || mp4ASC->objectTypeIndex == 19 ||
            mp4ASC->objectTypeIndex == 20 || mp4ASC->objectTypeIndex == 23) {
            mp4ASC->aacSectionDataResilienceFlag = get1bit(pBitRd);
            mp4ASC->aacScalefactorDataResilienceFlag = get1bit(pBitRd);
            mp4ASC->aacSpectralDataResilienceFlag = get1bit(pBitRd);
            /* 1 bit: extensionFlag3 for version 3 */
        }
    }
#endif
    return MP4NoErr;
}

MP4Err AudioSpecificConfig(unsigned char* decoder_info, mp4AudioSpecificConfig* mp4ASC) {
    BitReader bitRd;

    if (NULL == decoder_info || NULL == mp4ASC) {
        return MP4BadParamErr;
    }

    initbuffer(decoder_info, &bitRd);

    mp4ASC->sbr_present_flag = -1;

    /* Get the audio object type */
    mp4ASC->objectTypeIndex = getbits(&bitRd, 5);
    // MP4MSG("This objectTypeIndex is  %d\n", mp4ASC->objectTypeIndex);
    if (31 == mp4ASC->objectTypeIndex) {
        mp4ASC->objectTypeIndex = 32 + getbits(&bitRd, 6);
    }

    /* Get the sampling frequency index */
    mp4ASC->samplingFrequencyIndex = getbits(&bitRd, 4);

    if (0xf == mp4ASC->samplingFrequencyIndex) {
        mp4ASC->samplingFrequency = getbits(&bitRd, 24);
    } else {
        mp4ASC->samplingFrequency = get_sample_rate(mp4ASC->samplingFrequencyIndex);
    }

    MP4MSG("samplingFrequency:%ld, samplingFrequencyIndex:%d\n", mp4ASC->samplingFrequency,
           mp4ASC->samplingFrequencyIndex);

    /* Get the number of channels */
    mp4ASC->channelsConfiguration = getbits(&bitRd, 4);
    if (mp4ASC->channelsConfiguration > 7) {
        return MP4BadDataErr;
    }

#ifdef SBR_DEC
    if (mp4ASC->objectTypeIndex == 5) {
        mp4ASC->sbr_present_flag = 1;
        mp4ASC->samplingFrequencyIndex = (unsigned char)getbits(&bitRd, 4);
        if (mp4ASC->samplingFrequencyIndex == 15) {
            mp4ASC->samplingFrequency = getbits(&bitRd, 24);
        } else {
            mp4ASC->samplingFrequency = get_sample_rate(mp4ASC->samplingFrequencyIndex);
        }
        mp4ASC->objectTypeIndex = (unsigned char)getbits(&bitRd, 5);
    }
#endif

    switch (mp4ASC->objectTypeIndex) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 6:
        case 7:
        case 17:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
            GASpecificConfig(mp4ASC, &bitRd);
            break;
        default:
            break;
    }
#ifdef ERROR_RESILIENCE
    if (mp4ASC->objectTypeIndex >= ER_OBJECT_START) {
        mp4ASC->epConfig = (unsigned char)getbits(&bitRd, 2);
    }
#endif

#ifdef SSR_DEC
    /* shorter frames not allowed for SSR */
    if ((mp4ASC->objectTypeIndex == 4) && (mp4ASC->frameLengthFlag == 1)) {
        return;
    }
#endif

#ifdef SBR_DEC

    /* no SBR signalled, this could mean either implicit signalling or no SBR in this file */
    /* MPEG specification states: assume SBR on files with samplerate <= 24000 Hz */
    if (mp4ASC->sbr_present_flag == -1) {
        if (mp4ASC->samplingFrequency <= 24000) {
            mp4ASC->samplingFrequency *= 2;
            mp4ASC->forceUpSampling = 1;
        }
    }
#endif
    return MP4NoErr;
}
