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
    $Id: ALACSampleEntryAtom.c,v 1.0 2020/05/9 08:01:26 Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4ALACSampleEntryAtomPtr self;
    err = MP4NoErr;
    self = (MP4ALACSampleEntryAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->despExtension) {
        MP4LocalFree(self->despExtension);
        self->despExtension = NULL;
    }

    if (self->WaveAtomPtr) {
        self->WaveAtomPtr->destroy(self->WaveAtomPtr);
        self->WaveAtomPtr = NULL;
    }

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    u32 timeScale;

    MP4ALACSampleEntryAtomPtr self = (MP4ALACSampleEntryAtomPtr)s;

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

    if (1 == self->version) { /* QT version is 1 */
        self->bIsQTVersionOne = TRUE;
        self->despExtension = (char*)MP4LocalCalloc(1, 16);
        TESTMALLOC(self->despExtension)
        GETBYTES(16, despExtension);
    } else if (2 == self->version) {
        union {
            double fp;
            u64 val;
        } qtfp;

        GET32(samplePerPackage);
        GET64_V(qtfp.val);
        GET32(channels);
        self->timeScale = (u32)(qtfp.fp);
    }

    /* sometimes, timeScale might not be correct. Set it to -1 to indicate invalid,
     * so that upper level can use track time scale in 'mdhd' to get audio
     * sample rate */
    timeScale = self->timeScale;
    if ((96000 != timeScale) && (48000 != timeScale) && (24000 != timeScale) &&
        (12000 != timeScale) && (88200 != timeScale) && (44100 != timeScale) &&
        (22050 != timeScale) && (11025 != timeScale) && (64000 != timeScale) &&
        (32000 != timeScale) && (16000 != timeScale) && (8000 != timeScale) &&
        (7350 != timeScale)) {
        self->timeScale = -1;
    }

    if (self->version > 0) {
        /* QT version 1,
            alac
              |_wave
              |   |_fmra
              |   |_terminator atom(0x00000000) 8 bytes
              |
              |_chan
              ...
        overlook atoms following the 'wave' atom, eg 'chan' */

        /* There may be garbage before wave atom, seach the wave atom  */
        {
            u8 waveFound = FALSE;

            while ((!waveFound) && (self->size >= (self->bytesRead + 4))) {
                GET32(garbage);
                if (QTWaveAtomType == self->garbage) { /* step backward to the start of 'wave' atom.
                                                          WARNING: can not use SKIPBYTES_FORWARD*/
                    waveFound = TRUE;
                    inputStream->file_offset =
                            -8; /* modify relative offset, seek to the file beginning again */
                    inputStream->available += 8;
                    self->bytesRead -= 8;
                }
            }

            if (!waveFound) {
                BAILWITHERROR(MP4InvalidMediaErr)
            } else {
                GETATOM(WaveAtomPtr);
            }
        }

    } else {
        u32 size;
        u32 type;
        MP4MSG("Version 0 in ALAC Atom, expect alac specific info following\n");
        GET32_V(size);
        GET32_V(type);
        if (type == MP4ALACSampleEntryAtomType && size == ALAC_SPECIFIC_INFO_SIZE &&
            (self->size - self->bytesRead >= ALAC_SPECIFIC_INFO_SIZE - 8)) {
            inputStream->file_offset = -8;
            inputStream->available += 8;
            self->bytesRead -= 8;
            GETBYTES(ALAC_SPECIFIC_INFO_SIZE, AlacInfo);
            self->AlacInfoAvailable = 1;
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

MP4Err MP4CreateALACSampleEntryAtom(MP4ALACSampleEntryAtomPtr* outAtom) {
    MP4Err err;
    MP4ALACSampleEntryAtomPtr self;

    self = (MP4ALACSampleEntryAtomPtr)MP4LocalCalloc(1, sizeof(MP4ALACSampleEntryAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4ALACSampleEntryAtomType;
    self->name = "apple lossless audio sample entry";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    /* Default setting */
    self->channels = 2;
    self->sampleSize = 16;
    self->timeScale = 44100;
    self->despExtension = NULL;
    self->WaveAtomPtr = NULL;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
