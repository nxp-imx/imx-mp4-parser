/*
 ***********************************************************************
 * Copyright 2023, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    ItemPropertiesAtomPtr self;
    err = MP4NoErr;
    self = (ItemPropertiesAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err addAtom(ItemPropertiesAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    err = MP4NoErr;
    assert(atom);

    if (atom->type == ItemPropertyContainerAtomType)
        self->ipcoAtom = atom;
    else if (atom->type == ItemPropertyAssociationAtomType)
        self->ipmaAtom = atom;
    else
        BAILWITHERROR(MP4BadDataErr)

bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    PARSE_ATOM_LIST(ItemPropertiesAtom)
bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4CreateItemPropertiesAtom(ItemPropertiesAtomPtr* outAtom) {
    MP4Err err;
    ItemPropertiesAtomPtr self;

    self = (ItemPropertiesAtomPtr)MP4LocalCalloc(1, sizeof(ItemPropertiesAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = ItemPropertiesAtomType;
    self->name = "Item Properties Atom";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->addAtom = addAtom;
    /* Default setting */
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
