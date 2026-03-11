/*
***********************************************************************
* Copyright (c) 2005-2016, Freescale Semiconductor, Inc.
* Copyright 2017-2018, 2020-2026 NXP
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
derivative works. Copyright (c) 1999.
*/
/*
    $Id: MP4Atoms.c,v 1.2 2000/05/17 08:01:28 francesc Exp $
*/
//#define DEBUG

#include "MP4Atoms.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef INCLUDED_MJ2ATOMS_H
#include "MJ2Atoms.h"
#endif

static MP4Err baseAtomCreateFromInputStream(MP4AtomPtr self, MP4AtomPtr proto,
                                            MP4InputStreamPtr inputStream);
static void baseAtomDestroy(MP4AtomPtr self);
static char* baseAtomGetName(MP4AtomPtr self);

static MP4Err fullAtomCreateFromInputStream(MP4AtomPtr s, MP4AtomPtr proto,
                                            MP4InputStreamPtr inputStream);
static void fullAtomDestroy(MP4AtomPtr self);

static MP4Atom MP4BaseAtomClass = {0,                 /* type */
                                   {0},               //{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},
                                   0,                 /* size */
                                   0,                 /* size64 */
                                   0,                 /* bytesRead */
                                   0,                 /* bytesWritten */
                                   "base",            /* name */
                                   &MP4BaseAtomClass, /* super */
                                   (cisfunc)baseAtomCreateFromInputStream,
                                   baseAtomGetName,
                                   baseAtomDestroy};

static MP4FullAtom MP4FullAtomClass = {0,      /* type */
                                       {0},    //{0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},
                                       0,      /* size */
                                       0,      /* size64 */
                                       0,      /* bytesRead */
                                       0,      /* bytesWritten */
                                       "full", /* name */
                                       (struct MP4Atom*)&MP4FullAtomClass, /* super */
                                       (cisfunc)fullAtomCreateFromInputStream,
                                       baseAtomGetName,
                                       fullAtomDestroy,
                                       0,  /* version */
                                       0}; /* flags */

/* some atoms are critical and must be entire.
If moov is at the end and cut-short, as long as the it crital children (mvhd, trak) are entire,
it's still acceptiable.*/
static uint32 critialAtomTypes[] = {MP4MovieHeaderAtomType, MP4TrackAtomType};

/* some atoms are not critical and can be disposed if they are cut-short.
 */
static uint32 disposableAtomTypes[] = {MP4UserDataAtomType};

MP4Err MP4CreateAtom(u32 atomType, MP4AtomPtr* outAtom, MP4InputStreamPtr inputStream);

static void PrintTag(uint32 tag);
static int32 acceptCutShortAtom(uint32 atomType);

static char* baseAtomGetName(MP4AtomPtr self) {
    return self->name;
}

static void baseAtomDestroy(MP4AtomPtr self) {
    MP4LocalFree(self);
}

static MP4Err baseAtomCreateFromInputStream(MP4AtomPtr self, MP4AtomPtr proto,
                                            MP4InputStreamPtr inputStream) {
    self->type = proto->type;
    memcpy(self->uuid, proto->uuid, 16);
    self->size = proto->size;
    self->size64 = proto->size64;
    self->bytesRead = proto->bytesRead;
    (void)inputStream;
    return MP4NoErr;
}

MP4Err MP4CreateBaseAtom(MP4AtomPtr self) {
    MP4Err err = MP4NoErr;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    memcpy(self, &MP4BaseAtomClass, sizeof(MP4Atom));

bail:
    TEST_RETURN(err);

    return err;
}

static void fullAtomDestroy(MP4AtomPtr self) {
    baseAtomDestroy(self);
}

static MP4Err fullAtomCreateFromInputStream(MP4AtomPtr s, MP4AtomPtr proto,
                                            MP4InputStreamPtr inputStream) {
    MP4FullAtomPtr self;
    MP4Err err;
    u32 val;
    self = (MP4FullAtomPtr)s;

    err = baseAtomCreateFromInputStream(s, proto, inputStream);
    if (err)
        goto bail;
    err = inputStream->read32(inputStream, &val, NULL);
    if (err)
        goto bail;
    DEBUG_SPRINTF("atom version = %d", (int)(val >> 24));
    DEBUG_SPRINTF("atom flags = 0x%06x", (unsigned int)(val & 0xFFFFFF));
    self->bytesRead += 4;
    self->version = val >> 24;
    self->flags = val & 0xffffff;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateFullAtom(MP4AtomPtr s) {
    MP4FullAtomPtr self;
    MP4Err err = MP4NoErr;
    self = (MP4FullAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    memcpy(self, &MP4FullAtomClass, sizeof(MP4FullAtom));
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateAudioSampleEntryAtom(MP4AudioSampleEntryAtomPtr* outAtom);
MP4Err MP4CreateChunkLargeOffsetAtom(MP4ChunkLargeOffsetAtomPtr* outAtom);
MP4Err MP4CreateChunkOffsetAtom(MP4ChunkOffsetAtomPtr* outAtom);
MP4Err MP4CreateClockReferenceMediaHeaderAtom(MP4ClockReferenceMediaHeaderAtomPtr* outAtom);
MP4Err MP4CreateCompositionOffsetAtom(MP4CompositionOffsetAtomPtr* outAtom);
MP4Err MP4CreateCopyrightAtom(MP4CopyrightAtomPtr* outAtom);
MP4Err MP4CreateDataEntryURLAtom(MP4DataEntryURLAtomPtr* outAtom);
MP4Err MP4CreateDataEntryURNAtom(MP4DataEntryURNAtomPtr* outAtom);
MP4Err MP4CreateDataInformationAtom(MP4DataInformationAtomPtr* outAtom);
MP4Err MP4CreateDataReferenceAtom(MP4DataReferenceAtomPtr* outAtom);
MP4Err MP4CreateDegradationPriorityAtom(MP4DegradationPriorityAtomPtr* outAtom);
MP4Err MP4CreateESDAtom(MP4ESDAtomPtr* outAtom);
MP4Err MP4CreateEditAtom(MP4EditAtomPtr* outAtom);
MP4Err MP4CreateEditListAtom(MP4EditListAtomPtr* outAtom);
MP4Err MP4CreateFreeSpaceAtom(MP4FreeSpaceAtomPtr* outAtom);
MP4Err MP4CreateGenericSampleEntryAtom(MP4GenericSampleEntryAtomPtr* outAtom);
MP4Err MP4CreateHandlerAtom(MP4HandlerAtomPtr* outAtom);
MP4Err MP4CreateHintMediaHeaderAtom(MP4HintMediaHeaderAtomPtr* outAtom);
MP4Err MP4CreateMPEGMediaHeaderAtom(MP4MPEGMediaHeaderAtomPtr* outAtom);
MP4Err MP4CreateMPEGSampleEntryAtom(MP4MPEGSampleEntryAtomPtr* outAtom);
MP4Err MP4CreateMediaAtom(MP4MediaAtomPtr* outAtom);
MP4Err MP4CreateMediaDataAtom(MP4MediaDataAtomPtr* outAtom);
MP4Err MP4CreateMediaHeaderAtom(MP4MediaHeaderAtomPtr* outAtom);
MP4Err MP4CreateMediaInformationAtom(MP4MediaInformationAtomPtr* outAtom);
MP4Err MP4CreateMovieAtom(MP4MovieAtomPtr* outAtom);
MP4Err MP4CreateMovieHeaderAtom(MP4MovieHeaderAtomPtr* outAtom);
MP4Err MP4CreateObjectDescriptorAtom(MP4ObjectDescriptorAtomPtr* outAtom);
MP4Err MP4CreateObjectDescriptorMediaHeaderAtom(MP4ObjectDescriptorMediaHeaderAtomPtr* outAtom);
MP4Err MP4CreateSampleDescriptionAtom(MP4SampleDescriptionAtomPtr* outAtom);
MP4Err MP4CreateSampleSizeAtom(MP4SampleSizeAtomPtr* outAtom);
MP4Err MP4CreateCompactSampleSizeAtom(MP4CompactSampleSizeAtomPtr* outAtom);
MP4Err MP4CreateSampleTableAtom(MP4SampleTableAtomPtr* outAtom);
MP4Err MP4CreateSampleToChunkAtom(MP4SampleToChunkAtomPtr* outAtom);
MP4Err MP4CreateSceneDescriptionMediaHeaderAtom(MP4SceneDescriptionMediaHeaderAtomPtr* outAtom);
MP4Err MP4CreateShadowSyncAtom(MP4ShadowSyncAtomPtr* outAtom);
MP4Err MP4CreateSoundMediaHeaderAtom(MP4SoundMediaHeaderAtomPtr* outAtom);
MP4Err MP4CreateSyncSampleAtom(MP4SyncSampleAtomPtr* outAtom);
MP4Err MP4CreateTimeToSampleAtom(MP4TimeToSampleAtomPtr* outAtom);
MP4Err MP4CreateTrackAtom(MP4TrackAtomPtr* outAtom);
MP4Err MP4CreateTrackHeaderAtom(MP4TrackHeaderAtomPtr* outAtom);
MP4Err MP4CreateTrackReferenceAtom(MP4TrackReferenceAtomPtr* outAtom);
MP4Err MP4CreateTrackReferenceTypeAtom(u32 atomType, MP4TrackReferenceTypeAtomPtr* outAtom);
MP4Err MP4CreateUnknownAtom(MP4UnknownAtomPtr* outAtom);
MP4Err MP4CreateUserDataAtom(MP4UserDataAtomPtr* outAtom);
MP4Err MP4CreateUserDataEntryAtom(MP4UserDataEntryAtomPtr* outAtom);
MP4Err MP4Create3GppUserDataAtom(MP4UserData3GppAtomPtr* outAtom);
MP4Err MP4CreateID3v2UserDataAtom(MP4UserDataID3v2AtomPtr* outAtom);
MP4Err MP4CreateVideoMediaHeaderAtom(MP4VideoMediaHeaderAtomPtr* outAtom);
MP4Err MP4CreateGeneralAudioSampleEntryAtom(GeneralAudioSampleEntryAtomPtr* outAtom, u32 type);
MP4Err MP4CreateGeneralVideoSampleEntryAtom(GeneralVideoSampleEntryAtomPtr* outAtom, u32 type);
MP4Err MP4CreateVisualSampleEntryAtom(MP4VisualSampleEntryAtomPtr* outAtom);
MP4Err MP4CreateAvcSampleEntryAtom(MP4AvcSampleEntryAtomPtr* outAtom);
MP4Err MP4CreateAv1SampleEntryAtom(MP4Av1SampleEntryAtomPtr* outAtom);
MP4Err MP4CreateVp9SampleEntryAtom(MP4Vp9SampleEntryAtomPtr* outAtom);
MP4Err MP4CreateApvSampleEntryAtom(MP4ApvSampleEntryAtomPtr* outAtom);
MP4Err MP4CreateVpccAtom(MP4VPCCAtomPtr* outAtom);
MP4Err MP4CreateApvcAtom(MP4APVCAtomPtr* outAtom);
MP4Err MP4CreatePixelAspectRatioAtom(MP4PASPAtomPtr* outAtom);
MP4Err MP4CreateBitrateAtom(MP4BitrateAtomPtr* outAtom);
MP4Err MP4CreateHevcSampleEntryAtom(MP4HevcSampleEntryAtomPtr* outAtom, u32 type);
MP4Err MP4CreateQTWaveAtom(QTsiDecompressParaAtomPtr* outAtom);
MP4Err MP4CreateQTColorParameterAtom(QTColorParameterAtomPtr* outAtom);
MP4Err MP4CreateTimedTextSampleEntryAtom(TimedTextSampleEntryAtomPtr* outAtom);
MP4Err MP4CreateMetadataSampleEntryAtom(MetadataSampleEntryAtomPtr* outAtom);
MP4Err MP4CreateMotionJPEGSSampleEntryAtom(MP4MotionJPEGSampleEntryAtomPtr* outAtom, u32 type);
MP4Err MP4CreatePcmAudioSampleEntryAtom(PcmAudioSampleEntryAtomPtr* outAtom, u32 type);
MP4Err MP4CreateAdpcmSampleEntryAtom(AdpcmSampleEntryAtomPtr* outAtom, u32 type);
MP4Err MP4CreateMetadataAtom(MP4MetadataAtomPtr* outAtom);
MP4Err MP4CreateDivxSampleEntryAtom(MP4DivxSampleEntryAtomPtr* outAtom);
MP4Err MP4CreateMetadataItemAtom(MP4MetadataItemAtomPtr* outAtom);
MP4Err MP4CreateMetadataItemKeysAtom(MP4MetadataItemKeysAtomPtr* outAtom);
MP4Err MP4CreateMetadataItemListAtom(MP4MetadataItemListAtomPtr* outAtom);
MP4Err MP4CreateValueAtom(MP4ValueAtomPtr* outAtom);
MP4Err MP4CreateAC3SampleEntryAtom(AC3SampleEntryAtomPtr* outAtom, u32 type);
MP4Err MP4CreateNameAtom(MP4NameAtomPtr* outAtom);
MP4Err MP4CreateMeanAtom(MP4MeanAtomPtr* outAtom);
MP4Err MP4CreateEC3SampleEntryAtom(EC3SampleEntryAtomPtr* outAtom, u32 type);
MP4Err MP4CreateMovieExtendsAtom(MP4MovieExtendsAtomPtr* outAtom);
MP4Err MP4CreateMovieExtendsHeaderAtom(MP4MovieExtendsHeaderAtomPtr* outAtom);
MP4Err MP4CreateTrackExtendsAtom(MP4TrackExtendsAtomPtr* outAtom);
MP4Err MP4CreateSegmentIndexAtom(MP4SegmentIndexAtomPtr* outAtom);
MP4Err MP4CreateMovieFragmentAtom(MP4MovieFragmentAtomPtr* outAtom);
MP4Err MP4CreateTrackFragmentAtom(MP4TrackFragmentAtomPtr* outAtom);
MP4Err MP4CreateTrackFragmentHeaderAtom(MP4TrackFragmentHeaderAtomPtr* outAtom);
MP4Err MP4CreateTrackFragmentRunAtom(MP4TrackFragmentRunAtomPtr* outAtom);
MP4Err MP4CreateSampleAuxiliaryInfoSizeAtom(MP4SampleAuxiliaryInfoSizeAtomPtr* outAtom);
MP4Err MP4CreateSampleAuxiliaryInfoOffsetsAtom(MP4SampleAuxiliaryInfoOffsetsAtomPtr* outAtom);
MP4Err MP4CreateTrackFragmentDecodeTimeAtom(MP4TrackFragmentDecodeTimeAtomPtr* outAtom);
MP4Err MP4CreateMovieFragmentRandomAccessAtom(MP4MovieFragmentRandomAccessAtomPtr* outAtom);
MP4Err MP4CreateTrackFragmentRandomAccessAtom(MP4TrackFragmentRandomAccessAtomPtr* outAtom);
MP4Err MP4CreateMovieFragmentRandomAccessOffsetAtom(
        MP4MovieFragmentRandomAccessOffsetAtomPtr* outAtom);
MP4Err MP4CreateHvccAtom(MP4HVCCAtomPtr* outAtom);
MP4Err MP4CreateProtectionSystemSpecificHeaderAtom(MP4PSSHAtomPtr* outAtom);
MP4Err MP4CreateProtectedVideoSampleEntryAtom(MP4ProtectedVideoSampleAtomPtr* outAtom);
MP4Err MP4CreateProtectedAudioSampleEntryAtom(MP4ProtectedAudioSampleAtomPtr* outAtom);
MP4Err MP4CreateProtectionSchemeInfoAtom(MP4ProtectionSchemeInfoAtomPtr* outAtom);
MP4Err MP4CreateOriginFormatAtom(MP4OriginFormatAtomPtr* outAtom);
MP4Err MP4CreateSchemeTypeAtom(MP4SchemeTypeAtomPtr* outAtom);
MP4Err MP4CreateSchemeInfoAtom(MP4SchemeInfoAtomPtr* outAtom);
MP4Err MP4CreateTrackEncryptionAtom(MP4TrackEncryptionAtomPtr* outAtom);
MP4Err MP4CreateSampleEncryptionAtom(MP4SampleEncryptionAtomPtr* outAtom);
MP4Err MP4CreateALACSampleEntryAtom(MP4ALACSampleEntryAtomPtr* outAtom);
MP4Err MP4CreateOpusSampleEntryAtom(MP4OpusSampleEntryAtomPtr* outAtom);
MP4Err MP4CreateFlacSampleEntryAtom(MP4FlacSampleEntryAtomPtr* outAtom);
MP4Err MP4CreateMPEGHSampleEntryAtom(MPEGHSampleEntryAtomPtr* outAtom, u32 atomType);
MP4Err MP4CreateMhacAtom(MhacAtomPtr* outAtom);
MP4Err MP4CreateMhapAtom(MhapAtomPtr* outAtom);
MP4Err MP4CreateAC4SampleEntryAtom(AC4SampleEntryAtomPtr* outAtom);
MP4Err MP4CreateDolbyVisionSampleEntryAtom(DolbyVisionSampleEntryAtomPtr* outAtom);
MP4Err MP4CreatePrimaryItemAtom(PrimaryItemAtomPtr* outAtom);
MP4Err MP4CreateItemInfoAtom(ItemInfoAtomPtr* outAtom);
MP4Err MP4CreateInfoEntryAtom(InfoEntryAtomPtr* outAtom);
MP4Err MP4CreateItemReferenceAtom(ItemReferenceAtomPtr* outAtom);
MP4Err MP4CreateReferenceChunkAtom(ReferenceChunkAtomPtr* outAtom, u32 atomType);
MP4Err MP4CreateItemPropertiesAtom(ItemPropertiesAtomPtr* outAtom);
MP4Err MP4CreateItemPropertyContainerAtom(ItemPropertyContainerAtomPtr* outAtom);
MP4Err MP4CreateItemPropertyAssociationAtom(ItemPropertyAssociationAtomPtr* outAtom);
MP4Err MP4CreateItemLocationAtom(ItemLocationAtomPtr* outAtom);
MP4Err MP4CreateItemDataAtom(ItemDataAtomPtr* outAtom);

MP4Err MP4CreateAtom(u32 atomType, MP4AtomPtr* outAtom, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    MP4AtomPtr newAtom = NULL;

    switch (atomType) {
        case MP4HintTrackReferenceAtomType:
        case MP4StreamDependenceAtomType:
        case MP4ODTrackReferenceAtomType:
        case MP4SyncTrackReferenceAtomType:
            MP4MSG("MP4CreateTrackReferenceTypeAtom:\n");
            err = MP4CreateTrackReferenceTypeAtom(atomType,
                                                  (MP4TrackReferenceTypeAtomPtr*)&newAtom);
            break;

        case MP4FreeSpaceAtomType:
        case MP4SkipAtomType:
        case QTWideAtomType:
            MP4MSG("MP4CreateFreeSpaceAtom:\n");
            err = MP4CreateFreeSpaceAtom((MP4FreeSpaceAtomPtr*)&newAtom);
            break;

        case MP4MediaDataAtomType:
            MP4MSG("MP4MediaDataAtomType:\n");
            err = MP4CreateMediaDataAtom((MP4MediaDataAtomPtr*)&newAtom);
            inputStream->stream_flags |= mediadata_flag;
            break;

        case MP4MovieAtomType:
            MP4MSG("MP4CreateMovieAtom:\n");
            err = MP4CreateMovieAtom((MP4MovieAtomPtr*)&newAtom);
            inputStream->stream_flags |= metadata_flag;
            break;

        case MP4MovieHeaderAtomType:
            MP4MSG("MP4CreateMovieHeaderAtom:\n");
            err = MP4CreateMovieHeaderAtom((MP4MovieHeaderAtomPtr*)&newAtom);
            break;

        case MP4MediaHeaderAtomType:
            MP4MSG("MP4CreateMediaHeaderAtom:\n");
            err = MP4CreateMediaHeaderAtom((MP4MediaHeaderAtomPtr*)&newAtom);
            break;

        case MP4VideoMediaHeaderAtomType:
            MP4MSG("MP4VideoMediaHeaderAtomType: \n");
            err = MP4CreateVideoMediaHeaderAtom((MP4VideoMediaHeaderAtomPtr*)&newAtom);
            break;

        case MP4SoundMediaHeaderAtomType:
            MP4MSG("MP4SoundMediaHeaderAtomType: \n");
            err = MP4CreateSoundMediaHeaderAtom((MP4SoundMediaHeaderAtomPtr*)&newAtom);
            break;

        case MP4MPEGMediaHeaderAtomType:
            MP4MSG("MP4MPEGMediaHeaderAtomType: \n");
            err = MP4CreateMPEGMediaHeaderAtom((MP4MPEGMediaHeaderAtomPtr*)&newAtom);
            break;

        case MP4ObjectDescriptorMediaHeaderAtomType:
            MP4MSG("MP4ObjectDescriptorMediaHeaderAtomType: \n");
            err = MP4CreateObjectDescriptorMediaHeaderAtom(
                    (MP4ObjectDescriptorMediaHeaderAtomPtr*)&newAtom);
            break;

        case MP4ClockReferenceMediaHeaderAtomType:
            MP4MSG("MP4ClockReferenceMediaHeaderAtomType: \n");
            err = MP4CreateClockReferenceMediaHeaderAtom(
                    (MP4ClockReferenceMediaHeaderAtomPtr*)&newAtom);
            break;

        case MP4SceneDescriptionMediaHeaderAtomType:
            MP4MSG("MP4SceneDescriptionMediaHeaderAtomType: \n");
            err = MP4CreateSceneDescriptionMediaHeaderAtom(
                    (MP4SceneDescriptionMediaHeaderAtomPtr*)&newAtom);
            break;

        case MP4HintMediaHeaderAtomType:
            MP4MSG("MP4HintMediaHeaderAtomType: \n");
            err = MP4CreateHintMediaHeaderAtom((MP4HintMediaHeaderAtomPtr*)&newAtom);
            break;

        case MP4SampleTableAtomType:
            MP4MSG("MP4SampleTableAtomType: \n");
            err = MP4CreateSampleTableAtom((MP4SampleTableAtomPtr*)&newAtom);
            break;

        case MP4DataInformationAtomType:
            MP4MSG("MP4DataInformationAtomType: \n");
            err = MP4CreateDataInformationAtom((MP4DataInformationAtomPtr*)&newAtom);
            break;

        case MP4DataEntryURLAtomType:
        case QTDataEntryAlisAtomType:
        case MSDataEntryAtomType:
            MP4MSG("MP4DataEntryURLAtomType: \n");
            err = MP4CreateDataEntryURLAtom((MP4DataEntryURLAtomPtr*)&newAtom);
            break;

        case MP4CopyrightAtomType:
            MP4MSG("MP4CopyrightAtomType: \n");
            err = MP4CreateCopyrightAtom((MP4CopyrightAtomPtr*)&newAtom);
            break;

        case MP4DataEntryURNAtomType:
            MP4MSG("MP4DataEntryURNAtomType: \n");
            err = MP4CreateDataEntryURNAtom((MP4DataEntryURNAtomPtr*)&newAtom);
            break;

        case MP4HandlerAtomType:
            MP4MSG("MP4HandlerAtomType: \n");
            err = MP4CreateHandlerAtom((MP4HandlerAtomPtr*)&newAtom);
            break;

        case MP4ObjectDescriptorAtomType:
            MP4MSG("MP4ObjectDescriptorAtomType: \n");
            err = MP4CreateObjectDescriptorAtom((MP4ObjectDescriptorAtomPtr*)&newAtom);
            break;

        case MP4TrackAtomType:
            MP4MSG("\nMP4TrackAtomType: \n");
            err = MP4CreateTrackAtom((MP4TrackAtomPtr*)&newAtom);
            break;

        case MP4MPEGSampleEntryAtomType:
            MP4MSG("MP4MPEGSampleEntryAtomType: \n");
            err = MP4CreateMPEGSampleEntryAtom((MP4MPEGSampleEntryAtomPtr*)&newAtom);
            break;

        case MP4VisualSampleEntryAtomType:
            MP4MSG("MP4VisualSampleEntryAtomType: \n");
            err = MP4CreateVisualSampleEntryAtom((MP4VisualSampleEntryAtomPtr*)&newAtom);
            break;

        case MP4AudioSampleEntryAtomType:
            MP4MSG("MP4AudioSampleEntryAtomType: \n");
            err = MP4CreateAudioSampleEntryAtom((MP4AudioSampleEntryAtomPtr*)&newAtom);
            break;

        case MP4AvcSampleEntryAtomType:
            MP4MSG("MP4AvcSampleEntryAtomType: \n");
            err = MP4CreateAvcSampleEntryAtom((MP4AvcSampleEntryAtomPtr*)&newAtom);
            break;
        case MP4Av1SampleEntryAtomType:
            MP4MSG("MP4Av1SampleEntryAtomType: \n");
            err = MP4CreateAv1SampleEntryAtom((MP4Av1SampleEntryAtomPtr*)&newAtom);
            break;
        case MP4BitrateAtomType:
            MP4MSG("MP4BitrateAtomType: \n");
            err = MP4CreateBitrateAtom((MP4BitrateAtomPtr*)&newAtom);
            break;
        case MP4HevcSampleEntryAtomType:
        case MP4Hev1SampleEntryAtomType:
            MP4MSG("MP4HevcSampleEntryAtomType: \n");
            err = MP4CreateHevcSampleEntryAtom((MP4HevcSampleEntryAtomPtr*)&newAtom, atomType);
            break;

        case MP4H263SampleEntryAtomType:
        case QTH263SampleEntryAtomType:
        case H263SampleEntryAtomType:
            MP4MSG("H263SampleEntryAtomType: \n");
            err = MP4CreateH263SampleEntryAtom((MP4H263SampleEntryAtomPtr*)&newAtom);
            break;

        case AmrNBSampleEntryAtomType:
        case AmrWBSampleEntryAtomType:
            MP4MSG("Amr-NB/WB SampleEntryAtomType: \n");
            err = MP4CreateAmrSampleEntryAtom((MP4AmrSampleEntryAtomPtr*)&newAtom, atomType);
            break;

        case JPEGSampleEntryAtomType:
        case MJPEGFormatASampleEntryAtomType:
        case MJPEGFormatBSampleEntryAtomType:
        case MJPEG2000SampleEntryAtomType:
            MP4MSG("JPEG/M-JPEG SampleEntryAtomType: \n");
            err = MP4CreateMotionJPEGSSampleEntryAtom((MP4MotionJPEGSampleEntryAtomPtr*)&newAtom,
                                                      atomType);
            break;

        case SVQ3SampleEntryAtomType:
            MP4MSG("SVQ3 SampleEntryAtomType: \n");
            err = MP4CreateGeneralVideoSampleEntryAtom((GeneralVideoSampleEntryAtomPtr*)&newAtom,
                                                       atomType);
            break;
        case MP4AC3SampleEntryAtomType:
            err = MP4CreateAC3SampleEntryAtom((AC3SampleEntryAtomPtr*)&newAtom, atomType);
            break;
        case MP4EC3SampleEntryAtomType:
            err = MP4CreateEC3SampleEntryAtom((EC3SampleEntryAtomPtr*)&newAtom, atomType);
            break;
        case MuLawAudioSampleEntryAtomType:
            MP4MSG("MuLawAudioSampleEntryAtomType: \n");
            err = MP4CreateGeneralAudioSampleEntryAtom((GeneralAudioSampleEntryAtomPtr*)&newAtom,
                                                       atomType);
            break;

        case MP4GenericSampleEntryAtomType:
            MP4MSG("MP4GenericSampleEntryAtomType: \n");
            err = MP4CreateGenericSampleEntryAtom((MP4GenericSampleEntryAtomPtr*)&newAtom);
            break;

        case MP4EditAtomType:
            MP4MSG("MP4EditAtomType: \n");
            err = MP4CreateEditAtom((MP4EditAtomPtr*)&newAtom);
            break;

        case MP4UserDataAtomType:
            MP4MSG("MP4UserDataAtomType: \n");
            err = MP4CreateUserDataAtom((MP4UserDataAtomPtr*)&newAtom);
            break;

        case MP4DataReferenceAtomType:
            MP4MSG("MP4DataReferenceAtomType: \n");
            err = MP4CreateDataReferenceAtom((MP4DataReferenceAtomPtr*)&newAtom);
            break;

        case MP4SampleDescriptionAtomType:
            MP4MSG("MP4SampleDescriptionAtomType: \n");
            err = MP4CreateSampleDescriptionAtom((MP4SampleDescriptionAtomPtr*)&newAtom);
            break;

        case MP4TimeToSampleAtomType:
            MP4MSG("MP4TimeToSampleAtomType: \n");
            err = MP4CreateTimeToSampleAtom((MP4TimeToSampleAtomPtr*)&newAtom);
            break;

        case MP4CompositionOffsetAtomType:
            MP4MSG("MP4CompositionOffsetAtomType: \n");
            err = MP4CreateCompositionOffsetAtom((MP4CompositionOffsetAtomPtr*)&newAtom);
            break;

        case MP4ShadowSyncAtomType:
            MP4MSG("MP4ShadowSyncAtomType: \n");
            err = MP4CreateShadowSyncAtom((MP4ShadowSyncAtomPtr*)&newAtom);
            break;

        case MP4EditListAtomType:
            MP4MSG("MP4EditListAtomType: \n");
            err = MP4CreateEditListAtom((MP4EditListAtomPtr*)&newAtom);
            break;

        case MP4SampleToChunkAtomType:
            MP4MSG("MP4SampleToChunkAtomType: \n");
            err = MP4CreateSampleToChunkAtom((MP4SampleToChunkAtomPtr*)&newAtom);
            break;

        case MP4SampleSizeAtomType:
            MP4MSG("MP4SampleSizeAtomType\n");
            err = MP4CreateSampleSizeAtom((MP4SampleSizeAtomPtr*)&newAtom);
            break;

        case MP4CompactSampleSizeAtomType:
            MP4MSG("MP4CompactSampleSizeAtomType\n");
            err = MP4CreateCompactSampleSizeAtom((MP4CompactSampleSizeAtomPtr*)&newAtom);
            break;

        case MP4ChunkOffsetAtomType:
            MP4MSG("MP4ChunkOffsetAtomType\n");
            err = MP4CreateChunkOffsetAtom((MP4ChunkOffsetAtomPtr*)&newAtom);
            break;

        case MP4SyncSampleAtomType:
            MP4MSG("MP4SyncSampleAtomType\n");
            err = MP4CreateSyncSampleAtom((MP4SyncSampleAtomPtr*)&newAtom);
            break;

        case MP4DegradationPriorityAtomType:
            MP4MSG("MP4DegradationPriorityAtomType\n");
            err = MP4CreateDegradationPriorityAtom((MP4DegradationPriorityAtomPtr*)&newAtom);
            break;

        case MP4ChunkLargeOffsetAtomType:
            MP4MSG("MP4ChunkLargeOffsetAtomType\n");
            err = MP4CreateChunkLargeOffsetAtom((MP4ChunkLargeOffsetAtomPtr*)&newAtom);
            break;

        case MP4ESDAtomType:
            MP4MSG("MP4ESDAtomType\n");
            err = MP4CreateESDAtom((MP4ESDAtomPtr*)&newAtom);
            break;

        case MP4MediaInformationAtomType:
            MP4MSG("MP4MediaInformationAtomType\n");
            err = MP4CreateMediaInformationAtom((MP4MediaInformationAtomPtr*)&newAtom);
            break;

        case MP4TrackHeaderAtomType:
            MP4MSG("MP4TrackHeaderAtomType\n");
            err = MP4CreateTrackHeaderAtom((MP4TrackHeaderAtomPtr*)&newAtom);
            break;

        case MP4TrackReferenceAtomType:
            MP4MSG("MP4TrackReferenceAtomType\n");
            err = MP4CreateTrackReferenceAtom((MP4TrackReferenceAtomPtr*)&newAtom);
            break;

        case MP4MediaAtomType:
            MP4MSG("MP4MediaAtomType\n");
            err = MP4CreateMediaAtom((MP4MediaAtomPtr*)&newAtom);
            break;

        /* JPEG-2000 atom ("box") types */
        case MJ2JPEG2000SignatureAtomType:
            MP4MSG("MJ2JPEG2000SignatureAtomType\n");
            err = MJ2CreateSignatureAtom((MJ2JPEG2000SignatureAtomPtr*)&newAtom);
            break;

        case MJ2FileTypeAtomType:
            MP4MSG("MJ2FileTypeAtomType\n");
            err = MJ2CreateFileTypeAtom((MJ2FileTypeAtomPtr*)&newAtom);
            break;

        case MJ2ImageHeaderAtomType:
            MP4MSG("MJ2ImageHeaderAtomType\n");
            err = MJ2CreateImageHeaderAtom((MJ2ImageHeaderAtomPtr*)&newAtom);
            break;

        case MJ2BitsPerComponentAtomType:
            MP4MSG("MJ2BitsPerComponentAtomType\n");
            err = MJ2CreateBitsPerComponentAtom((MJ2BitsPerComponentAtomPtr*)&newAtom);
            break;

#ifdef QUICKTIME_COLOR_PARAMETER_FIRST
        case QTColorParameterAtomType:
            MP4MSG("QTColorParameterAtomType\n");
            err = MP4CreateQTColorParameterAtom((QTColorParameterAtomPtr*)&newAtom);
            break;
#else
        case MJ2ColorSpecificationAtomType:
            MP4MSG("MJ2ColorSpecificationAtomType\n");
            err = MJ2CreateColorSpecificationAtom((MJ2ColorSpecificationAtomPtr*)&newAtom);
            break;
#endif

        case MJ2JP2HeaderAtomType:
            err = MJ2CreateHeaderAtom((MJ2HeaderAtomPtr*)&newAtom);
            break;

        case MP4AvccAtomType:
        case MP4Av1CAtomType:
        case MP4dvvCAtomType:
            err = MP4CreateAvccAtom((MP4AVCCAtomPtr*)&newAtom);
            break;
        case MP4HvccAtomType:
            err = MP4CreateHvccAtom((MP4HVCCAtomPtr*)&newAtom);
            break;

        case MP4D263AtomType:
            err = MP4CreateD263Atom((MP4H263AtomPtr*)&newAtom);
            break;

        case MP4DamrAtomType:
            err = MP4CreateDamrAtom((MP4DAMRAtomPtr*)&newAtom);
            break;

        case MP4Mp3SampleEntryAtomType:
        case MP4Mp3CbrSampleEntryAtomType:
            err = MP4CreateMp3SampleEntryAtom((MP4Mp3SampleEntryAtomPtr*)&newAtom);
            break;

        case MP4DivxSampleEntryAtomType:
            err = MP4CreateDivxSampleEntryAtom((MP4DivxSampleEntryAtomPtr*)&newAtom);
            break;

        case QTWaveAtomType:
            err = MP4CreateQTWaveAtom((QTsiDecompressParaAtomPtr*)&newAtom);
            break;

        case QTSowtSampleEntryAtomType:
        case QTTwosSampleEntryAtomType:
        case RawAudioSampleEntryAtomType:
            MP4MSG("sowt/twos/raw SampleEntryAtomType\n");
            err = MP4CreatePcmAudioSampleEntryAtom((PcmAudioSampleEntryAtomPtr*)&newAtom, atomType);
            break;

        case ImaAdpcmSampleEntryAtomType:
        case MP4MSAdpcmEntryAtomType:
            MP4MSG("IMA ADPCM SampleEntryAtomType\n");
            err = MP4CreateAdpcmSampleEntryAtom((AdpcmSampleEntryAtomPtr*)&newAtom, atomType);
            break;

        case TimedTextEntryType:
            MP4MSG("3GPP TimedTextEntryType\n");
            err = MP4CreateTimedTextSampleEntryAtom((TimedTextSampleEntryAtomPtr*)&newAtom);
            break;
        case MP4MetadataSampleEntryAtomType:
            MP4MSG("Timed Text Metadata Entry Type\n");
            err = MP4CreateMetadataSampleEntryAtom((MetadataSampleEntryAtomPtr*)&newAtom);
            break;

        case UdtaTitleType:
        case UdtaArtistType:
        case UdtaAlbumType:
        case UdtaCommentType:
        case UdtaGenreType:
        case UdtaDateType:
        case UdtaToolType:
        case UdtaCopyrightType:
        case UdtaDescriptionType:
        case UdtaCoverType:
        case UdtaDescType2:
        case UdtaLocationType:
        case UdtaDirectorType:
        case UdtaInformationType:
        case UdtaCreatorType:
        case UdtaKeywordType:
        case UdtaProducerType:
        case UdtaPerformerType:
        case UdtaRequirementType:
        case UdtaSongWritorType:
        case UdtaMovieWritorType:
            MP4MSG("User data atom entry\n");
            /*ENGR114401. No longer use unkown atom type because there are size & language code
             * fields before the string field */
            err = MP4CreateUserDataEntryAtom((MP4UserDataEntryAtomPtr*)&newAtom);
            break;
        case Udta3GppTitleType:
        case Udta3GppArtistType:
        case Udta3GppAuthType:
        case Udta3GppAlbumType:
        case Udta3GppYearType:
            err = MP4Create3GppUserDataAtom((MP4UserData3GppAtomPtr*)&newAtom);
            break;
        case UdtaGenreType2:  // same value for Udta3GppGenreType
            if (inputStream->stream_flags & flag_3gp) {
                err = MP4Create3GppUserDataAtom((MP4UserData3GppAtomPtr*)&newAtom);
            } else {
                err = MP4CreateUserDataEntryAtom((MP4UserDataEntryAtomPtr*)&newAtom);
            }
            break;
        case UdtaID3V2Type:
            err = MP4CreateID3v2UserDataAtom((MP4UserDataID3v2AtomPtr*)&newAtom);
            break;

        case QTMetadataItemAtomType:
            MP4MSG("Metadata item atom\n");
            err = MP4CreateMetadataItemAtom((MP4MetadataItemAtomPtr*)&newAtom);
            break;

        case QTMetadataAtomType:
            MP4MSG("QT metadata atom\n");
            err = MP4CreateMetadataAtom((MP4MetadataAtomPtr*)&newAtom);
            break;

        case QTMetadataItemKeysAtomType:
            MP4MSG("QT metadata item keys atom\n");
            err = MP4CreateMetadataItemKeysAtom((MP4MetadataItemKeysAtomPtr*)&newAtom);
            break;

        case QTMetadataItemListAtomType:
            MP4MSG("QT metadata item list atom\n");
            err = MP4CreateMetadataItemListAtom((MP4MetadataItemListAtomPtr*)&newAtom);
            break;

        case QTValueAtomType:
            MP4MSG("value atom\n");
            err = MP4CreateValueAtom((MP4ValueAtomPtr*)&newAtom);
            break;
        case QTNameAtomType:
            err = MP4CreateNameAtom((MP4NameAtomPtr*)&newAtom);
            break;
        case MP4MeanAtomType:
            err = MP4CreateMeanAtom((MP4MeanAtomPtr*)&newAtom);
            break;
        case MP4MovieExtendsAtomType:
            MP4MSG("MP4MovieExtendsAtomType: \n");
            err = MP4CreateMovieExtendsAtom((MP4MovieExtendsAtomPtr*)&newAtom);
            break;
        case MP4MovieExtendsHeaderAtomType:
            MP4MSG("MP4MovieExtendsHeaderAtomType: \n");
            err = MP4CreateMovieExtendsHeaderAtom((MP4MovieExtendsHeaderAtomPtr*)&newAtom);
            break;
        case MP4TrackExtendsAtomType:
            MP4MSG("MP4TrackExtendsAtomType: \n");
            err = MP4CreateTrackExtendsAtom((MP4TrackExtendsAtomPtr*)&newAtom);
            break;
        case MP4SegmentIndexAtomType:
            MP4MSG("MP4SegmentIndexAtomType: \n");
            err = MP4CreateSegmentIndexAtom((MP4SegmentIndexAtomPtr*)&newAtom);
            break;
        case MP4MovieFragmentAtomType:
            MP4MSG("MP4MovieFragmentAtomType: \n");
            err = MP4CreateMovieFragmentAtom((MP4MovieFragmentAtomPtr*)&newAtom);
            break;
        case MP4TrackFragmentAtomType:
            MP4MSG("MP4TrackFragmentAtomType: \n");
            err = MP4CreateTrackFragmentAtom((MP4TrackFragmentAtomPtr*)&newAtom);
            break;
        case MP4TrackFragmentHeaderAtomType:
            MP4MSG("MP4TrackFragmentHeaderAtomType: \n");
            err = MP4CreateTrackFragmentHeaderAtom((MP4TrackFragmentHeaderAtomPtr*)&newAtom);
            break;
        case MP4TrackFragmentRunAtomType:
            MP4MSG("MP4TrackFragmentRunAtomType: \n");
            err = MP4CreateTrackFragmentRunAtom((MP4TrackFragmentRunAtomPtr*)&newAtom);
            break;
        case MP4SampleAuxiliaryInfoSizeAtomType:
            MP4MSG("MP4SampleAuxiliaryInfoSizeAtomType: \n");
            err = MP4CreateSampleAuxiliaryInfoSizeAtom(
                    (MP4SampleAuxiliaryInfoSizeAtomPtr*)&newAtom);
            break;
        case MP4SampleAuxiliaryInfoOffsetAtomType:
            MP4MSG("MP4SampleAuxiliaryInfoOffsetAtomType: \n");
            err = MP4CreateSampleAuxiliaryInfoOffsetsAtom(
                    (MP4SampleAuxiliaryInfoOffsetsAtomPtr*)&newAtom);
            break;
        case MP4TrackFragmentDecodeTimeAtomType:
            MP4MSG("MP4TrackFragmentDecodeTimeAtomType: \n");
            err = MP4CreateTrackFragmentDecodeTimeAtom(
                    (MP4TrackFragmentDecodeTimeAtomPtr*)&newAtom);
            break;
        case MP4MovieFragmentRandomAccessAtomType:
            MP4MSG("MP4MovieFragmentRandomAccessAtomType: \n");
            err = MP4CreateMovieFragmentRandomAccessAtom(
                    (MP4MovieFragmentRandomAccessAtomPtr*)&newAtom);
            break;
        case MP4TrackFragmentRandomAccessAtomType:
            MP4MSG("MP4TrackFragmentRandomAccessAtomType: \n");
            err = MP4CreateTrackFragmentRandomAccessAtom(
                    (MP4TrackFragmentRandomAccessAtomPtr*)&newAtom);
            break;
        case MP4MovieFragmentRandomAccessOffsetAtomType:
            MP4MSG("MP4MovieFragmentRandomAccessOffsetAtomType: \n");
            err = MP4CreateMovieFragmentRandomAccessOffsetAtom(
                    (MP4MovieFragmentRandomAccessOffsetAtomPtr*)&newAtom);
            break;
        case MP4ProtectionSystemSpecificHeaderAtomType:
            MP4MSG("MP4ProtectionSystemSpecificHeaderAtomType: \n");
            err = MP4CreateProtectionSystemSpecificHeaderAtom((MP4PSSHAtomPtr*)&newAtom);
            break;
        case MP4ProtectedVideoSampleEntryAtomType:
            MP4MSG("MP4ProtectedVideoSampleEntryAtomType: \n");
            err = MP4CreateProtectedVideoSampleEntryAtom((MP4ProtectedVideoSampleAtomPtr*)&newAtom);
            break;
        case MP4ProtectedAudioSampleEntryAtomType:
            MP4MSG("MP4ProtectedAudioSampleEntryAtomType: \n");
            err = MP4CreateProtectedAudioSampleEntryAtom((MP4ProtectedAudioSampleAtomPtr*)&newAtom);
            break;
        case MP4ProtectionSchemeInfoAtomType:
            MP4MSG("MP4ProtectionSchemeInfoAtomType: \n");
            err = MP4CreateProtectionSchemeInfoAtom((MP4ProtectionSchemeInfoAtomPtr*)&newAtom);
            break;
        case MP4OriginFormatAtomType:
            MP4MSG("MP4OriginFormatAtomType: \n");
            err = MP4CreateOriginFormatAtom((MP4OriginFormatAtomPtr*)&newAtom);
            break;
        case MP4SchemeTypeAtomType:
            MP4MSG("MP4SchemeTypeAtomType: \n");
            err = MP4CreateSchemeTypeAtom((MP4SchemeTypeAtomPtr*)&newAtom);
            break;
        case MP4SchemeInfoAtomType:
            MP4MSG("MP4SchemeInfoAtomType: \n");
            err = MP4CreateSchemeInfoAtom((MP4SchemeInfoAtomPtr*)&newAtom);
            break;
        case MP4TrackEncryptionAtomType:
            MP4MSG("MP4TrackEncryptionAtomType: \n");
            err = MP4CreateTrackEncryptionAtom((MP4TrackEncryptionAtomPtr*)&newAtom);
            break;
        case MP4SampleEncryptionAtomType:
            MP4MSG("MP4SampleEncryptionAtomType: \n");
            err = MP4CreateSampleEncryptionAtom((MP4SampleEncryptionAtomPtr*)&newAtom);
            break;
        case MP4ALACSampleEntryAtomType:
            MP4MSG("MP4ALACSampleEntryAtomType: \n");
            err = MP4CreateALACSampleEntryAtom((MP4ALACSampleEntryAtomPtr*)&newAtom);
            break;
        case MP4OpusSampleEntryAtomType:
            MP4MSG("MP4OpusSampleEntryAtomType: \n");
            err = MP4CreateOpusSampleEntryAtom((MP4OpusSampleEntryAtomPtr*)&newAtom);
            break;
        case MP4FlacSampleEntryAtomType:
            MP4MSG("MP4FlacSampleEntryAtomType: \n");
            err = MP4CreateFlacSampleEntryAtom((MP4FlacSampleEntryAtomPtr*)&newAtom);
            break;
        case MPEGHA1SampleEntryAtomType:
        case MPEGHM1SampleEntryAtomType:
            MP4MSG("MPEGHSampleEntryAtomType: \n");
            err = MP4CreateMPEGHSampleEntryAtom((MPEGHSampleEntryAtomPtr*)&newAtom, atomType);
            break;
        case MHACAtomType:
            MP4MSG("MHACAtomType: \n");
            err = MP4CreateMhacAtom((MhacAtomPtr*)&newAtom);
            break;
        case MHAPAtomType:
            MP4MSG("MHAPAtomType: \n");
            err = MP4CreateMhapAtom((MhapAtomPtr*)&newAtom);
            break;
        case MP4AC4SampleEntryAtomType:
            MP4MSG("MP4AC4SampleEntryAtomType: \n");
            err = MP4CreateAC4SampleEntryAtom((AC4SampleEntryAtomPtr*)&newAtom);
            break;
        case DVAVSampleEntryAtomType:
        case DVA1SampleEntryAtomType:
        case DVHESampleEntryAtomType:
        case DVH1SampleEntryAtomType:
        case DAV1SampleEntryAtomType:
            MP4MSG("DolbyVision SampleEntryAtomType: \n");
            err = MP4CreateDolbyVisionSampleEntryAtom((DolbyVisionSampleEntryAtomPtr*)&newAtom);
            break;

        case PrimaryItemAtomType:
            MP4MSG("PrimaryItemAtomType: \n");
            err = MP4CreatePrimaryItemAtom((PrimaryItemAtomPtr*)&newAtom);
            break;

        case ItemInfoAtomType:
            MP4MSG("ItemInfoAtomType: \n");
            err = MP4CreateItemInfoAtom((ItemInfoAtomPtr*)&newAtom);
            break;

        case InfoEntryAtomType:
            MP4MSG("InfoEntryAtomType: \n");
            err = MP4CreateInfoEntryAtom((InfoEntryAtomPtr*)&newAtom);
            break;

        case ItemReferenceAtomType:
            MP4MSG("ItemReferenceAtomType: \n");
            err = MP4CreateItemReferenceAtom((ItemReferenceAtomPtr*)&newAtom);
            break;

        case ItemPropertiesAtomType:
            MP4MSG("ItemPropertiesAtomType: \n");
            err = MP4CreateItemPropertiesAtom((ItemPropertiesAtomPtr*)&newAtom);
            break;

        case ItemPropertyContainerAtomType:
            MP4MSG("ItemPropertyContainerAtomType: \n");
            err = MP4CreateItemPropertyContainerAtom((ItemPropertyContainerAtomPtr*)&newAtom);
            break;

        case ItemPropertyAssociationAtomType:
            MP4MSG("ItemPropertyAssociationAtomType: \n");
            err = MP4CreateItemPropertyAssociationAtom((ItemPropertyAssociationAtomPtr*)&newAtom);
            break;

        case ItemLocationAtomType:
            MP4MSG("ItemLocationAtomType: \n");
            err = MP4CreateItemLocationAtom((ItemLocationAtomPtr*)&newAtom);
            break;

        case ItemDataAtomType:
            MP4MSG("ItemDataAtomType: \n");
            err = MP4CreateItemDataAtom((ItemDataAtomPtr*)&newAtom);
            break;

        case MP4Vp9SampleEntryAtomType:
            MP4MSG("MP4Vp9SampleEntryAtomType: \n");
            err = MP4CreateVp9SampleEntryAtom((MP4Vp9SampleEntryAtomPtr*)&newAtom);
            break;

        case MP4PixelAspectRatioAtomType:
            MP4MSG("MP4PixelAspectRatioAtomType: \n");
            err = MP4CreatePixelAspectRatioAtom((MP4PASPAtomPtr*)&newAtom);
            break;

        case MP4VpccAtomType:
            MP4MSG("MP4VpccAtomType: \n");
            err = MP4CreateVpccAtom((MP4VPCCAtomPtr*)&newAtom);
            break;

        case MP4ApvSampleEntryAtomType:
            MP4MSG("MP4CreateApvSampleEntryAtomTppe: \n");
            err = MP4CreateApvSampleEntryAtom((MP4ApvSampleEntryAtomPtr*)&newAtom);
            break;

        case MP4ApvCAtomType:
            err = MP4CreateApvcAtom((MP4APVCAtomPtr*)&newAtom);
            break;

        default:
            MP4MSG("Unknown atom type 0x%x\n", atomType);
            err = MP4CreateUnknownAtom((MP4UnknownAtomPtr*)&newAtom);
            //			MP4MSG("default\n");
            break;
    }

    if (err == MP4NoErr) {
        *outAtom = newAtom;
    }

    // MP4MSG("err code is %d\n", err);

    return err;
}

void MP4TypeToString(u32 inType, char* ioStr) {
    u32 i;
    int ch;

    for (i = 0; i < 4; i++, ioStr++) {
        ch = inType >> (8 * (3 - i)) & 0xff;
        if (isprint(ch))
            *ioStr = ch;
        else
            *ioStr = '.';
    }
    *ioStr = '\0';
}

MP4Err MP4ParseAtomUsingProtoList(MP4InputStreamPtr inputStream, u32* protoList, u32 defaultAtom,
                                  MP4AtomPtr* outAtom) {
    MP4AtomPtr atomProto = NULL; /* pointer to the temporary prototype atom */
    MP4Err err = MP4NoErr;
    u32 size; /* 32 bit atom size, temp variable */
    u64 bytesParsed = 0L;
    MP4Atom protoAtom; /* temporary prototype atom, before entire atom is created according to
                          stream */
    MP4AtomPtr newAtom = NULL; /* pointer to the entire atom to be created */
    char typeString[8];
#ifndef COLDFIRE
    char msgString[80];
#endif
    u64 beginAvail = 0;
    u64 consumedBytes = 0;
    u32 useDefaultAtom = 0;

    atomProto = &protoAtom;

    if ((inputStream == NULL) || (outAtom == NULL)) {
        BAILWITHERROR(MP4BadParamErr)
    }

    *outAtom = NULL;
    beginAvail = inputStream->available;

    if (beginAvail > 0 && beginAvail < 4)
        BAILWITHERROR(MP4EOF)

    inputStream->msg(inputStream, "{");
    inputStream->indent++;
    err = MP4CreateBaseAtom(atomProto);
    if (err)
        goto bail;

    /* atom size */
    err = inputStream->read32(inputStream, &size, NULL);
    if (err)
        goto bail;
    atomProto->size = size;

    MP4MSG("atom size is %llu\n", atomProto->size);

    // fix f44, engr171231
    if (atomProto->size > inputStream->available + 4) {
        MP4MSG("atomProto->size (%lld) > inputStream->availablesize (%lld) + 4\n", atomProto->size,
               inputStream->available);
        atomProto->size = inputStream->available + 4;
    }

    if (atomProto->size == 0) {
        atomProto->size = beginAvail;
        /* if the atom siz is 0 and media data and meta data is already parsed*/
        // MP4DbgLog("WARNING. Atom size is ZERO, extends to end of stream\n");
        if ((inputStream->stream_flags & metadata_flag) &&
            (inputStream->stream_flags & mediadata_flag)) {
            BAILWITHERROR(MP4EOF) /* if this is the 2nd 'mdat', still assume EOF. In fact, only
                                     'mdat' proceding 'moov' need to be skipped */
        } else if ((inputStream->stream_flags & metadata_flag) ||
                   (inputStream->stream_flags & mediadata_flag)) {
            ;
        } else {
            MP4MSG("error MP4NoQTAtomErr\n");
            BAILWITHERROR(MP4NoQTAtomErr)
        }
    }

    bytesParsed += 4L;

#ifndef COLDFIRE
    sprintf(msgString, "atom size is %d", (int)atomProto->size);
    inputStream->msg(inputStream, msgString);
#endif
    /* atom type */
    err = inputStream->read32(inputStream, &atomProto->type, NULL);
    if (err)
        goto bail;
    bytesParsed += 4L;
    MP4TypeToString(atomProto->type, typeString);

    MP4MSG("indent %d, atom type is %c%c%c%c\n", inputStream->indent, typeString[0], typeString[1],
           typeString[2], typeString[3]);

#ifndef COLDFIRE
    sprintf(msgString, "atom type is '%s'", typeString);
    inputStream->msg(inputStream, msgString);
#endif

#if 0 /* Never read more data and disturb normal atom parsing ! */
    if ( atomProto->type == MP4ExtendedAtomType )
    {
        err = inputStream->readData( inputStream, 16, (char*) atomProto->uuid, NULL );	if ( err ) goto bail;
        bytesParsed += 16L;
    }
#endif

    /* large atom */
    if (atomProto->size == 1) {
        err = inputStream->read32(inputStream, &size, NULL);
        if (err)
            goto bail;
        //	fix ENGR00223614
        atomProto->size64 = size;
        atomProto->size64 <<= 32;
        err = inputStream->read32(inputStream, &size, NULL);
        if (err)
            goto bail;
        atomProto->size64 |= size;
        atomProto->size = atomProto->size64; /* "size" is still right for large atoms */
        bytesParsed += 8L;
    }

    if (((s64)atomProto->size < 0) ||
        (((atomProto->size - 8) > inputStream->available) &&
         (inputStream->stream_flags &
          mediadata_flag))) /* amanda: give some size error tolerance for 1st 'mdat'*/
    {
        /* the moov atom and udta atom may be after mdat atom.
        And not only all atoms must be entire */
        MP4MSG("atomProto->size = %lld, bytes available %lld\n", atomProto->size,
               inputStream->available);
        err = acceptCutShortAtom(atomProto->type);
        if (PARSER_SUCCESS != err)
            goto bail;
    }

    atomProto->bytesRead = bytesParsed;
    if ((s64)(atomProto->size - bytesParsed) < 0)
        BAILWITHERROR(MP4BadDataErr)
    if (protoList) {
        while (*protoList) {
            if (*protoList == atomProto->type) {
                break;
            }
            protoList++;
        }
        if (*protoList == 0) {
            useDefaultAtom = 1;
        }
    }
    err = MP4CreateAtom(useDefaultAtom ? defaultAtom : atomProto->type, &newAtom, inputStream);
    if (err)
        goto bail;

#ifndef COLDFIRE
    sprintf(msgString, "atom name is '%s'", newAtom->name);
    inputStream->msg(inputStream, msgString);
#endif
    err = newAtom->createFromInputStream(newAtom, atomProto, (char*)inputStream);
    if (err && (MP4EOF !=
                err)) { /*ENGR56127: entire atoms may still ends with MP4EOF.
                        So corrupt file header shall return value other than MP4EOF.
                        BLR code uses "inputStream->available==0" for EOF checking, same effect.*/
        goto bail;
    }

    consumedBytes = beginAvail - inputStream->available;
    if ((consumedBytes != atomProto->size) && (!(inputStream->stream_flags & live_flag))) {
        /* ERROR CONCEALMENT: wrong atom size, seek to the end of atom */
        s32 offset = (s32)((s64)atomProto->size - (s64)consumedBytes);
        inputStream->file_offset += offset;
        inputStream->available -= offset;
        atomProto->bytesRead = atomProto->size;

#ifndef COLDFIRE
        sprintf(msgString, "##### atom size is %d but parse used %d bytes ####",
                (int)atomProto->size, (int)consumedBytes);
        inputStream->msg(inputStream, msgString);
#endif
    }
    /* else if (inputStream->isLive) */
    /* for live steaming, we should not seek */
    if (atomProto->type == MP4MovieExtendsAtomType) {
        inputStream->stream_flags |= fragmented_flag;
    }

    if (atomProto->type == MP4MediaDataAtomType) {
        if ((inputStream->stream_flags & mediadata_flag) &&
            (inputStream->stream_flags & metadata_flag) &&
            (!(inputStream->stream_flags & fragmented_flag))) {
            inputStream->available = 0; /* parsing ends if both 1st 'mdat' and 'moov' are found*/
        }
    }
    *outAtom = newAtom;
    inputStream->indent--;
    inputStream->msg(inputStream, "}");

bail:
    TEST_RETURN(err);

    // MP4MSG("err code is %d\n", err);

    return err;
}

MP4Err MP4ParseAtom(MP4InputStreamPtr inputStream, MP4AtomPtr* outAtom) {
    MP4Err err = MP4ParseAtomUsingProtoList(inputStream, NULL, 0, outAtom);

    return err;
}

MP4Err MP4ParseAtomInBuf(MP4InputStreamPtr inputStream, MP4AtomPtr* outAtom, char* headerBuf) {
    MP4AtomPtr atomProto = NULL; /* pointer to the temporary prototype atom */
    MP4Err err = MP4NoErr;
    MP4Atom protoAtom; /* temporary prototype atom, before entire atom is created according to
                          stream */
    MP4AtomPtr newAtom = NULL; /* pointer to the entire atom to be created */
    char typeString[8];
    char msgString[80];

    atomProto = &protoAtom;

    if ((inputStream == NULL) || (outAtom == NULL)) {
        BAILWITHERROR(MP4BadParamErr)
    }

    *outAtom = NULL;

    inputStream->msg(inputStream, "{");
    inputStream->indent++;
    err = MP4CreateBaseAtom(atomProto);
    if (err)
        goto bail;

    atomProto->size = headerBuf[0] << 24 | headerBuf[1] << 16 | headerBuf[2] << 8 | headerBuf[3];
    MP4MSG("atom size is %llu\n", atomProto->size);

    sprintf(msgString, "atom size is %d", (int)atomProto->size);
    inputStream->msg(inputStream, msgString);

    atomProto->type = headerBuf[4] << 24 | headerBuf[5] << 16 | headerBuf[6] << 8 | headerBuf[7];
    MP4TypeToString(atomProto->type, typeString);
    MP4MSG("indent %d, atom type is %c%c%c%c\n", inputStream->indent, typeString[0], typeString[1],
           typeString[2], typeString[3]);

    sprintf(msgString, "atom type is '%s'", typeString);
    inputStream->msg(inputStream, msgString);

    atomProto->bytesRead = 8;
    err = MP4CreateAtom(atomProto->type, &newAtom, inputStream);
    if (err)
        goto bail;

    sprintf(msgString, "atom name is '%s'", newAtom->name);
    inputStream->msg(inputStream, msgString);

    err = newAtom->createFromInputStream(newAtom, atomProto, (char*)inputStream);
    if (err && (MP4EOF !=
                err)) { /*ENGR56127: entire atoms may still ends with MP4EOF.
                        So corrupt file header shall return value other than MP4EOF.
                        BLR code uses "inputStream->available==0" for EOF checking, same effect.*/
        goto bail;
    }

    *outAtom = newAtom;
    inputStream->indent--;
    inputStream->msg(inputStream, "}");

bail:
    TEST_RETURN(err);
    // MP4MSG("err code is %d\n", err);
    return err;
}

MP4Err MP4CalculateBaseAtomFieldSize(struct MP4Atom* self) {
    self->size = 8;
    return MP4NoErr;
}

MP4Err MP4CalculateFullAtomFieldSize(struct MP4FullAtom* self) {
    MP4Err err = MP4CalculateBaseAtomFieldSize((MP4AtomPtr)self);
    self->size += 4;
    return err;
}

MP4Err MP4SerializeCommonBaseAtomFields(struct MP4Atom* self, char* buffer) {
    MP4Err err;
    err = MP4NoErr;
    self->bytesWritten = 0;
    assert(self->size);
    assert(self->type);
    PUT32(size);
    PUT32(type);
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4SerializeCommonFullAtomFields(struct MP4FullAtom* self, char* buffer) {
    MP4Err err;
    err = MP4SerializeCommonBaseAtomFields((MP4AtomPtr)self, buffer);
    if (err)
        goto bail;
    buffer += self->bytesWritten;
    PUT8(version);
    PUT24(flags);
bail:
    TEST_RETURN(err);

    return err;
}

/* Three cases:
1. critical atoms can not be cut short, return error.eg. mvhd, trak
2. some atoms can be disposed, return EOS and no need for further parsing. eg. udta.
3. some atoms need further parsing. eg moov */
static int32 acceptCutShortAtom(uint32 atomType) {
    int32 err = PARSER_SUCCESS;
    uint32 i;

    MP4MSG("----- cut-short atom found: ");
    PrintTag(atomType);
    for (i = 0; i < (sizeof(critialAtomTypes) / sizeof(uint32)); i++) {
        // MP4MSG("Critical atom type %d, ", i); PrintTag(critialAtomTypes[i]);
        if (atomType == critialAtomTypes[i]) {
            MP4MSG("----- Critical atom type %u, abort parsing.\n", i);
            BAILWITHERROR(MP4BadDataErr)
        }
    }

    for (i = 0; i < (sizeof(disposableAtomTypes) / sizeof(uint32)); i++) {
        // MP4MSG("Disposable atom type %d, ", i); PrintTag(disposableAtomTypes[i]);
        if (atomType == disposableAtomTypes[i]) {
            MP4MSG("----- Disposable atom type %u, EOS. stop parsing.\n", i);
            BAILWITHERROR(MP4EOF)
        }
    }

    MP4MSG("----- Accept it!\n");

bail:
    return err;
}

static void PrintTag(uint32 tag) {
    MP4MSG("%c%c%c%c \n", ((tag) >> 24) & 0xff, ((tag) >> 16) & 0xff, ((tag) >> 8) & 0xff,
           (tag)&0xff);
    (void)tag;
}
