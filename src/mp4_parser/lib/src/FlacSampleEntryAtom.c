/*
 ***********************************************************************
 * Copyright 2020, 2026 NXP
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
    MP4FlacSampleEntryAtomPtr self;
    err = MP4NoErr;
    self = (MP4FlacSampleEntryAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->csd) {
        MP4LocalFree(self->csd);
        self->csd = NULL;
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
    u8* writePtr = 0;

    MP4FlacSampleEntryAtomPtr self = (MP4FlacSampleEntryAtomPtr)s;

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

    // From https://github.com/xiph/flac/blob/master/doc/isoflac.txt
    // data in stream:      SKIP 12 | INFO 38
    // data to pass as csd : MIME 4 | INFO 38

    left = self->size - self->bytesRead;
    if (left < FLAC_SKIP_SIZE + FLAC_INFO_SIZE)
        BAILWITHERROR(MP4InvalidMediaErr)

    self->csd = (u8*)MP4LocalCalloc(FLAC_MIME_SIZE + FLAC_INFO_SIZE, sizeof(u8));
    TESTMALLOC(self->csd)
    // skipping dFla, version
    GETBYTES(FLAC_SKIP_SIZE, reserved3);
    // Add flaC header mime type to CSD
    memcpy((char*)self->csd, "fLaC", FLAC_MIME_SIZE);
    writePtr = self->csd + FLAC_MIME_SIZE;
    GETBYTES_V(FLAC_INFO_SIZE, writePtr);
    self->csdSize = FLAC_MIME_SIZE + FLAC_INFO_SIZE;

    if (self->size > self->bytesRead) {
        u64 skipped_size = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(skipped_size);
        self->skipped_size += skipped_size;
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateFlacSampleEntryAtom(MP4FlacSampleEntryAtomPtr* outAtom) {
    MP4Err err;
    MP4FlacSampleEntryAtomPtr self;

    self = (MP4FlacSampleEntryAtomPtr)MP4LocalCalloc(1, sizeof(MP4FlacSampleEntryAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4FlacSampleEntryAtomType;
    self->name = "Flac audio sample entry";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    /* Default setting */
    self->channels = 2;
    self->sampleSize = 16;
    self->timeScale = 44100;
    self->csd = NULL;
    self->csdSize = 0;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
