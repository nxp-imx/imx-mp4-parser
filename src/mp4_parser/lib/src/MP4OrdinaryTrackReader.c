/*
***********************************************************************
* Copyright (c) 2005-2011, Freescale Semiconductor Inc.,
* Copyright 2026 NXP
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
    $Id: MP4OrdinaryTrackReader.c,v 1.3 2000/04/21 14:42:42 francesc Exp $
*/
#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"
#include "MP4Impl.h"
#include "MP4Movies.h"
#include "MP4TrackReader.h"

__attribute__((unused)) static void put32(u32 val, char* buf) {
    buf[3] = val & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[0] = (val >> 24) & 0xFF;
}

static MP4Err destroy(struct MP4TrackReaderStruct* self) {
    MP4DisposeHandle(self->sampleH);
    self->sampleH = NULL;
    MP4LocalFree(self);
    return MP4NoErr;
}

/* segments are used for edit list */
static MP4Err getNextSegment(MP4TrackReaderPtr self) {
    MP4Err err;
    MP4EditListAtomPtr elst;
    s64 segmentMediaTime;
    u64 segmentDuration;
    u64 segmentMediaDuration;
    u32 segmentEndSampleNumber;

    err = MP4NoErr;

    elst = (MP4EditListAtomPtr)self->elst;
    if (elst == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);

    for (;;) {
        u32 empty;
        err = elst->isEmptyEdit(elst, self->nextSegment, &empty);
        if (err)
            goto bail;
        if (empty) {
            if (self->nextSegment == self->trackSegments) {
                BAILWITHERROR(MP4InvalidMediaErr);
            } else
                self->nextSegment++;
        } else
            break;
    }
    err = elst->getIndSegmentTime((MP4AtomPtr)elst, self->nextSegment, &self->segmentMovieTime,
                                  &segmentMediaTime, &segmentDuration);
    if (err)
        goto bail;

    err = MP4MediaTimeToSampleNum(self->media, segmentMediaTime, &self->nextSampleNumber, NULL,
                                  NULL, NULL);
    if (err)
        goto bail;

    self->segmentEndTime = self->mediaTimeScale * (self->segmentMovieTime + segmentDuration) /
                           self->movieTimeScale;

    segmentMediaDuration = (self->mediaTimeScale * segmentDuration) / self->movieTimeScale;

    err = MP4MediaTimeToSampleNum(self->media, segmentMediaTime + segmentMediaDuration,
                                  &segmentEndSampleNumber, NULL, NULL, NULL);
    if (err)
        goto bail;

    if (segmentEndSampleNumber < self->nextSampleNumber)
        BAILWITHERROR(MP4InvalidMediaErr);

    self->segmentSampleCount = (segmentEndSampleNumber - self->nextSampleNumber);
    self->segmentBeginTime = (self->mediaTimeScale * self->segmentMovieTime) / self->movieTimeScale;
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err getNextAccessUnit(struct MP4TrackReaderStruct* self, MP4Handle outAccessUnit,
                                u32* outSize, u32* outSampleFlags, u64* outCTS, u64* outDTS,
                                u32* outDuration) {
    MP4Err err;
    u32 sampleFlags;
    u64 sampleDTS;
    s32 sampleCTSOffset;
    u64 duration;

    u32 sdsm_offset;
    u32 odsm_offset;
    s32 suggestCache;

    odsm_offset = self->odsm_offset;
    sdsm_offset = self->sdsm_offset;

    err = MP4NoErr;
    *outSize = 0;
    (void)outAccessUnit;

    if (self->nextSegment > self->trackSegments) {
        // BAILWITHERROR( MP4EOF )
    }
    if ((self->elst == 0) && (self->nextSampleNumber > self->segmentSampleCount)) {
        BAILWITHERROR(MP4EOF)
    }

    /* first get the sample or samples,
    "nextSampleNumber" will be updated, and it will increase more than one if sample cache is
    applied. A cache is used for PCM audio track to speed up reading.*/
    suggestCache = ((MP4MediaAtomPtr)(self->media))->suggestCache;
    if (suggestCache)
        err = MP4GetCachedMediaSamples(self->media, &self->nextSampleNumber,
                                       // outAccessUnit,
                                       outSize, &sampleDTS, &sampleCTSOffset, &duration,
                                       &sampleFlags, &self->sampleDescIndex, odsm_offset,
                                       sdsm_offset);
    else
        err = MP4GetIndMediaSample(self->media, &self->nextSampleNumber,
                                   // outAccessUnit,
                                   outSize, &sampleDTS, &sampleCTSOffset, &duration, &sampleFlags,
                                   &self->sampleDescIndex, odsm_offset, sdsm_offset);
    if (err)
        goto bail;

    if (self->isODTrack) {
        BAILWITHERROR(MP4NotImplementedErr)
    }

    *outSampleFlags = sampleFlags;
    *outDTS = sampleDTS + self->segmentBeginTime;

    if (sampleFlags & MP4MediaSampleHasCTSOffset)
        *outCTS = *outDTS + sampleCTSOffset;
    else
        *outCTS = *outDTS;

#if 0
    if ( (self->elst) && ((duration + sampleDTS + self->segmentBeginTime) >= self->segmentEndTime) )
    {
        self->nextSegment += 1;
        if ( self->nextSegment <= self->trackSegments )
        {
            err = getNextSegment( self ); if (err) goto bail;
        }
    }
#endif
    if (outDuration) {
        *outDuration = (u32)duration;
    }

bail:
    return err;
}

static MP4Err setSLConfig(struct MP4TrackReaderStruct* self, MP4SLConfig slconfig) {
    // self->slconfig = slconfig;
    // return MP4NoErr;
    (void)self;
    (void)slconfig;

    return MP4NotImplementedErr;
}

static MP4Err getNextPacket(MP4TrackReaderPtr self, MP4Handle outPacket, u32* outSize) {
    *outSize = 0;
    (void)self;
    (void)outPacket;
    return MP4NotImplementedErr;
}

static MP4Err setupReader(MP4TrackReaderPtr self) {
    MP4Err err;
    MP4TrackAtomPtr trak;
    MP4EditAtomPtr edts;
    u32 sampleCount;

    err = MP4NoErr;
    trak = (MP4TrackAtomPtr)self->track;
    edts = (MP4EditAtomPtr)trak->trackEditAtom;
    // only use edts atom to get encoder delay and padding. so remove it.
    if (1 /* (edts == 0) || (edts->editListAtom == 0) */) {
        err = MP4GetMediaSampleCount(self->media, &sampleCount);
        if (err)
            goto bail;
        self->elst = 0;
        self->trackSegments = 1;
        self->nextSegment = 0;
        self->segmentSampleCount = sampleCount;
        self->nextSampleNumber = 1;
        self->segmentMovieTime = 0;
        self->sampleDescIndex = 1;
        self->segmentBeginTime = 0;
    } else {
        self->elst = (MP4EditListAtomPtr)edts->editListAtom;
        self->trackSegments = self->elst->getEntryCount(self->elst);
        self->nextSegment = 1;
        self->sampleDescIndex = 1;
        err = getNextSegment(self);
        if (err)
            goto bail;
    }

bail:
    TEST_RETURN(err);

    return err;
}

/******************************************************************
 * To Set the reder at the desired postion for the seek. Ranjeet.
 ******************************************************************/
static MP4Err setupReaderRevised(MP4TrackReaderPtr self, u32 number) {
    MP4Err err;
    MP4TrackAtomPtr trak;
    MP4EditAtomPtr edts;
    u32 sampleCount;

    err = MP4NoErr;
    trak = (MP4TrackAtomPtr)self->track;
    edts = (MP4EditAtomPtr)trak->trackEditAtom;
    // only use edts atom to get encoder delay and padding. so remove it.
    if (1 /* (edts == 0) || (edts->editListAtom == 0) */) {
        err = MP4GetMediaSampleCount(self->media, &sampleCount);
        if (err)
            goto bail;
        self->elst = 0;
        self->trackSegments = 1;
        self->nextSegment = 0;
        self->segmentSampleCount = sampleCount;  // Ranjeet 1000 "sampleCount"
        self->nextSampleNumber = number;
        self->segmentMovieTime = 0;
        self->sampleDescIndex = 1;
        self->segmentBeginTime = 0;
    } else {
        err = MP4GetMediaSampleCount(self->media, &sampleCount);
        if (err)
            goto bail;
        self->elst = (MP4EditListAtomPtr)edts->editListAtom;
        self->trackSegments = self->elst->getEntryCount(self->elst);
        self->segmentSampleCount = sampleCount;
        self->segmentMovieTime = 0;
        self->sampleDescIndex = 1;
        self->segmentBeginTime = 0;

        // find the correct segment for this sample number
        for (self->nextSegment = 1; self->nextSegment <= self->trackSegments; self->nextSegment++) {
            err = getNextSegment(self);
            if (err)
                goto bail;
            if (number < self->nextSampleNumber) {
                number = self->nextSampleNumber;
                break;
            }

            if (number >= self->nextSampleNumber &&
                number <= self->nextSampleNumber + self->segmentSampleCount)
                break;
        }
        self->nextSampleNumber = number;
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateOrdinaryTrackReader(MP4Movie theMovie, MP4Track theTrack,
                                    MP4TrackReaderPtr* outReader) {
    MP4Err err;
    MP4TrackReaderPtr self;

    err = MP4NoErr;
    self = (void*)MP4LocalCalloc(1, sizeof(struct MP4TrackReaderStruct));
    TESTMALLOC(self)
    self->movie = theMovie;
    self->track = theTrack;
    self->destroy = destroy;
    self->getNextPacket = getNextPacket;
    self->getNextAccessUnit = getNextAccessUnit;
    self->setSLConfig = setSLConfig;
    err = MP4NewHandle(4096, &self->sampleH);
    if (err)
        goto bail;
    err = MP4GetTrackMedia(theTrack, &self->media);
    if (err)
        goto bail;
    err = MP4CheckMediaDataReferences(self->media);
    if (err)
        goto bail;
    err = MP4GetMovieTimeScale(theMovie, &self->movieTimeScale);
    if (err)
        goto bail;
    err = MP4GetMediaTimeScale(self->media, &self->mediaTimeScale);
    if (err)
        goto bail;
    err = setupReader(self);
    if (err)
        goto bail;
    *outReader = self;
bail:
    TEST_RETURN(err);

    return err;
}

/****************** Changed Function for the setting the Reader. Ranjeet ******************/

MP4Err MP4GetOrdinaryTrackReader(MP4Movie theMovie, u32 FrameNumber, MP4Track theTrack,
                                 MP4TrackReaderPtr* outReader) {
    MP4Err err;
    MP4TrackReaderPtr self;

    err = MP4NoErr;
    self = (void*)MP4LocalCalloc(1, sizeof(struct MP4TrackReaderStruct));
    TESTMALLOC(self)
    self->movie = theMovie;
    self->track = theTrack;
    self->destroy = destroy;

    self->getNextPacket = getNextPacket;
    self->getNextAccessUnit = getNextAccessUnit;
    self->setSLConfig = setSLConfig;

    err = MP4NewHandle(4096, &self->sampleH);
    if (err)
        goto bail;
    err = MP4GetTrackMedia(theTrack, &self->media);
    if (err)
        goto bail;
    err = MP4CheckMediaDataReferences(self->media);
    if (err)
        goto bail;
    err = MP4GetMovieTimeScale(theMovie, &self->movieTimeScale);
    if (err)
        goto bail;
    err = MP4GetMediaTimeScale(self->media, &self->mediaTimeScale);
    if (err)
        goto bail;
    err = setupReaderRevised(self, FrameNumber);
    if (err)
        goto bail;  // Ranjeet to seek
    *outReader = self;

bail:
    TEST_RETURN(err);

    return err;
}

/* Guido : inserted to clean-up resources */
MP4Err MP4DisposeOrdinaryTrackReader(MP4TrackReaderPtr self) {
    MP4Err err;
    MP4Media theMedia;
    MP4MediaInformationAtomPtr minf;

    err = MP4NoErr;
    if (self == NULL) {
        BAILWITHERROR(MP4BadParamErr);
    }
    theMedia = self->media;
    if (theMedia == NULL) {
        BAILWITHERROR(MP4BadParamErr);
    }
    minf = (MP4MediaInformationAtomPtr)((MP4MediaAtomPtr)theMedia)->information;
    if (minf == NULL) {
        // MP4MSG("minf is nil\n");
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    err = minf->closeDataHandler((MP4AtomPtr)minf);
    if (err)
        goto bail;

bail:
    TEST_RETURN(err);

    return err;
}
