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
    MP4Err err = MP4NoErr;
    ItemInfoAtomPtr self;
    u32 i;

    self = (ItemInfoAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    DESTROY_ATOM_LIST

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err addAtom(ItemInfoAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    err = MP4NoErr;
    assert(atom);
    err = MP4AddListEntry(atom, self->atomList);
    if (err)
        goto bail;

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    int entryCountSize;
    ItemInfoAtomPtr self = (ItemInfoAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    entryCountSize = self->version == 0 ? 2 : 4;

    if (entryCountSize == 2) {
        GET16(infoEntryCount);
    } else {
        GET32(infoEntryCount);
    }

    while (self->bytesRead < self->size) {
        MP4AtomPtr atom;
        err = MP4ParseAtom((MP4InputStreamPtr)inputStream, &atom);
        if (err)
            goto bail;
        self->bytesRead += atom->size;
        err = addAtom(self, atom);
        if (err)
            goto bail;

        if (self->needIref == FALSE) {
            InfoEntryAtomPtr infoEntry = (InfoEntryAtomPtr)atom;
            if (infoEntry->isXmp(infoEntry) || infoEntry->isExif(infoEntry) ||
                infoEntry->isGrid(infoEntry))
                self->needIref = TRUE;
        }
    }
    if (self->bytesRead > self->size) {
        /* ERROR CONCEALMENT: wrong atom size, roll back */
        u32 rollbackBytes = (u32)(self->bytesRead - self->size);
        inputStream->file_offset -= rollbackBytes;
        inputStream->available += rollbackBytes;
        self->bytesRead -= rollbackBytes;
    }

    if (self->size > self->bytesRead) {
        u64 redundance_size = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(redundance_size);
    }

bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4CreateItemInfoAtom(ItemInfoAtomPtr* outAtom) {
    MP4Err err;
    ItemInfoAtomPtr self;

    self = (ItemInfoAtomPtr)MP4LocalCalloc(1, sizeof(ItemInfoAtom));
    TESTMALLOC(self);

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = ItemInfoAtomType;
    self->name = "Item Info Atom";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->addAtom = addAtom;
    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;
    /* Default setting */
    self->infoEntryCount = 0;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
