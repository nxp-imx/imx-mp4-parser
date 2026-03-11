/*
 ***********************************************************************
 * Copyright 2021, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

//#define DEBUG
#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

#define kAC4SpecificBoxPayloadSize 1176

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    AC4SampleEntryAtomPtr self;
    err = MP4NoErr;
    self = (AC4SampleEntryAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->dsiData) {
        MP4LocalFree(self->dsiData);
        self->dsiData = NULL;
    }
    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);
    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    AC4SampleEntryAtomPtr self = (AC4SampleEntryAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GETBYTES(6, reserved1);
    GET16(dataReferenceIndex);
    GET16(version);
    GETBYTES(6, reserved2);
    GET16(channels);
    GET16(sampleSize);
    GET32(reserved5);
    GET16(timeScale);
    GET16(reserved6);

    if (self->size > self->bytesRead) {
        if (self->size >= self->bytesRead + 8) {
            GET32(dacsize);
            GET32(dactype);
            if (self->dactype == MP4AC4SpecificAtomType &&
                self->size >= self->bytesRead + self->dacsize - 8) {
                self->dsiLength = self->dacsize - 8;
                if (self->dsiLength >= kAC4SpecificBoxPayloadSize)
                    BAILWITHERROR(MP4InvalidMediaErr)
                self->dsiData = MP4LocalCalloc(1, self->dsiLength);
                GETBYTES(self->dsiLength, dsiData);
            } else {
                u64 sizeToSkip = self->size - self->bytesRead;
                SKIPBYTES_FORWARD(sizeToSkip);
            }
        } else {
            u64 sizeToSkip = self->size - self->bytesRead;
            SKIPBYTES_FORWARD(sizeToSkip);
        }
    }

bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4CreateAC4SampleEntryAtom(AC4SampleEntryAtomPtr* outAtom) {
    MP4Err err;
    AC4SampleEntryAtomPtr self;

    self = (AC4SampleEntryAtomPtr)MP4LocalCalloc(1, sizeof(AC4SampleEntryAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4AC4SampleEntryAtomType;
    self->name = "ac4 audio sample entry";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    /* Default setting */
    self->channels = 6;
    self->timeScale = 48000;
    self->sampleSize = 16;
    self->dsiData = NULL;
    self->dsiLength = 0;

    *outAtom = self;
bail:
    TEST_RETURN(err);
    return err;
}
