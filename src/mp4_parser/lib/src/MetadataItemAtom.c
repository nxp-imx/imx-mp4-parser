/*
 ***********************************************************************
 * Copyright (c) 2005-2013, Freescale Semiconductor, Inc.
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
    $Id: MetadataItemAtom.c,v 1.1 2010/12/08 09:28:29 francesc Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4MetadataItemAtomPtr self;
    u32 i;
    err = MP4NoErr;
    self = (MP4MetadataItemAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    DESTROY_ATOM_LIST

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err addAtom(MP4MetadataItemAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    err = MP4NoErr;
    if (atom == NULL)
        BAILWITHERROR(MP4BadParamErr);
    err = MP4AddListEntry(atom, self->atomList);
    if (err)
        goto bail;
    switch (atom->type) {
        case QTItemInformationAtomType:
            if (NULL == self->itemInfo)
                self->itemInfo = atom;
            break;

        case QTNameAtomType:
            if (NULL == self->dataName)
                self->dataName = atom;
            break;

        case QTValueAtomType:
            if (NULL == self->data)
                self->data = atom;
            break;
        case MP4MeanAtomType:
            if (NULL == self->mean)
                self->mean = atom;
            break;
    }
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    PARSE_ATOM_LIST(MP4MetadataItemAtom)
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateMetadataItemAtom(MP4MetadataItemAtomPtr* outAtom) {
    MP4Err err;
    MP4MetadataItemAtomPtr self;

    self = (MP4MetadataItemAtomPtr)MP4LocalCalloc(1, sizeof(MP4MetadataItemAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->name = "metadata item";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
