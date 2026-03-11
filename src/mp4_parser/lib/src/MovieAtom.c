/*
 ***********************************************************************
 * Copyright 2005-2014 by Freescale Semiconductor, Inc.
 *
 * Copyright 2017-2018, 2026 NXP
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
    $Id: MovieAtom.c,v 1.2 2000/05/17 08:01:30 francesc Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static MP4Err addAtom(MP4MovieAtomPtr self, MP4AtomPtr atom);

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4MovieAtomPtr self;
    u32 i;
    err = MP4NoErr;
    self = (MP4MovieAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    DESTROY_ATOM_LIST

    err = MP4DeleteLinkedList(self->trackList);
    if (err)
        goto bail;

    SAFE_DESTROY(self->jp2h);
    SAFE_DESTROY(self->ftyp);
    SAFE_DESTROY(self->sgnt);
    // SAFE_DESTROY(self->meta);
    err = MP4DeleteLinkedList(self->udtaList);
    if (err)
        goto bail;

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err getNextTrackID(MP4MovieAtomPtr self, u32* outID) {
    MP4MovieHeaderAtomPtr mvhd;
    MP4Err err;
    err = MP4NoErr;
    mvhd = (MP4MovieHeaderAtomPtr)self->mvhd;
    if (mvhd == NULL)
        BAILWITHERROR(MP4InvalidMediaErr)
    *outID = mvhd->nextTrackID;
bail:
    TEST_RETURN(err);

    return err;
}

/*
    Try to reserve the specified track ID.
    Sets *outSuccess nonzero if we may use the requested trackID.
    Has a side effect of setting the movie's nextTrackID to one
    greater than the requested trackID if this would make it the
    highest numbered track in the movie.
*/
__attribute__((unused)) static MP4Err requestTrackID(struct MP4MovieAtom* self, u32 trackID,
                                                     u32* outSuccess) {
    MP4MovieHeaderAtomPtr mvhd;
    MP4Err err;

    err = MP4NoErr;
    if ((outSuccess == 0) || (trackID == 0))
        BAILWITHERROR(MP4BadParamErr);
    mvhd = (MP4MovieHeaderAtomPtr)self->mvhd;
    *outSuccess = 0;
    if (mvhd == NULL)
        BAILWITHERROR(MP4InvalidMediaErr)
    if (trackID >= mvhd->nextTrackID) {
        mvhd->nextTrackID = trackID + 1;
        *outSuccess = 1;
    } else {
        u32 i;
        u32 trackCount = 0;
        err = MP4GetListEntryCount(self->trackList, &trackCount);
        if (err)
            goto bail;
        for (i = 0; i < trackCount; i++) {
            MP4TrackAtomPtr trak;
            MP4TrackHeaderAtomPtr tkhd;
            err = MP4GetListEntry(self->trackList, i, (char**)&trak);
            if (err)
                goto bail;
            if (trak == NULL)
                BAILWITHERROR(MP4InvalidMediaErr);
            tkhd = (MP4TrackHeaderAtomPtr)trak->trackHeader;
            if (tkhd == NULL)
                BAILWITHERROR(MP4InvalidMediaErr);
            if (tkhd->trackID == trackID)
                goto bail;
        }
        *outSuccess = 1;
    }
bail:
    return err;
}

static MP4Err getTimeScale(struct MP4MovieAtom* self, u32* outTimeScale) {
    MP4MovieHeaderAtomPtr mvhd;
    MP4Err err;
    err = MP4NoErr;
    mvhd = (MP4MovieHeaderAtomPtr)self->mvhd;
    if (mvhd == NULL)
        BAILWITHERROR(MP4InvalidMediaErr)
    *outTimeScale = mvhd->timeScale;
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err getMatrix(struct MP4MovieAtom* self, u32 outMatrix[9]) {
    MP4MovieHeaderAtomPtr mvhd;
    MP4Err err;
    err = MP4NoErr;
    mvhd = (MP4MovieHeaderAtomPtr)self->mvhd;
    if (mvhd == NULL)
        BAILWITHERROR(MP4InvalidMediaErr)
    memcpy(outMatrix, &(mvhd->qt_matrixA), sizeof(ISOMatrixRecord));
/*	outMatrix[0] = mvhd->qt_matrixA;
    outMatrix[1] = mvhd->qt_matrixB;
    outMatrix[2] = mvhd->qt_matrixU;
    outMatrix[3] = mvhd->qt_matrixC;
    outMatrix[4] = mvhd->qt_matrixD;
    outMatrix[5] = mvhd->qt_matrixV;
    outMatrix[6] = mvhd->qt_matrixX;
    outMatrix[7] = mvhd->qt_matrixY;
    outMatrix[8] = mvhd->qt_matrixW; */
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err getPreferredRate(struct MP4MovieAtom* self, u32* outRate) {
    MP4MovieHeaderAtomPtr mvhd;
    MP4Err err;
    err = MP4NoErr;
    mvhd = (MP4MovieHeaderAtomPtr)self->mvhd;
    if (mvhd == NULL)
        BAILWITHERROR(MP4InvalidMediaErr)
    if (outRate == NULL)
        BAILWITHERROR(MP4BadParamErr);
    *outRate = mvhd->qt_preferredRate;
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err getPreferredVolume(struct MP4MovieAtom* self, s16* outVolume) {
    MP4MovieHeaderAtomPtr mvhd;
    MP4Err err;
    err = MP4NoErr;
    mvhd = (MP4MovieHeaderAtomPtr)self->mvhd;
    if (mvhd == NULL)
        BAILWITHERROR(MP4InvalidMediaErr)
    if (outVolume == NULL)
        BAILWITHERROR(MP4BadParamErr);
    *outVolume = (s16)mvhd->qt_preferredVolume;
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err addTrack(struct MP4MovieAtom* self, MP4AtomPtr track) {
    MP4Err err;
    err = MP4NoErr;
    err = addAtom(self, track);

    TEST_RETURN(err);

    return err;
}

static MP4Err addAtom(MP4MovieAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    err = MP4NoErr;
    assert(atom);
    err = MP4AddListEntry(atom, self->atomList);
    if (err)
        goto bail;
    switch (atom->type) {
        case MP4ObjectDescriptorAtomType:
            if (self->iods)
                BAILWITHERROR(MP4BadDataErr)
            self->iods = atom;
            break;

        case MP4MovieHeaderAtomType:
            if (self->mvhd)
                BAILWITHERROR(MP4BadDataErr)
            self->mvhd = atom;
            break;

        case MP4UserDataAtomType:
            if (self->udta) {
                err = MP4AddListEntry(atom, self->udtaList);
                if (err)
                    goto bail;
                // BAILWITHERROR( MP4BadDataErr )
            } else
                self->udta = atom;
            break;

        case QTMetadataAtomType:
            if (self->meta)
                BAILWITHERROR(MP4BadDataErr)
            self->meta = atom;
            break;

        case MP4TrackAtomType:
            err = MP4AddListEntry(atom, self->trackList);
            if (err)
                goto bail;
            break;
        case MP4MovieExtendsAtomType:
            if (self->mvex)
                BAILWITHERROR(MP4BadDataErr)
            self->mvex = atom;
            break;
        case MP4ProtectionSystemSpecificHeaderAtomType:
            if (self->pssh) {
                MP4PSSHAtomPtr pssh = (MP4PSSHAtomPtr)self->pssh;
                err = pssh->appendPssh(pssh, (MP4PSSHAtomPtr)atom);
                if (err)
                    goto bail;
            } else
                self->pssh = atom;
            break;
        default:
            break;
    }
bail:
    TEST_RETURN(err);

    return err;
}

static u32 getTrackCount(MP4MovieAtomPtr self) {
    u32 trackCount = 0;
    (void)MP4GetListEntryCount(self->trackList, &trackCount);
    return trackCount;
}

#if 1

MP4Err MP4GetTrack(MP4Movie theMoov, u32 TrackID, MP4AtomPtr* outReferencedTrack) {
    GETMOVIEATOM(theMoov);
    err = movieAtom->getIndTrack(movieAtom, TrackID, outReferencedTrack);
bail:
    return err;
}

MP4Err SetupReferences(MP4MovieAtomPtr self) {
    u32 i = 0;
    MP4Err err = MP4NoErr;
    u32 trackCount = 0;
    MP4Movie moov = NULL;

    trackCount = getTrackCount(self);

    for (i = 0; i < trackCount; i++) {
        MP4TrackAtomPtr trak = NULL;
        MP4TrackAtomPtr outReferencedTrack = NULL;
        MP4TrackReferenceAtomPtr tref = NULL;
        MP4TrackReferenceTypeAtomPtr dpnd = NULL;

        u32 selectedTrackID = 0;
        err = MP4GetListEntry(self->trackList, i, (char**)&trak);
        if (err) {
            return err;
        }
        err = MP4GetTrackMovie(trak, &moov);
        if (err || NULL == moov) {
            MP4MSG("Get moov failed!\n");
            return err;
        }
        trak->trak_indx = i;
        tref = (MP4TrackReferenceAtomPtr)trak->trackReferences;
        if (NULL != tref) {
            err = tref->findAtomOfType(tref, MP4StreamDependenceAtomType, (MP4AtomPtr*)&dpnd);
            if (err) {
                return err;
            }

            if (NULL != dpnd) {
                selectedTrackID = dpnd->trackIDs[0];
            }
            if (selectedTrackID == 0) {
                return MP4InvalidMediaErr;
            }
            err = MP4GetTrack(moov, selectedTrackID, (MP4AtomPtr*)&outReferencedTrack);

            if (err) {
                return err;
            }
            outReferencedTrack->trak_reference = (MP4AtomPtr)trak;
            {
#ifdef DEBUG
                u32 track1 = 0;
                u32 track2 = 0;
                MP4GetTrackID((MP4Track)outReferencedTrack, &track1);
                MP4GetTrackID((MP4Track)trak, &track2);
                MP4MSG("track%d referenced by track%d\n", track1, track2);
#endif
            }
        }
    }
    return MP4NoErr;
}
#endif

static MP4Err setupTracks(MP4MovieAtomPtr self, MP4PrivateMovieRecordPtr moov) {
    u32 i = 0;
    MP4Err err = MP4NoErr;
    u32 trackCount = 0;

    self->moov = moov;
    trackCount = getTrackCount(self);
    // MP4MSG("trackCount = %d", trackCount);
    for (i = 0; i < trackCount; i++) {
        MP4TrackAtomPtr atom;
        err = MP4GetListEntry(self->trackList, i, (char**)&atom);
        if (err)
            goto bail;
        atom->moov = self->moov;
    }
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err getIndTrack(struct MP4MovieAtom* self, u32 trackNumber, MP4AtomPtr* outTrack) {
    MP4Err err;

    err = MP4NoErr;
    if ((trackNumber == 0) || (outTrack == NULL) || (trackNumber > getTrackCount(self)))
        BAILWITHERROR(MP4BadParamErr)
    err = MP4GetListEntry(self->trackList, trackNumber - 1, (char**)outTrack);
    if (err)
        goto bail;
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err calculateDuration(struct MP4MovieAtom* self) {
    MP4MovieHeaderAtomPtr mvhd;
    long double maxDuration;
    u32 timeScale;
    u32 trackCount;
    u32 i;
    MP4Err err;

    maxDuration = 0;
    err = MP4NoErr;
    mvhd = (MP4MovieHeaderAtomPtr)self->mvhd;
    if (mvhd == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);
    err = getTimeScale(self, &timeScale);
    if (err)
        goto bail;
    trackCount = getTrackCount(self);

    for (i = 0; i < trackCount; i++) {
        MP4TrackAtomPtr trak;
        long double trackDuration;

        err = MP4GetListEntry(self->trackList, i, (char**)&trak);
        if (err)
            goto bail;
        if (trak == NULL)
            BAILWITHERROR(MP4InvalidMediaErr);
        err = trak->calculateDuration(trak, timeScale);
        if (err)
            goto bail;
        err = trak->getDuration(trak, &trackDuration);
        if (err)
            goto bail;
        if (trackDuration > maxDuration)
            maxDuration = trackDuration;
    }
    mvhd->duration = (s64)maxDuration;
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
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

    PARSE_ATOM_LIST(MP4MovieAtom)

bail:
    TEST_RETURN(err);

#ifdef FSL_MP4_PARSER_DBG_TIME
#if defined(__WINCE) || defined(WIN32)
    end_time = timeGetTime();
    MP4DbgLog("\ntime for moov: %ld ms\n\n", end_time - start_time);
    MP4DbgLog("start %ld ms, end %ld ms\n\n", start_time, end_time);
#else
    gettimeofday(&end_time, &time_zone);
    if (end_time.tv_usec >= start_time.tv_usec) {
        MP4DbgLog("moov: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec,
                  end_time.tv_usec - start_time.tv_usec);
    } else {
        MP4DbgLog("moov: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec - 1,
                  end_time.tv_usec + 1000000000 - start_time.tv_usec);
    }

    MP4DbgLog("start %ld sec, %ld us\n\n", start_time.tv_sec, start_time.tv_usec);
#endif
#endif

    return err;
}

MP4Err MP4CreateMovieAtom(MP4MovieAtomPtr* outAtom) {
    MP4Err err;
    MP4MovieAtomPtr self;

    self = (MP4MovieAtomPtr)MP4LocalCalloc(1, sizeof(MP4MovieAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4MovieAtomType;
    self->name = "movie";
    self->meta = NULL;
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->setupTracks = setupTracks;
    self->SetupReferences = SetupReferences;
    self->addTrack = addTrack;
    self->addAtom = addAtom;
    self->calculateDuration = calculateDuration;
    self->getNextTrackID = getNextTrackID;

    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;
    err = MP4MakeLinkedList(&self->trackList);
    if (err)
        goto bail;
    self->getTrackCount = getTrackCount;
    self->getIndTrack = getIndTrack;
    self->getTimeScale = getTimeScale;
    self->getMatrix = getMatrix;
    self->getPreferredRate = getPreferredRate;
    self->getPreferredVolume = getPreferredVolume;
    self->pssh = NULL;
    err = MP4MakeLinkedList(&self->udtaList);
    if (err)
        goto bail;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
