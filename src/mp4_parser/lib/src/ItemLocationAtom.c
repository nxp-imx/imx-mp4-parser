/*
 ***********************************************************************
 * Copyright 2023, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    ItemLocationAtomPtr self;
    err = MP4NoErr;
    self = (ItemLocationAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->itemLocationArray) {
        MP4LocalFree(self->itemLocationArray);
        self->itemLocationArray = NULL;
    }

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err getLoc(ItemLocationAtomPtr self, u32 itemId, u64* offset, u32* size, u64 idatOffset,
                     u32 idatSize) {
    MP4Err err = MP4NoErr;
    ItemLocationEntryPtr loc = NULL;
    u32 i;

    for (i = 0; i < self->itemLocationCount; i++) {
        if (itemId == self->itemLocationArray[i].itemId) {
            loc = self->itemLocationArray + i;
            break;
        }
    }

    if (loc == NULL)
        BAILWITHERROR(MP4InvalidMediaErr)

    if (loc->extentCount != 1)
        BAILWITHERROR(MP4InvalidMediaErr)
    if (loc->constructionMethod == 0) {
        *offset = loc->baseOffset + loc->extent.extentOffset;
        *size = loc->extent.extentLength;
        MP4MSG("getLoc method 0 offset %lld (base %lld, extent %lld)->\n", *offset, loc->baseOffset,
               loc->extent.extentOffset);
    } else if (loc->constructionMethod == 1) {
        if (loc->baseOffset + loc->extent.extentOffset + loc->extent.extentLength > idatSize)
            BAILWITHERROR(MP4InvalidMediaErr)
        *offset = loc->baseOffset + loc->extent.extentOffset + idatOffset;
        *size = loc->extent.extentLength;
        MP4MSG("getLoc method 1 offset %lld (base %lld, extent %lld, idat %lld)->\n", *offset,
               loc->baseOffset, loc->extent.extentOffset, idatOffset);
    }
bail:
    TEST_RETURN(err);

    return err;
}

static bool validSize(u8 size) {
    return size == 0 || size == 4 || size == 8;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    u32 itemFieldSize = 0;
    u32 offsetSize = 0;
    u32 lengthSize = 0;
    u32 baseOffsetSize = 0;
    u32 indexSize = 0;
    u32 i = 0;
    u32 buf;

    ItemLocationAtomPtr self = (ItemLocationAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    if (self->version > 2)
        BAILWITHERROR(MP4InvalidMediaErr)

    itemFieldSize = self->version < 2 ? 2 : 4;

    if (self->size - self->bytesRead < 2)
        BAILWITHERROR(MP4InvalidMediaErr)

    GET8_V(offsetSize);
    lengthSize = offsetSize & 0xF;
    offsetSize >>= 4;

    GET8_V(baseOffsetSize);
    if (self->version == 1 || self->version == 2)
        indexSize = baseOffsetSize & 0xF;

    baseOffsetSize >>= 4;

    if (!validSize(offsetSize) || !validSize(lengthSize) || !validSize(baseOffsetSize) ||
        !validSize((indexSize)))
        BAILWITHERROR(MP4InvalidMediaErr)

    if (itemFieldSize == 2) {
        GET16(itemLocationCount);
    } else {
        GET32(itemLocationCount);
    }

    if (self->itemLocationCount > 0) {
        self->itemLocationArray = (ItemLocationEntryPtr)MP4LocalCalloc(self->itemLocationCount,
                                                                       sizeof(ItemLocationEntry));
        memset(self->itemLocationArray, 0, self->itemLocationCount * sizeof(ItemLocationEntry));
    }

    for (i = 0; i < self->itemLocationCount; i++) {
        ItemLocationEntryPtr entry = self->itemLocationArray + i;
        if (itemFieldSize == 2) {
            GET16_V(entry->itemId);
        } else {
            GET32_V(entry->itemId);
        }

        if (self->version == 1 || self->version == 2) {
            GET16_V(buf);
            entry->constructionMethod = buf & 0xF;
            if (entry->constructionMethod == 1)
                self->hasConstructMethod1 = TRUE;
        }
        GET16_V(entry->dataReferenceIndex);
        if (entry->dataReferenceIndex != 0)
            BAILWITHERROR(MP4InvalidMediaErr)

        if (baseOffsetSize == 4) {
            GET32_V(entry->baseOffset);
        } else if (baseOffsetSize == 8) {
            GET64_V(entry->baseOffset);
        }

        GET16_V(entry->extentCount);
        if (entry->extentCount > 1)
            BAILWITHERROR(MP4InvalidMediaErr)

        if (entry->extentCount == 1) {
            if (self->version == 1 || self->version == 2) {
                if (indexSize == 4) {
                    GET32_V(entry->extent.extentIndex);
                } else if (indexSize == 8) {
                    GET64_V(entry->extent.extentIndex);
                }
            }

            if (offsetSize == 4) {
                GET32_V(entry->extent.extentOffset);
            } else if (offsetSize == 8) {
                GET64_V(entry->extent.extentOffset);
            }

            if (lengthSize == 4) {
                GET32_V(entry->extent.extentLength);
            } else if (lengthSize == 8) {
                GET64_V(entry->extent.extentLength);
            }
        }
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

MP4Err MP4CreateItemLocationAtom(ItemLocationAtomPtr* outAtom) {
    MP4Err err;
    ItemLocationAtomPtr self;

    self = (ItemLocationAtomPtr)MP4LocalCalloc(1, sizeof(ItemLocationAtom));
    TESTMALLOC(self);

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = ItemLocationAtomType;
    self->name = "Item Location Atom";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->getLoc = getLoc;
    /* Default setting */
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
