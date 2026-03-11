/************************************************************************
 * Copyright (c) 2014, Freescale Semiconductor, Inc.
 * Copyright 2017, 2020, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    u32 i;
    MP4TrackFragmentAtomPtr self;
    self = (MP4TrackFragmentAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    DESTROY_ATOM_LIST

    err = MP4DeleteLinkedList(self->trunList);
    if (err)
        goto bail;

    if (self->super)
        self->super->destroy(s);

bail:
    TEST_RETURN(err);

    return;
}

static MP4Err addAtom(MP4TrackFragmentAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    err = MP4NoErr;
    assert(atom);

    err = MP4AddListEntry(atom, self->atomList);
    if (err)
        goto bail;
    MP4MSG("TrackFragmentAtom add type=%x\n", atom->type);
    switch (atom->type) {
        case MP4TrackFragmentHeaderAtomType:
            self->tfhd = atom;
            break;
        case MP4TrackFragmentDecodeTimeAtomType:
            self->tfdt = atom;
            break;
        case MP4TrackFragmentRunAtomType:
            err = MP4AddListEntry(atom, self->trunList);
            if (err)
                goto bail;
            break;
        case MP4SampleAuxiliaryInfoSizeAtomType:
            self->saiz = atom;
            break;
        case MP4SampleAuxiliaryInfoOffsetAtomType:
            self->saio = atom;
            break;
        case MP4SampleEncryptionAtomType:
            self->senc = atom;
            break;
        default:
            break;
    }
bail:
    TEST_RETURN(err);

    return err;
}

u32 getTrunCount(MP4TrackFragmentAtomPtr self) {
    u32 trunCount = 0;
    if (self == NULL)
        return trunCount;

    if (MP4NoErr != MP4GetListEntryCount(self->trunList, &trunCount))
        return 0;
    return trunCount;
}

MP4Err getTrun(MP4TrackFragmentAtomPtr self, u32 TrunID, MP4AtomPtr* outTrack) {
    MP4Err err;

    err = MP4NoErr;
    if ((TrunID == 0) || (self == NULL) || (TrunID > getTrunCount(self)))
        BAILWITHERROR(MP4BadParamErr)
    err = MP4GetListEntry(self->trunList, TrunID - 1, (char**)outTrack);
    if (err)
        goto bail;
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    PARSE_ATOM_LIST(MP4TrackFragmentAtom)
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateTrackFragmentAtom(MP4TrackFragmentAtomPtr* outAtom) {
    MP4Err err;
    MP4TrackFragmentAtomPtr self;

    self = (MP4TrackFragmentAtomPtr)MP4LocalCalloc(1, sizeof(MP4TrackFragmentAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4TrackFragmentAtomType;
    self->name = "track fragment";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->tfhd = NULL;
    self->tfdt = NULL;
    self->saiz = NULL;
    self->saio = NULL;
    self->senc = NULL;
    self->addAtom = addAtom;
    self->getTrunCount = getTrunCount;
    self->getTrun = getTrun;
    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;
    err = MP4MakeLinkedList(&self->trunList);
    if (err)
        goto bail;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
