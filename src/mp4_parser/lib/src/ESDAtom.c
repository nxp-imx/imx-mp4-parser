/*
 ***********************************************************************
 * Copyright 2005-2006 by Freescale Semiconductor, Inc.
 * Copyright 2024, 2026 NXP
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
    $Id: ESDAtom.c,v 1.2 2000/05/17 08:01:27 francesc Exp $
*/

#include <stdlib.h>
#include "MP4Atoms.h"
#include "MP4Descriptors.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4ESDAtomPtr self;
    err = MP4NoErr;
    self = (MP4ESDAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->descriptor) {
        self->descriptor->destroy(self->descriptor);
        self->descriptor = NULL;
    }

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    MP4ESDAtomPtr self = (MP4ESDAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;
    self->descriptorLength = (u32)(self->size - self->bytesRead);
    if (self->descriptorLength == 0) {
        DEBUG_MSG("Warning: ESD size is zero");
        goto bail;
    } else {
        // MP4MSG( "ESD size %lld, bytes read %lld, size left for descriptors %lld\n",  self->size,
        // self->bytesRead,  self->size - self->bytesRead);
        err = MP4ParseDescriptor(inputStream, self->descriptorLength, &self->descriptor);
        if (err)
            goto bail;
        MP4MSG("ESD size %lld, bytes read %lld, size left for descriptors %lld, returned decriptor "
               "size %d\n",
               self->size, self->bytesRead, self->size - self->bytesRead, self->descriptor->size);
        self->bytesRead += self->descriptor->size;

        if (self->size < self->bytesRead) {
            MP4MSG("There is error in parsing 'esds' descriptors. may be cut short!\n");
        }
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateESDAtom(MP4ESDAtomPtr* outAtom) {
    MP4Err err;
    MP4ESDAtomPtr self;

    self = (MP4ESDAtomPtr)MP4LocalCalloc(1, sizeof(MP4ESDAtom));
    TESTMALLOC(self);

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4ESDAtomType;
    self->name = "ESD";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
