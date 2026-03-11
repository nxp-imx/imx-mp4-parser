/*
 * Copyright 2026 NXP
 *
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
    $Id:SowtSampleEntryAtom.c,v 1.2 2000/05/17 08:01:26 francesc Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    PcmAudioSampleEntryAtomPtr self;
    err = MP4NoErr;
    self = (PcmAudioSampleEntryAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->decoderSpecificInfo) {
        MP4LocalFree(self->decoderSpecificInfo);
        self->decoderSpecificInfo = NULL;
    }

    if (self->super)
        self->super->destroy(s);

bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;

    PcmAudioSampleEntryAtomPtr self = (PcmAudioSampleEntryAtomPtr)s;

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
    GET16(bitsPerSample);
    GETBYTES(4, reserved3);
    GET16(timeScale); /* fixed point 16.16, get the integer part*/
    GET16(reserved4);

    if (self->version) { /* QT version is 1, usually 'sowt' & 'twos' is version 1, 'raw ' is version
                            0. */
        GETBYTES(16, reserved5);
    }

    self->decoderSpecificInfoSize = (u32)(self->size - self->bytesRead);
    // printf("sowt/twos version %d, channels %d, sample size %d, sample Rate %d, decoder specific
    // info size %d\n", self->version, self->channels, self->bitsPerSample , self->timeScale,
    // self->decoderSpecificInfoSize);

    if (0 < (s32)self->decoderSpecificInfoSize) {
        /* found a "chan" atom. QT allows you to have a 'chan' atom which allows multichannel
        configuration to be saved into a file. The mechanisms used are the quicktime chan atom as
        well as some codec specific multichannel setups. But its format is NOT known yet. Amanda*/
        self->decoderSpecificInfo = (u8*)MP4LocalCalloc(1, self->decoderSpecificInfoSize);
        TESTMALLOC(self->decoderSpecificInfo);
        GETBYTES(self->decoderSpecificInfoSize, decoderSpecificInfo);
    } else if (0 > (s32)self->decoderSpecificInfoSize)
        BAILWITHERROR(MP4BadDataErr)

bail:
    TEST_RETURN(err);

    return err;
}

/* Create QT sowt atom */
MP4Err MP4CreatePcmAudioSampleEntryAtom(PcmAudioSampleEntryAtomPtr* outAtom, u32 type) {
    MP4Err err;
    PcmAudioSampleEntryAtomPtr self;

    self = (PcmAudioSampleEntryAtomPtr)MP4LocalCalloc(1, sizeof(PcmAudioSampleEntryAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->type = type;

    self->name = "sowt/twos sample entry";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;

    /* Default setting */
    self->channels = 2;
    self->timeScale = 44100;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
