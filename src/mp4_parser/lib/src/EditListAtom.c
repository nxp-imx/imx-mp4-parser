/*
 ***********************************************************************
 * Copyright 2005-2006,2016 by Freescale Semiconductor, Inc.
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
    $Id: EditListAtom.c,v 1.2 2000/05/17 08:01:27 francesc Exp $
*/

#include <stdlib.h>
#include "MP4Atoms.h"

#define USE_OLD_FILES_WORKAROUND 1

typedef struct {
    u64 segmentDuration;
    s64 mediaTime;
    u32 mediaRate;
    u32 emptyEdit;
} edtsEntry, *edtsEntryPtr;

static u32 getEntryCount(struct MP4EditListAtom* self) {
    u32 count = 0;
    MP4Err err = MP4NoErr;
    if (self == NULL)
        return 0;

    err = MP4GetListEntryCount(self->entryList, &count);
    if (err != MP4NoErr)
        count = 0;

    return count;
}

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4EditListAtomPtr self;
    u32 entryCount;
    u32 i;

    err = MP4NoErr;
    self = (MP4EditListAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    entryCount = getEntryCount(self);
    for (i = 0; i < entryCount; i++) {
        char* p;
        err = MP4GetListEntry(self->entryList, i, &p);
        if (err)
            goto bail;
        if (p)
            MP4LocalFree(p);
    }
    if (self->entryList != NULL) {
        err = MP4DeleteLinkedList(self->entryList);
        self->entryList = NULL;
    }
    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err getTrackOffset(struct MP4EditListAtom* self, u32* outTrackStartTime) {
    MP4Err err = MP4NoErr;
    u32 entryCount;

    err = MP4GetListEntryCount(self->entryList, &entryCount);
    if (entryCount == 0) {
        outTrackStartTime = 0;
    } else {
        edtsEntryPtr firstSegment;

        err = MP4GetListEntry(self->entryList, 0, (char**)&firstSegment);
        if (err)
            goto bail;
        if (firstSegment->emptyEdit) {
            /* existing track offset  */
            *outTrackStartTime = (u32)firstSegment->segmentDuration;
        } else {
            outTrackStartTime = 0;
        }
    }
bail:
    return err;
}

static MP4Err getEffectiveDuration(struct MP4EditListAtom* self, u64* outDuration) {
    u64 duration;
    u32 entryCount;
    u32 i;
    MP4Err err;

    err = MP4NoErr;
    entryCount = getEntryCount(self);
    duration = 0;
    for (i = 0; i < entryCount; i++) {
        edtsEntryPtr p;
        err = MP4GetListEntry(self->entryList, i, (char**)&p);
        if (err)
            goto bail;
        duration += p->segmentDuration;
    }
    *outDuration = duration;
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err isEmptyEdit(struct MP4EditListAtom* self, u32 segmentNumber, u32* outIsEmpty) {
    MP4Err err;
    u32 entryCount;
    edtsEntryPtr p;

    err = MP4NoErr;
    err = MP4GetListEntryCount(self->entryList, &entryCount);
    if (err)
        goto bail;
    if ((segmentNumber == 0) || (segmentNumber > entryCount) || (outIsEmpty == NULL))
        BAILWITHERROR(MP4BadParamErr);
    err = MP4GetListEntry(self->entryList, segmentNumber - 1, (char**)&p);
    if (err)
        goto bail;
    *outIsEmpty = p->emptyEdit;
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err getIndSegmentTime(MP4AtomPtr s, u32 segmentIndex, /* one based */
                                u64* outSegmentMovieTime, s64* outSegmentMediaTime,
                                u64* outSegmentDuration /* in movie's time scale */
) {
    MP4Err err;
    u32 i;
    u32 entryCount;
    u64 currentMovieTime;
    MP4EditListAtomPtr self = (MP4EditListAtomPtr)s;

    err = MP4NoErr;

    if ((self == NULL) || (segmentIndex == 0))
        BAILWITHERROR(MP4BadParamErr)

    entryCount = getEntryCount(self);

    if (segmentIndex > entryCount)
        BAILWITHERROR(MP4BadParamErr)

    currentMovieTime = 0;
    for (i = 0; i < entryCount; i++) {
        edtsEntryPtr p;
        err = MP4GetListEntry(self->entryList, i, (char**)&p);
        if (err)
            goto bail;
        if (i == (segmentIndex - 1)) {
            if (outSegmentMovieTime)
                *outSegmentMovieTime = currentMovieTime;
            if (outSegmentMediaTime) {
                if (p->emptyEdit)
                    *outSegmentMediaTime = -1;
                else
                    *outSegmentMediaTime = p->mediaTime;
            }
            if (outSegmentDuration)
                *outSegmentDuration = p->segmentDuration;
        } else {
            currentMovieTime += p->segmentDuration;
        }
    }
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err getTimeAndRate(MP4AtomPtr s, u64 movieTime, u32 movieTimeScale, u32 mediaTimeScale,
                             s64* outMediaTime, u32* outMediaRate, u64* outPriorSegmentEndTime,
                             u64* outNextSegmentBeginTime) {
    MP4Err err;

    u32 i;
    u32 entryCount;
    u64 currentMovieTime;
    u64 priorSegmentEndTime;
    u64 nextSegmentBeginTime;
    MP4EditListAtomPtr self = (MP4EditListAtomPtr)s;

    err = MP4NoErr;
    if ((self == NULL) || (movieTimeScale == 0) || (mediaTimeScale == 0))
        BAILWITHERROR(MP4BadParamErr)
    currentMovieTime = 0;
    *outMediaTime = -1;
    *outMediaRate = 1 << 16;
    priorSegmentEndTime = 0;
    nextSegmentBeginTime = 0;

    /*
        We rely on there being no consecutive empty edits, and
        this is not tested !!!
    */
    entryCount = getEntryCount(self);
    for (i = 0; i < entryCount; i++) {
        edtsEntryPtr p;
        u64 secondsOffset;
        err = MP4GetListEntry(self->entryList, i, (char**)&p);
        if (err)
            goto bail;
        if ((currentMovieTime + p->segmentDuration) >= movieTime) {
            /* found the correct segment */
            if (p->emptyEdit) {
                /* an empty edit */
                *outMediaTime = -1;
                *outMediaRate = p->mediaRate;
                if (i < entryCount)
                    nextSegmentBeginTime = (p + 1)->mediaTime;
                break;
            } else {
                /* normal edit */
                secondsOffset = (movieTime - currentMovieTime) / movieTimeScale;
                *outMediaTime = p->mediaTime + (secondsOffset * mediaTimeScale);
                *outMediaRate = p->mediaRate;
            }
        } else {
            /* advance to next segment */
            secondsOffset = p->segmentDuration / movieTimeScale;
            priorSegmentEndTime = secondsOffset * mediaTimeScale;
            currentMovieTime += p->segmentDuration;
        }
    }
    if (outPriorSegmentEndTime)
        *outPriorSegmentEndTime = priorSegmentEndTime;
    if (outNextSegmentBeginTime)
        *outNextSegmentBeginTime = nextSegmentBeginTime;
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    u32 entries;
    u32 entryCount;
    MP4EditListAtomPtr self = (MP4EditListAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32_V(entryCount);
    for (entries = 0; entries < entryCount; entries++) {
        u64 segmentDuration;
        s64 mediaTime;
        u32 mediaRate;
        edtsEntryPtr p;
        p = (edtsEntryPtr)MP4LocalCalloc(1, sizeof(edtsEntry));
        TESTMALLOC(p);
        if (self->version == 1) {
            GET64_V(segmentDuration);
            GET64_V(mediaTime);
            if (mediaTime < 0)
                p->emptyEdit = 1;
        } else {
            u32 val;
            s32 sval;
            GET32_V_MSG(val, "segment-duration");
            segmentDuration = val;
            GET32_V_MSG(sval, "media-time");
            mediaTime = sval;
            if (sval < 0)
                p->emptyEdit = 1;
        }
        GET32_V(mediaRate);
        /*
            Earlier versions of this code
            forgot that mediaRate was fixed32!
        */
#ifdef USE_OLD_FILES_WORKAROUND
        if (mediaRate != 1)
#endif
        {
            mediaRate >>= 16;
        }
        p->segmentDuration = segmentDuration;
        p->mediaTime = mediaTime;
        p->mediaRate = mediaRate;
        err = MP4AddListEntry(p, self->entryList);
        if (err)
            goto bail;
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateEditListAtom(MP4EditListAtomPtr* outAtom) {
    MP4Err err;
    MP4EditListAtomPtr self;

    self = (MP4EditListAtomPtr)MP4LocalCalloc(1, sizeof(MP4EditListAtom));
    TESTMALLOC(self);

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    err = MP4MakeLinkedList(&self->entryList);
    if (err)
        goto bail;
    self->type = MP4EditListAtomType;
    self->name = "edit list";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->getTimeAndRate = getTimeAndRate;
    self->getIndSegmentTime = getIndSegmentTime;
    self->getEffectiveDuration = getEffectiveDuration;
    self->getEntryCount = getEntryCount;
    self->getTrackOffset = getTrackOffset;
    self->isEmptyEdit = isEmptyEdit;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
