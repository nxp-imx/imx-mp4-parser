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

extern MP4Err MP4CreatePropertyChunkAtom(PropertyChunkAtomPtr* outAtom, u32 atomType);

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    u32 i;
    ItemPropertyContainerAtomPtr self;
    err = MP4NoErr;
    self = (ItemPropertyContainerAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    DESTROY_ATOM_LIST

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err addAtom(ItemPropertyContainerAtomPtr self, MP4AtomPtr atom) {
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

// "colr" type is duplicated with QTColorParameterAtomType, can't use PARSE_ATOM_LIST because
// it calls MP4ParseAtom, which calls MP4CreateAtom
static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    u64 beginAvail = 0;
    u32 propertyIndex = 1;
    ItemPropertyContainerAtomPtr self = (ItemPropertyContainerAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    while (self->bytesRead < self->size) {
        MP4Atom protoAtom;
        MP4AtomPtr atomProto = &protoAtom;
        u64 bytesParsed = 0L;
        u32 size;
        char typeString[8];
        MP4AtomPtr newAtom = NULL;

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
        MP4MSG("atom size is %llu\n", atomProto->size);

        /* atom type */
        err = inputStream->read32(inputStream, &atomProto->type, NULL);
        if (err)
            goto bail;
        bytesParsed += 4L;
        MP4TypeToString(atomProto->type, typeString);
        MP4MSG("property %d type %s\n", propertyIndex, typeString);

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

        // create atom even if it's unsupported property, because property index in association atom
        // is based on a list of all properties.
        err = MP4CreatePropertyChunkAtom((PropertyChunkAtomPtr*)&newAtom, atomProto->type);
        if (err)
            goto bail;
        err = newAtom->createFromInputStream(newAtom, atomProto, (char*)inputStream);
        if (err)
            goto bail;
        err = addAtom(self, newAtom);
        if (err)
            goto bail;
        self->bytesRead += atomProto->size;

        propertyIndex++;
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

static MP4Err attachTo(struct ItemPropertyContainerAtom* self, u32 propertyIndex, void* imageItem) {
    MP4Err err = MP4NoErr;
    u32 count;
    PropertyChunkAtomPtr atom;

    err = MP4GetListEntryCount(self->atomList, &count);
    if (err)
        goto bail;
    if (count <= propertyIndex)
        BAILWITHERROR(MP4BadParamErr)

    err = MP4GetListEntry(self->atomList, propertyIndex, (char**)&atom);
    if (err)
        goto bail;
    err = atom->attachTo(atom, imageItem);
    if (err)
        goto bail;

bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4CreateItemPropertyContainerAtom(ItemPropertyContainerAtomPtr* outAtom) {
    MP4Err err;
    ItemPropertyContainerAtomPtr self;

    self = (ItemPropertyContainerAtomPtr)MP4LocalCalloc(1, sizeof(ItemPropertyContainerAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = ItemPropertyContainerAtomType;
    self->name = "Item Property Container Atom";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->addAtom = addAtom;
    self->attachTo = attachTo;
    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;
    /* Default setting */
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
