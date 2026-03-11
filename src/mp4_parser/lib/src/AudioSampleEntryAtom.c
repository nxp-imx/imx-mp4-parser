/*
 ***********************************************************************
 * Copyright 2005-2006 by Freescale Semiconductor, Inc.
 * Copyright 2017, 2026 NXP
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
    $Id: AudioSampleEntryAtom.c,v 1.2 2000/05/17 08:01:26 francesc Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err = MP4NoErr;
    MP4AudioSampleEntryAtomPtr self;

    self = (MP4AudioSampleEntryAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->despExtension) {
        MP4LocalFree(self->despExtension);
        self->despExtension = NULL;
    }

    if (self->ESDAtomPtr) {
        self->ESDAtomPtr->destroy(self->ESDAtomPtr);
        self->ESDAtomPtr = NULL;
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
    MP4AudioSampleEntryAtomPtr self = (MP4AudioSampleEntryAtomPtr)s;
    u64 extendStart = 0;
    u32 timeScale;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    if (12 == self->size) { /*engr94385:
                            this 'mp4a' is not a real audio sample entry in 'stsd', but a child of
                            quicktime 'wave' atom. All data is zero*/
        self->bIsSmallMp4aAtom = TRUE;
        GET32(reserved5);
        goto bail;
    }

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

    extendStart = self->bytesRead;

    /* engr94385:
    QT sound sample description (version 1) have 4 fields extension (each 4 bytes long).
    overlook them now. When to serialize, fill 0 for this.
    NOT count for "skipped_size" because the position
    - amanda */
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

    /* engr94385:
    In QT movie, if compression id = -2 (but not always valid)
    there may be redefined sample tables like 'wave' 'chan' etc.
    And in such case, 'esds' is a child atom in the 'wave' atom.

    so member "ESDAtomPtr" points to 'wave' atom.

    This's NOT compatible to MP4, where 'esds' is a child atom of this 'mp4a' atom.*/
    // amanda: if for version 1 and compression id -2 !!!!!!!!!!!!!!!!!!

    if (self->version > 0) {
        /* QT version 1,
            mp4a
              |_wave
              |   |_fmra
              |   |_mp4a (small, child of the wave)
              |   |_esds
              |   |_terminator atom(0x00000000) 8 bytes
              |   |_optional: several bytes of 0x00, as place holder
              |
              |_chan
              ...
        overlook atoms following the 'wave' atom, eg 'chan' */

        /* engr 115018: There may be garbage before wave atom, seach the wave atom  */
        {
            u8 waveFound = FALSE;
            u32 atom_size = 0;

            while ((!waveFound) && (self->size >= (self->bytesRead + 4))) {
                GET32(garbage);
                if (QTWaveAtomType == self->garbage) { /* step backward to the start of 'wave' atom.
                                                          WARNING: can not use SKIPBYTES_FORWARD*/
                    waveFound = TRUE;
                    inputStream->file_offset =
                            -8; /* modify relative offset, seek to the file beginning again */
                    inputStream->available += 8;
                    self->bytesRead -= 8;

                    /* Store data for backward seek operation. The position of data
                     * in the buffer is equal to absolute value of file_offset minus 1 */
                    if (inputStream->stream_flags & live_flag) {
                        u8 pos, offset;
                        u32 data;

                        if (inputStream->buf_mask) {
                            MP4MSG("Live stream: There is data stored in the buffer, mask: %x\n",
                                   inputStream->buf_mask);
                        }

                        pos = 0;
                        offset = pos;
                        data = self->garbage;
                        for (; pos < (offset + 4); pos++) {
                            inputStream->buf[pos] = data & 0xFF;
                            data >>= 8;
                        }

                        offset = pos;
                        data = atom_size;
                        for (; pos < (offset + 4); pos++) {
                            inputStream->buf[pos] = data & 0xFF;
                            data >>= 8;
                        }

                        inputStream->buf_mask = 0xFF;
                        MP4MSG("Live stream: Store data for seek: 0x%x, 0x%x\n", self->garbage,
                               atom_size);
                    }
                } else {
                    atom_size = self->garbage;
                }
            }

            if (!waveFound) {
                /* ENGR126966 : if wave is not found, try to parse as version 0 */
                u64 rollBack = self->bytesRead - extendStart;
                inputStream->file_offset -= rollBack;
                inputStream->available += rollBack;
                self->bytesRead -= rollBack;

                GETATOM(ESDAtomPtr);
            } else {
                GETATOM(WaveAtomPtr);
            }
            // BAILWITHERROR( MP4InvalidMediaErr )
        }

    } else {
        /* version 0, MP4 standard, only a 'esds' child.
           ENGR117943: There may be following atoms after esds.
            mp4a
             |_esds
             |_ other atoms*/
        GETATOM(ESDAtomPtr);
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

MP4Err MP4CreateAudioSampleEntryAtom(MP4AudioSampleEntryAtomPtr* outAtom) {
    MP4Err err;
    MP4AudioSampleEntryAtomPtr self;

    self = (MP4AudioSampleEntryAtomPtr)MP4LocalCalloc(1, sizeof(MP4AudioSampleEntryAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4AudioSampleEntryAtomType;
    self->name = "audio sample entry";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;

    self->channels = 2;
    self->sampleSize = 16;
    self->timeScale = 44100;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
