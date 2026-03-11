/*
 ***********************************************************************
 * Copyright 2016 by Freescale Semiconductor, Inc.
 * Copyright 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */
#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4SchemeInfoAtomPtr self;
    err = MP4NoErr;
    self = (MP4SchemeInfoAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = MP4DeleteLinkedList(self->atomList);
    if (err)
        goto bail;
    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err addAtom(MP4SchemeInfoAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    err = MP4NoErr;
    assert(atom);
    err = MP4AddListEntry(atom, self->atomList);
    if (err)
        goto bail;
    switch (atom->type) {
        case MP4TrackEncryptionAtomType:
            self->tenc = atom;
            break;
        default:
            break;
    }
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    PARSE_ATOM_LIST(MP4SchemeInfoAtom)

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateSchemeInfoAtom(MP4SchemeInfoAtomPtr* outAtom) {
    MP4Err err;
    MP4SchemeInfoAtomPtr self;

    self = (MP4SchemeInfoAtomPtr)MP4LocalCalloc(1, sizeof(MP4SchemeInfoAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4SchemeInfoAtomType;
    self->name = "scheme info atom";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->addAtom = addAtom;
    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
