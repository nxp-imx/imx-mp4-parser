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
    $Id: MJ2ImageHeaderAtom.c,v 1.0 2000/10/05 14:20:00 tm Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "MJ2Atoms.h"

static void destroy(MP4AtomPtr s) {
    ISOErr err = ISONoErr;
    MJ2ImageHeaderAtomPtr self = (MJ2ImageHeaderAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(ISOBadParamErr)

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static ISOErr createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    ISOErr err;
    MJ2ImageHeaderAtomPtr self = (MJ2ImageHeaderAtomPtr)s;

    err = ISONoErr;
    if (self == NULL)
        BAILWITHERROR(ISOBadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32(height);
    GET32(width);
    GET16(compCount);
    GET8(compBits);
    GET8(compressionType);
    GET8(colorspaceKnown);
    GET8(ip);

bail:
    TEST_RETURN(err);

    return err;
}

ISOErr MJ2CreateImageHeaderAtom(MJ2ImageHeaderAtomPtr* outAtom) {
    ISOErr err;
    MJ2ImageHeaderAtomPtr self;

    self = (MJ2ImageHeaderAtomPtr)MP4LocalCalloc(1, sizeof(MJ2ImageHeaderAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->type = MJ2ImageHeaderAtomType;
    self->name = "JPEG 2000 image header atom";
    self->destroy = destroy;
    self->createFromInputStream = (cisfunc)createFromInputStream;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
