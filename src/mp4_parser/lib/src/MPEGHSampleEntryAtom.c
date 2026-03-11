/*
 ***********************************************************************
 * Copyright 2021, 2024, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

#define FLAC_SKIP_SIZE 12
#define FLAC_INFO_SIZE 38
#define FLAC_MIME_SIZE 4

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MPEGHSampleEntryAtomPtr self;
    err = MP4NoErr;
    self = (MPEGHSampleEntryAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->MhacAtom) {
        self->MhacAtom->destroy(self->MhacAtom);
        self->MhacAtom = NULL;
    }
    if (self->MhapAtom) {
        self->MhapAtom->destroy(self->MhapAtom);
        self->MhapAtom = NULL;
    }

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    int left = 0;

    MPEGHSampleEntryAtomPtr self = (MPEGHSampleEntryAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    /* normal audio sample description entry, 28 bytes */
    GETBYTES(6, reserved1);
    GET16(dataReferenceIndex);
    GET16(version);
    GETBYTES(6, reserved2); /* QT reserved: version(2B) + revision level(2B) + vendor(4B) */
    GET16(channels);
    GET16(sampleSize);
    GET32(reserved5);
    GET16(timeScale);  // high 16 bit (integer part)of time scale
    GET16(reserved6);  // low 16 bit of time scale

    left = self->size - self->bytesRead;
    if (left < 8)
        BAILWITHERROR(MP4InvalidMediaErr)

    while (self->bytesRead + 8 < self->size) {
        MP4AtomPtr outAtom = NULL;
        err = MP4ParseAtom(inputStream, &outAtom);
        if (err)
            break;

        self->bytesRead += outAtom->size;

        if (MHACAtomType == outAtom->type && self->MhacAtom == NULL) {
            self->MhacAtom = outAtom;
        } else if (MHAPAtomType == outAtom->type && self->MhapAtom == NULL) {
            self->MhapAtom = outAtom;
        } else {
            self->skipped_size += outAtom->size;
            outAtom->destroy(outAtom);
            outAtom = NULL;
        }
    }

    if (self->size > self->bytesRead) {
        u64 redundance_size = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(redundance_size);
        self->skipped_size += redundance_size;
    }

bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4CreateMPEGHSampleEntryAtom(MPEGHSampleEntryAtomPtr* outAtom, u32 atomType) {
    MP4Err err;
    MPEGHSampleEntryAtomPtr self;

    self = (MPEGHSampleEntryAtomPtr)MP4LocalCalloc(1, sizeof(MPEGHSampleEntryAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = atomType;
    self->name = "MPEGH audio sample entry";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    /* Default setting */
    self->channels = 2;
    self->sampleSize = 16;
    self->timeScale = 44100;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
