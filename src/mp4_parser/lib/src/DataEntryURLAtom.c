/*
 ***********************************************************************
 * Copyright (c) 2005-2012, Freescale Semiconductor, Inc.
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
    $Id: DataEntryURLAtom.c,v 1.15 1999/05/20 04:08:38 mc Exp $
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4DataEntryURLAtomPtr self = (MP4DataEntryURLAtomPtr)s;
    err = MP4NoErr;
    if (s == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->location) {
        MP4LocalFree(self->location);
        self->location = NULL;
    }

    if (s->super)
        s->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err getOffset(struct MP4DataEntryAtom* self, u32* outOffset) {
    MP4Err err;

    err = MP4NoErr;
    if (outOffset == NULL)
        BAILWITHERROR(MP4BadParamErr);
    if (self->mdat) {
        MP4MediaDataAtomPtr mdat;
        mdat = (MP4MediaDataAtomPtr)self->mdat;
        self->offset = (u32)mdat->dataSize;
    }
    *outOffset = self->offset;
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    u32 bytesToRead;
#ifndef COLDFIRE
    char debugstr[256];
#endif
    MP4DataEntryURLAtomPtr self = (MP4DataEntryURLAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;
    bytesToRead = (u32)(self->size - self->bytesRead);
    if ((s32)bytesToRead < 0)
        BAILWITHERROR(MP4BadDataErr)
    if (bytesToRead > 0) {
        self->location = (char*)MP4LocalCalloc(1, bytesToRead);
        if (self->location == NULL)
            BAILWITHERROR(MP4NoMemoryErr)
        GETBYTES(bytesToRead, location);
        self->locationLength = bytesToRead;
#ifndef COLDFIRE
        if (self->locationLength < 200) {
            sprintf(debugstr, "URL location is \"%s\"", self->location);
            DEBUG_MSG(debugstr);
        }
#endif
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateDataEntryURLAtom(MP4DataEntryURLAtomPtr* outAtom) {
    MP4Err err;
    MP4DataEntryURLAtomPtr self;

    self = (MP4DataEntryURLAtomPtr)MP4LocalCalloc(1, sizeof(MP4DataEntryURLAtom));
    if (self == NULL)
        BAILWITHERROR(MP4NoMemoryErr)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4DataEntryURLAtomType;
    self->name = "data entry URL";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->getOffset = getOffset;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
