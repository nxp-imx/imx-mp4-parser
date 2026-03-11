/************************************************************************
 * Copyright (c) 2014, Freescale Semiconductor, Inc.
 * Copyright 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    u32 i;
    MP4MovieFragmentRandomAccessAtomPtr self;
    self = (MP4MovieFragmentRandomAccessAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    DESTROY_ATOM_LIST

    err = MP4DeleteLinkedList(self->tfraList);
    if (err)
        goto bail;

    if (self->super)
        self->super->destroy(s);

bail:
    TEST_RETURN(err);

    return;
}

static MP4Err addAtom(MP4MovieFragmentRandomAccessAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    err = MP4NoErr;
    assert(atom);
    err = MP4AddListEntry(atom, self->atomList);
    if (err)
        goto bail;

    switch (atom->type) {
        case MP4TrackFragmentRandomAccessAtomType:
            err = MP4AddListEntry(atom, self->tfraList);
            if (err)
                goto bail;
            break;
        case MP4MovieFragmentRandomAccessOffsetAtomType:
            self->mfro = atom;
            break;
    }
bail:
    TEST_RETURN(err);

    return err;
}

static u32 getTrackCount(MP4MovieFragmentRandomAccessAtomPtr self) {
    u32 trackCount = 0;
    MP4Err err = MP4NoErr;
    err = MP4GetListEntryCount(self->tfraList, &trackCount);
    if (err)
        trackCount = 0;
    return trackCount;
}

static MP4Err getTrack(MP4MovieFragmentRandomAccessAtomPtr self, u32 TrackID,
                       MP4AtomPtr* outTrack) {
    MP4Err err;

    err = MP4NoErr;
    if ((TrackID == 0) || (self == NULL) || (TrackID > getTrackCount(self)))
        BAILWITHERROR(MP4BadParamErr)
    err = MP4GetListEntry(self->tfraList, TrackID - 1, (char**)outTrack);
    if (err)
        goto bail;
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    PARSE_ATOM_LIST(MP4MovieFragmentRandomAccessAtom)

bail:

    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateMovieFragmentRandomAccessAtom(MP4MovieFragmentRandomAccessAtomPtr* outAtom) {
    MP4Err err;
    MP4MovieFragmentRandomAccessAtomPtr self;

    self = (MP4MovieFragmentRandomAccessAtomPtr)MP4LocalCalloc(
            1, sizeof(MP4MovieFragmentRandomAccessAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4MovieFragmentRandomAccessAtomType;
    self->name = "movie fragment random access";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;

    self->addAtom = addAtom;

    self->getTrackCount = getTrackCount;
    self->getTrack = getTrack;
    self->mfro = NULL;
    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;
    err = MP4MakeLinkedList(&self->tfraList);
    if (err)
        goto bail;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
