/*
 ***********************************************************************
 * Copyright 2005-2008 by Freescale Semiconductor, Inc.
 * Copyright 2020, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    QTsiDecompressParaAtomPtr self;
    err = MP4NoErr;
    self = (QTsiDecompressParaAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->ESDAtomPtr) {
        self->ESDAtomPtr->destroy(self->ESDAtomPtr);
        self->ESDAtomPtr = NULL;
    }
    if (self->OriginFormatAtomPtr) {
        self->OriginFormatAtomPtr->destroy(self->OriginFormatAtomPtr);
        self->OriginFormatAtomPtr = NULL;
    }
    if (self->TerminatorAtomPtr) {
        self->TerminatorAtomPtr->destroy(self->TerminatorAtomPtr);
        self->TerminatorAtomPtr = NULL;
    }

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    QTsiDecompressParaAtomPtr self = (QTsiDecompressParaAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    /* wave
        |_fmra
        |_mp4a (small, child of the wave)
        |_esds
        |_terminator (0x00000000) 8 bytes atom
        |_optional: several bytes of 0x00, as place holder */

    /* skip 'frma' and small 'mp4a' child atoms if exist */
    while (1) {
        GETATOM(ESDAtomPtr);
        if (MP4ESDAtomType == GET_ATOM_TYPE(ESDAtomPtr))  // self->ESDAtomPtr->type
        {                                                 /* the 'esds' child atom found */
            break;
        } else if (MP4OriginFormatAtomType == GET_ATOM_TYPE(ESDAtomPtr)) {
            MP4OriginFormatAtomPtr ptr;
            self->OriginFormatAtomPtr = self->ESDAtomPtr;
            self->ESDAtomPtr = NULL;
            ptr = (MP4OriginFormatAtomPtr)self->OriginFormatAtomPtr;
            if (ptr->format == MP4ALACSampleEntryAtomType &&
                (self->size - self->bytesRead >= ALAC_SPECIFIC_INFO_SIZE)) {
                // can't handle this in an atom, it'll has same name "alac" with
                // MP4ALACSampleEntryAtom
                u32 infoSize;
                u32 infoId;
                u32 versionFlag;
                GET32_V(infoSize);
                GET32_V(infoId);
                GET32_V(versionFlag);
                MP4MSG("ALAC infoSize %d, infoId %d, versionFlag %d", infoSize, infoId,
                       versionFlag);
                if ((infoSize != ALAC_SPECIFIC_INFO_SIZE) ||
                    (infoId != MP4_FOUR_CHAR_CODE('a', 'l', 'a', 'c')) || (versionFlag != 0)) {
                    BAILWITHERROR(MP4InvalidMediaErr)
                } else {
                    // according to gstreamer opensource parser, alac info contains 12 bytes header
                    inputStream->file_offset = -12;
                    inputStream->available += 12;
                    self->bytesRead -= 12;
                    GETBYTES(ALAC_SPECIFIC_INFO_SIZE, AlacInfo);
                }
            }
        } else if (QTTerminatorAtomType ==
                   GET_ATOM_TYPE(ESDAtomPtr)) { /* the terminator child atom found, 'wave' ends!
                                                   'esds' only exists for mp4a audio. */
            self->TerminatorAtomPtr = self->ESDAtomPtr;
            self->ESDAtomPtr = NULL;
            break;
        } else {                                              /* other atoms, such as small 'mp4a'
                                                              NOTE: add handling of small mp4a atom */
            self->skipped_size += GET_ATOM_SIZE(ESDAtomPtr);  // self->ESDAtomPtr->size;
            self->ESDAtomPtr->destroy(self->ESDAtomPtr);
            self->ESDAtomPtr = NULL;
        }
    }

    /* skip following atoms, until the terminator atom(0x00000000), which is the last atom!*/
    if (!self->TerminatorAtomPtr) {
        while (1) {
            GETATOM(TerminatorAtomPtr);
            if (QTTerminatorAtomType ==
                GET_ATOM_TYPE(TerminatorAtomPtr)) { /* the terminator child atom found */
                break;
            } else { /* other child atoms, skip it */
                self->skipped_size += GET_ATOM_SIZE(TerminatorAtomPtr);
                self->TerminatorAtomPtr->destroy(self->TerminatorAtomPtr);
                self->TerminatorAtomPtr = NULL;
            }
        }
    }

    /* engr96382:
    Warning: after the terminator atom, there may be several bytes of 0x00 as a placehoder.
    This is not explicitly declared in QT spec. Overlook them.
    -Amanda    */
    if (self->size > self->bytesRead) {
        u64 redundance_size = self->size - self->bytesRead;
        // printf("redundant atoms found, total size %d\n", redundance_size);

        SKIPBYTES_FORWARD(redundance_size);
        self->skipped_size += redundance_size;
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateQTWaveAtom(QTsiDecompressParaAtomPtr* outAtom) {
    MP4Err err;
    QTsiDecompressParaAtomPtr self;

    self = (QTsiDecompressParaAtomPtr)MP4LocalCalloc(1, sizeof(QTsiDecompressParaAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = QTWaveAtomType;
    self->name = "QT wave atom";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
