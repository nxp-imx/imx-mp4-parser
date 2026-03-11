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
    $Id: HandlerAtom.c,v 1.2 2000/05/17 08:01:28 francesc Exp $
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4HandlerAtomPtr self = (MP4HandlerAtomPtr)s;
    err = MP4NoErr;
    if (s == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->nameUTF8) {
        MP4LocalFree(self->nameUTF8);
        self->nameUTF8 = NULL;
    }
    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    u32 bytesLeft;
#ifndef COLDFIRE
    char debugmsg[256];
#endif
    char htype[8];
    MP4HandlerAtomPtr self = (MP4HandlerAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32(qt_componentType);
    GET32(handlerType);
    MP4TypeToString(self->handlerType, htype);
#ifndef COLDFIRE
    sprintf(debugmsg, "handler type is '%s'", htype);
    DEBUG_MSG(debugmsg);
#endif
    GET32(qt_componentManufacturer);
    GET32(qt_componentFlags);
    GET32(qt_componentFlagsMask);

    bytesLeft = (u32)(self->size - self->bytesRead);
    if ((s32)bytesLeft < 0)
        BAILWITHERROR(MP4BadDataErr)
    if (bytesLeft > 0) {
        self->nameUTF8 = (char*)MP4LocalCalloc(1, bytesLeft);
        TESTMALLOC(self->nameUTF8);
        GETBYTES_MSG(bytesLeft, nameUTF8, "handler name");
    }
    self->nameLength = bytesLeft;
#ifndef COLDFIRE
    if (self->nameLength > 0) {
        // sprintf( debugmsg, "handler name is '%s'", self->nameUTF8 );
        DEBUG_MSG(debugmsg);
    }
#endif
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateHandlerAtom(MP4HandlerAtomPtr* outAtom) {
    MP4Err err;
    MP4HandlerAtomPtr self;

    self = (MP4HandlerAtomPtr)MP4LocalCalloc(1, sizeof(MP4HandlerAtom));
    TESTMALLOC(self);

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4HandlerAtomType;
    self->name = "handler reference";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
