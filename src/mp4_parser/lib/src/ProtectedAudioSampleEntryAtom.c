/*
 ***********************************************************************
 * Copyright 2017, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */
#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4ProtectedAudioSampleAtomPtr self;
    err = MP4NoErr;
    self = (MP4ProtectedAudioSampleAtomPtr)s;
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

static MP4Err addAtom(MP4ProtectedAudioSampleAtomPtr self, MP4AtomPtr atom) {
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
        case MP4ESDAtomType:
            self->ESDAtomPtr = atom;
            break;
        default:
            break;
    }
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err getOriginFormat(MP4ProtectedAudioSampleAtomPtr self, u32* type) {
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
    MP4ProtectedAudioSampleAtomPtr self = (MP4ProtectedAudioSampleAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GETBYTES(6, reserved1);
    GET16(dataReferenceIndex);
    GET16(version);
    GETBYTES(6, reserved2); /* QT reserved: version(2B) + revision level(2B) + vendor(4B) */
    GET16(channels);
    GET16(sampleSize); /* bits per sample */
    GET32(reserved5);
    GET16(timeScale);
    GET16(reserved6);

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

MP4Err MP4CreateProtectedAudioSampleEntryAtom(MP4ProtectedAudioSampleAtomPtr* outAtom) {
    MP4Err err;
    MP4ProtectedAudioSampleAtomPtr self;

    self = (MP4ProtectedAudioSampleAtomPtr)MP4LocalCalloc(1, sizeof(MP4ProtectedVideoSampleAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4ProtectedAudioSampleEntryAtomType;
    self->name = "Protected Audio Sample";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->addAtom = addAtom;
    self->getOriginFormat = getOriginFormat;
    self->channels = 2;
    self->sampleSize = 16;
    self->timeScale = 44100;
    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
