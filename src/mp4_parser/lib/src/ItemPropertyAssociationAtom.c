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

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    ItemPropertyAssociationAtomPtr self;
    err = MP4NoErr;
    self = (ItemPropertyAssociationAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->entryArray) {
        MP4LocalFree(self->entryArray);
        self->entryArray = NULL;
    }

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    u32 index;
    u32 itemIdSize;
    u32 propIndexSize;
    u32 bitmask;

    ItemPropertyAssociationAtomPtr self = (ItemPropertyAssociationAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32(entryCount);
    itemIdSize = self->version == 0 ? 2 : 4;
    propIndexSize = self->flags & 1 ? 2 : 1;
    bitmask = (1 << (8 * propIndexSize - 1));

    MP4MSG("association entry count %d\n", self->entryCount);
    if (self->entryCount > 0) {
        self->entryArray =
                (AssociationEntryPtr)MP4LocalCalloc(self->entryCount, sizeof(AssociationEntry));
        memset(self->entryArray, 0, self->entryCount * sizeof(AssociationEntry));
        for (index = 0; index < self->entryCount; index++) {
            u32 i;
            AssociationEntryPtr entry = self->entryArray + index;

            if (itemIdSize == 2) {
                GET16(entryArray[index].itemId);
            } else {
                GET32(entryArray[index].itemId);
            }
            GET8(entryArray[index].propCount);
            if (entry->propCount > 0) {
                entry->propIndexArray = (u32*)MP4LocalCalloc(entry->propCount, sizeof(u32));
                memset(entry->propIndexArray, 0, entry->propCount * sizeof(u32));
                for (i = 0; i < entry->propCount; i++) {
                    u32 propIndex;
                    if (propIndexSize == 2) {
                        GET16_V(propIndex);
                    } else {
                        GET8_V(propIndex);
                    }
                    entry->propIndexArray[i] = propIndex & ~bitmask;
                    MP4MSG("association itemId %d prop %d\n", self->entryArray[index].itemId,
                           entry->propIndexArray[i]);
                }
            }
        }
    }

    if (self->size > self->bytesRead) {
        u64 redundance_size = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(redundance_size);
    }

bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4CreateItemPropertyAssociationAtom(ItemPropertyAssociationAtomPtr* outAtom) {
    MP4Err err;
    ItemPropertyAssociationAtomPtr self;

    self = (ItemPropertyAssociationAtomPtr)MP4LocalCalloc(1, sizeof(ItemPropertyAssociationAtom));
    TESTMALLOC(self);

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = ItemReferenceAtomType;
    self->name = "Item Property Association Atom";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    /* Default setting */
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
