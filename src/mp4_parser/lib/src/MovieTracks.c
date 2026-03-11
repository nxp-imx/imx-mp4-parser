/*
 ***********************************************************************
 * Copyright (c) 2005-2013, Freescale Semiconductor, Inc.
 *
 * Copyright 2017-2018, 2024, 2026 NXP
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
    $Id: MovieTracks.c,v 1.28 2000/01/11 22:59:55 mc Exp $
*/

#include "MovieTracks.h"
#include <stdio.h>
#include <string.h>
#include "MP4Atoms.h"
#include "MP4Impl.h"
#include "MP4Movies.h"

MP4Err MP4GetTrackID(MP4Track theTrack, u32* outTrackID) {
    MP4Err err;
    MP4TrackAtomPtr trackAtom;
    MP4TrackHeaderAtomPtr trackHeaderAtom;
    err = MP4NoErr;
    if (theTrack == NULL)
        BAILWITHERROR(MP4BadParamErr)
    trackAtom = (MP4TrackAtomPtr)theTrack;
    trackHeaderAtom = (MP4TrackHeaderAtomPtr)trackAtom->trackHeader;
    *outTrackID = trackHeaderAtom->trackID;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetTrackEnabled(MP4Track theTrack, u32* outEnabled) {
    MP4Err err;
    MP4TrackAtomPtr trackAtom;
    err = MP4NoErr;
    if (theTrack == NULL)
        BAILWITHERROR(MP4BadParamErr)
    trackAtom = (MP4TrackAtomPtr)theTrack;
    err = trackAtom->getEnabled(trackAtom, outEnabled);
    if (err)
        goto bail;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetTrackDuration(MP4Track theTrack, long double* outDuration) {
    MP4Err err;
    MP4TrackAtomPtr trak;
    MP4Movie moov;
    MP4Media mdia;
    u32 ts;
    u64 duration;
    long double trakDuration;

    if ((theTrack == 0) || (outDuration == 0)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    err = MP4GetTrackMovie(theTrack, &moov);
    if (err)
        goto bail;
    err = MP4GetTrackMedia(theTrack, &mdia);
    if (err)
        goto bail;
    err = MP4GetMovieTimeScale(moov, &ts);
    if (err)
        goto bail;
    err = MP4GetMediaDuration(mdia, &duration);
    if (err)
        goto bail; /* get media duration from stts atom */
    trak = (MP4TrackAtomPtr)theTrack;
    /* choose a better one from media duration and edit list duraiton */
    err = trak->calculateDuration(trak, ts);
    if (err)
        goto bail;
    err = trak->getDuration(trak, &trakDuration);
    if (err)
        goto bail;
    *outDuration = trakDuration;

bail:
    TEST_RETURN(err);
    return err;
}

ISOErr MJ2GetTrackMatrix(ISOTrack theTrack, u32 outMatrix[9]) {
    MP4Err err;
    MP4TrackAtomPtr trak;

    if (theTrack == 0) {
        BAILWITHERROR(MP4BadParamErr);
    }

    trak = (MP4TrackAtomPtr)theTrack;

    if (outMatrix == NULL)
        BAILWITHERROR(MP4BadParamErr);

    err = trak->getMatrix(trak, outMatrix);
bail:
    return err;
}

ISOErr MJ2GetTrackLayer(ISOTrack theTrack, s16* outLayer) {
    MP4Err err;
    MP4TrackAtomPtr trak;

    if (theTrack == 0) {
        BAILWITHERROR(MP4BadParamErr);
    }

    trak = (MP4TrackAtomPtr)theTrack;

    if (outLayer == NULL)
        BAILWITHERROR(MP4BadParamErr);
    err = trak->getLayer(trak, outLayer);
bail:
    return err;
}

ISOErr MJ2GetTrackDimensions(ISOTrack theTrack, u32* outWidth, u32* outHeight) {
    MP4Err err;
    MP4TrackAtomPtr trak;

    if (theTrack == 0) {
        BAILWITHERROR(MP4BadParamErr);
    }

    trak = (MP4TrackAtomPtr)theTrack;

    if ((outWidth == NULL) || (outHeight == NULL))
        BAILWITHERROR(MP4BadParamErr);
    err = trak->getDimensions(trak, outWidth, outHeight);
bail:
    return err;
}

ISOErr MJ2GetTrackVolume(ISOTrack theTrack, s16* outVolume) {
    MP4Err err;
    MP4TrackAtomPtr trak;

    if (theTrack == 0) {
        BAILWITHERROR(MP4BadParamErr);
    }

    trak = (MP4TrackAtomPtr)theTrack;

    if (outVolume == NULL)
        BAILWITHERROR(MP4BadParamErr);
    err = trak->getVolume(trak, outVolume);
bail:
    return err;
}

MP4Err MP4GetTrackReference(MP4Track theTrack, u32 referenceType, u32 referenceIndex,
                            MP4Track* outReferencedTrack) {
    MP4Err err;
    MP4TrackAtomPtr trak;
    MP4Movie moov;
    MP4TrackReferenceAtomPtr tref;
    MP4TrackReferenceTypeAtomPtr dpnd;
    u32 selectedTrackID;

    err = MP4NoErr;
    if ((theTrack == NULL) || (referenceType == 0) || (referenceIndex == 0) ||
        (outReferencedTrack == NULL))
        BAILWITHERROR(MP4BadParamErr);
    err = MP4GetTrackMovie(theTrack, &moov);
    if (err)
        goto bail;
    trak = (MP4TrackAtomPtr)theTrack;
    tref = (MP4TrackReferenceAtomPtr)trak->trackReferences;
    if (tref == NULL)
        BAILWITHERROR(MP4BadParamErr);
    err = tref->findAtomOfType(tref, referenceType, (MP4AtomPtr*)&dpnd);
    if (err)
        goto bail;
    if ((dpnd == NULL) || (dpnd->trackIDCount < referenceIndex))
        BAILWITHERROR(MP4BadParamErr);
    selectedTrackID = dpnd->trackIDs[referenceIndex - 1];
    if (selectedTrackID == 0)
        BAILWITHERROR(MP4InvalidMediaErr);
    err = MP4GetMovieTrack(moov, selectedTrackID, outReferencedTrack);
    if (err)
        goto bail;
bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4GetTrackReferenceCount(MP4Track theTrack, u32 referenceType, u32* outReferenceCount) {
    MP4Err err;
    MP4TrackAtomPtr trak;
    MP4TrackReferenceAtomPtr tref;
    MP4TrackReferenceTypeAtomPtr dpnd;

    err = MP4NoErr;
    if ((theTrack == NULL) || (referenceType == 0) || (outReferenceCount == NULL))
        BAILWITHERROR(MP4BadParamErr);
    trak = (MP4TrackAtomPtr)theTrack;
    tref = (MP4TrackReferenceAtomPtr)trak->trackReferences;
    *outReferenceCount = 0;
    if (tref != NULL) {
        err = tref->findAtomOfType(tref, referenceType, (MP4AtomPtr*)&dpnd);
        if ((err == MP4NoErr) && (dpnd != NULL))
            *outReferenceCount = dpnd->trackIDCount;
        else
            err = MP4NoErr;
    }
bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4GetTrackMovie(MP4Track theTrack, MP4Movie* outMovie) {
    MP4Err err;
    MP4TrackAtomPtr trackAtom;
    trackAtom = (MP4TrackAtomPtr)theTrack;
    err = MP4NoErr;
    if (theTrack == NULL)
        BAILWITHERROR(MP4BadParamErr)
    *outMovie = (MP4Movie)trackAtom->moov;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetTrackMedia(MP4Track theTrack, MP4Media* outMedia) {
    MP4Err err;
    MP4TrackAtomPtr trackAtom;
    trackAtom = (MP4TrackAtomPtr)theTrack;
    err = MP4NoErr;
    if (theTrack == NULL) {
        BAILWITHERROR(MP4BadParamErr);
    }
    if (trackAtom->trackMedia) {
        *outMedia = (MP4Media)trackAtom->trackMedia;
    } else {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetTrackRotationDegree(MP4Track theTrack, u32* rotationDegrees) {
    MP4Err err;
    MP4TrackAtomPtr trak;
    s32 outMatrix[9];
    static const s32 kFixedOne = 0x10000;

    if (theTrack == NULL) {
        BAILWITHERROR(MP4BadParamErr);
    }

    trak = (MP4TrackAtomPtr)theTrack;

    err = trak->getMatrix(trak, (u32*)outMatrix);

    if (err != 0) {
        goto bail;
    }

    if (outMatrix[0] == 0 && outMatrix[1] == kFixedOne && outMatrix[3] == -kFixedOne &&
        outMatrix[4] == 0) {
        *rotationDegrees = 90;
    } else if (outMatrix[0] == 0 && outMatrix[1] == -kFixedOne && outMatrix[3] == kFixedOne &&
               outMatrix[4] == 0) {
        *rotationDegrees = 270;
    } else if (outMatrix[0] == -kFixedOne && outMatrix[1] == 0 && outMatrix[3] == 0 &&
               outMatrix[4] == -kFixedOne) {
        *rotationDegrees = 180;
    } else {
        *rotationDegrees = 0;
    }

bail:
    return err;
}

MP4Err MP4GetTrackUserData(MP4Track theTrack, MP4UserData* outUserData) {
    MP4Err err;
    MP4UserData udta;
    MP4TrackAtomPtr trackAtom;
    trackAtom = (MP4TrackAtomPtr)theTrack;
    err = MP4NoErr;
    if (theTrack == NULL)
        BAILWITHERROR(MP4BadParamErr)
    udta = (MP4UserData)trackAtom->udta;
    if (trackAtom->udta == 0) {
        err = MP4NewUserData(&udta);
        if (err)
            goto bail;
        err = trackAtom->addAtom(trackAtom, (MP4AtomPtr)udta);
        if (err)
            goto bail;
    }
    *outUserData = (MP4UserData)udta;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetTrackOffset(MP4Track track, u32* outMovieOffsetTime) {
    MP4Err err;
    MP4TrackAtomPtr trak;
    MP4EditAtomPtr edts;
    MP4EditListAtomPtr elst;

    err = MP4NoErr;
    if ((track == 0) || (outMovieOffsetTime == 0))
        BAILWITHERROR(MP4BadParamErr);

    /* see if we have an edit list */
    trak = (MP4TrackAtomPtr)track;
    edts = (MP4EditAtomPtr)trak->trackEditAtom;

    if (edts == 0) {
        *outMovieOffsetTime = 0;
    } else {
        elst = (MP4EditListAtomPtr)edts->editListAtom;
        if (elst == 0) {
            *outMovieOffsetTime = 0;
        } else {
            err = elst->getTrackOffset(elst, outMovieOffsetTime);
            if (err)
                goto bail;
        }
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetTrackReferenceType(MP4Track track, u32 atomType,
                                MP4TrackReferenceTypeAtomPtr* outAtom) {
    MP4Err err;
    MP4TrackReferenceAtomPtr tref;
    MP4TrackReferenceTypeAtomPtr foundAtom;
    MP4TrackAtomPtr trak;

    err = MP4NoErr;
    trak = (MP4TrackAtomPtr)track;

    if ((track == 0) || (outAtom == 0))
        BAILWITHERROR(MP4BadParamErr)
    foundAtom = 0;
    tref = (MP4TrackReferenceAtomPtr)trak->trackReferences;
    if (tref) {
        err = tref->findAtomOfType(tref, atomType, (MP4AtomPtr*)&foundAtom);
        if (err)
            goto bail;
    }
    *outAtom = foundAtom;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4TrackTimeToMediaTime(MP4Track theTrack, u64 inTrackTime, s64* outMediaTime) {
    MP4Err err;
    MP4Movie theMovie;
    MP4Media theMedia;
    u32 movieTimeScale;
    u32 mediaTimeScale;
    s64 mediaTime;
    MP4TrackAtomPtr trak;
    MP4EditAtomPtr edts;
    MP4EditListAtomPtr elst;

    err = MP4NoErr;
    if ((theTrack == 0) || (outMediaTime == 0))
        BAILWITHERROR(MP4BadParamErr)

    err = MP4GetTrackMovie(theTrack, &theMovie);
    if (err)
        goto bail;
    err = MP4GetMovieTimeScale(theMovie, &movieTimeScale);
    if (err)
        goto bail;
    err = MP4GetTrackMedia(theTrack, &theMedia);
    if (err)
        goto bail;
    err = MP4GetMediaTimeScale(theMedia, &mediaTimeScale);
    if (err)
        goto bail;

    if (movieTimeScale == 0)
        BAILWITHERROR(MP4InvalidMediaErr)

    trak = (MP4TrackAtomPtr)theTrack;
    edts = (MP4EditAtomPtr)trak->trackEditAtom;
    if (edts == 0) {
        /* no edits */
        mediaTime = (inTrackTime / movieTimeScale) * mediaTimeScale;
        *outMediaTime = mediaTime;
    } else {
        /* edit atom is present */
        elst = (MP4EditListAtomPtr)edts->editListAtom;
        if (elst == 0) {
            /* edit atom but no edit list, hmm... */
            mediaTime = (inTrackTime / movieTimeScale) * mediaTimeScale;
            *outMediaTime = mediaTime;
        } else {
            /* edit list is present */
            u32 mediaRate;
            u64 prior;
            u64 next;
            err = elst->getTimeAndRate((MP4AtomPtr)elst, inTrackTime, movieTimeScale,
                                       mediaTimeScale, &mediaTime, &mediaRate, &prior, &next);
            if (err)
                goto bail;
            *outMediaTime = mediaTime;
        }
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetTrackEditListInfo(MP4Track theTrack, u64* segment_duration, s64* media_time) {
    MP4Err err = MP4NoErr;
    MP4TrackAtomPtr trak;
    MP4EditAtomPtr edts;
    MP4EditListAtomPtr elst;
    u64 currentMovieTime = 0;
    u32 isEmpty = 0;

    if (theTrack == NULL || segment_duration == NULL || media_time == NULL)
        BAILWITHERROR(MP4BadParamErr);

    trak = (MP4TrackAtomPtr)theTrack;

    edts = (MP4EditAtomPtr)trak->trackEditAtom;

    if (edts == NULL)
        BAILWITHERROR(MP4BadParamErr);

    elst = (MP4EditListAtomPtr)edts->editListAtom;
    if (elst == NULL)
        BAILWITHERROR(MP4BadParamErr);

    err = elst->getIndSegmentTime((MP4AtomPtr)elst, 1, &currentMovieTime, media_time,
                                  segment_duration);
    if (err)
        goto bail;

    // get elst ticks

    err = elst->isEmptyEdit(elst, 1, &isEmpty);
    if (err)
        goto bail;
    if (isEmpty) {
        u64 segment_duration2;
        s64 media_time2;
        trak->elstInitialEmptyEditTicks = *segment_duration;
        err = elst->getIndSegmentTime((MP4AtomPtr)elst, 2, &currentMovieTime, &media_time2,
                                      &segment_duration2);
        if (err)
            goto bail;
        trak->elstShiftStartTicks = media_time2 > 0 ? media_time2 : 0;
    } else {
        trak->elstShiftStartTicks = *media_time > 0 ? *media_time : 0;
    }

    if (trak->elstInitialEmptyEditTicks > 0) {
        MP4Movie movie;
        MP4Media theMedia;
        u32 movieTimeScale;
        u32 mediaTimeScale;
        err = MP4GetTrackMovie(theTrack, &movie);
        if (err)
            goto bail;
        err = MP4GetMovieTimeScale(movie, &movieTimeScale);
        if (err)
            goto bail;
        err = MP4GetTrackMedia(theTrack, &theMedia);
        if (err)
            goto bail;
        err = MP4GetMediaTimeScale(theMedia, &mediaTimeScale);
        if (err)
            goto bail;
        if (movieTimeScale > 0) {
            trak->elstInitialEmptyEditTicks *= mediaTimeScale;
            trak->elstInitialEmptyEditTicks += movieTimeScale / 2;
            trak->elstInitialEmptyEditTicks /= movieTimeScale;
        }
    }
    MP4MSG("elstInitialEmptyEditTicks %lld, elstShiftStartTicks %lld",
           trak->elstInitialEmptyEditTicks, trak->elstShiftStartTicks);

bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4GetTrackDimensions(MP4Track theTrack, u32* outWidth, u32* outHeight) {
    return MJ2GetTrackDimensions(theTrack, outWidth, outHeight);
}
