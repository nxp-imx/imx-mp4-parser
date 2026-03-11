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
    EC3SampleEntryAtomPtr self;
    err = MP4NoErr;
    self = (EC3SampleEntryAtomPtr)s;
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
    EC3SampleEntryAtomPtr self = (EC3SampleEntryAtomPtr)s;
    static u32 ac3_channel[] = {2, 1, 2, 3, 3, 4, 4, 5};
    u32 acmod = 0;
    u32 lfeon = 0;
    u64 skipBytes = 3;
    u64 sizeToSkip;
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

    do {
        if (self->size <= self->bytesRead + 8)
            break;

        GET32(dec3size);
        GET32(dec3type);

        if (self->dec3type != MP4EC3SpecificAtomType || self->dec3size < 13)
            break;

        // so do not parse the bitrate and channel number.
        // skip data_rate ,num_ind_sub,fscod & bsid
        SKIPBYTES_FORWARD(skipBytes);
        GETBYTES(2, ec3Info);
        acmod = (self->ec3Info[0] >> 1) & 0x07;
        lfeon = self->ec3Info[0] & 0x01;
        self->channels = ac3_channel[acmod] + lfeon;
        if (!((self->ec3Info[1] >> 1) & 0x0F)) {
            break;
        }

        GET8(extension);
        if (self->ec3Info[1] & 0x01) {
            self->channels += 1;  // LFE2
        }

        if (self->extension & 0x01) {
            self->channels += 2;
        }

        if ((self->extension >> 1) & 0x01) {
            self->channels += 2;
        }
        if ((self->extension >> 2) & 0x01) {
            self->channels += 1;
        }

        if ((self->extension >> 3) & 0x01) {
            self->channels += 1;
        }
        if ((self->extension >> 4) & 0x01) {
            self->channels += 2;
        }
        if ((self->extension >> 5) & 0x01) {
            self->channels += 2;
        }
        if ((self->extension >> 6) & 0x01) {
            self->channels += 2;
        }
        if ((self->extension >> 7) & 0x01) {
            self->channels += 1;
        }

    } while (0);
    sizeToSkip = self->size - self->bytesRead;
    SKIPBYTES_FORWARD(sizeToSkip);

bail:
    TEST_RETURN(err);

    return err;
}

// please check ETSI TS 102 366 Annex F
MP4Err MP4CreateEC3SampleEntryAtom(EC3SampleEntryAtomPtr* outAtom, u32 type) {
    MP4Err err;
    EC3SampleEntryAtomPtr self;

    self = (EC3SampleEntryAtomPtr)MP4LocalCalloc(1, sizeof(EC3SampleEntryAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = type;
    self->name = "ec3 audio sample entry";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    /* Default setting */
    self->channels = 2;
    self->sampleSize = 16;
    self->timeScale = 44100;
    self->extension = 0;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
