/*
 ***********************************************************************
 * Copyright (c) 2005-2011,2016, Freescale Semiconductor Inc.,
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
    $Id: TrackAtom.c,v 1.2 2000/05/17 08:01:31 francesc Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static MP4Err addAtom(MP4TrackAtomPtr self, MP4AtomPtr atom);

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4TrackAtomPtr self;
    u32 i;
    err = MP4NoErr;
    self = (MP4TrackAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    DESTROY_ATOM_LIST

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err setMdat(struct MP4TrackAtom* self, MP4AtomPtr mdat) {
    self->mdat = mdat;
    return MP4NoErr;
}

static MP4Err setMoov(struct MP4TrackAtom* self, MP4PrivateMovieRecordPtr moov) {
    self->moov = moov;
    return MP4NoErr;
}

static MP4Err adjustedDuration(u64 mediaDuration, u32 mediaTimeScale, u32 movieTimeScale,
                               long double* outDuration) {
    MP4Err err;

    err = MP4NoErr;

    if (mediaTimeScale == 0)
        BAILWITHERROR(MP4BadParamErr);

    *outDuration = ((long double)mediaDuration * movieTimeScale) / mediaTimeScale;
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err calculateDuration(struct MP4TrackAtom* self, u32 movieTimeScale) {
    MP4Err err = MP4NoErr;
    MP4TrackHeaderAtomPtr tkhd;
    MP4MediaAtomPtr mdia;
    MP4MediaHeaderAtomPtr mhdr;
    u64 mediaDuration;
    u32 mediaTimeScale;
    long double trackDuration = 0;

    tkhd = (MP4TrackHeaderAtomPtr)self->trackHeader;
    if (tkhd == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);

    mdia = (MP4MediaAtomPtr)self->trackMedia;
    if (mdia == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);
    mhdr = (MP4MediaHeaderAtomPtr)mdia->mediaHeader;
    mediaDuration = mhdr->duration;
    mediaTimeScale = (u32)mhdr->timeScale;

    /* if edit list is not present or empty (duration remains 0) */
    if (0 == trackDuration) {
        err = adjustedDuration(mediaDuration, mediaTimeScale, movieTimeScale, &trackDuration);
        if (err)
            goto bail;
    }

    tkhd->duration = trackDuration;
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err getDuration(struct MP4TrackAtom* self, long double* outDuration) {
    MP4Err err;
    MP4TrackHeaderAtomPtr tkhd;
    tkhd = (MP4TrackHeaderAtomPtr)self->trackHeader;

    err = MP4NoErr;
    if (outDuration == NULL)
        BAILWITHERROR(MP4BadParamErr);
    if (tkhd == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);
    *outDuration = tkhd->duration;
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err getMatrix(struct MP4TrackAtom* self, u32 outMatrix[9]) {
    MP4TrackHeaderAtomPtr tkhd;
    MP4Err err;
    err = MP4NoErr;
    tkhd = (MP4TrackHeaderAtomPtr)self->trackHeader;
    if (tkhd == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);
    memcpy(outMatrix, &(tkhd->qt_matrixA), sizeof(ISOMatrixRecord));
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err getLayer(struct MP4TrackAtom* self, s16* outLayer) {
    MP4TrackHeaderAtomPtr tkhd;
    MP4Err err;
    err = MP4NoErr;
    tkhd = (MP4TrackHeaderAtomPtr)self->trackHeader;
    if (tkhd == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);
    *outLayer = (s16)tkhd->qt_layer;
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err getDimensions(struct MP4TrackAtom* self, u32* outWidth, u32* outHeight) {
    MP4TrackHeaderAtomPtr tkhd;
    MP4Err err;
    err = MP4NoErr;
    tkhd = (MP4TrackHeaderAtomPtr)self->trackHeader;
    if (tkhd == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);
    *outWidth = (u32)tkhd->qt_trackWidth;
    *outHeight = (u32)tkhd->qt_trackHeight;
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err getVolume(struct MP4TrackAtom* self, s16* outVolume) {
    MP4TrackHeaderAtomPtr tkhd;
    MP4Err err;
    err = MP4NoErr;
    tkhd = (MP4TrackHeaderAtomPtr)self->trackHeader;
    if (tkhd == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);
    *outVolume = (s16)tkhd->qt_volume;
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err getEnabled(struct MP4TrackAtom* self, u32* outEnabled) {
    MP4TrackHeaderAtomPtr tkhd;
    tkhd = (MP4TrackHeaderAtomPtr)self->trackHeader;
    *outEnabled = tkhd->flags & 1 ? 1 : 0;
    return MP4NoErr;
}

static MP4Err addAtom(MP4TrackAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    err = MP4NoErr;
    err = MP4AddListEntry(atom, self->atomList);
    if (err)
        goto bail;
    switch (atom->type) {
        case MP4TrackHeaderAtomType:
            if (self->trackHeader)
                BAILWITHERROR(MP4BadDataErr);
            self->trackHeader = atom;
            break;

        case MP4EditAtomType:
            if (self->trackEditAtom)
                BAILWITHERROR(MP4BadDataErr);
            self->trackEditAtom = atom;
            break;

        case MP4UserDataAtomType:
            if (self->udta)
                BAILWITHERROR(MP4BadDataErr);
            self->udta = atom;
            break;

        case MP4TrackReferenceAtomType:
            if (self->trackReferences)
                BAILWITHERROR(MP4BadDataErr);
            self->trackReferences = atom;
            break;

        case MP4MediaAtomType:
            if (self->trackMedia)
                BAILWITHERROR(MP4BadDataErr);
            self->trackMedia = atom;
            ((MP4MediaAtomPtr)atom)->mediaTrack = (MP4AtomPtr)self;
            break;

        case QTMetadataAtomType:
            if (self->meta)
                BAILWITHERROR(MP4BadDataErr);
            self->meta = atom;
            break;
    }
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err trakAtomCreateFromInputStream(MP4AtomPtr s, MP4AtomPtr proto,
                                            MP4InputStreamPtr inputStream) {
#ifdef FSL_MP4_PARSER_DBG_TIME
#if defined(__WINCE) || defined(WIN32)
    u32 start_time = timeGetTime();
    u32 end_time;
#else
    struct timezone time_zone;
    struct timeval start_time;
    struct timeval end_time;
    struct timeval start_time_max_size;
    gettimeofday(&start_time, &time_zone);
#endif
#endif

    PARSE_ATOM_LIST(MP4TrackAtom)

bail:
    TEST_RETURN(err);

#ifdef FSL_MP4_PARSER_DBG_TIME
#if defined(__WINCE) || defined(WIN32)
    end_time = timeGetTime();
    MP4DbgLog("\ntime for trak: %ld ms\n\n", end_time - start_time);
#else
    gettimeofday(&end_time, &time_zone);
    if (end_time.tv_usec >= start_time.tv_usec) {
        MP4DbgLog("\ntime for trak: %ld sec, %ld us\n\n", end_time.tv_sec - start_time.tv_sec,
                  end_time.tv_usec - start_time.tv_usec);
    } else {
        MP4DbgLog("\ntime for trak: %ld us\n\n", end_time.tv_sec - start_time.tv_sec - 1,
                  end_time.tv_usec + 1000000000 - start_time.tv_usec);
    }
#endif
#endif

    return err;
}

MP4Err MP4CreateTrackAtom(MP4TrackAtomPtr* outAtom) {
    MP4Err err;
    MP4TrackAtomPtr self;

    self = (MP4TrackAtomPtr)MP4LocalCalloc(1, sizeof(MP4TrackAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4TrackAtomType;
    self->name = "track";
    self->createFromInputStream = (cisfunc)trakAtomCreateFromInputStream;
    self->destroy = destroy;
    self->addAtom = addAtom;
    self->setMoov = setMoov;
    self->setMdat = setMdat;
    self->getEnabled = getEnabled;
    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;

    self->calculateDuration = calculateDuration;
    self->getDuration = getDuration;
    self->getMatrix = getMatrix;
    self->getLayer = getLayer;
    self->getDimensions = getDimensions;
    self->getVolume = getVolume;
    *outAtom = self;
bail:
    TEST_RETURN(err);
    return err;
}
