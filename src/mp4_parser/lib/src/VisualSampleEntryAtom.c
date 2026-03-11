/*
 ***********************************************************************
 * Copyright 2005-2006 by Freescale Semiconductor, Inc.
 * Copyright 2026 NXP
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
    $Id: VisualSampleEntryAtom.c,v 1.2 2000/05/17 08:01:32 francesc Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4VisualSampleEntryAtomPtr self;
    err = MP4NoErr;
    self = (MP4VisualSampleEntryAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->ESDAtomPtr) {
        self->ESDAtomPtr->destroy(self->ESDAtomPtr);
        self->ESDAtomPtr = NULL;
    }
    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    MP4VisualSampleEntryAtomPtr self = (MP4VisualSampleEntryAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GETBYTES(6, reserved1);
    GET16(dataReferenceIndex);
    GETBYTES(16, reserved2);

    GET16(video_width);
    GET16(video_height);

    GET32(reserved4);
    GET32(reserved5);
    GET32(reserved6);
    GET16(reserved7);
    GET8(nameLength);
    GETBYTES(31, name31);
    GET16(reserved8);
    GET16(reserved9);

    while (self->bytesRead + 8 < self->size) {
        MP4AtomPtr outAtom = NULL;
        err = MP4ParseAtom(inputStream, &outAtom);
        if (err)
            break;

        self->bytesRead += outAtom->size;

        if (MP4ESDAtomType == outAtom->type && self->ESDAtomPtr == NULL) {
            /* the 'esds' atom is found */
            self->ESDAtomPtr = outAtom;
        } else if (QTColorParameterAtomType == outAtom->type && self->colrAtomPtr == NULL) {
            /* the 'colr' atom is found */
            self->colrAtomPtr = outAtom;
        } else {
            /* other atoms. Overlook it.*/
            self->skipped_size += outAtom->size;
            outAtom->destroy(outAtom);
            outAtom = NULL;
        }
    }

    if (self->size > self->bytesRead) {
        /*ENGR114741:  'mp4v' atom may end with 4 bytes of 0 (0x00 00 00 00), following the 'esds'
        atom. may result in MP4NoQTAtomErr (-500) In fact, there can be any bytes of data after esds
        atom, skip them all! */
        u64 skipped_size = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(skipped_size);
        self->skipped_size += skipped_size;
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateVisualSampleEntryAtom(MP4VisualSampleEntryAtomPtr* outAtom) {
    MP4Err err;
    MP4VisualSampleEntryAtomPtr self;

    self = (MP4VisualSampleEntryAtomPtr)MP4LocalCalloc(1, sizeof(MP4VisualSampleEntryAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4VisualSampleEntryAtomType;
    self->name = "visual sample entry";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
