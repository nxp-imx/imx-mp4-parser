/*
 ***********************************************************************
 * Copyright 2023, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */
//#define DEBUG

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"
#include "MP4Impl.h"

extern MP4Err MP4CreateReferenceChunkAtom(ReferenceChunkAtomPtr* outAtom, u32 atomType);

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    u32 i;
    ItemReferenceAtomPtr self;
    err = MP4NoErr;
    self = (ItemReferenceAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    DESTROY_ATOM_LIST

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err addAtom(ItemReferenceAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    err = MP4NoErr;
    assert(atom);
    err = MP4AddListEntry(atom, self->atomList);
    if (err)
        goto bail;

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    u64 beginAvail = 0;
    u32 itemIdSize;

    ItemReferenceAtomPtr self = (ItemReferenceAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    itemIdSize = self->version == 0 ? 2 : 4;

    while (self->bytesRead < self->size) {
        MP4Atom protoAtom;
        MP4AtomPtr atomProto = &protoAtom;
        u64 bytesParsed = 0L;
        u32 size;
        char typeString[8];
        MP4AtomPtr newAtom = NULL;
        ReferenceChunkAtomPtr chunk;

        beginAvail = inputStream->available;

        if (beginAvail > 0 && beginAvail < 4)
            BAILWITHERROR(MP4EOF)
        err = MP4CreateBaseAtom(atomProto);
        if (err)
            goto bail;

        /* atom size */
        err = inputStream->read32(inputStream, &size, NULL);
        if (err)
            goto bail;
        atomProto->size = size;
        if (atomProto->size == 0)
            BAILWITHERROR(MP4BadDataErr)
        bytesParsed += 4L;
        MP4MSG("reference atom size is %llu\n", atomProto->size);

        /* atom type */
        err = inputStream->read32(inputStream, &atomProto->type, NULL);
        if (err)
            goto bail;
        bytesParsed += 4L;
        MP4TypeToString(atomProto->type, typeString);
        MP4MSG("reference atom type is %c%c%c%c\n", typeString[0], typeString[1], typeString[2],
               typeString[3]);

        /* large atom */
        if (atomProto->size == 1) {
            err = inputStream->read32(inputStream, &size, NULL);
            if (err)
                goto bail;
            atomProto->size64 = size;
            atomProto->size64 <<= 32;
            err = inputStream->read32(inputStream, &size, NULL);
            if (err)
                goto bail;
            atomProto->size64 |= size;
            atomProto->size = atomProto->size64; /* "size" is still right for large atoms */
            bytesParsed += 8L;
        }

        atomProto->bytesRead = bytesParsed;
        if ((s64)(atomProto->size - bytesParsed) < 0)
            BAILWITHERROR(MP4BadDataErr)

        if (atomProto->type != DerivedImageAtomType && atomProto->type != ThumbnailAtomType &&
            atomProto->type != ContentDescribsAtomType && atomProto->type != AuxiliaryAtomType)
            BAILWITHERROR(MP4BadDataErr)

        err = MP4CreateReferenceChunkAtom((ReferenceChunkAtomPtr*)&newAtom, atomProto->type);
        if (err)
            goto bail;

        chunk = (ReferenceChunkAtomPtr)newAtom;
        chunk->setItemIdSize(chunk, itemIdSize);

        err = chunk->createFromInputStream(newAtom, atomProto, (char*)inputStream);
        if (err)
            goto bail;
        MP4MSG("reference atom id is %u\n", ((ReferenceChunkAtomPtr)newAtom)->itemId);

        self->bytesRead += atomProto->size;
        err = addAtom(self, (MP4AtomPtr)chunk);
        if (err)
            goto bail;
    }

    if (self->bytesRead > self->size) {
        /* ERROR CONCEALMENT: wrong atom size, roll back */
        u32 rollbackBytes = (u32)(self->bytesRead - self->size);
        inputStream->file_offset -= rollbackBytes;
        inputStream->available += rollbackBytes;
        self->bytesRead -= rollbackBytes;
    }

    if (self->size > self->bytesRead) {
        u64 redundance_size = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(redundance_size);
    }

bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4CreateItemReferenceAtom(ItemReferenceAtomPtr* outAtom) {
    MP4Err err;
    ItemReferenceAtomPtr self;

    self = (ItemReferenceAtomPtr)MP4LocalCalloc(1, sizeof(ItemReferenceAtom));
    TESTMALLOC(self);

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = ItemReferenceAtomType;
    self->name = "Item Reference Atom";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->addAtom = addAtom;
    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;
    /* Default setting */
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
