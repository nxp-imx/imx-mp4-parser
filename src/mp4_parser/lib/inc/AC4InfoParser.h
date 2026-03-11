/*
 ***********************************************************************
 * Copyright 2021, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#ifndef AC4_INFO_PARSER_H_
#define AC4_INFO_PARSER_H_

#include "BitReader.h"

// TS 103 190-1 v1.2.1 4.3.3.8.1
enum ContentClassifiers {
    kCompleteMain,
    kMusicAndEffects,
    kVisuallyImpaired,
    kHearingImpaired,
    kDialog,
    kCommentary,
    kEmergency,
    kVoiceOver
};

// ETSI TS 103 190-2 V1.1.1 (2015-09) Table 79: channel_mode
enum InputChannelMode {
    CHANNEL_MODE_Mono,
    CHANNEL_MODE_Stereo,
    CHANNEL_MODE_3_0,
    CHANNEL_MODE_5_0,
    CHANNEL_MODE_5_1,
    CHANNEL_MODE_7_0_34,
    CHANNEL_MODE_7_1_34,
    CHANNEL_MODE_7_0_52,
    CHANNEL_MODE_7_1_52,
    CHANNEL_MODE_7_0_322,
    CHANNEL_MODE_7_1_322,
    CHANNEL_MODE_7_0_4,
    CHANNEL_MODE_7_1_4,
    CHANNEL_MODE_9_0_4,
    CHANNEL_MODE_9_1_4,
    CHANNEL_MODE_22_2,
    CHANNEL_MODE_Reserved,
};

typedef struct {
    int32 mChannelMode;
    int32 mProgramID;
    int32 mGroupIndex;
    uint32 mContentClassifier;
    bool mHasDialogEnhancements;
    bool mPreVirtualized;
    bool mEnabled;
    char* mLanguage;
    char* mDescription;
} AC4Presentation;

typedef struct {
    AC4Presentation* mPresentations;
    uint8 mNumPresentation;
    BitReader* mBitReader;
    uint64 mDSISize;
} AC4InfoParser;

bool AC4Parse(AC4InfoParser* this, BitReader* br);
void AC4Free(AC4InfoParser* this);

#endif
