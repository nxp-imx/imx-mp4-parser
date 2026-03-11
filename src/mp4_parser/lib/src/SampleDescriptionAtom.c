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
    $Id: SampleDescriptionAtom.c,v 1.2 2000/05/17 08:01:30 francesc Exp $
*/

#include <stdlib.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4SampleDescriptionAtomPtr self;
    u32 i;

    err = MP4NoErr;
    self = (MP4SampleDescriptionAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    DESTROY_ATOM_LIST

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

IMPLEMENT_NEW_ADDATOM(MP4SampleDescriptionAtom)

static MP4Err addEntry(struct MP4SampleDescriptionAtom* self, MP4AtomPtr entry) {
    MP4Err err;

    err = addAtom(self, entry);

    TEST_RETURN(err);

    return err;
}

static u32 getEntryCount(struct MP4SampleDescriptionAtom* self) {
    MP4Err err = MP4NoErr;
    u32 entryCount = 0;

    err = MP4GetListEntryCount(self->atomList, &entryCount);
    if (err)
        entryCount = 0;

    return entryCount;
}

static MP4Err getEntry(struct MP4SampleDescriptionAtom* self, u32 entryNumber,
                       struct GenericSampleEntryAtom** outEntry) {
    MP4Err err;
    u32 entryCount;

    err = MP4NoErr;
    entryCount = getEntryCount(self);
    if ((entryNumber == 0) || (entryNumber > entryCount))
        BAILWITHERROR(MP4BadParamErr)
    err = MP4GetListEntry(self->atomList, entryNumber - 1, (char**)outEntry);
    if (err)
        goto bail;
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err MP4ParseAtomUsingProtoList(MP4InputStreamPtr inputStream, u32 * protoList,
                                      u32 defaultAtom, MP4AtomPtr * outAtom);
    extern u32 MP4SampleEntryProtos[];
    MP4Err err;
    u32 entryCount;
    u32 i;
    MP4SampleDescriptionAtomPtr self = (MP4SampleDescriptionAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32_V(entryCount);
    for (i = 0; i < entryCount; i++) {
        MP4AtomPtr atom;
#if 0
		err = MP4ParseAtom( inputStream, &atom ); if (err) goto bail;
#else
        err = MP4ParseAtomUsingProtoList(inputStream, MP4SampleEntryProtos,
                                         MP4GenericSampleEntryAtomType, &atom);
        if (err)
            goto bail;
#endif
        self->bytesRead += atom->size;
        err = addAtom(self, atom);
        if (err)
            goto bail;
    }
    if (self->bytesRead != self->size)
        BAILWITHERROR(MP4BadDataErr)
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateSampleDescriptionAtom(MP4SampleDescriptionAtomPtr* outAtom) {
    MP4Err err;
    MP4SampleDescriptionAtomPtr self;

    self = (MP4SampleDescriptionAtomPtr)MP4LocalCalloc(1, sizeof(MP4SampleDescriptionAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;
    self->type = MP4SampleDescriptionAtomType;
    self->name = "sample description";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->addEntry = addEntry;
    self->getEntryCount = getEntryCount;
    self->getEntry = getEntry;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
