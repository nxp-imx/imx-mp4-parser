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
    $Id: AudioSampleEntryAtom.c,v 1.2 2000/05/17 08:01:26 francesc Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4Mp3SampleEntryAtomPtr self;
    err = MP4NoErr;
    self = (MP4Mp3SampleEntryAtomPtr)s;
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
    char tmp;
    int i = 0;
    int left = 0;
    MP4Mp3SampleEntryAtomPtr self = (MP4Mp3SampleEntryAtomPtr)s;

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
    GET16(sampleSize);
    GET32(reserved5);
    GET16(timeScale);
    GET16(reserved6);
    if (self->version) { /* QT version is 1, usually 'sowt' & 'twos' is version 1, 'raw ' is version
                            0. */
        GET32(samples_per_packet);
        GET32(bytes_per_packet);
        GET32(bytes_per_frame);
        GET32(bytes_per_sample);
    }

    left = (int)(self->size - self->bytesRead);
    for (i = 0; i < left; i++) {
        inputStream->readData(inputStream, 1, &tmp, "unused data");
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateMp3SampleEntryAtom(MP4Mp3SampleEntryAtomPtr* outAtom) {
    MP4Err err;
    MP4Mp3SampleEntryAtomPtr self;

    self = (MP4Mp3SampleEntryAtomPtr)MP4LocalCalloc(1, sizeof(MP4Mp3SampleEntryAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4Mp3SampleEntryAtomType;
    self->name = "mp3 audio sample entry";
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
