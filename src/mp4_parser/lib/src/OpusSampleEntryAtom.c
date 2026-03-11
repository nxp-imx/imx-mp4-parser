/*
 ***********************************************************************
 * Copyright 2020, 2026 NXP
 ***********************************************************************
 */

/*
This software module was originally developed by Apple Computer, Inc.
in the course of development of MPEG-4.
This software module is an implementation of a part of one or
more MPEG-4 tools as specified by MPEG-4.
ISO/IEC gives users of MPEG-4 free license to this
software module or modifications thereof for use in hardware
or software products claiming conformance to MPEG-4.
Those intending to use this software module in hardware or software
products are advised that its use may infringe existing patents.
The original developer of this software module and his/her company,
the subsequent editors and their companies, and ISO/IEC have no
liability for use of this software module or modifications thereof
in an implementation.
Copyright is not released for non MPEG-4 conforming
products. Apple Computer, Inc. retains full right to use the code for its own
purpose, assign or donate the code to a third party and to
inhibit third parties from using the code for non
MPEG-4 conforming products.
This copyright notice must be included in all copies or
derivative works. Copyright (c) 1999.
*/
/*
    $Id: OpusSampleEntryAtom.c,v 1.0 2020/05/9 08:01:26 Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

#define AOPUS_OPUSHEAD_MINSIZE 19
#define AOPUS_OPUSHEAD_MAXSIZE ((AOPUS_OPUSHEAD_MINSIZE) + 2 + 255)
#define AOPUS_CSD1_CSD2_SIZE 20

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4OpusSampleEntryAtomPtr self;
    err = MP4NoErr;
    self = (MP4OpusSampleEntryAtomPtr)s;
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
    u32 opusInfoSize;

    MP4OpusSampleEntryAtomPtr self = (MP4OpusSampleEntryAtomPtr)s;

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

    opusInfoSize = self->size - self->bytesRead;

    if (opusInfoSize < AOPUS_OPUSHEAD_MINSIZE || opusInfoSize > AOPUS_OPUSHEAD_MAXSIZE) {
        BAILWITHERROR(MP4InvalidMediaErr)
    }

    // output opus info is:
    // raw opus info | "csdi" (4Bytes) | codecDelay (8Bytes) | kSeekPreRollNs (8Bytes)
    // so we need 20 bytes more than raw data

    self->csd = (u8*)MP4LocalCalloc(opusInfoSize + AOPUS_CSD1_CSD2_SIZE, sizeof(u8));
    TESTMALLOC(self->csd)
    GETBYTES(opusInfoSize, csd);
    self->csdSize = opusInfoSize + AOPUS_CSD1_CSD2_SIZE;

    // must start with magic sequence
    memcpy((char*)self->csd, "OpusHead", 8);

    // version shall be 0
    if (self->csd[8]) {
        BAILWITHERROR(MP4InvalidMediaErr)
    }

    // force version to 1
    self->csd[8] = 1;

    {
        // convert info to LE
        static const u64 kSeekPreRollNs = 80000000;  // Fixed 80 msec
        static const u32 kOpusSampleRate = 48000;
        u16 pre_skip, out_gain;
        u32 sample_rate;
        u64 codecDelay;
        u8* ptr = self->csd + 10;
        pre_skip = ptr[0] << 8 | ptr[1];
        ptr += 2;
        sample_rate = ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3];
        ptr += 4;
        out_gain = ptr[0] << 8 | ptr[1];

        memcpy(self->csd + 10, &pre_skip, sizeof(pre_skip));
        memcpy(self->csd + 12, &sample_rate, sizeof(sample_rate));
        memcpy(self->csd + 16, &out_gain, sizeof(out_gain));

        codecDelay = pre_skip * 1000000000ll / kOpusSampleRate;

        // now for csd1,2
        ptr = self->csd + opusInfoSize;
        memcpy(ptr, "csdi", 4);
        ptr += 4;
        memcpy(ptr, &codecDelay, sizeof(codecDelay));
        ptr += sizeof(codecDelay);
        memcpy(ptr, &kSeekPreRollNs, sizeof(kSeekPreRollNs));
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateOpusSampleEntryAtom(MP4OpusSampleEntryAtomPtr* outAtom) {
    MP4Err err;
    MP4OpusSampleEntryAtomPtr self;

    self = (MP4OpusSampleEntryAtomPtr)MP4LocalCalloc(1, sizeof(MP4OpusSampleEntryAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4OpusSampleEntryAtomType;
    self->name = "opus audio sample entry";
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
