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
    MP4ProtectionSchemeInfoAtomPtr self;
    err = MP4NoErr;
    self = (MP4ProtectionSchemeInfoAtomPtr)s;
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

static MP4Err addAtom(MP4ProtectionSchemeInfoAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    err = MP4NoErr;
    assert(atom);
    err = MP4AddListEntry(atom, self->atomList);
    if (err)
        goto bail;
    switch (atom->type) {
        case MP4OriginFormatAtomType:
            self->frma = atom;
            break;
        case MP4SchemeTypeAtomType:
            self->schm = atom;
            break;
        case MP4SchemeInfoAtomType:
            self->schi = atom;
            break;
        default:
            break;
    }
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    PARSE_ATOM_LIST(MP4ProtectionSchemeInfoAtom)
bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4CreateProtectionSchemeInfoAtom(MP4ProtectionSchemeInfoAtomPtr* outAtom) {
    MP4Err err;
    MP4ProtectionSchemeInfoAtomPtr self;

    self = (MP4ProtectionSchemeInfoAtomPtr)MP4LocalCalloc(1, sizeof(MP4ProtectionSchemeInfoAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4ProtectionSchemeInfoAtomType;
    self->name = "Protection Scheme Info";
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