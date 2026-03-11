
/************************************************************************
 * Copyright (c) 2005-2016, Freescale Semiconductor, Inc.
 * Copyright 2017-2018, 2020-2026 NXP
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
    $Id: MP4Atoms.h,v 1.44 2000/03/04 07:58:05 mc Exp $
*/

#ifndef INCLUDED_MP4ATOMS_H
#define INCLUDED_MP4ATOMS_H

#include "MP4InputStream.h"
#include "MP4Movies.h"
#include "MP4Impl.h"
#include "MP4LinkedList.h"

//#include "MP4UtilityFile.h"

#define PCM_AUDIO_CACHE_SIZE \
    3840 /* can contain 20ms data for 48KHZ, 16 bits/sample, 2 channel PCM audio */

#ifndef SAFE_DESTROY
#define SAFE_DESTROY(ObjectAtom)             \
    do {                                     \
        if (NULL != ObjectAtom) {            \
            ObjectAtom->destroy(ObjectAtom); \
            ObjectAtom = NULL;               \
        }                                    \
    } while (0)
#endif

#ifndef SAFE_LOCAL_FREE
#define SAFE_LOCAL_FREE(ptr)   \
    do {                       \
        if (NULL != ptr) {     \
            MP4LocalFree(ptr); \
            ptr = NULL;        \
        }                      \
    } while (0)
#endif

// to alloc enough size buffer to store whole frame, not just a nal
// nal len field may be 1 byte, we need change to 4 byte start code (0x00000001)
#define MAXNALNUM_PERFRAME (uint32)128
#define NALLENFIELD_INCERASE (uint32)(MAXNALNUM_PERFRAME * 3)

enum {
    QTTerminatorAtomType = 0x00000000,
    MP4Mp3SampleEntryAtomType = MP4_FOUR_CHAR_CODE('.', 'm', 'p', '3'),
    MP4GenericSampleEntryAtomType = MP4_FOUR_CHAR_CODE('!', 'g', 'n', 'r'),
    MP4AudioInfoAtomType = MP4_FOUR_CHAR_CODE('-', '-', '-', '-'),
    MP4DivxSampleEntryAtomType = MP4_FOUR_CHAR_CODE('D', 'I', 'V', 'X'),
    MP4AC3SampleEntryAtomType = MP4_FOUR_CHAR_CODE('a', 'c', '-', '3'),
    MP4AC4SampleEntryAtomType = MP4_FOUR_CHAR_CODE('a', 'c', '-', '4'),
    MP4ALACSampleEntryAtomType = MP4_FOUR_CHAR_CODE('a', 'l', 'a', 'c'),
    QTDataEntryAlisAtomType = MP4_FOUR_CHAR_CODE('a', 'l', 'i', 's'),
    MP4ApvSampleEntryAtomType = MP4_FOUR_CHAR_CODE('a', 'p', 'v', '1'),
    MP4ApvCAtomType = MP4_FOUR_CHAR_CODE('a', 'p', 'v', 'C'),
    MP4Av1SampleEntryAtomType = MP4_FOUR_CHAR_CODE('a', 'v', '0', '1'),
    MP4Av1CAtomType = MP4_FOUR_CHAR_CODE('a', 'v', '1', 'C'),
    MP4AvccAtomType = MP4_FOUR_CHAR_CODE('a', 'v', 'c', 'C'),
    MP4AvcSampleEntryAtomType = MP4_FOUR_CHAR_CODE('a', 'v', 'c', '1'),
    MP4BitrateAtomType = MP4_FOUR_CHAR_CODE('b', 't', 'r', 't'),
    MSDataEntryAtomType = MP4_FOUR_CHAR_CODE('c', 'i', 'o', 's'),
    MP4ChunkLargeOffsetAtomType = MP4_FOUR_CHAR_CODE('c', 'o', '6', '4'),
    QTColorParameterAtomType = MP4_FOUR_CHAR_CODE('c', 'o', 'l', 'r'),
    MP4ClockReferenceMediaHeaderAtomType = MP4_FOUR_CHAR_CODE('c', 'r', 'h', 'd'),
    MP4CopyrightAtomType = MP4_FOUR_CHAR_CODE('c', 'p', 'r', 't'),
    MP4CompositionOffsetAtomType = MP4_FOUR_CHAR_CODE('c', 't', 't', 's'),
    MP4AC3SpecificAtomType = MP4_FOUR_CHAR_CODE('d', 'a', 'c', '3'),
    MP4AC4SpecificAtomType = MP4_FOUR_CHAR_CODE('d', 'a', 'c', '4'),
    MP4EC3SpecificAtomType = MP4_FOUR_CHAR_CODE('d', 'e', 'c', '3'),
    MP4D263AtomType = MP4_FOUR_CHAR_CODE('d', '2', '6', '3'),
    MP4DamrAtomType = MP4_FOUR_CHAR_CODE('d', 'a', 'm', 'r'),
    QTValueAtomType = MP4_FOUR_CHAR_CODE('d', 'a', 't', 'a'),
    DAV1SampleEntryAtomType = MP4_FOUR_CHAR_CODE('d', 'a', 'v', '1'),
    MP4DataInformationAtomType = MP4_FOUR_CHAR_CODE('d', 'i', 'n', 'f'),
    MP4StreamDependenceAtomType = MP4_FOUR_CHAR_CODE('d', 'p', 'n', 'd'),
    MP4DataReferenceAtomType = MP4_FOUR_CHAR_CODE('d', 'r', 'e', 'f'),
    DVAVSampleEntryAtomType = MP4_FOUR_CHAR_CODE('d', 'v', 'a', 'v'),
    DVA1SampleEntryAtomType = MP4_FOUR_CHAR_CODE('d', 'v', 'a', '1'),
    DVHESampleEntryAtomType = MP4_FOUR_CHAR_CODE('d', 'v', 'h', 'e'),
    DVH1SampleEntryAtomType = MP4_FOUR_CHAR_CODE('d', 'v', 'h', '1'),
    MP4dvvCAtomType = MP4_FOUR_CHAR_CODE('d', 'v', 'v', 'C'),
    MP4EC3SampleEntryAtomType = MP4_FOUR_CHAR_CODE('e', 'c', '-', '3'),
    MP4EditAtomType = MP4_FOUR_CHAR_CODE('e', 'd', 't', 's'),
    MP4EditListAtomType = MP4_FOUR_CHAR_CODE('e', 'l', 's', 't'),
    MP4FlacSampleEntryAtomType = MP4_FOUR_CHAR_CODE('f', 'L', 'a', 'C'),
    MP4FreeSpaceAtomType = MP4_FOUR_CHAR_CODE('f', 'r', 'e', 'e'),
    MP4ESDAtomType = MP4_FOUR_CHAR_CODE('e', 's', 'd', 's'),
    MP4FileTypeAtomType = MP4_FOUR_CHAR_CODE('f', 't', 'y', 'p'),
    QTH263SampleEntryAtomType = MP4_FOUR_CHAR_CODE('H', '2', '6', '3'),
    H263SampleEntryAtomType = MP4_FOUR_CHAR_CODE('h', '2', '6', '3'),
    MP4HandlerAtomType = MP4_FOUR_CHAR_CODE('h', 'd', 'l', 'r'),
    MP4HintTrackReferenceAtomType = MP4_FOUR_CHAR_CODE('h', 'i', 'n', 't'),
    MP4HintMediaHeaderAtomType = MP4_FOUR_CHAR_CODE('h', 'm', 'h', 'd'),
    MP4HvccAtomType = MP4_FOUR_CHAR_CODE('h', 'v', 'c', 'C'),
    MP4HevcSampleEntryAtomType = MP4_FOUR_CHAR_CODE('h', 'v', 'c', '1'),
    MP4Hev1SampleEntryAtomType = MP4_FOUR_CHAR_CODE('h', 'e', 'v', '1'),
    QTMetadataItemListAtomType = MP4_FOUR_CHAR_CODE('i', 'l', 's', 't'),
    ImaAdpcmSampleEntryAtomType = MP4_FOUR_CHAR_CODE('i', 'm', 'a', '4'),
    MP4ObjectDescriptorAtomType = MP4_FOUR_CHAR_CODE('i', 'o', 'd', 's'),
    QTItemInformationAtomType = MP4_FOUR_CHAR_CODE('i', 't', 'i', 'f'),
    QTMetadataItemAtomType = MP4_FOUR_CHAR_CODE('i', 't', 'e', 'm'),
    JPEGSampleEntryAtomType = MP4_FOUR_CHAR_CODE('j', 'p', 'e', 'g'),
    QTMetadataItemKeysAtomType = MP4_FOUR_CHAR_CODE('k', 'e', 'y', 's'),

    MP4MediaDataAtomType = MP4_FOUR_CHAR_CODE('m', 'd', 'a', 't'),
    MP4MediaAtomType = MP4_FOUR_CHAR_CODE('m', 'd', 'i', 'a'),
    MP4MediaHeaderAtomType = MP4_FOUR_CHAR_CODE('m', 'd', 'h', 'd'),
    MP4MeanAtomType = MP4_FOUR_CHAR_CODE('m', 'e', 'a', 'n'),
    QTMetadataAtomType = MP4_FOUR_CHAR_CODE('m', 'e', 't', 'a'),
    MP4MetadataSampleEntryAtomType = MP4_FOUR_CHAR_CODE('m', 'e', 't', 't'),
    MPEGHA1SampleEntryAtomType = MP4_FOUR_CHAR_CODE('m', 'h', 'a', '1'),
    MPEGHM1SampleEntryAtomType = MP4_FOUR_CHAR_CODE('m', 'h', 'm', '1'),
    MHACAtomType = MP4_FOUR_CHAR_CODE('m', 'h', 'a', 'C'),
    MHAPAtomType = MP4_FOUR_CHAR_CODE('m', 'h', 'a', 'P'),
    MP4MediaInformationAtomType = MP4_FOUR_CHAR_CODE('m', 'i', 'n', 'f'),
    MJPEGFormatASampleEntryAtomType = MP4_FOUR_CHAR_CODE('m', 'j', 'p', 'a'),
    MJPEGFormatBSampleEntryAtomType = MP4_FOUR_CHAR_CODE('m', 'j', 'p', 'b'),
    MJPEG2000SampleEntryAtomType = MP4_FOUR_CHAR_CODE('m', 'j', 'p', '2'),
    MP4MovieAtomType = MP4_FOUR_CHAR_CODE('m', 'o', 'o', 'v'),
    MP4AudioSampleEntryAtomType = MP4_FOUR_CHAR_CODE('m', 'p', '4', 'a'),
    MP4MPEGSampleEntryAtomType = MP4_FOUR_CHAR_CODE('m', 'p', '4', 's'),
    MP4VisualSampleEntryAtomType = MP4_FOUR_CHAR_CODE('m', 'p', '4', 'v'),
    MP4ODTrackReferenceAtomType = MP4_FOUR_CHAR_CODE('m', 'p', 'o', 'd'),
    MP4Mp3CbrSampleEntryAtomType = MP4_FOUR_CHAR_CODE('m', 's', 0, 0x55),
    MP4MSAdpcmEntryAtomType = MP4_FOUR_CHAR_CODE('m', 's', 0x0, 0x11),
    MP4MovieHeaderAtomType = MP4_FOUR_CHAR_CODE('m', 'v', 'h', 'd'),

    QTNameAtomType = MP4_FOUR_CHAR_CODE('n', 'a', 'm', 'e'),
    MP4MPEGMediaHeaderAtomType = MP4_FOUR_CHAR_CODE('n', 'm', 'h', 'd'),
    MP4ObjectDescriptorMediaHeaderAtomType = MP4_FOUR_CHAR_CODE('o', 'd', 'h', 'd'),
    MP4OpusSampleEntryAtomType = MP4_FOUR_CHAR_CODE('O', 'p', 'u', 's'),
    MP4PixelAspectRatioAtomType = MP4_FOUR_CHAR_CODE('p', 'a', 's', 'p'),

    RawAudioSampleEntryAtomType = MP4_FOUR_CHAR_CODE('r', 'a', 'w', ' '),
    MP4RtpSampleEntryAtomType = MP4_FOUR_CHAR_CODE('r', 't', 'p', ' '),

    MP4H263SampleEntryAtomType = MP4_FOUR_CHAR_CODE('s', '2', '6', '3'),
    AmrNBSampleEntryAtomType = MP4_FOUR_CHAR_CODE('s', 'a', 'm', 'r'), /* AMR-NB */
    AmrWBSampleEntryAtomType = MP4_FOUR_CHAR_CODE('s', 'a', 'w', 'b'), /* AMR-WB */
    MP4SceneDescriptionMediaHeaderAtomType = MP4_FOUR_CHAR_CODE('s', 'd', 'h', 'd'),
    MP4SkipAtomType = MP4_FOUR_CHAR_CODE('s', 'k', 'i', 'p'),
    MP4SoundMediaHeaderAtomType = MP4_FOUR_CHAR_CODE('s', 'm', 'h', 'd'),
    QTSowtSampleEntryAtomType = MP4_FOUR_CHAR_CODE(
            's', 'o', 'w', 't'), /* apple little-endian uncompressed, twos(aiff) complement */
    MP4SampleTableAtomType = MP4_FOUR_CHAR_CODE('s', 't', 'b', 'l'),
    MP4ChunkOffsetAtomType = MP4_FOUR_CHAR_CODE('s', 't', 'c', 'o'),
    MP4DegradationPriorityAtomType = MP4_FOUR_CHAR_CODE('s', 't', 'd', 'p'),
    MP4SampleDescriptionAtomType = MP4_FOUR_CHAR_CODE('s', 't', 's', 'd'),
    MP4SampleSizeAtomType = MP4_FOUR_CHAR_CODE('s', 't', 's', 'z'),
    MP4CompactSampleSizeAtomType = MP4_FOUR_CHAR_CODE('s', 't', 'z', '2'),
    MP4SampleToChunkAtomType = MP4_FOUR_CHAR_CODE('s', 't', 's', 'c'),
    MP4ShadowSyncAtomType = MP4_FOUR_CHAR_CODE('s', 't', 's', 'h'),
    MP4SyncSampleAtomType = MP4_FOUR_CHAR_CODE('s', 't', 's', 's'),
    MP4TimeToSampleAtomType = MP4_FOUR_CHAR_CODE('s', 't', 't', 's'),
    SVQ3SampleEntryAtomType = MP4_FOUR_CHAR_CODE('S', 'V', 'Q', '3'),
    MP4SyncTrackReferenceAtomType = MP4_FOUR_CHAR_CODE('s', 'y', 'n', 'c'),

    QTTextEntryAtomType = MP4_FOUR_CHAR_CODE('t', 'e', 'x', 't'), /* apple text */
    MP4RtpAtomType = MP4_FOUR_CHAR_CODE('t', 'i', 'm', 's'),
    MP4TrackHeaderAtomType = MP4_FOUR_CHAR_CODE('t', 'k', 'h', 'd'),
    QTTimeCodeSampleEntryAtomType = MP4_FOUR_CHAR_CODE('t', 'm', 'c', 'd'), /* apple time code */
    MP4TrackAtomType = MP4_FOUR_CHAR_CODE('t', 'r', 'a', 'k'),
    MP4TrackReferenceAtomType = MP4_FOUR_CHAR_CODE('t', 'r', 'e', 'f'),
    QTTwosSampleEntryAtomType =
            MP4_FOUR_CHAR_CODE('t', 'w', 'o', 's'),              /* apple big-endian uncompressed */
    TimedTextEntryType = MP4_FOUR_CHAR_CODE('t', 'x', '3', 'g'), /* 3GPP timed text */

    MP4UserDataAtomType = MP4_FOUR_CHAR_CODE('u', 'd', 't', 'a'),
    MuLawAudioSampleEntryAtomType = MP4_FOUR_CHAR_CODE('u', 'l', 'a', 'w'),
    MP4DataEntryURLAtomType = MP4_FOUR_CHAR_CODE('u', 'r', 'l', ' '),
    MP4DataEntryURNAtomType = MP4_FOUR_CHAR_CODE('u', 'r', 'n', ' '),
    MP4ExtendedAtomType = MP4_FOUR_CHAR_CODE('u', 'u', 'i', 'd'),
    MP4VideoMediaHeaderAtomType = MP4_FOUR_CHAR_CODE('v', 'm', 'h', 'd'),
    MP4VpccAtomType = MP4_FOUR_CHAR_CODE('v', 'p', 'c', 'C'),
    MP4Vp9SampleEntryAtomType = MP4_FOUR_CHAR_CODE('v', 'p', '0', '9'),
    QTWaveAtomType = MP4_FOUR_CHAR_CODE('w', 'a', 'v', 'e'),
    QTWideAtomType = MP4_FOUR_CHAR_CODE('w', 'i', 'd', 'e'),

    MP4SegmentIndexAtomType = MP4_FOUR_CHAR_CODE('s', 'i', 'd', 'x'),
    MP4MovieExtendsAtomType = MP4_FOUR_CHAR_CODE('m', 'v', 'e', 'x'),
    MP4MovieExtendsHeaderAtomType = MP4_FOUR_CHAR_CODE('m', 'e', 'h', 'd'),
    MP4TrackExtendsAtomType = MP4_FOUR_CHAR_CODE('t', 'r', 'e', 'x'),
    MP4MovieFragmentAtomType = MP4_FOUR_CHAR_CODE('m', 'o', 'o', 'f'),
    MP4MovieFragmentHeaderAtomType = MP4_FOUR_CHAR_CODE('m', 'f', 'h', 'd'),
    MP4TrackFragmentAtomType = MP4_FOUR_CHAR_CODE('t', 'r', 'a', 'f'),
    MP4TrackFragmentHeaderAtomType = MP4_FOUR_CHAR_CODE('t', 'f', 'h', 'd'),
    MP4TrackFragmentRunAtomType = MP4_FOUR_CHAR_CODE('t', 'r', 'u', 'n'),
    MP4SampleAuxiliaryInfoSizeAtomType = MP4_FOUR_CHAR_CODE('s', 'a', 'i', 'z'),
    MP4SampleAuxiliaryInfoOffsetAtomType = MP4_FOUR_CHAR_CODE('s', 'a', 'i', 'o'),
    MP4TrackFragmentDecodeTimeAtomType = MP4_FOUR_CHAR_CODE('t', 'f', 'd', 't'),
    MP4MovieFragmentRandomAccessAtomType = MP4_FOUR_CHAR_CODE('m', 'f', 'r', 'a'),
    MP4TrackFragmentRandomAccessAtomType = MP4_FOUR_CHAR_CODE('t', 'f', 'r', 'a'),
    MP4MovieFragmentRandomAccessOffsetAtomType = MP4_FOUR_CHAR_CODE('m', 'f', 'r', 'o'),

    MP4ProtectionSystemSpecificHeaderAtomType = MP4_FOUR_CHAR_CODE('p', 's', 's', 'h'),
    MP4ProtectedVideoSampleEntryAtomType = MP4_FOUR_CHAR_CODE('e', 'n', 'c', 'v'),
    MP4ProtectedAudioSampleEntryAtomType = MP4_FOUR_CHAR_CODE('e', 'n', 'c', 'a'),
    MP4ProtectedTextSampleEntryAtomType = MP4_FOUR_CHAR_CODE('e', 'n', 'c', 't'),
    MP4ProtectionSchemeInfoAtomType = MP4_FOUR_CHAR_CODE('s', 'i', 'n', 'f'),
    MP4OriginFormatAtomType = MP4_FOUR_CHAR_CODE('f', 'r', 'm', 'a'),
    MP4SchemeTypeAtomType = MP4_FOUR_CHAR_CODE('s', 'c', 'h', 'm'),
    MP4SchemeInfoAtomType = MP4_FOUR_CHAR_CODE('s', 'c', 'h', 'i'),
    MP4TrackEncryptionAtomType = MP4_FOUR_CHAR_CODE('t', 'e', 'n', 'c'),
    MP4SampleEncryptionAtomType = MP4_FOUR_CHAR_CODE('s', 'e', 'n', 'c'),

    MP4SchemeTypeCBC1 = MP4_FOUR_CHAR_CODE('c', 'b', 'c', '1'),
    MP4SchemeTypeCBCS = MP4_FOUR_CHAR_CODE('c', 'b', 'c', 's'),
    MP4SchemeTypeCENC = MP4_FOUR_CHAR_CODE('c', 'e', 'n', 'c'),
    MP4SchemeTypeCENS = MP4_FOUR_CHAR_CODE('c', 'e', 'n', 's'),

    /* user data types */
    UdtaArtistType = MP4_FOUR_CHAR_CODE(169, 'A', 'R', 'T'),    /* artist */
    UdtaAlbumType = MP4_FOUR_CHAR_CODE(169, 'a', 'l', 'b'),     /* album */
    UdtaCommentType = MP4_FOUR_CHAR_CODE(169, 'c', 'm', 't'),   /* comment */
    UdtaCopyrightType = MP4_FOUR_CHAR_CODE(169, 'c', 'p', 'y'), /* copy right */
    UdtaDateType =
            MP4_FOUR_CHAR_CODE(169, 'd', 'a', 'y'), /* date the movie content was created (year) */
    UdtaDescriptionType = MP4_FOUR_CHAR_CODE(169, 'd', 'e', 's'), /* description*/
    UdtaDescType2 = MP4_FOUR_CHAR_CODE('d', 'e', 's', 'c'),       /* description*/
    UdtaGenreType = MP4_FOUR_CHAR_CODE(169, 'g', 'e', 'n'),       /* genre */
    UdtaGenreType2 = MP4_FOUR_CHAR_CODE('g', 'n', 'r', 'e'),      /* genre */
    UdtaTitleType = MP4_FOUR_CHAR_CODE(169, 'n', 'a', 'm'),       /* title */
    UdtaToolType = MP4_FOUR_CHAR_CODE(169, 't', 'o', 'o'),        /* writing application */
    UdtaCoverType = MP4_FOUR_CHAR_CODE('c', 'o', 'v', 'r'),       /* artwork */
    UdtaComposerType = MP4_FOUR_CHAR_CODE(169, 'c', 'o', 'm'),    /* name of composer */
    UdtaTrackNumberType = MP4_FOUR_CHAR_CODE('t', 'r', 'k', 'n'), /* track number */
    UdtaLocationType = MP4_FOUR_CHAR_CODE(169, 'x', 'y', 'z'),    /* geographic location */
    FSLiTunSMPBAtomType = MP4_FOUR_CHAR_CODE(
            169, 'f', 'i', 'S'), /* i created this FOURCC type for encoder delay & padding */

    UdtaDirectorType = MP4_FOUR_CHAR_CODE(169, 'd', 'i', 'r'),    /* director */
    UdtaInformationType = MP4_FOUR_CHAR_CODE(169, 'i', 'n', 'f'), /* information */
    UdtaCreatorType = MP4_FOUR_CHAR_CODE(169, 'm', 'a', 'k'),     /* creator */
    UdtaKeywordType = MP4_FOUR_CHAR_CODE(169, 'n', 'a', 'k'),     /* keyword */
    UdtaProducerType = MP4_FOUR_CHAR_CODE(169, 'p', 'r', 'd'),    /* producer */
    UdtaPerformerType = MP4_FOUR_CHAR_CODE(169, 'p', 'r', 'f'),   /* performer */
    UdtaRequirementType = MP4_FOUR_CHAR_CODE(169, 'r', 'e', 'q'), /* requirements */
    UdtaSongWritorType = MP4_FOUR_CHAR_CODE(169, 's', 'w', 'f'),  /* songwriter */
    UdtaMovieWritorType = MP4_FOUR_CHAR_CODE(169, 'w', 'r', 't'), /* movie's writer */
    UdtaAlbumArtistType = MP4_FOUR_CHAR_CODE('a', 'A', 'R', 'T'),
    UdtaCompilationType = MP4_FOUR_CHAR_CODE('c', 'p', 'i', 'l'),

    Udta3GppTitleType = MP4_FOUR_CHAR_CODE('t', 'i', 't', 'l'),
    Udta3GppArtistType = MP4_FOUR_CHAR_CODE('p', 'e', 'r', 'f'),
    Udta3GppAuthType = MP4_FOUR_CHAR_CODE('a', 'u', 't', 'h'),
    Udta3GppGenreType = MP4_FOUR_CHAR_CODE('g', 'n', 'r', 'e'),
    Udta3GppAlbumType = MP4_FOUR_CHAR_CODE('a', 'l', 'b', 'm'),
    Udta3GppYearType = MP4_FOUR_CHAR_CODE('y', 'r', 'r', 'c'),

    UdtaID3V2Type = MP4_FOUR_CHAR_CODE('I', 'D', '3', '2'),

    // quick time item metadata, the FOURCC is fake.
    UdtaAuthorType = MP4_FOUR_CHAR_CODE(169, 'f', 'A', 'u'),     /* author fake FOURCC type */
    UdtaRateType = MP4_FOUR_CHAR_CODE(169, 'f', 'R', 'a'),       /* rate fake FOURCC type */
    UdtaCollectionType = MP4_FOUR_CHAR_CODE(169, 'f', 'C', 'l'), /* rate fake FOURCC type */
    UdtaPublisherType = MP4_FOUR_CHAR_CODE(169, 'f', 'P', 's'),  /* rate fake FOURCC type */
    UdtaSoftwareType = MP4_FOUR_CHAR_CODE(169, 'f', 'S', 'w'),   /* software fake FOURCC type */
    UdtaYearType = MP4_FOUR_CHAR_CODE(169, 'f', 'Y', 'r'),       /* year fake FOURCC type */

    UdtaCDTrackNumType = MP4_FOUR_CHAR_CODE(169, 'f', 'C', 't'), /* cd track number type*/
    UdtaCaptureFpsType = MP4_FOUR_CHAR_CODE(169, 'A', 'C', 'f'), /* capture fps fake FOURCC type */
    UdtaAndroidVersionType =
            MP4_FOUR_CHAR_CODE(169, 'A', 'V', 'n'), /* android version fake FOURCC type */

    // HEIF format
    PrimaryItemAtomType = MP4_FOUR_CHAR_CODE('p', 'i', 't', 'm'),
    ItemInfoAtomType = MP4_FOUR_CHAR_CODE('i', 'i', 'n', 'f'),
    InfoEntryAtomType = MP4_FOUR_CHAR_CODE('i', 'n', 'f', 'e'),
    ItemReferenceAtomType = MP4_FOUR_CHAR_CODE('i', 'r', 'e', 'f'),
    ItemPropertiesAtomType = MP4_FOUR_CHAR_CODE('i', 'p', 'r', 'p'),
    ItemPropertyContainerAtomType = MP4_FOUR_CHAR_CODE('i', 'p', 'c', 'o'),
    ItemPropertyAssociationAtomType = MP4_FOUR_CHAR_CODE('i', 'p', 'm', 'a'),
    ItemLocationAtomType = MP4_FOUR_CHAR_CODE('i', 'l', 'o', 'c'),
    ItemDataAtomType = MP4_FOUR_CHAR_CODE('i', 'd', 'a', 't'),
    // HEIF reference chunk type
    DerivedImageAtomType = MP4_FOUR_CHAR_CODE('d', 'i', 'm', 'g'),
    ThumbnailAtomType = MP4_FOUR_CHAR_CODE('t', 'h', 'm', 'b'),
    ContentDescribsAtomType = MP4_FOUR_CHAR_CODE('c', 'd', 's', 'c'),
    AuxiliaryAtomType = MP4_FOUR_CHAR_CODE('a', 'u', 'x', 'l'),
    // HEIF property chunk type
    HEVCConfigurationAtomType = MP4_FOUR_CHAR_CODE('h', 'v', 'c', 'C'),
    ImageSpatialExtentsAtomType = MP4_FOUR_CHAR_CODE('i', 's', 'p', 'e'),
    ImageRotationAtomType = MP4_FOUR_CHAR_CODE('i', 'r', 'o', 't'),
    CleanApertureAtomType = MP4_FOUR_CHAR_CODE('c', 'l', 'a', 'p'),
    // QTColorParameterAtomType  = MP4_FOUR_CHAR_CODE( 'c', 'o', 'l', 'r' ),
    // MP4Av1CAtomType           = MP4_FOUR_CHAR_CODE( 'a', 'v', '1', 'C' ),
};

/* Member "size" changed from u32 to u64. It's the real size for both normal or large atom.
So that "size64" need not be used in later parsing. To minimize change to support file larger than
2GB.*/
#define MP4_BASE_ATOM                                                                     \
    u32 type;                                                                             \
    u8 uuid[16];                                                                          \
    u64 size;                                                                             \
    u64 size64;                                                                           \
    u64 bytesRead;                                                                        \
    u32 bytesWritten;                                                                     \
    char* name;                                                                           \
    struct MP4Atom* super;                                                                \
    MP4Err (*createFromInputStream)(struct MP4Atom * self, struct MP4Atom * proto,        \
                                    /*struct MP4InputStreamRecord* */ char* inputStream); \
    char* (*getName)(struct MP4Atom * self);                                              \
    void (*destroy)(struct MP4Atom * self);

#define MP4_FULL_ATOM \
    MP4_BASE_ATOM     \
    u32 version;      \
    u32 flags;

// u32 chunk_offset;   /* the first chunk offset */

typedef struct MP4Atom {
    MP4_BASE_ATOM
} MP4Atom, *MP4AtomPtr;

typedef struct MP4FullAtom {
    MP4_FULL_ATOM
} MP4FullAtom, *MP4FullAtomPtr;

typedef MP4Err (*cisfunc)(struct MP4Atom* self, struct MP4Atom* proto, char* inputStream);

typedef struct MP4PrivateMovieRecord {
    u32 referenceCount;
    struct MP4InputStreamRecord* inputStream;
    MP4AtomPtr moovAtomPtr; /* 'moov' atom */
    MP4AtomPtr mdat;        /* 'mdat' atom */
    MP4AtomPtr meta;        /* 'meta' atom */
    MP4AtomPtr filetype;
    MP4AtomPtr FreeSpaceAtom[MP4_PARSER_MAX_STREAM];
    // MP4AtomPtr BaseAtomType;
    // MP4AtomPtr FullAtomType;
    u32 fileType; /* the file type: MPEG-4, Motion JPEG-2000, or QuickTime.
                  Some files may not have this atom. */
    void* inMemoryDataHandler;
    MP4AtomPtr sidx; /* 'sidx' atom */
    MP4AtomPtr mfra; /* 'mfra' atom */
    MP4AtomPtr first_moof;
    bool fragmented;
    u64 moofOffset;
    MP4AtomPtr moof; /* 'moof' atom */
    bool isHeif;
    bool isAvif;
    void* itemTable;
} MP4PrivateMovieRecord, *MP4PrivateMovieRecordPtr;

MP4Err MP4CreateBaseAtom(MP4AtomPtr self);
MP4Err MP4CreateFullAtom(MP4AtomPtr s);
MP4Err MP4ParseAtom(struct MP4InputStreamRecord* inputStream, MP4AtomPtr* outAtom);
MP4Err MP4ParseAtomInBuf(struct MP4InputStreamRecord* inputStream, MP4AtomPtr* outAtom,
                         char* headerBuf);
MP4Err MP4SerializeCommonBaseAtomFields(struct MP4Atom* self, char* buffer);
MP4Err MP4SerializeCommonFullAtomFields(struct MP4FullAtom* self, char* buffer);
MP4Err MP4CalculateBaseAtomFieldSize(struct MP4Atom* self);
MP4Err MP4CalculateFullAtomFieldSize(struct MP4FullAtom* self);

void MP4TypeToString(u32 inType, char* ioStr);

typedef struct MP4MediaDataAtom {
    MP4_BASE_ATOM

    // char *data; Ranjeet
    char* data;  // Ranjeet
    u32 dataSize;
} MP4MediaDataAtom, *MP4MediaDataAtomPtr;

typedef struct MP4UnknownAtom {
    MP4_BASE_ATOM
    u32 version;
    u32 flags;
    char* data;
    u32 dataSize;
} MP4UnknownAtom, *MP4UnknownAtomPtr;

typedef struct MP4MovieAtom {
    MP4_BASE_ATOM
    MP4Err (*setupTracks)(struct MP4MovieAtom* self, MP4PrivateMovieRecordPtr moov);
    MP4Err (*SetupReferences)(struct MP4MovieAtom* self);
    u32 (*getTrackCount)(struct MP4MovieAtom* self);
    MP4Err (*getIndTrack)(struct MP4MovieAtom* self, u32 trackNumber, MP4AtomPtr* outTrack);
    MP4Err (*addAtom)(struct MP4MovieAtom* self, MP4AtomPtr atom);
    MP4Err (*getNextTrackID)(struct MP4MovieAtom* self, u32* outID);
    MP4Err (*addTrack)(struct MP4MovieAtom* self, MP4AtomPtr track);
    MP4Err (*calculateDuration)(struct MP4MovieAtom* self);
    MP4Err (*getTimeScale)(struct MP4MovieAtom* self, u32* outTimeScale);
    MP4Err (*getMatrix)(struct MP4MovieAtom* self, u32 outMatrix[9]);
    MP4Err (*getPreferredRate)(struct MP4MovieAtom* self, u32* outRate);
    MP4Err (*getPreferredVolume)(struct MP4MovieAtom* self, s16* outVolume);

    MP4AtomPtr mvhd;        /* the movie header */
    MP4AtomPtr iods;        /* the initial object descriptor */
    MP4AtomPtr ftyp;        /* for JPEG-2000, the file type atom */
    MP4AtomPtr jp2h;        /* for JPEG-2000, the JP2 header atom */
    MP4AtomPtr sgnt;        /* for JPEG-2000, the signature atom */
    MP4AtomPtr udta;        /* user data */
    MP4LinkedList udtaList; /* user data */
    MP4AtomPtr pssh;        /* pssh */
    MP4AtomPtr meta;        /* metadata */
    MP4AtomPtr mvex;        /* the movie extend atom */
    MP4PrivateMovieRecordPtr moov;
    MP4LinkedList atomList;       /* list of all child atoms within it */
    MP4LinkedList trackList;      /* list of 'trak' atoms within it */
} MP4MovieAtom, *MP4MovieAtomPtr; /* 'moov' atom */

typedef struct MP4MovieHeaderAtom {
    MP4_FULL_ATOM
    u64 creationTime;
    u64 modificationTime;
    u32 timeScale;
    s64 duration;
    u32 qt_preferredRate;
    u32 qt_preferredVolume;
    char qt_reserved[10];
    u32 qt_matrixA;
    u32 qt_matrixB;
    u32 qt_matrixU;
    u32 qt_matrixC;
    u32 qt_matrixD;
    u32 qt_matrixV;
    u32 qt_matrixX;
    u32 qt_matrixY;
    u32 qt_matrixW;
    u32 qt_previewTime;
    u32 qt_previewDuration;
    u32 qt_posterTime;
    u32 qt_selectionTime;
    u32 qt_selectionDuration;
    u32 qt_currentTime;
    u32 nextTrackID;
} MP4MovieHeaderAtom, *MP4MovieHeaderAtomPtr;

typedef struct MP4ObjectDescriptorAtom {
    MP4_FULL_ATOM
    MP4Err (*setDescriptor)(struct MP4Atom* self, /*struct MP4DescriptorRecord */ char* desc);
    u32 ODSize;
    struct MP4DescriptorRecord* descriptor;
} MP4ObjectDescriptorAtom, *MP4ObjectDescriptorAtomPtr;

typedef struct MP4TrackAtom {
    MP4_BASE_ATOM
    MP4PrivateMovieRecordPtr moov;
    MP4Err (*addAtom)(struct MP4TrackAtom* self, MP4AtomPtr atom);
    MP4Err (*setMoov)(struct MP4TrackAtom* self, MP4PrivateMovieRecordPtr moov);
    MP4Err (*setMdat)(struct MP4TrackAtom* self, MP4AtomPtr mdat);
    MP4Err (*getEnabled)(struct MP4TrackAtom* self, u32* outEnabled);
    MP4Err (*calculateDuration)(struct MP4TrackAtom* self, u32 movieTimeScale);
    MP4Err (*getDuration)(struct MP4TrackAtom* self, long double* outDuration);
    MP4Err (*getMatrix)(struct MP4TrackAtom* self, u32 outMatrix[9]);
    MP4Err (*getLayer)(struct MP4TrackAtom* self, s16* outLayer);
    MP4Err (*getDimensions)(struct MP4TrackAtom* self, u32* outWidth, u32* outHeight);
    MP4Err (*getVolume)(struct MP4TrackAtom* self, s16* outVolume);

    u32 newTrackFlags;
    MP4AtomPtr mdat;
    MP4AtomPtr udta;            /* to 'udta' atom */
    MP4AtomPtr meta;            /* to 'meta' atom */
    MP4AtomPtr trackHeader;     /* to 'tkhd' atom */
    MP4AtomPtr trackMedia;      /* to 'mdia' atom */
    MP4AtomPtr trackEditAtom;   /* to 'edts' atom */
    MP4AtomPtr trackReferences; /* to 'tref' atom */
    MP4LinkedList atomList;     /* list of its child atoms, in the layout order in file */
    // add for bsac(mutli-slice)
    MP4AtomPtr trak_reference;
    u32 trak_indx;
    u64 elstShiftStartTicks;
    u64 elstInitialEmptyEditTicks;
} MP4TrackAtom, *MP4TrackAtomPtr;

typedef struct MP4TrackHeaderAtom {
    MP4_FULL_ATOM
    u64 creationTime;
    u64 modificationTime;
    u32 trackID;
    u32 qt_reserved1;
    long double duration;
    char qt_reserved2[8];
    u32 qt_layer;
    u32 qt_alternateGroup;
    u32 qt_volume;
    u32 qt_reserved3;
    u32 qt_matrixA;
    u32 qt_matrixB;
    u32 qt_matrixU;
    u32 qt_matrixC;
    u32 qt_matrixD;
    u32 qt_matrixV;
    u32 qt_matrixX;
    u32 qt_matrixY;
    u32 qt_matrixW;
    u32 qt_trackWidth;
    u32 qt_trackHeight;
} MP4TrackHeaderAtom, *MP4TrackHeaderAtomPtr;

typedef struct MP4TrackReferenceAtom {
    MP4_BASE_ATOM
    MP4Err (*addAtom)(struct MP4TrackReferenceAtom* self, MP4AtomPtr atom);
    MP4Err (*findAtomOfType)(struct MP4TrackReferenceAtom* self, u32 atomType, MP4AtomPtr* outAtom);
    MP4LinkedList atomList;

} MP4TrackReferenceAtom, *MP4TrackReferenceAtomPtr;

typedef struct MP4MediaAtom {
    MP4_BASE_ATOM
    MP4Err (*calculateDuration)(struct MP4MediaAtom* self);

    MP4AtomPtr mediaTrack;  /* to parent 'trak' atom */
    MP4AtomPtr mediaHeader; /* to 'mdhd' atom */
    MP4AtomPtr handler;     /* to 'hdlr' atom */
    MP4AtomPtr information; /* to 'minf' atom */
    MP4LinkedList atomList;

    /* for PCM audio, cache can be used to speed up sample reading because one sample is usually
     * very small */
    s32 suggestCache;
    u32 suggestedCacheSize;

    // for sound sample description version 1
    u32 samples_per_packet;
    u32 bytes_per_frame;

    void* parserStream; /* hook to the parser's stream it belongs to,
                        for reverse reference. The creation and detruction
                        need not care about this member*/

} MP4MediaAtom, *MP4MediaAtomPtr;

typedef struct MP4MediaHeaderAtom {
    MP4_FULL_ATOM
    u64 creationTime;
    u64 modificationTime;
    u32 timeScale;
    u64 duration;
    u32 packedLanguage;
    u32 qt_quality;
} MP4MediaHeaderAtom, *MP4MediaHeaderAtomPtr;

typedef struct MP4HandlerAtom {
    MP4_FULL_ATOM

    u32 nameLength;
    u32 qt_componentType;
    u32 handlerType;
    u32 qt_componentManufacturer;
    u32 qt_componentFlags;
    u32 qt_componentFlagsMask;
    char* nameUTF8;
} MP4HandlerAtom, *MP4HandlerAtomPtr; /* hdlr */

typedef struct MP4MediaInformationAtom {
    MP4_BASE_ATOM
    MP4Err (*closeDataHandler)(MP4AtomPtr self);
    MP4Err (*openDataHandler)(MP4AtomPtr self, u32 dataEntryIndex);
    MP4Err (*getMediaDuration)(struct MP4MediaInformationAtom* self, u64* outDuration);
    MP4Err (*testDataEntry)(struct MP4MediaInformationAtom* self, u32 dataEntryIndex);
    MP4AtomPtr dataInformation;
    MP4AtomPtr sampleTable; /* to child atom- sample table 'stbl' */
    MP4AtomPtr mediaHeader;
    struct MP4InputStreamRecord* inputStream;
    void* dataHandler;
    u32 dataEntryIndex;
    MP4LinkedList atomList;
} MP4MediaInformationAtom, *MP4MediaInformationAtomPtr; /* 'minf' */

typedef struct MP4VideoMediaHeaderAtom {
    MP4_FULL_ATOM
    u32 graphicsMode;
    u32 opColorRed;
    u32 opColorGreen;
    u32 opColorBlue;
} MP4VideoMediaHeaderAtom, *MP4VideoMediaHeaderAtomPtr;

typedef struct MP4SoundMediaHeaderAtom {
    MP4_FULL_ATOM
    u32 balance;
    u32 reserved;
} MP4SoundMediaHeaderAtom, *MP4SoundMediaHeaderAtomPtr;

typedef struct MP4HintMediaHeaderAtom {
    MP4_FULL_ATOM
    u32 maxPDUSize;
    u32 avgPDUSize;
    u32 maxBitrate;
    u32 avgBitrate;
    u32 slidingAverageBitrate;
} MP4HintMediaHeaderAtom, *MP4HintMediaHeaderAtomPtr;

typedef struct MP4MPEGMediaHeaderAtom {
    MP4_FULL_ATOM
} MP4MPEGMediaHeaderAtom, *MP4MPEGMediaHeaderAtomPtr;

typedef struct MP4ObjectDescriptorMediaHeaderAtom {
    MP4_FULL_ATOM
} MP4ObjectDescriptorMediaHeaderAtom, *MP4ObjectDescriptorMediaHeaderAtomPtr;

typedef struct MP4ClockReferenceMediaHeaderAtom {
    MP4_FULL_ATOM
} MP4ClockReferenceMediaHeaderAtom, *MP4ClockReferenceMediaHeaderAtomPtr;

typedef struct MP4SceneDescriptionMediaHeaderAtom {
    MP4_FULL_ATOM
} MP4SceneDescriptionMediaHeaderAtom, *MP4SceneDescriptionMediaHeaderAtomPtr;

typedef struct MP4DataInformationAtom {
    MP4_BASE_ATOM
    MP4Err (*getOffset)(struct MP4DataInformationAtom* self, u32 dataReferenceIndex,
                        u32* outOffset);
    MP4Err (*addAtom)(struct MP4DataInformationAtom* self, MP4AtomPtr atom);

    MP4AtomPtr dataReference;
    MP4LinkedList atomList;
} MP4DataInformationAtom, *MP4DataInformationAtomPtr;

#define COMMON_DATAENTRY_ATOM_FIELDS                                      \
    MP4Err (*getOffset)(struct MP4DataEntryAtom * self, u32 * outOffset); \
    MP4AtomPtr mdat;                                                      \
    u32 offset;                                                           \
    u32 locationLength;                                                   \
    char* location;

typedef struct MP4DataEntryAtom {
    MP4_FULL_ATOM
    COMMON_DATAENTRY_ATOM_FIELDS
} MP4DataEntryAtom, *MP4DataEntryAtomPtr;

typedef struct MP4DataEntryURLAtom {
    MP4_FULL_ATOM
    COMMON_DATAENTRY_ATOM_FIELDS
} MP4DataEntryURLAtom, *MP4DataEntryURLAtomPtr;

typedef struct MP4DataEntryURNAtom {
    MP4_FULL_ATOM
    COMMON_DATAENTRY_ATOM_FIELDS
    u32 nameLength;
    char* nameURN;
} MP4DataEntryURNAtom, *MP4DataEntryURNAtomPtr;

typedef struct MP4DataReferenceAtom {
    MP4_FULL_ATOM
    MP4Err (*addDataEntry)(struct MP4DataReferenceAtom* self, MP4AtomPtr entry);
    MP4Err (*getOffset)(struct MP4DataReferenceAtom* self, u32 dataReferenceIndex, u32* outOffset);
    u32 (*getEntryCount)(struct MP4DataReferenceAtom* self);
    MP4Err (*getEntry)(struct MP4DataReferenceAtom* self, u32 dataReferenceIndex,
                       struct MP4DataEntryAtom** outEntry);
    MP4LinkedList atomList;

} MP4DataReferenceAtom, *MP4DataReferenceAtomPtr;

typedef struct MP4SampleTableAtom {
    MP4_BASE_ATOM
    MP4Err (*calculateDuration)(struct MP4SampleTableAtom* self, u64* outDuration);
    MP4Err (*setSampleEntry)(struct MP4SampleTableAtom* self, MP4AtomPtr entry);
    MP4Err (*getCurrentDataReferenceIndex)(struct MP4SampleTableAtom* self,
                                           u32* outDataReferenceIndex);

    MP4AtomPtr TimeToSample;
    MP4AtomPtr CompositionOffset;
    MP4AtomPtr SyncSample; /* sample description, 'stsd' */
    MP4AtomPtr SampleDescription;
    MP4AtomPtr SampleSize;
    MP4AtomPtr CompactSampleSize;
    MP4AtomPtr SampleToChunk;
    MP4AtomPtr ChunkOffset;
    MP4AtomPtr ShadowSync;
    MP4AtomPtr DegradationPriority;

    MP4LinkedList atomList;

    MP4AtomPtr currentSampleEntry;

} MP4SampleTableAtom, *MP4SampleTableAtomPtr; /* stbl */

typedef struct MP4TimeToSampleAtom {
    MP4_FULL_ATOM
    MP4Err (*getTimeForSampleNumber)(MP4AtomPtr self, u32 sampleNumber, u64* outSampleCTS,
                                     s32* outSampleDuration);
    MP4Err (*findSamples)(MP4AtomPtr self, u64 desiredTime, s64* outPriorSample,
                          s64* outExactSample, s64* outNextSample, u32* outSampleNumber,
                          s32* outSampleDuration);
    MP4Err (*getTotalDuration)(struct MP4TimeToSampleAtom* self, u64* outDuration);

    void* foundEntry;     /* pointer to the matched stts entry during previous search. NULL means
                             invalid */
    u32 foundEntryNumber; /* index of the stts entry, 0-based */
    u32 foundEntrySampleNumber; /* 1st sample number of the entry */
    u64 foundEntryTime;         /*start DTS of the entry,  DTS of 1st sample of the entry */

    u64 totalDuration; /* total duration in media time scale */

    u32 entryCount;
    u32 tableSize; /* number of entries loaded into memory */
    u64* sampleDurationEntries;

    u32 first_entry_idx;      /* Index of 1st entry read in memory, 0-based. */
    LONGLONG tab_file_offset; /* Absolute file offset of the entry table */
    void* inputStream;

} MP4TimeToSampleAtom, *MP4TimeToSampleAtomPtr;

typedef struct MP4CompositionOffsetAtom {
    MP4_FULL_ATOM
    MP4Err (*getOffsetForSampleNumber)(MP4AtomPtr self, u32 sampleNumber, s32* outOffset);

    u32 foundEntryNumber;       /* matched entry number in last search, 0-based */
    u32 foundEntrySampleNumber; /* 1st sample number of the entry in last search, 1-based */

    u32 entryCount;
    u32 tableSize; /* number of entries loaded into memory */
    u64* compositionOffsetEntries;

    u32 first_entry_idx;      /* Index of 1st entry read in memory, 0-based. */
    LONGLONG tab_file_offset; /* Absolute file offset of the entry table */
    void* inputStream;

} MP4CompositionOffsetAtom, *MP4CompositionOffsetAtomPtr;

#define COMMON_SAMPLE_ENTRY_FIELDS \
    u32 dataReferenceIndex;        \
    MP4AtomPtr ESDAtomPtr;         \
    MP4AtomPtr AVCCAtomPtr;        \
    MP4AtomPtr AV1CAtomPtr;        \
    MP4AtomPtr HVCCAtomPtr;        \
    MP4AtomPtr H263AtomPtr;        \
    MP4AtomPtr DAMRAtomPtr;        \
    MP4AtomPtr RtpAtomPtr;         \
    MP4AtomPtr WaveAtomPtr;        \
    MP4AtomPtr colrAtomPtr;        \
    MP4AtomPtr DVVCAtomPtr;        \
    MP4AtomPtr VPCCAtomPtr;        \
    MP4AtomPtr PASPAtomPtr;        \
    MP4AtomPtr APVCAtomPtr;

typedef struct GenericSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
} GenericSampleEntryAtom, *GenericSampleEntryAtomPtr;

typedef struct MP4GenericSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved[6];
    char* data;
    u32 dataSize;
} MP4GenericSampleEntryAtom, *MP4GenericSampleEntryAtomPtr;

typedef struct MP4MPEGSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved[6];
} MP4MPEGSampleEntryAtom, *MP4MPEGSampleEntryAtomPtr;

typedef struct GeneralVideoSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    char reserved2[16]; /* uint(32)[4] */

    u32 video_width;
    u32 video_height;

    u32 reserved4; /* uint(32) = 0x0048000 */
    u32 reserved5; /* uint(32) = 0x0048000 */
    u32 reserved6; /* uint(32) = 0 */
    u32 reserved7; /* uint(16) = 1 */
    u32 nameLength;
    char name31[31];
    u32 reserved8; /* uint(16) = 24 */
    s32 reserved9; /* int(16) = -1 */

    u64 skipped_size; /* total size of child atoms skipped */

} GeneralVideoSampleEntryAtom, *GeneralVideoSampleEntryAtomPtr;

typedef struct MP4VisualSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    char reserved2[16]; /* uint(32)[4] */

    u32 video_width;
    u32 video_height;

    u32 reserved4;  /* uint(32) = 0x0048000, horizontal resolution */
    u32 reserved5;  /* uint(32) = 0x0048000, vertical resolution */
    u32 reserved6;  /* uint(32) = 0, MP4 reserved */
    u32 reserved7;  /* uint(16) = 1, frame count */
    u32 nameLength; /* compressor name, total 32 bytes, 1st byte tell the following string length */
    char name31[31];
    u32 reserved8; /* uint(16) = 24 (0x0018), depth. QuickTime allow more value. */
    s32 reserved9; /* int(16) = -1, predefined.
                      Color table ID for QuickTime. -1 means default color table. 0 means a color
                      table is contained within the the sample description itself, immediately
                      following this color table ID field.*/

    u64 skipped_size; /* total size of child atoms skipped */

} MP4VisualSampleEntryAtom, *MP4VisualSampleEntryAtomPtr;

typedef struct MP4MotionJPEGSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    char reserved2[16]; /* uint(32)[4] */

    u32 video_width;
    u32 video_height;

    u32 reserved4; /* uint(32) = 0x0048000 */
    u32 reserved5; /* uint(32) = 0x0048000 */
    u32 reserved6; /* uint(32) = 0 */
    u32 reserved7; /* uint(16) = 1 */
    u32 nameLength;
    char name31[31];
    u32 reserved8; /* uint(16) = 24 */
    s32 reserved9; /* int(16) = -1 */

    u64 skipped_size; /* total size of child atoms skipped */

} MP4MotionJPEGSampleEntryAtom, *MP4MotionJPEGSampleEntryAtomPtr;

typedef struct MP4DivxSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    char reserved2[16]; /* uint(32)[4] */

    u32 video_width;
    u32 video_height;

    u32 reserved4;  /* uint(32) = 0x0048000, horizontal resolution */
    u32 reserved5;  /* uint(32) = 0x0048000, vertical resolution */
    u32 reserved6;  /* uint(32) = 0, MP4 reserved */
    u32 reserved7;  /* uint(16) = 1, frame count */
    u32 nameLength; /* compressor name, total 32 bytes, 1st byte tell the following string length */
    char name31[31];
    u32 reserved8; /* uint(16) = 24 (0x0018), depth. QuickTime allow more value. */
    s32 reserved9; /* int(16) = -1, predefined.
                      Color table ID for QuickTime. -1 means default color table. 0 means a color
                      table is contained within the the sample description itself, immediately
                      following this color table ID field.*/

    u64 skipped_size; /* total size of child atoms skipped */

} MP4DivxSampleEntryAtom, *MP4DivxSampleEntryAtomPtr;

typedef struct MP4AmrSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    u32 version;       /* uint16*/
    char reserved2[6]; /* revison leelve & vendor */
    u32 channels;      /* uint(16) = 2 */
    u32 reserved4;     /* uint(16) = 16 */
    u32 reserved5;     /* uint(32) = 0 */
    u32 timeScale;     /* uint(16) copied from track! */
    u32 reserved6;

} MP4AmrSampleEntryAtom, *MP4AmrSampleEntryAtomPtr;

typedef struct MP4H263SampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    char reserved2[16]; /* uint(32)[4] */
    u32 video_width;
    u32 video_height;
    u32 reserved4; /* uint(32) = 0x0048000 */
    u32 reserved5; /* uint(32) = 0x0048000 */
    u32 reserved6; /* uint(32) = 0 */
    u32 reserved7; /* uint(16) = 1 */
    u32 nameLength;
    char name31[31];
    u32 reserved8; /* uint(16) = 24 */
    s32 reserved9; /* int(16) = -1 */

} MP4H263SampleEntryAtom, *MP4H263SampleEntryAtomPtr;

typedef struct GeneralAudioSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    u32 version;       /* uint16*/
    char reserved2[6]; /* revison leelve & vendor */
    u32 channels;      /* uint(16) = 2 */
    u32 sampleSize;    /* uint(16) = 16, bits per sample */
    u32 reserved5;     /* uint(32) = 0 */
    u32 timeScale;     /* uint(16) copied from track! */
    u32 reserved6;

} GeneralAudioSampleEntryAtom, *GeneralAudioSampleEntryAtomPtr;

typedef struct AC3SampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    u32 version;       /* uint16*/
    char reserved2[6]; /* revison leelve & vendor */
    u32 channels;      /* uint(16) = 2 */
    u32 sampleSize;    /* uint(16) = 16, bits per sample */
    u32 reserved5;     /* uint(32) = 0 */
    u32 timeScale;     /* uint(16) copied from track! */
    u32 reserved6;
    u32 dac3size;  // size of dac3 box
    u32 dac3type;  // dac3 type
    u8 ac3Info[3];
} AC3SampleEntryAtom, *AC3SampleEntryAtomPtr;

typedef struct EC3SampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    u32 version;       /* uint16*/
    char reserved2[6]; /* revison leelve & vendor */
    u32 channels;      /* uint(16) = 2 */
    u32 sampleSize;    /* uint(16) = 16, bits per sample */
    u32 reserved5;     /* uint(32) = 0 */
    u32 timeScale;     /* uint(16) copied from track! */
    u32 reserved6;
    u32 dec3size;   // size of dec3 box
    u32 dec3type;   // dec3 type
    u8 ec3Info[2];  // first byte is for acmod and lfe, last bytes is used to detect additional
                    // channel.
    u8 extension;
} EC3SampleEntryAtom, *EC3SampleEntryAtomPtr;

typedef struct MP4AudioSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    u32 version;         /* uint16*/
    char reserved2[6];   /* revison leelve & vendor */
    u32 channels;        /* uint(16) = 2 */
    u32 sampleSize;      /* uint(16) = 16, bits per sample */
    u32 reserved5;       /* uint(32) = 0 */
    u32 timeScale;       /* uint(16) copied from track! integer part of sample rate (16.16)*/
    u32 reserved6;       /* uint(16) = 0, float part of sample rate (16.16) */
    char* despExtension; /* 16 bytes extensiton for QT version 1 */
    u64 skipped_size;    /* total size of child atoms skipped */
    u8 bIsSmallMp4aAtom; /* not real audio sample, but a small child atom of QT 'wave' atom */
    u8 bIsQTVersionOne;
    u32 samplePerPackage; /* version 2 only: samples per package */
    u32 garbage;          /* there may be garbage data before child atom 'wave' */

} MP4AudioSampleEntryAtom, *MP4AudioSampleEntryAtomPtr;

typedef struct MP4AvcSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    char reserved2[16];

    u32 video_width;
    u32 video_height;

    u32 reserved3;
    u32 reserved4;
    u32 reserved5;
    u16 reserved6;
    u32 nameLength;  // nameLength + name4 + reserved7 -> compressorname, string(32)
    char name4[4];  // actual name string length may larger than 4, only record the 1st 4 characters
    char reserved7[27];
    u32 reserved8;    // depth + predefined
    u64 skipped_size; /* total size of child atoms skipped */
} MP4AvcSampleEntryAtom, *MP4AvcSampleEntryAtomPtr;

typedef struct MP4BitrateAtom {
    MP4_BASE_ATOM
    char reserved1[4];
    u32 maxBitrate;
    u32 avgBitrate;
} MP4BitrateAtom, *MP4BitrateAtomPtr;

typedef struct MP4Av1SampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    char reserved2[16];

    u32 video_width;
    u32 video_height;

    u32 reserved3;
    u32 reserved4;
    u32 reserved5;
    u16 reserved6;
    u32 nameLength;  // nameLength + name4 + reserved7 -> compressorname, string(32)
    char name4[4];  // actual name string length may larger than 4, only record the 1st 4 characters
    char reserved7[27];
    u32 reserved8;    // depth + predefined
    u64 skipped_size; /* total size of child atoms skipped */
    MP4AtomPtr BitrateAtomPtr;
} MP4Av1SampleEntryAtom, *MP4Av1SampleEntryAtomPtr;

typedef struct MP4ApvSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    char reserved2[16];

    u32 video_width;
    u32 video_height;

    u32 reserved3;
    u32 reserved4;
    u32 reserved5;
    u16 reserved6;
    u32 nameLength;  // nameLength + name4 + reserved7 -> compressorname, string(32)
    char name4[4];  // actual name string length may larger than 4, only record the 1st 4 characters
    char reserved7[27];
    u32 reserved8;    // depth + predefined
    u64 skipped_size; /* total size of child atoms skipped */
    MP4AtomPtr BitrateAtomPtr;
} MP4ApvSampleEntryAtom, *MP4ApvSampleEntryAtomPtr;

typedef struct MP4HevcSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    char reserved2[16];

    u32 video_width;
    u32 video_height;

    u32 reserved3;
    u32 reserved4;
    u32 reserved5;
    u16 reserved6;
    u32 nameLength;  // nameLength + name4 + reserved7 -> compressorname, string(32)
    char name4[4];  // actual name string length may larger than 4, only record the 1st 4 characters
    char reserved7[27];
    u32 reserved8;    // depth + predefined
    u64 skipped_size; /* total size of child atoms skipped */
} MP4HevcSampleEntryAtom, *MP4HevcSampleEntryAtomPtr;

typedef struct MP4RtpSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    char reserved2[8];

} MP4RtpSampleEntryAtom, *MP4RtpSampleEntryAtomPtr;

typedef struct MP4Mp3SampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    u32 version;       /* uint16*/
    char reserved2[6]; /* revison leelve & vendor */
    u32 channels;      /* uint(16) = 2 */
    u32 sampleSize;    /* uint(16) = 16 */
    u32 reserved5;     /* uint(32) = 0 */
    u32 timeScale;     /* uint(16) copied from track! */
    u32 reserved6;
    // extra 16 bytes for version 1
    u32 samples_per_packet;
    u32 bytes_per_packet;
    u32 bytes_per_frame;
    u32 bytes_per_sample;

} MP4Mp3SampleEntryAtom, *MP4Mp3SampleEntryAtomPtr;

typedef struct PcmAudioSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];  /* QT general structure of a sample description entry */
    u32 version;        /* uint16*/
    char reserved2[6];  /* revison leelve & vendor */
    u32 channels;       /* uint16*/
    u32 bitsPerSample;  /* uint16, audio sample size, in bits */
    char reserved3[4];  /* compression ID + packet size */
    u32 timeScale;      /* integer part of 16.16 sample rate */
    u32 reserved4;      /* float part of 16.16 sample rate */
    char reserved5[16]; /* extension for version 1 */
    // u32     skipped_size;
    u32 decoderSpecificInfoSize;
    u8* decoderSpecificInfo;

} PcmAudioSampleEntryAtom, *PcmAudioSampleEntryAtomPtr; /* QT sowt or twos audio sample entry
                                                        (uncompressed audio, sowt is little-endian
                                                        and twos is big endian) */

typedef struct AdpcmSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6]; /* QT general structure of a sample description entry */
    u32 version;       /* uint16*/
    char reserved2[6]; /* revison leelve & vendor */
    u32 channels;      /* uint16*/
    u32 bitsPerSample; /* uint16, audio sample size, in bits */
    char reserved3[4]; /* compression ID + packet size */
    u32 timeScale;     /* integer part of 16.16 sample rate */
    u32 reserved4;     /* float part of 16.16 sample rate */
    // extra 16 bytes for version 1
    u32 samples_per_packet;
    u32 bytes_per_packet;
    u32 bytes_per_frame;
    u32 bytes_per_sample;

    u32 decoderSpecificInfoSize;
    u8* decoderSpecificInfo;

} AdpcmSampleEntryAtom, *AdpcmSampleEntryAtomPtr; /* IMA ADPCM audio sample entry */

/* 3GPP timed text sample entry, same as GenericSampleEntryAtom now */
typedef struct TimedTextSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved[6];
    char* data;
    u32 dataSize;
} TimedTextSampleEntryAtom, *TimedTextSampleEntryAtomPtr;

typedef struct MetadataSampleEntryAtom {
    MP4_BASE_ATOM
    u32 dataReferenceIndex;
    char* data;
    u32 dataSize;
} MetadataSampleEntryAtom, *MetadataSampleEntryAtomPtr;

typedef struct MP4Vp9SampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    char reserved2[16];

    u32 video_width;
    u32 video_height;

    u32 reserved3;
    u32 reserved4;
    u32 reserved5;
    u16 reserved6;
    u32 nameLength;  // nameLength + name4 + reserved7 -> compressorname, string(32)
    char name4[4];  // actual name string length may larger than 4, only record the 1st 4 characters
    char reserved7[27];
    u32 reserved8;    // depth + predefined
    u64 skipped_size; /* total size of child atoms skipped */
    MP4AtomPtr BitrateAtomPtr;
} MP4Vp9SampleEntryAtom, *MP4Vp9SampleEntryAtomPtr;

typedef struct MP4PASPAtom {
    MP4_BASE_ATOM
    u32 hSpacing;
    u32 vSpacing;
} MP4PASPAtom, *MP4PASPAtomPtr;

typedef struct MP4SampleDescriptionAtom {
    MP4_FULL_ATOM

    MP4Err (*addEntry)(struct MP4SampleDescriptionAtom* self, MP4AtomPtr entry);
    u32 (*getEntryCount)(struct MP4SampleDescriptionAtom* self);
    MP4Err (*getEntry)(struct MP4SampleDescriptionAtom* self, u32 entryNumber,
                       struct GenericSampleEntryAtom** outEntry);

    MP4LinkedList atomList; /* all the child A/V/Hint sample entry atoms */
} MP4SampleDescriptionAtom, *MP4SampleDescriptionAtomPtr; /* 'stsd' atom */

typedef struct MP4ESDAtom {
    MP4_FULL_ATOM
    u32 descriptorLength;
    struct MP4DescriptorRecord* descriptor;
} MP4ESDAtom, *MP4ESDAtomPtr;

typedef struct MP4AVCCAtom {
    MP4_BASE_ATOM
    u8* data;
    u32 dataSize;
} MP4AVCCAtom, *MP4AVCCAtomPtr;

typedef struct MP4HVCCAtom {
    MP4_BASE_ATOM
    u8* data;
    u32 dataSize;
} MP4HVCCAtom, *MP4HVCCAtomPtr;

typedef struct MP4VPCCAtom {
    MP4_BASE_ATOM
    u8* data;
    u32 dataSize;
} MP4VPCCAtom, *MP4VPCCAtomPtr;

typedef struct MP4APVCAtom {
    MP4_BASE_ATOM
    u8* data;
    u32 dataSize;
} MP4APVCAtom, *MP4APVCAtomPtr;

typedef struct MP4H263Atom {
    MP4_BASE_ATOM
    u8* data;
    u32 dataSize;
} MP4H263Atom, *MP4H263AtomPtr;

typedef struct MP4DAMRAtom {
    MP4_BASE_ATOM
    u8* data;
    u32 dataSize;
} MP4DAMRAtom, *MP4DAMRAtomPtr;

typedef struct MP4RtpAtom {
    MP4_BASE_ATOM
    u8* data;
    u32 dataSize;
} MP4RtpAtom, *MP4RtpAtomPtr;

typedef struct MP4SampleSizeAtom {
    MP4_FULL_ATOM
    MP4Err (*getSampleSize)(MP4AtomPtr self, u32 sampleNumber, u32* outSize);
    MP4Err (*getSampleSizeAndOffset)(MP4AtomPtr self, u32 sampleNumber, u32* outSize,
                                     u32 startingSampleNumber, u32* outOffsetSize);
    MP4Err (*finishScan)(MP4AtomPtr self);

    u32 sampleSize; /* Sample size in bytes.0 for variable size samples.*/

    u32 sampleCount;
    u32 tableSize; /* number of entries loaded into memory */
    u32* sizes;    /* Pointer to sample size entries read in memory */

    u32 scanFinished;    /* whethe the whole index table has been scanned,
                          max sample size & totalBytes are valid now.
                          Parser does not necessarily scan the whole table at the loading phase.*/
    u32 maxSampleSize;   /* Max sample size, in bytes. */
    LONGLONG totalBytes; /* Total bytes of all samples */

    u32 first_entry_idx;      /* Index of 1st entry read in memory, 0-based. */
    LONGLONG tab_file_offset; /* Absolute file offset of the entry table */
    void* inputStream;
    u32 start_sample_number; /* corresponding chunk start sample number of first_entry_offset. */
    LONGLONG first_entry_offset; /* Absolute file offset of the 1st entry read in memory */
} MP4SampleSizeAtom, *MP4SampleSizeAtomPtr; /* 'stsz' atom */

typedef struct MP4CompactSampleSizeAtom {
    MP4_FULL_ATOM
    MP4Err (*getSampleSize)(MP4AtomPtr self, u32 sampleNumber, u32* outSize);
    MP4Err (*getSampleSizeAndOffset)(MP4AtomPtr self, u32 sampleNumber, u32* outSize,
                                     u32 startingSampleNumber, u32* outOffsetSize);
    MP4Err (*finishScan)(MP4AtomPtr self);

    char reserved1[3];
    u8 fieldSize; /* Field size in bits.*/

    u32 sampleCount;
    u32 tableSize; /* number of entries loaded into memory */
    u8* sizes;     /* Pointer to sample size entries read in memory */

    u32 scanFinished;    /* whethe the whole index table has been scanned,
                          max sample size & totalBytes are valid now.
                          Parser does not necessarily scan the whole table at the loading phase.*/
    u32 maxSampleSize;   /* Max sample size, in bytes. */
    LONGLONG totalBytes; /* Total bytes of all samples */

    u32 first_entry_idx;      /* Index of 1st entry read in memory, 0-based. */
    LONGLONG tab_file_offset; /* Absolute file offset of the entry table */
    void* inputStream;
    u32 start_sample_number; /* corresponding chunk start sample number of first_entry_offset. */
    LONGLONG first_entry_offset; /* Absolute file offset of the 1st entry read in memory */
} MP4CompactSampleSizeAtom, *MP4CompactSampleSizeAtomPtr; /* 'stz2' atom */

typedef struct MP4ChunkOffsetAtom {
    MP4_FULL_ATOM
    MP4Err (*getChunkOffset)(MP4AtomPtr self, u32 chunkNumber, u64* outOffset);
    MP4Err (*addOffset)(struct MP4ChunkOffsetAtom* self, u32 offset);

    u32 firstChunkOffset; /* file offset of 1st chunk of this track. Will decide movie is
                             interleaved or not! */

    u32 entryCount;
    u32 tableSize; /* number of entries loaded into memory */
    u32* offsets;  /* entries loaded in memory */

    u32 first_entry_idx;      /* Index of 1st entry read in memory, 0-based. */
    LONGLONG tab_file_offset; /* Absolute file offset of the entry table */
    void* inputStream;

} MP4ChunkOffsetAtom, *MP4ChunkOffsetAtomPtr; /* 'stco' atom */

typedef struct MP4ChunkLargeOffsetAtom {
    MP4_FULL_ATOM
    MP4Err (*getChunkOffset)(MP4AtomPtr self, u32 chunkNumber, u64* outOffset);

    u64 firstChunkOffset;  //??? u32 or u64 /* file offset of 1st chunk of this track. Will decide
                           //movie is interleaved or not! */

    u32 entryCount;
    u32 tableSize; /* number of entries loaded into memory */
    u64* offsets;

    u32 first_entry_idx;      /* Index of 1st entry read in memory, 0-based. */
    LONGLONG tab_file_offset; /* Absolute file offset of the entry table */
    void* inputStream;
} MP4ChunkLargeOffsetAtom, *MP4ChunkLargeOffsetAtomPtr;

typedef struct MP4SampleToChunkAtom {
    MP4_FULL_ATOM
    MP4Err (*lookupSample)(MP4AtomPtr self, u32 sampleNumber, u32* outChunkNumber,
                           u32* outSampleDescriptionIndex, u32* outFirstSampleNumberInChunk,
                           u32* lastSampleNumberInChunk);
    MP4Err (*setEntry)(struct MP4SampleToChunkAtom* self, u32 beginningSampleNumber,
                       u32 sampleCount, u32 sampleDescriptionIndex);
    u32 (*getEntryCount)(struct MP4SampleToChunkAtom* self);

    u32 foundEntryNumber;            /* Number of the matched entry, 0-based, in last search */
    u32 foundEntryFirstSampleNumber; /* 1-based */

    u32 entryCount;
    u32 tableSize; /* number of entries loaded into memory */
    u32* sampleToChunkEntries;

    u32 first_entry_idx;      /* Index of 1st entry read in memory, 0-based. */
    LONGLONG tab_file_offset; /* Absolute file offset of the entry table */
    void* inputStream;

} MP4SampleToChunkAtom, *MP4SampleToChunkAtomPtr;

typedef struct MP4SyncSampleAtom {
    MP4_FULL_ATOM
    MP4Err (*isSyncSample)(MP4AtomPtr self, u32 sampleNumber, u32* outSync);
    MP4Err (*nextSyncSample)(MP4AtomPtr s, u32 sampleNumber, u32* outSync, s32 forward);

    u32 entryCount;
    u32 tableSize;      /* number of entries loaded into memory */
    u32* sampleNumbers; /* entries loaded in memory */
    u32 nonSyncFlag;

    /* for speeding up searching - cache the nearest PREVIOUS sync sample of latest searching. */
    u32 cached_entry_idx;          /* entry index, 0-based */
    u32 cached_sync_sample_number; /* sync sample number, 1-based*/

    u32 first_entry_idx;      /* Index of 1st entry read in memory, 0-based. */
    LONGLONG tab_file_offset; /* Absolute file offset of the entry table */
    void* inputStream;
} MP4SyncSampleAtom, *MP4SyncSampleAtomPtr;

typedef struct MP4ShadowSyncAtom {
    MP4_FULL_ATOM
    u32 entryCount;
    void* entries;
} MP4ShadowSyncAtom, *MP4ShadowSyncAtomPtr;

typedef struct MP4DegradationPriorityAtom {
    MP4_FULL_ATOM
    u32 entryCount;
    u32* priorities;
} MP4DegradationPriorityAtom, *MP4DegradationPriorityAtomPtr;

typedef struct MP4FreeSpaceAtom {
    MP4_BASE_ATOM
    char* data;
    u32 dataSize;
} MP4FreeSpaceAtom, *MP4FreeSpaceAtomPtr;

typedef struct MP4EditAtom {
    MP4_BASE_ATOM
    MP4Err (*addAtom)(struct MP4EditAtom* self, MP4AtomPtr atom);
    MP4Err (*getEffectiveDuration)(struct MP4EditAtom* self, u64* outDuration);

    MP4LinkedList atomList;
    MP4AtomPtr editListAtom;
} MP4EditAtom, *MP4EditAtomPtr;

typedef struct MP4EditListAtom {
    MP4_FULL_ATOM

    MP4Err (*getTrackOffset)(struct MP4EditListAtom* self, u32* outTrackStartTime);

    MP4Err (*isEmptyEdit)(struct MP4EditListAtom* self, u32 segmentNumber, u32* outIsEmpty);

    MP4Err (*getEffectiveDuration)(struct MP4EditListAtom* self, u64* outDuration);
    MP4Err (*getIndSegmentTime)(MP4AtomPtr self, u32 segmentIndex, /* one based */
                                u64* outSegmentMovieTime, s64* outSegmentMediaTime,
                                u64* outSegmentDuration /* in movie's time scale */
    );
    MP4Err (*getTimeAndRate)(MP4AtomPtr self, u64 movieTime, u32 movieTimeScale, u32 mediaTimeScale,
                             s64* outMediaTime, u32* outMediaRate, u64* outPriorSegmentEndTime,
                             u64* outNextSegmentBeginTime);
    u32 (*getEntryCount)(struct MP4EditListAtom* self);
    MP4LinkedList entryList;
} MP4EditListAtom, *MP4EditListAtomPtr;

typedef struct UserDataRecord {
    u32 type;
    MP4LinkedList list;
} UserDataRecord, *UserDataRecordPtr;

typedef struct UserDataItem {
    u32 format;
    void* data;
    u32 size;
} UserDataItem, *UserDataItemPtr;

typedef struct MP4UserDataAtom {
    MP4_BASE_ATOM
    MP4Err (*addUserData)(struct MP4UserDataAtom* self, MP4Handle userDataH, u32 userDataType,
                          u32* outIndex);
    MP4Err (*getEntryCount)(struct MP4UserDataAtom* self, u32 userDataType, u32* outCount);
    MP4Err (*getIndType)(struct MP4UserDataAtom* self, u32 typeIndex, u32* outType);
    MP4Err (*getItem)(struct MP4UserDataAtom* self, MP4Handle userDataH, u32 userDataType,
                      u32 itemIndex);
    MP4Err (*getTypeCount)(struct MP4UserDataAtom* self, u32* outCount);
    MP4Err (*appendItemList)(struct MP4UserDataAtom* self, MP4LinkedList list);
    MP4LinkedList recordList;
    MP4AtomPtr meta; /* 'meta' atom */

    /* engr94385 */
    char* data; /* for small 'udat' atom */
    u32 dataSize;
} MP4UserDataAtom, *MP4UserDataAtomPtr;

typedef struct MP4MetadataAtom {
    MP4_FULL_ATOM
    MP4LinkedList atomList;
    MP4AtomPtr handler;  /* 'hdlr' atom */
    MP4AtomPtr itemKeys; /* 'keys' atom */
    MP4AtomPtr itemList; /* 'ilst' atom */
    MP4AtomPtr id32;     /* 'id32' atom */
    void* itemTable;
} MP4MetadataAtom, *MP4MetadataAtomPtr;

typedef struct MP4MetadataItemKeysAtom {
    MP4_FULL_ATOM
    MP4Err (*getTypebyKeyIndex)(struct MP4Atom* self, u32 index, u32* type);
    u32 entryCount;
    MP4LinkedList keyList;
} MP4MetadataItemKeysAtom, *MP4MetadataItemKeysAtomPtr;

typedef struct MP4MetadataItemListAtom {
    MP4_BASE_ATOM
    MP4Err (*appendItemList)(struct MP4MetadataItemListAtom* self, MP4LinkedList list);
    MP4LinkedList atomList;
    MP4AtomPtr parentAtom;
} MP4MetadataItemListAtom, *MP4MetadataItemListAtomPtr;

typedef struct MP4NameAtom {
    MP4_FULL_ATOM
    char* data;
    u32 dataSize; /* size of the string size in *data */
} MP4NameAtom, *MP4NameAtomPtr;

typedef struct MP4MeanAtom {
    MP4_FULL_ATOM
    char* data;
    u32 dataSize; /* size of the string size in *data */
} MP4MeanAtom, *MP4MeanAtomPtr;

typedef struct MP4MetadataItemAtom {
    MP4_BASE_ATOM
    MP4LinkedList atomList;
    MP4AtomPtr itemInfo; /* 'itif' atom */
    MP4AtomPtr dataName; /* 'name' atom */
    MP4AtomPtr data;     /* 'data' atom */
    MP4AtomPtr mean;     /* 'mean' atom */
} MP4MetadataItemAtom, *MP4MetadataItemAtomPtr;

typedef struct MP4ValueAtom {
    MP4_BASE_ATOM
    u32 valueType;
    u32 countryCode;
    u32 languageCode;
    char* data;
    u32 dataSize; /* size of the string size in *data */
} MP4ValueAtom, *MP4ValueAtomPtr;

typedef struct MP4UserDataEntryAtom {
    MP4_BASE_ATOM

    // fix ENGR00214496, move data, dataSize right after MP4_BASE_ATOM
    // When create UserDataAtom, some user data may not defined in our MP4Atoms.h
    // So will create using unknown type. But in appendItemList, the atom is transcribed as
    // MP4UserDataEntryAtom, so memcheck failed on item->data = atom->data;
    char* data;
    u32 dataSize; /* size of the string size in *data */

    MP4Err (*serialize)(struct MP4Atom* self, char* buffer);
    MP4Err (*calculateSize)(struct MP4Atom* self);

    u32 stringSize;   /* 16-bit, size defined by quick time, different from what I found in clip, so
                         not used. Amanda */
    u32 languageCode; /* 16-bit */
                      //  char *data;
                      //  u32 dataSize; /* size of the string size in *data */
} MP4UserDataEntryAtom, *MP4UserDataEntryAtomPtr; /* entry atom in the user data atom */

typedef struct MP4UserData3GppAtom {
    MP4_BASE_ATOM
    char* data;
    u32 dataSize; /* size of the string size in *data */

    MP4Err (*serialize)(struct MP4Atom* self, char* buffer);
    MP4Err (*calculateSize)(struct MP4Atom* self);

    u32 stringSize;
    u32 languageCode;
} MP4UserData3GppAtom, *MP4UserData3GppAtomPtr;

typedef struct MP4UserDataID3v2Atom {
    MP4_BASE_ATOM
    char* data;
    u32 dataSize; /* size of the string size in *data */
    MP4Err (*appendItemList)(struct MP4UserDataID3v2Atom* self, MP4LinkedList list);
    MP4LinkedList atomList;
    MP4AtomPtr parentAtom;

} MP4UserDataID3v2Atom, *MP4UserDataID3v2AtomPtr;

typedef struct MP4CopyrightAtom {
    MP4_FULL_ATOM
    u32 packedLanguageCode;
    char* notice;
} MP4CopyrightAtom, *MP4CopyrightAtomPtr;

typedef struct MP4TrackReferenceTypeAtom {
    MP4_BASE_ATOM
    MP4Err (*addTrackID)(struct MP4TrackReferenceTypeAtom* self, u32 trackID);
    u32 trackIDCount;
    u32* trackIDs;
} MP4TrackReferenceTypeAtom, *MP4TrackReferenceTypeAtomPtr;

#define ALAC_SPECIFIC_INFO_SIZE (36)

typedef struct QTsiDecompressParaAtom /* 'wave' atom, QT spec p120*/
{
    MP4_BASE_ATOM
    MP4AtomPtr ESDAtomPtr;          /* child 'esds' atom */
    MP4AtomPtr TerminatorAtomPtr;   /* child terminator atom */
    MP4AtomPtr OriginFormatAtomPtr; /* child origin format atom */
    u8 AlacInfo[ALAC_SPECIFIC_INFO_SIZE];
    u64 skipped_size; /* total size of child atoms skipped */
} QTsiDecompressParaAtom, *QTsiDecompressParaAtomPtr;

#define QUICKTIME_COLOR_PARAMETER_FIRST

typedef struct QTColorParameterAtom /* 'colr' atom, QT spec p102, another definition of
                                       "MJ2ColorSpecificationAtomType" */
{
    MP4_BASE_ATOM
    u32 colorParamType;
    u32 primariesIndex; /*ENGR98463: define all 16 bytes fields to be u32, to avoid unaligned memory
                           access. See "read16()" */
    u32 transferFuncIndex;
    u32 matrixIndex;
    u8 full_range_flag;  // read according to ISO_IEC_14496-12, ColourInfomationBox('colr')
} QTColorParameterAtom, *QTColorParameterAtomPtr;

typedef struct MP4TrackExtendsAtom {
    MP4_FULL_ATOM
    u32 track_ID;
    u32 default_sample_description_index;
    u32 default_sample_duration;
    u32 default_sample_size;
    u32 default_sample_flags;
} MP4TrackExtendsAtom, *MP4TrackExtendsAtomPtr;

typedef struct MP4MovieExtendsHeaderAtom {
    MP4_FULL_ATOM
    u64 fragment_duration;
} MP4MovieExtendsHeaderAtom, *MP4MovieExtendsHeaderAtomPtr;

typedef struct MP4MovieExtendsAtom {
    MP4_BASE_ATOM
    MP4Err (*addAtom)(struct MP4MovieExtendsAtom* self, MP4AtomPtr atom);
    MP4Err (*getTrex)(struct MP4MovieExtendsAtom* self, u32 track_ID,
                      MP4TrackExtendsAtomPtr* outTrack);
    MP4AtomPtr mehd;
    MP4LinkedList atomList; /* list of all child atoms within it */
    MP4LinkedList trexList; /* list of 'trex' atoms within it */
} MP4MovieExtendsAtom, *MP4MovieExtendsAtomPtr;

typedef struct MP4TrackFragmentHeaderAtom {
    MP4_FULL_ATOM
    u32 track_ID;
    u64 base_data_offset;
    u32 sample_description_index;
    u32 default_sample_duration;
    u32 default_sample_size;
    u32 default_sample_flags;
    MP4Err (*getInfo)(struct MP4TrackFragmentHeaderAtom* self, u64* base_data_offset,
                      u32* default_sample_duration, u32* default_sample_size,
                      u32* default_sample_flags);
} MP4TrackFragmentHeaderAtom, *MP4TrackFragmentHeaderAtomPtr;

#define FRAGMENT_SAMPLE_DRM_FLAG_FULL 1
#define FRAGMENT_SAMPLE_DRM_FLAG_SUB 2
#define FRAGMENT_SAMPLE_DRM_MAX_SUBSAMPLE 16

typedef struct MP4TrackFragmentSample {
    u64 offset;
    u32 size;
    u32 duration;
    int64 ts; /* in units of timescale */
    u32 flags;
    int32 compositionOffset;
    u32 drm_flags;  // 1: full sample encryption, 2: sub sample encryption
    u8 iv[16];
    u32 clearsSize;
    u32 encryptedSize;
    u32 clearArray[FRAGMENT_SAMPLE_DRM_MAX_SUBSAMPLE];
    u32 encryptedArray[FRAGMENT_SAMPLE_DRM_MAX_SUBSAMPLE];
} MP4TrackFragmentSample;

typedef struct MP4TrackFragmentRunAtom {
    MP4_FULL_ATOM
    u32 sample_count;
    u32 data_size;
    s32 data_offset;
    u32 sample_size;
    u32 first_sample_flags;
    MP4TrackFragmentSample* sample;
} MP4TrackFragmentRunAtom, *MP4TrackFragmentRunAtomPtr;

typedef struct MP4TrackFragmentDecodeTimeAtom {
    MP4_FULL_ATOM
    u64 time;
} MP4TrackFragmentDecodeTimeAtom, *MP4TrackFragmentDecodeTimeAtomPtr;

typedef struct MP4TrackFragmentAtom {
    MP4_BASE_ATOM
    MP4AtomPtr tfhd;
    MP4AtomPtr tfdt;
    MP4AtomPtr saiz;
    MP4AtomPtr saio;
    MP4AtomPtr senc;
    MP4Err (*getTrun)(struct MP4TrackFragmentAtom* self, u32 TrunID, MP4AtomPtr* outTrun);
    u32 (*getTrunCount)(struct MP4TrackFragmentAtom* self);
    MP4Err (*addAtom)(struct MP4TrackFragmentAtom* self, MP4AtomPtr atom);
    MP4LinkedList atomList; /* list of all child atoms within it */
    MP4LinkedList trunList; /* list of trun child atoms within it */
} MP4TrackFragmentAtom, *MP4TrackFragmentAtomPtr;

typedef struct MP4MovieFragmentAtom {
    MP4_BASE_ATOM
    s64 start_offset;
    s64 end_offset;
    MP4Err (*getStartOffset)(struct MP4MovieFragmentAtom* self, s64* outOffset);
    MP4Err (*getEndOffset)(struct MP4MovieFragmentAtom* self, s64* outOffset);
    MP4Err (*addAtom)(struct MP4MovieFragmentAtom* self, MP4AtomPtr atom);
    u32 (*getTrackCount)(struct MP4MovieFragmentAtom* self);
    MP4Err (*getTrack)(struct MP4MovieFragmentAtom* self, u32 TrackID, MP4AtomPtr* outTrack);
    MP4LinkedList atomList; /* list of all child atoms within it */
    MP4LinkedList trackList;
} MP4MovieFragmentAtom, *MP4MovieFragmentAtomPtr;

typedef struct MP4SegmentIndexAtom {
    MP4_FULL_ATOM
    u32 id;
    u16 entryCount;
    u64 duration;
    u32* sampleNumbers; /* entries loaded in memory */
    MP4Err (*seek)(struct MP4SegmentIndexAtom* self, u32 flags, uint64* targetTime, u64* offset);
    MP4Err (*nextIndex)(struct MP4SegmentIndexAtom* self, uint64* targetTime, u64* offset);
    MP4Err (*previousIndex)(struct MP4SegmentIndexAtom* self, uint64* targetTime, u64* offset);
} MP4SegmentIndexAtom, *MP4SegmentIndexAtomPtr;

typedef struct MP4TrackFragmentRandomAccessAtom {
    MP4_FULL_ATOM
    u32 id;
    u32 number_of_entry;
    u32 sample_size;
    u32 trun_size;
    u32 traf_size;
    u32 sample_count;
    u64 last_ts;
    u64 last_offset;
    void* sample;
    MP4Err (*seek)(struct MP4TrackFragmentRandomAccessAtom* self, u32 flags, uint64* targetTime,
                   u64* offset);
    MP4Err (*nextIndex)(struct MP4TrackFragmentRandomAccessAtom* self, uint64* targetTime,
                        u64* offset);
    MP4Err (*previousIndex)(struct MP4TrackFragmentRandomAccessAtom* self, uint64* targetTime,
                            u64* offset);
} MP4TrackFragmentRandomAccessAtom, *MP4TrackFragmentRandomAccessAtomPtr;

typedef struct MP4MovieFragmentRandomAccessOffsetAtom {
    MP4_FULL_ATOM
    u32 mfra_size;
} MP4MovieFragmentRandomAccessOffsetAtom, *MP4MovieFragmentRandomAccessOffsetAtomPtr;

typedef struct MP4MovieFragmentRandomAccessAtom {
    MP4_BASE_ATOM
    MP4AtomPtr mfro;
    MP4LinkedList atomList; /* list of all child atoms within it */
    MP4LinkedList tfraList; /* list of tfra child atoms within it */
    MP4Err (*addAtom)(struct MP4MovieFragmentRandomAccessAtom* self, MP4AtomPtr atom);
    u32 (*getTrackCount)(struct MP4MovieFragmentRandomAccessAtom* self);
    MP4Err (*getTrack)(struct MP4MovieFragmentRandomAccessAtom* self, u32 TrackID,
                       MP4AtomPtr* outTrack);
} MP4MovieFragmentRandomAccessAtom, *MP4MovieFragmentRandomAccessAtomPtr;

typedef struct MP4PSSHAtom {
    MP4_FULL_ATOM
    u8 uid[16];
    u32 dataLen;
    void* data;
    u32 totalSize;
    void* totalData;
    MP4Err (*appendPssh)(struct MP4PSSHAtom* self, struct MP4PSSHAtom* inAtom);
} MP4PSSHAtom, *MP4PSSHAtomPtr;

typedef struct MP4ProtectedVideoSampleAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    char reserved2[16]; /* uint(32)[4] */

    u32 video_width;
    u32 video_height;

    u32 reserved4;  /* uint(32) = 0x0048000, horizontal resolution */
    u32 reserved5;  /* uint(32) = 0x0048000, vertical resolution */
    u32 reserved6;  /* uint(32) = 0, MP4 reserved */
    u32 reserved7;  /* uint(16) = 1, frame count */
    u32 nameLength; /* compressor name, total 32 bytes, 1st byte tell the following string length */
    char name31[31];
    u32 reserved8; /* uint(16) = 24 (0x0018), depth. QuickTime allow more value. */
    s32 reserved9; /* int(16) = -1, predefined. */
    MP4AtomPtr sinf;
    MP4LinkedList atomList;
    MP4Err (*addAtom)(struct MP4ProtectedVideoSampleAtom* self, MP4AtomPtr atom);
    MP4Err (*getOriginFormat)(struct MP4ProtectedVideoSampleAtom* self, u32* type);
} MP4ProtectedVideoSampleAtom, *MP4ProtectedVideoSampleAtomPtr;

typedef struct MP4ProtectedAudioSampleAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    u32 version;       /* uint16*/
    char reserved2[6]; /* revison leelve & vendor */
    u32 channels;      /* uint(16) = 2 */
    u32 sampleSize;    /* uint(16) = 16, bits per sample */
    u32 reserved5;     /* uint(32) = 0 */
    u32 timeScale;     /* uint(16) copied from track! */
    u32 reserved6;
    MP4AtomPtr sinf;
    MP4LinkedList atomList;
    MP4Err (*addAtom)(struct MP4ProtectedAudioSampleAtom* self, MP4AtomPtr atom);
    MP4Err (*getOriginFormat)(struct MP4ProtectedAudioSampleAtom* self, u32* type);
} MP4ProtectedAudioSampleAtom, *MP4ProtectedAudioSampleAtomPtr;

typedef struct MP4ProtectionSchemeInfoAtom {
    MP4_BASE_ATOM
    MP4AtomPtr frma;
    MP4AtomPtr schm;
    MP4AtomPtr schi;
    MP4LinkedList atomList;
    MP4Err (*addAtom)(struct MP4ProtectionSchemeInfoAtom* self, MP4AtomPtr atom);
} MP4ProtectionSchemeInfoAtom, *MP4ProtectionSchemeInfoAtomPtr;

typedef struct MP4OriginFormatAtom {
    MP4_BASE_ATOM
    u32 format;
} MP4OriginFormatAtom, *MP4OriginFormatAtomPtr;

typedef struct MP4SchemeTypeAtom {
    MP4_FULL_ATOM
    u32 scheme_type;
    u32 scheme_version;
    void* uri;
    u32 mode;
    bool bSubsampleEncryption;
} MP4SchemeTypeAtom, *MP4SchemeTypeAtomPtr;

typedef struct MP4SchemeInfoAtom {
    MP4_BASE_ATOM
    MP4AtomPtr tenc;
    MP4LinkedList atomList;
    MP4Err (*addAtom)(struct MP4SchemeInfoAtom* self, MP4AtomPtr atom);
} MP4SchemeInfoAtom, *MP4SchemeInfoAtomPtr;

typedef struct MP4TrackEncryptionAtom {
    MP4_FULL_ATOM
    u8 default_IsEncrypted_bytes[3];
    u32 default_IsEncrypted;
    u32 origin_default_IsEncrypted;
    u8 default_IV_size;
    u8 default_KID[16];
    u8 default_EnctyptedByteBlock;
    u8 default_SkipByteBlock;
    u8 default_key_iv_Length;
    u8* default_key_constant_iv;
} MP4TrackEncryptionAtom, *MP4TrackEncryptionAtomPtr;

typedef struct MP4SampleAuxiliaryInfoSizeAtom {
    MP4_FULL_ATOM
    u32 aux_info_type;
    u32 aux_info_type_parameter;
    u8 default_sample_info_size;
    u32 sample_count;
    u8* sample_info_size;
    MP4Err (*getSampleSize)(struct MP4SampleAuxiliaryInfoSizeAtom* self, u32 sampleNumber,
                            u32* outSize);
} MP4SampleAuxiliaryInfoSizeAtom, *MP4SampleAuxiliaryInfoSizeAtomPtr;

typedef struct MP4SampleAuxiliaryInfoOffsetsAtom {
    MP4_FULL_ATOM
    u32 aux_info_type;
    u32 aux_info_type_parameter;
    u32 entry_count;
    u32* offsets_u32;
    u64* offsets_u64;
    MP4Err (*getSampleOffsets)(struct MP4SampleAuxiliaryInfoOffsetsAtom* self, u32 sampleNumber,
                               u64* outOffset);
} MP4SampleAuxiliaryInfoOffsetsAtom, *MP4SampleAuxiliaryInfoOffsetsAtomPtr;

typedef struct MP4SampleEncryptionAtom {
    MP4_FULL_ATOM
    u32 sample_count;
    u32 dataSize;
    u8* data;

    bool bSubsampleEncryption;
    MP4Err (*getSampleCount)(struct MP4SampleEncryptionAtom* self, u32* sample_count);
} MP4SampleEncryptionAtom, *MP4SampleEncryptionAtomPtr;

typedef struct MP4ALACSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    u32 version; /* uint16*/
    char reserved2[6];
    u32 channels;        /* uint(16) = 2 */
    u32 sampleSize;      /* uint(16) = 16, bits per sample */
    u32 reserved5;       /* uint(32) = 0 */
    u32 timeScale;       /* uint(16) copied from track! integer part of sample rate (16.16)*/
    u32 reserved6;       /* uint(16) = 0, float part of sample rate (16.16) */
    char* despExtension; /* 16 bytes extensiton for QT version 1 */
    u64 skipped_size;    /* total size of child atoms skipped */
    u8 bIsQTVersionOne;
    u32 samplePerPackage; /* version 2 only: samples per package */
    u32 garbage;          /* there may be garbage data before child atom 'wave' */
    u8 AlacInfo[ALAC_SPECIFIC_INFO_SIZE];
    u8 AlacInfoAvailable;
} MP4ALACSampleEntryAtom, *MP4ALACSampleEntryAtomPtr;

typedef struct MP4OpusSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    u32 version; /* uint16*/
    char reserved2[6];
    u32 channels;   /* uint(16) = 2 */
    u32 sampleSize; /* uint(16) = 16, bits per sample */
    u32 reserved5;  /* uint(32) = 0 */
    u32 timeScale;  /* uint(16) copied from track! integer part of sample rate (16.16)*/
    u32 reserved6;  /* uint(16) = 0, float part of sample rate (16.16) */

    u8* csd;
    u32 csdSize;
} MP4OpusSampleEntryAtom, *MP4OpusSampleEntryAtomPtr;

typedef struct MP4FlacSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    u32 version; /* uint16*/
    char reserved2[6];
    u32 channels;   /* uint(16) = 2 */
    u32 sampleSize; /* uint(16) = 16, bits per sample */
    u32 reserved5;  /* uint(32) = 0 */
    u32 timeScale;  /* uint(16) copied from track! integer part of sample rate (16.16)*/
    u32 reserved6;  /* uint(16) = 0, float part of sample rate (16.16) */
    char reserved3[12];
    u64 skipped_size;

    u8* csd;
    u32 csdSize;
} MP4FlacSampleEntryAtom, *MP4FlacSampleEntryAtomPtr;

typedef struct MPEGHSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    u32 version; /* uint16*/
    char reserved2[6];
    u32 channels;   /* uint(16) = 2 */
    u32 sampleSize; /* uint(16) = 16, bits per sample */
    u32 reserved5;  /* uint(32) = 0 */
    u32 timeScale;  /* uint(16) copied from track! integer part of sample rate (16.16)*/
    u32 reserved6;  /* uint(16) = 0, float part of sample rate (16.16) */
    char reserved3[12];
    u64 skipped_size;
    MP4AtomPtr MhacAtom;
    MP4AtomPtr MhapAtom;
} MPEGHSampleEntryAtom, *MPEGHSampleEntryAtomPtr;

typedef struct MhacAtom {
    MP4_BASE_ATOM
    u32 configVersion;
    u32 profileLevelIndication;
    u32 referenceChannelLayout;
    u8* csd;
    u32 csdSize;
} MhacAtom, *MhacAtomPtr;

typedef struct MhapAtom {
    MP4_BASE_ATOM
    u8* compatibleSets;
    u32 compatibleSetsSize;

} MhapAtom, *MhapAtomPtr;

typedef struct AC4SampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    u32 version;
    char reserved2[6];
    u32 channels;
    u32 sampleSize;
    u32 reserved5;
    u32 timeScale;  // sample rate
    u32 reserved6;
    u32 dacsize;  // size of dac box
    u32 dactype;  // dac type
    u8* dsiData;
    u32 dsiLength;
} AC4SampleEntryAtom, *AC4SampleEntryAtomPtr;

#define AC4_SYNC_HEADER_SIZE 7

typedef struct DolbyVisionSampleEntryAtom {
    MP4_BASE_ATOM
    COMMON_SAMPLE_ENTRY_FIELDS
    char reserved1[6];
    char reserved2[16];
    u32 video_width;
    u32 video_height;
    char reserved3[50];
} DolbyVisionSampleEntryAtom, *DolbyVisionSampleEntryAtomPtr;

typedef struct PrimaryItemAtom {
    MP4_FULL_ATOM
    u32 itemId;
} PrimaryItemAtom, *PrimaryItemAtomPtr;

typedef struct ItemInfoAtom {
    MP4_FULL_ATOM
    u32 infoEntryCount;
    MP4LinkedList atomList;
    bool needIref;
    MP4Err (*addAtom)(struct ItemInfoAtom* self, MP4AtomPtr atom);

} ItemInfoAtom, *ItemInfoAtomPtr;

typedef struct InfoEntryAtom {
    MP4_FULL_ATOM
    u32 itemId;
    u32 protectionIndex;
    u32 itemType;
    bool hidden;
    char itemName[256];
    char* contentType;
    char* contentEncoding;
    char* uriType;
    bool (*isXmp)(struct InfoEntryAtom* self);
    bool (*isExif)(struct InfoEntryAtom* self);
    bool (*isGrid)(struct InfoEntryAtom* self);
    bool (*isSample)(struct InfoEntryAtom* self);
} InfoEntryAtom, *InfoEntryAtomPtr;

typedef struct ItemReferenceAtom {
    MP4_FULL_ATOM
    MP4LinkedList atomList;
    MP4Err (*addAtom)(struct ItemReferenceAtom* self, MP4AtomPtr atom);

} ItemReferenceAtom, *ItemReferenceAtomPtr;

typedef struct ReferenceChunkAtom {
    MP4_BASE_ATOM
    u32 itemId;
    u32 itemIdSize;
    u32 referenceItemCount;
    u32* referenceItemArray;
    void (*setItemIdSize)(struct ReferenceChunkAtom* self, u32 size);
    MP4Err (*apply)(struct ReferenceChunkAtom* self, void* images, u32 imageCount, void* metas,
                    u32 metaCount);

} ReferenceChunkAtom, *ReferenceChunkAtomPtr;

typedef struct ItemPropertiesAtom {
    MP4_BASE_ATOM
    MP4AtomPtr ipcoAtom;
    MP4AtomPtr ipmaAtom;
    MP4Err (*addAtom)(struct ItemPropertiesAtom* self, MP4AtomPtr atom);

} ItemPropertiesAtom, *ItemPropertiesAtomPtr;

typedef struct ItemPropertyContainerAtom {
    MP4_BASE_ATOM
    MP4LinkedList atomList;
    MP4Err (*addAtom)(struct ItemPropertyContainerAtom* self, MP4AtomPtr atom);
    MP4Err (*attachTo)(struct ItemPropertyContainerAtom* self, u32 propertyIndex, void* imageItem);
} ItemPropertyContainerAtom, *ItemPropertyContainerAtomPtr;

typedef struct PropertyChunkAtom {
    MP4_BASE_ATOM
    u32 dataSize;
    u8* data;  // hvcc, av1c, or ICC data
    u32 rotation;
    u32 width;
    u32 height;
    u32 colorType;
    u32 seenClap;
    u32 clapWidthN;
    u32 clapWidthD;
    u32 clapHeightN;
    u32 clapHeightD;
    u32 clapHorizOffN;
    u32 clapHorizOffD;
    u32 clapVertOffN;
    u32 clapVertOffD;
    MP4Err (*attachTo)(struct PropertyChunkAtom* self, void* imageItem);
} PropertyChunkAtom, *PropertyChunkAtomPtr;

typedef struct AssociationEntry {
    u32 itemId;
    u32 propCount;
    u32* propIndexArray;
} AssociationEntry, *AssociationEntryPtr;

typedef struct ItemPropertyAssociationAtom {
    MP4_FULL_ATOM
    u32 entryCount;
    AssociationEntryPtr entryArray;

} ItemPropertyAssociationAtom, *ItemPropertyAssociationAtomPtr;

typedef struct ExtentEntry {
    u64 extentIndex;
    u64 extentOffset;
    u64 extentLength;
} ExtentEntry, *ExtentEntryPtr;

typedef struct ItemLocationEntry {
    u32 itemId;
    u32 constructionMethod;
    u32 dataReferenceIndex;
    u64 baseOffset;
    u32 extentCount;
    ExtentEntry extent;
} ItemLocationEntry, *ItemLocationEntryPtr;

typedef struct ItemLocationAtom {
    MP4_FULL_ATOM
    u32 itemLocationCount;
    ItemLocationEntryPtr itemLocationArray;
    bool hasConstructMethod1;
    MP4Err (*getLoc)(struct ItemLocationAtom* self, u32 itemId, u64* offset, u32* size,
                     u64 idatOffset, u32 idatSize);
} ItemLocationAtom, *ItemLocationAtomPtr;

typedef struct ItemDataAtom {
    MP4_BASE_ATOM
    u64 offset;
    u32 dataSize;
} ItemDataAtom, *ItemDataAtomPtr;

#endif
