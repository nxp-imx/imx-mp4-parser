/*
 ***********************************************************************
 * Copyright 2016 by Freescale Semiconductor, Inc.
 *
 * Copyright 2017-2018, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */
#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4ProtectedVideoSampleAtomPtr self;
    err = MP4NoErr;
    self = (MP4ProtectedVideoSampleAtomPtr)s;
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

static MP4Err addAtom(MP4ProtectedVideoSampleAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    err = MP4NoErr;
    assert(atom);
    err = MP4AddListEntry(atom, self->atomList);
    if (err)
        goto bail;
    switch (atom->type) {
        case MP4ProtectionSchemeInfoAtomType:
            self->sinf = atom;
            break;
        case MP4AvccAtomType:
            self->AVCCAtomPtr = atom;
            break;
        case MP4HvccAtomType:
            self->HVCCAtomPtr = atom;
            break;
        default:
            break;
    }
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err getOriginFormat(MP4ProtectedVideoSampleAtomPtr self, u32* type) {
    MP4Err err = MP4NoErr;
    MP4ProtectionSchemeInfoAtomPtr sinf = NULL;
    MP4OriginFormatAtomPtr frma = NULL;

    if (self->sinf == NULL || type == NULL)
        BAILWITHERROR(MP4BadParamErr)

    sinf = (MP4ProtectionSchemeInfoAtomPtr)self->sinf;
    frma = (MP4OriginFormatAtomPtr)sinf->frma;
    if (frma == NULL)
        BAILWITHERROR(MP4BadParamErr)

    *type = frma->format;

bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    MP4ProtectedVideoSampleAtomPtr self = (MP4ProtectedVideoSampleAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GETBYTES(6, reserved1);
    GET16(dataReferenceIndex);
    GETBYTES(16, reserved2);

    GET16(video_width);
    GET16(video_height);

    GET32(reserved4);
    GET32(reserved5);
    GET32(reserved6);
    GET16(reserved7);
    GET8(nameLength);
    GETBYTES(31, name31);
    GET16(reserved8);
    GET16(reserved9);

    while (self->bytesRead < self->size) {
        MP4AtomPtr atom;
        err = MP4ParseAtom((MP4InputStreamPtr)inputStream, &atom);
        if (err)
            goto bail;
        self->bytesRead += atom->size;
        err = addAtom(self, atom);
        if (err)
            goto bail;
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateProtectedVideoSampleEntryAtom(MP4ProtectedVideoSampleAtomPtr* outAtom) {
    MP4Err err;
    MP4ProtectedVideoSampleAtomPtr self;

    self = (MP4ProtectedVideoSampleAtomPtr)MP4LocalCalloc(1, sizeof(MP4ProtectedVideoSampleAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4ProtectedVideoSampleEntryAtomType;
    self->name = "Protected Video Sample";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->addAtom = addAtom;
    self->getOriginFormat = getOriginFormat;
    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;
    self->AVCCAtomPtr = NULL;
    self->HVCCAtomPtr = NULL;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
