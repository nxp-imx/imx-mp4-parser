/*
 ***********************************************************************
 * Copyright 2005-2011 by Freescale Semiconductor, Inc.
 * Copyright 2018, 2026 NXP
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
    $Id: SampleTableAtom.c,v 1.2 2000/05/17 08:01:31 francesc Exp $
*/

#include <stdlib.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4SampleTableAtomPtr self;
    u32 i;
    err = MP4NoErr;
    self = (MP4SampleTableAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    DESTROY_ATOM_LIST

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

#define ADDCASE(atomName)                \
    case MP4##atomName##AtomType:        \
        if (self->atomName)              \
            BAILWITHERROR(MP4BadDataErr) \
        self->atomName = atom;           \
        break

static MP4Err addAtom(MP4SampleTableAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    err = MP4NoErr;
    err = MP4AddListEntry(atom, self->atomList);
    if (err)
        goto bail;
    switch (atom->type) {
        ADDCASE(TimeToSample);
        ADDCASE(CompositionOffset);
        ADDCASE(SyncSample);
        ADDCASE(SampleDescription);
        ADDCASE(SampleSize);
        ADDCASE(CompactSampleSize);
        ADDCASE(SampleToChunk);

        case MP4ChunkLargeOffsetAtomType:
            ADDCASE(ChunkOffset);

            ADDCASE(ShadowSync);
            ADDCASE(DegradationPriority);
    }
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err calculateDuration(struct MP4SampleTableAtom* self, u64* outDuration) {
    MP4Err err;
    MP4TimeToSampleAtomPtr ctts;

    err = MP4NoErr;
    if (outDuration == NULL)
        BAILWITHERROR(MP4BadParamErr)

    ctts = (MP4TimeToSampleAtomPtr)self->TimeToSample;
    if (ctts == NULL)
        BAILWITHERROR(MP4InvalidMediaErr)
    err = ctts->getTotalDuration(ctts, outDuration);
    if (err)
        goto bail;

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err setSampleEntry(struct MP4SampleTableAtom* self, MP4AtomPtr entry) {
    MP4Err err;
    MP4SampleDescriptionAtomPtr stsd;

    err = MP4NoErr;
    if (entry == NULL)
        BAILWITHERROR(MP4BadParamErr)

    stsd = (MP4SampleDescriptionAtomPtr)self->SampleDescription;
    if (stsd == NULL)
        BAILWITHERROR(MP4InvalidMediaErr)
    err = stsd->addEntry(stsd, entry);
    if (err)
        goto bail;
    self->currentSampleEntry = entry;
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err getCurrentDataReferenceIndex(struct MP4SampleTableAtom* self,
                                           u32* outDataReferenceIndex) {
    GenericSampleEntryAtomPtr entry;
    MP4Err err;

    err = MP4NoErr;
    entry = (GenericSampleEntryAtomPtr)self->currentSampleEntry;
    if (entry == NULL)
        BAILWITHERROR(MP4InvalidMediaErr)
    *outDataReferenceIndex = entry->dataReferenceIndex;
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    PARSE_ATOM_LIST(MP4SampleTableAtom)
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateSampleTableAtom(MP4SampleTableAtomPtr* outAtom) {
    MP4Err err;
    MP4SampleTableAtomPtr self;

    self = (MP4SampleTableAtomPtr)MP4LocalCalloc(1, sizeof(MP4SampleTableAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4SampleTableAtomType;
    self->name = "sample table";
    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->calculateDuration = calculateDuration;
    self->setSampleEntry = setSampleEntry;
    self->getCurrentDataReferenceIndex = getCurrentDataReferenceIndex;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
