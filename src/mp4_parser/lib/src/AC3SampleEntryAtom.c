/*
***********************************************************************
* Copyright (c) 2013, Freescale Semiconductor, Inc.
* Copyright 2026 NXP
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************
*/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    AC3SampleEntryAtomPtr self;
    err = MP4NoErr;
    self = (AC3SampleEntryAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    AC3SampleEntryAtomPtr self = (AC3SampleEntryAtomPtr)s;
    static u32 ac3_channel[] = {2, 1, 2, 3, 3, 4, 4, 5};
    u32 acmod = 0;
    u32 lfeon = 0;

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

    if (self->size > self->bytesRead) {
        if (self->size >= self->bytesRead + 8) {
            GET32(dac3size);
            GET32(dac3type);
            if (self->dac3type == MP4AC3SpecificAtomType && self->dac3size == 11) {
                GETBYTES(3, ac3Info);
                acmod = (self->ac3Info[1] >> 3) & 0x07;
                lfeon = (self->ac3Info[1] >> 2) & 0x01;
                self->channels = ac3_channel[acmod] + lfeon;
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

// please check ETSI TS 102 366 Annex F
MP4Err MP4CreateAC3SampleEntryAtom(AC3SampleEntryAtomPtr* outAtom, u32 type) {
    MP4Err err;
    AC3SampleEntryAtomPtr self;

    self = (AC3SampleEntryAtomPtr)MP4LocalCalloc(1, sizeof(AC3SampleEntryAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = type;
    self->name = "ac3 audio sample entry";
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
