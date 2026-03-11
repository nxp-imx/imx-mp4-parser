/*
 ***********************************************************************
 * Copyright 2021, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */
//#define DEBUG

#include <string.h>
#include "AC4InfoParser.h"
#include "W32FileMappingObject.h"
#include "MP4Impl.h"


#define NELEM(x) ((int)(sizeof(x) / sizeof((x)[0])))

#define NUM_LEFT() this->mBitReader->numBitsLeft(this->mBitReader)
#define CHECK_BITS_LEFT(n) \
    if (NUM_LEFT() < n) {  \
        return FALSE;      \
    }

#define GET_BITS(num, var) \
    CHECK_BITS_LEFT(num)   \
    var = this->mBitReader->getBits(this->mBitReader, num)

#define SKIP_BITS(num)   \
    CHECK_BITS_LEFT(num) \
    this->mBitReader->skipBits(this->mBitReader, num)

#define BYTE_ALIGN this->mBitReader->skipBits(this->mBitReader, NUM_LEFT() % 8)

#define CHECK_RES(x) \
    if (!x)          \
    return FALSE

#define kAC4UUIDSizeInBytes 16

static bool parseLanguageTag(AC4InfoParser* this, uint32 presentationID) {
    uint32 n_language_tag_bytes = 0;
    uint32 i = 0;
    char language_tag_bytes[42];
    bool b_language_indicator = FALSE;

    GET_BITS(1, b_language_indicator);

    if (!b_language_indicator)
        return TRUE;

    GET_BITS(6, n_language_tag_bytes);
    if (n_language_tag_bytes < 2 || n_language_tag_bytes >= 42) {
        return FALSE;
    }

    for (i = 0; i < n_language_tag_bytes; i++) {
        GET_BITS(8, language_tag_bytes[i]);
    }
    language_tag_bytes[n_language_tag_bytes] = 0;

    this->mPresentations[presentationID].mLanguage = strdup(&language_tag_bytes[0]);

    return TRUE;
}

static bool parseSubstreamDSI(AC4InfoParser* this, uint32 presentationID) {
    uint32 channel_mode = 0;
    uint32 content_classifier = 0;
    bool b_substream_bitrate_indicator = FALSE;
    bool b_content_type = FALSE;

    GET_BITS(5, channel_mode);
    SKIP_BITS(2);  // dsi_sf_multiplier
    GET_BITS(1, b_substream_bitrate_indicator);

    if (b_substream_bitrate_indicator) {
        SKIP_BITS(5);  // substream_bitrate_indicator
    }
    if (channel_mode >= 7 && channel_mode <= 10) {
        SKIP_BITS(1);  // add_ch_base
    }
    GET_BITS(1, b_content_type);
    if (b_content_type) {
        GET_BITS(3, content_classifier);

        if (this->mPresentations[presentationID].mChannelMode == -1 &&
            (content_classifier == 0 || content_classifier == 1)) {
            this->mPresentations[presentationID].mChannelMode = channel_mode;
        }
        this->mPresentations[presentationID].mContentClassifier = content_classifier;

        if (!parseLanguageTag(this, presentationID))
            return FALSE;
    }

    return TRUE;
}

static bool parseSubstreamGroupDSI(AC4InfoParser* this, uint32 presentationID) {
    uint32 n_substreams = 0;
    uint32 i = 0;
    bool b_content_type = FALSE;
    bool b_channel_coded = FALSE;

    SKIP_BITS(1);  // b_substreams_present
    SKIP_BITS(1);  // b_hsf_ext
    GET_BITS(1, b_channel_coded);
    GET_BITS(8, n_substreams);

    for (i = 0; i < n_substreams; i++) {
        bool b_substream_bitrate_indicator = FALSE;
        SKIP_BITS(2);  // dsi_sf_multiplier
        GET_BITS(1, b_substream_bitrate_indicator);

        if (b_substream_bitrate_indicator) {
            SKIP_BITS(5);  // substream_bitrate_indicator
        }
        if (b_channel_coded) {
            SKIP_BITS(24);  // dsi_substream_channel_mask
        } else {
            bool b_ajoc = FALSE;
            GET_BITS(1, b_ajoc);
            if (b_ajoc) {
                bool b_static_dmx = FALSE;
                GET_BITS(1, b_static_dmx);
                if (!b_static_dmx) {
                    SKIP_BITS(4);  // n_dmx_objects_minus1
                }
                SKIP_BITS(6);  // n_umx_objects_minus1
            }
            SKIP_BITS(4);  // objects_assignment_mask
        }
    }

    GET_BITS(1, b_content_type);
    if (b_content_type) {
        uint32 content_classifier = 0;
        GET_BITS(3, content_classifier);

        this->mPresentations[presentationID].mContentClassifier = content_classifier;

        if (!parseLanguageTag(this, presentationID))
            return FALSE;
    }

    return TRUE;
}

static bool parseChannelMode(AC4InfoParser* this, uint32 presentationID,
                             uint32 presentation_version) {
    bool b_presentation_channel_coded = FALSE;
    if (presentation_version == 0) {
        b_presentation_channel_coded = TRUE;
    } else {
        GET_BITS(1, b_presentation_channel_coded);
    }
    if (b_presentation_channel_coded) {
        if (presentation_version == 1 || presentation_version == 2) {
            uint32 dsi_presentation_ch_mode = 0;
            GET_BITS(5, dsi_presentation_ch_mode);
            this->mPresentations[presentationID].mChannelMode = dsi_presentation_ch_mode;

            if (dsi_presentation_ch_mode >= 11 && dsi_presentation_ch_mode <= 14) {
                SKIP_BITS(1);  // pres_b_4_back_channels_present
                SKIP_BITS(2);  // pres_top_channel_pairs
            }
        }
        SKIP_BITS(24);  // presentation_channel_mask_v1
    }
    return TRUE;
}

static bool parseDescription(AC4InfoParser* this, uint32 presentationID) {
    bool b_alternative = FALSE;
    GET_BITS(1, b_alternative);
    if (b_alternative) {
        uint32 i = 0;
        uint32 name_len = 0;
        uint32 n_targets = 0;
        char* presentation_name = NULL;
        BYTE_ALIGN;
        GET_BITS(16, name_len);

        presentation_name = MP4LocalCalloc(1, name_len + 1);
        for (i = 0; i < name_len; i++) {
            GET_BITS(8, presentation_name[i]);
        }
        presentation_name[i] = 0;
        this->mPresentations[presentationID].mDescription = presentation_name;
        GET_BITS(5, n_targets);
        SKIP_BITS(n_targets * (3 + 8));  // target_md_compat, target_device_category
    }
    return TRUE;
}

static bool parseDSI(AC4InfoParser* this, uint32 presentationID, uint32 presentation_version,
                     bool b_single_substream_group, uint32 presentation_config) {
    if (b_single_substream_group) {
        if (presentation_version == 0) {
            CHECK_RES(parseSubstreamDSI(this, presentationID));
        } else {
            CHECK_RES(parseSubstreamGroupDSI(this, presentationID));
        }
    } else {
        uint32 i = 0;
        SKIP_BITS(1);  // b_multi_pid or b_hsf_ext

        switch (presentation_config) {
            case 0:
            case 1:
            case 2:
                for (i = 0; i < 2; i++) {
                    if (presentation_version == 0) {
                        CHECK_RES(parseSubstreamDSI(this, presentationID));
                    } else {
                        CHECK_RES(parseSubstreamGroupDSI(this, presentationID));
                    }
                }
                break;
            case 3:
            case 4:
                for (i = 0; i < 3; i++) {
                    if (presentation_version == 0) {
                        CHECK_RES(parseSubstreamDSI(this, presentationID));
                    } else {
                        CHECK_RES(parseSubstreamGroupDSI(this, presentationID));
                    }
                }
                break;
            case 5:
                if (presentation_version == 0) {
                    CHECK_RES(parseSubstreamDSI(this, presentationID));
                } else {
                    uint32 n_substream_groups_minus2 = 0;
                    uint32 sg = 0;
                    GET_BITS(3, n_substream_groups_minus2);
                    for (sg = 0; sg < n_substream_groups_minus2 + 2; sg++) {
                        CHECK_RES(parseSubstreamGroupDSI(this, presentationID));
                    }
                }
                break;
            default: {
                uint32 n_skip_bytes = 0;
                GET_BITS(7, n_skip_bytes);
                SKIP_BITS(8 * n_skip_bytes);
            } break;
        }
    }
    return TRUE;
}

// ETSI TS 103 190-2 E.6 ac4_dsi_v1
bool AC4Parse(AC4InfoParser* this, BitReader* br) {
    uint32 bitstream_version = 0;
    uint32 presentation = 0;
    uint32 ac4_dsi_version = 0;
    int32_t short_program_id = -1;

    MP4MSG("=== AC4Parse ==");
    this->mBitReader = br;
    this->mDSISize = NUM_LEFT();

    GET_BITS(3, ac4_dsi_version);
    if (ac4_dsi_version > 1) {
        MP4MSG("E.6.2 ac4_dsi_version: only versions 0 and 1 are supported");
        return FALSE;
    }

    GET_BITS(7, bitstream_version);
    SKIP_BITS(1);  // fs_index
    SKIP_BITS(4);  // frame_rate_index
    GET_BITS(9, this->mNumPresentation);

    if (bitstream_version > 1) {
        bool b_program_id = FALSE;
        if (ac4_dsi_version == 0) {
            MP4MSG("invalid ac4 dsi");
            return FALSE;
        }
        GET_BITS(1, b_program_id);
        if (b_program_id) {
            bool b_uuid = FALSE;
            GET_BITS(16, short_program_id);
            GET_BITS(1, b_uuid);
            if (b_uuid) {
                SKIP_BITS(8 * kAC4UUIDSizeInBytes);
            }
        }
    }

    if (ac4_dsi_version == 1) {
        SKIP_BITS(66);  // bitrate
        BYTE_ALIGN;
    }

    this->mPresentations = MP4LocalCalloc(this->mNumPresentation, sizeof(AC4Presentation));
    memset(this->mPresentations, 0, this->mNumPresentation * sizeof(AC4Presentation));

    for (presentation = 0; presentation < this->mNumPresentation; presentation++) {
        bool b_single_substream_group = FALSE;
        bool b_add_emdf_substreams = FALSE;
        uint32 presentation_config = 0, presentation_version = 0;
        uint32 pres_bytes = 0;
        uint64 start = 0;

        this->mPresentations[presentation].mProgramID = short_program_id;
        this->mPresentations[presentation].mChannelMode = -1;
        this->mPresentations[presentation].mGroupIndex = -1;
        this->mPresentations[presentation].mEnabled = TRUE;

        if (ac4_dsi_version == 0) {
            GET_BITS(1, b_single_substream_group);
            GET_BITS(5, presentation_config);
            GET_BITS(5, presentation_version);
        } else if (ac4_dsi_version == 1) {
            GET_BITS(8, presentation_version);
            GET_BITS(8, pres_bytes);
            if (pres_bytes == 0xff) {
                uint32 temp;
                GET_BITS(16, temp);
                pres_bytes += temp;
            }
            if (presentation_version > 2) {
                SKIP_BITS(pres_bytes * 8);
                continue;
            }
            start = (this->mDSISize - NUM_LEFT()) / 8;
            GET_BITS(5, presentation_config);
            b_single_substream_group = (presentation_config == 0x1f);
        }

        b_add_emdf_substreams = FALSE;
        if (!b_single_substream_group && presentation_config == 6) {
            b_add_emdf_substreams = TRUE;
        } else {
            bool b_pre_virtualized = FALSE;
            bool b_presentation_group_index = FALSE;

            SKIP_BITS(3);  // mdcompat
            GET_BITS(1, b_presentation_group_index);
            if (b_presentation_group_index) {
                GET_BITS(5, this->mPresentations[presentation].mGroupIndex);
            }
            SKIP_BITS(2);  // dsi_frame_rate_multiply_info
            if (ac4_dsi_version == 1 && (presentation_version == 1 || presentation_version == 2)) {
                SKIP_BITS(2);  // dsi_frame_rate_fraction_info);
            }
            SKIP_BITS(5);   // presentation_emdf_version
            SKIP_BITS(10);  // presentation_key_id

            if (ac4_dsi_version == 1) {
                CHECK_RES(parseChannelMode(this, presentation, presentation_version));

                if (presentation_version == 1 || presentation_version == 2) {
                    bool b_presentation_core_differs = FALSE;
                    bool b_presentation_filter = FALSE;
                    GET_BITS(1, b_presentation_core_differs);
                    if (b_presentation_core_differs) {
                        bool b_presentation_core_channel_coded = FALSE;
                        GET_BITS(1, b_presentation_core_channel_coded);
                        if (b_presentation_core_channel_coded) {
                            SKIP_BITS(2);  // dsi_presentation_channel_mode_core
                        }
                    }
                    GET_BITS(1, b_presentation_filter);
                    if (b_presentation_filter) {
                        bool b_enable_presentation = FALSE;
                        uint32 n_filter_bytes = 0;
                        GET_BITS(1, b_enable_presentation);
                        if (!b_enable_presentation) {
                            this->mPresentations[presentation].mEnabled = FALSE;
                        }
                        GET_BITS(8, n_filter_bytes);
                        SKIP_BITS(8 * n_filter_bytes);
                    }
                }
            }

            CHECK_RES(parseDSI(this, presentation, presentation_version, b_single_substream_group,
                               presentation_config));

            GET_BITS(1, b_pre_virtualized);
            this->mPresentations[presentation].mPreVirtualized = b_pre_virtualized;
            GET_BITS(1, b_add_emdf_substreams);
        }
        if (b_add_emdf_substreams) {
            uint32 n_add_emdf_substreams = 0;
            uint32 j = 0;
            GET_BITS(7, n_add_emdf_substreams);
            for (j = 0; j < n_add_emdf_substreams; j++) {
                SKIP_BITS(5);   // substream_emdf_version
                SKIP_BITS(10);  // substream_key_id
            }
        }

        if (presentation_version > 0) {
            bool b_presentation_bitrate_info = FALSE;
            GET_BITS(1, b_presentation_bitrate_info);
            if (b_presentation_bitrate_info) {
                SKIP_BITS(66);
            }
            CHECK_RES(parseDescription(this, presentation));
        }

        BYTE_ALIGN;

        if (ac4_dsi_version == 1) {
            uint64 skip_bytes;
            uint64 end = (this->mDSISize - NUM_LEFT()) / 8;
            uint64 presentation_bytes = end - start;
            if (pres_bytes < presentation_bytes) {
                MP4MSG("pres_bytes is smaller than presentation_bytes.");
                return FALSE;
            }
            skip_bytes = pres_bytes - presentation_bytes;
            SKIP_BITS(skip_bytes * 8);
        }

        if (this->mPresentations[presentation].mChannelMode == -1) {
            MP4MSG("could not determing channel mode of presentation %d", presentation);
            return FALSE;
        }
    }

    return TRUE;
}

void AC4Free(AC4InfoParser* this) {
    uint32 i = 0;
    AC4Presentation* ptr = this->mPresentations;

    for (i = 0; i < this->mNumPresentation; i++) {
        if (ptr[i].mLanguage) {
            MP4LocalFree(ptr[i].mLanguage);
            ptr[i].mLanguage = NULL;
        }
        if (ptr[i].mDescription) {
            MP4LocalFree(ptr[i].mDescription);
            ptr[i].mDescription = NULL;
        }
    }

    if (this->mPresentations) {
        MP4LocalFree(this->mPresentations);
        this->mPresentations = NULL;
    }
}
