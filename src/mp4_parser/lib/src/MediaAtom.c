/*
 ***********************************************************************
 * Copyright 2005-2011 by Freescale Semiconductor, Inc.
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
    $Id: MediaAtom.c,v 1.4 2001/06/05 14:06:23 rtm Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"
#include "MP4Descriptors.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4MediaAtomPtr self;
    u32 i;
    err = MP4NoErr;
    self = (MP4MediaAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    DESTROY_ATOM_LIST

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err calculateDuration(struct MP4MediaAtom* self) {
    MP4Err err;
    u64 mediaDuration;  // b31070,09/30/2011: must use u64, if u32, large duration is truncated.
    MP4MediaInformationAtomPtr minf;
    MP4MediaHeaderAtomPtr mdhd;

    err = MP4NoErr;
    minf = (MP4MediaInformationAtomPtr)self->information;
    if (minf == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);
    mdhd = (MP4MediaHeaderAtomPtr)self->mediaHeader;
    if (mdhd == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);
    err = minf->getMediaDuration(minf, &mediaDuration);
    if (err)
        goto bail;
    mdhd->duration = mediaDuration;
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err addAtom(MP4MediaAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    err = MP4NoErr;
    if (atom == NULL)
        BAILWITHERROR(MP4BadParamErr);
    err = MP4AddListEntry(atom, self->atomList);
    if (err)
        goto bail;
    switch (atom->type) {
        case MP4MediaHeaderAtomType:
            if (self->mediaHeader)
                BAILWITHERROR(MP4BadDataErr)
            self->mediaHeader = atom;
            break;

        case MP4HandlerAtomType:
            if (self->handler)
                BAILWITHERROR(MP4BadDataErr)
            self->handler = atom;
            break;

        case MP4MediaInformationAtomType:
            if (self->information)
                BAILWITHERROR(MP4BadDataErr)
            self->information = atom;
            break;
    }
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    PARSE_ATOM_LIST(MP4MediaAtom)
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateMediaAtom(MP4MediaAtomPtr* outAtom) {
    MP4Err err;
    MP4MediaAtomPtr self;

    self = (MP4MediaAtomPtr)MP4LocalCalloc(1, sizeof(MP4MediaAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4MediaAtomType;
    self->name = "media";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;
    self->calculateDuration = calculateDuration;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
