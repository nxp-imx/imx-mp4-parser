/************************************************************************
 * Copyright (c) 2014, Freescale Semiconductor, Inc.
 *
 * Copyright 2017-2018, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4MovieExtendsAtomPtr self;
    u32 i;

    err = MP4NoErr;
    self = (MP4MovieExtendsAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    DESTROY_ATOM_LIST
    err = MP4DeleteLinkedList(self->trexList);
    if (err)
        goto bail;

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err addAtom(MP4MovieExtendsAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    err = MP4NoErr;
    assert(atom);

    err = MP4AddListEntry(atom, self->atomList);
    if (err)
        goto bail;
    MP4MSG("MovieExtendsAtom add 1\n");

    switch (atom->type) {
        case MP4MovieExtendsHeaderAtomType:
            if (self->mehd)
                BAILWITHERROR(MP4BadDataErr)
            self->mehd = atom;
            break;
        case MP4TrackExtendsAtomType:
            err = MP4AddListEntry(atom, self->trexList);
            if (err)
                goto bail;
            break;
        default:
            break;
    }
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    PARSE_ATOM_LIST(MP4MovieExtendsAtom)

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err getTrex(struct MP4MovieExtendsAtom* self, u32 track_ID, MP4TrackExtendsAtomPtr* outTrack) {
    MP4Err err = MP4NoErr;
    u32 trexCount = 0;
    u32 i = 0;

    if (self == NULL || outTrack == NULL)
        BAILWITHERROR(MP4BadDataErr)

    err = MP4GetListEntryCount(self->trexList, &trexCount);
    if (err)
        goto bail;

    if (trexCount < 1)
        BAILWITHERROR(MP4BadDataErr)

    for (i = 0; i < trexCount; i++) {
        MP4TrackExtendsAtomPtr trex = NULL;
        err = MP4GetListEntry(self->trexList, i, (char**)outTrack);
        if (err)
            goto bail;
        trex = *outTrack;
        if (trex && track_ID == trex->track_ID) {
            break;
        }
    }

bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4CreateMovieExtendsAtom(MP4MovieExtendsAtomPtr* outAtom) {
    MP4Err err;
    MP4MovieExtendsAtomPtr self;

    self = (MP4MovieExtendsAtomPtr)MP4LocalCalloc(1, sizeof(MP4MovieExtendsAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->type = MP4MovieExtendsAtomType;
    self->name = "movie extends";
    self->mehd = NULL;

    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->addAtom = addAtom;
    self->destroy = destroy;
    self->getTrex = getTrex;

    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;
    err = MP4MakeLinkedList(&self->trexList);
    if (err)
        goto bail;
    *outAtom = self;

bail:
    TEST_RETURN(err);

    return err;
}
