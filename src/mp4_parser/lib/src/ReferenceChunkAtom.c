/*
 ***********************************************************************
 * Copyright 2023, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */
//#define DEBUG

#include <stdlib.h>
#include <string.h>
#include "ItemTable.h"
#include "MP4Atoms.h"
#include "MP4Impl.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    ReferenceChunkAtomPtr self;
    err = MP4NoErr;
    self = (ReferenceChunkAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->referenceItemArray) {
        MP4LocalFree(self->referenceItemArray);
        self->referenceItemArray = NULL;
    }

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static void setItemIdSize(ReferenceChunkAtomPtr self, u32 size) {
    self->itemIdSize = size;
}

static MP4Err apply(ReferenceChunkAtomPtr self, void* images, u32 imageCount, void* metas,
                    u32 metaCount) {
    u32 i, j;
    MP4Err err = MP4NoErr;
    ImageItemPtr imageArray = (ImageItemPtr)images;
    ExternalMetaItemPtr metaArray = (ExternalMetaItemPtr)metas;

    switch (self->type) {
        case DerivedImageAtomType: {
            if (imageArray == NULL)
                BAILWITHERROR(MP4InvalidMediaErr)

            for (i = 0; i < imageCount; i++) {
                if (self->itemId == imageArray[i].itemId)
                    break;
            }
            // ignore non-image items
            if (i >= imageCount)
                BAILWITHERROR(MP4NoErr)

            MP4MSG("apply reference dimg to itemId %d\n", self->itemId);
            if (imageArray[i].dimgRefs != NULL)
                MP4LocalFree(imageArray[i].dimgRefs);
            // copy source image id from atom to ImageItem
            if (self->referenceItemCount > 0) {
                imageArray[i].dimgRefs =
                        (u32*)MP4LocalCalloc(self->referenceItemCount, sizeof(u32));
                memcpy(imageArray[i].dimgRefs, self->referenceItemArray,
                       self->referenceItemCount * sizeof(u32));
                imageArray[i].dimgRefCount = self->referenceItemCount;
            }
            // set source images to hidden
            for (i = 0; i < self->referenceItemCount; i++) {
                for (j = 0; j < imageCount; j++) {
                    if (self->referenceItemArray[i] == imageArray[j].itemId) {
                        imageArray[j].hidden = TRUE;
                        break;
                    }
                }
            }

            break;
        }
        case ThumbnailAtomType: {
            if (imageArray == NULL)
                BAILWITHERROR(MP4InvalidMediaErr)

            // set thumbnail image to hidden
            for (i = 0; i < imageCount; i++) {
                if (self->itemId == imageArray[i].itemId) {
                    imageArray[i].hidden = TRUE;
                    imageArray[i].isThumbnail = TRUE;
                    break;
                }
            }
            if (i >= imageCount)
                BAILWITHERROR(MP4InvalidMediaErr)

            MP4MSG("apply reference thmb to itemId %d\n", self->itemId);

            // copy thumbnail image id to source image ref
            for (i = 0; i < self->referenceItemCount; i++) {
                for (j = 0; j < imageCount; j++) {
                    if (self->referenceItemArray[i] == imageArray[j].itemId) {
                        if (imageArray[j].thumbnailCount > 0) {
                            imageArray[j].thumbnails = MP4LocalReAlloc(
                                    imageArray[j].thumbnails,
                                    (imageArray[j].thumbnailCount + 1) * sizeof(u32));
                        } else
                            imageArray[j].thumbnails = MP4LocalCalloc(1, sizeof(u32));

                        imageArray[j].thumbnails[imageArray[j].thumbnailCount] = self->itemId;
                        imageArray[j].thumbnailCount++;
                        break;
                    }
                }
            }

            break;
        }
        case ContentDescribsAtomType: {
            u32 metaIndex = 0;
            if (metaArray == NULL || imageArray == NULL)
                BAILWITHERROR(MP4NoErr)

            for (i = 0; i < metaCount; i++) {
                if (self->itemId == metaArray[i].itemId) {
                    metaIndex = i;
                    break;
                }
            }
            // ignore non-meta items
            if (i >= metaCount)
                BAILWITHERROR(MP4NoErr)

            MP4MSG("apply reference cdsc for self itemId %d, self ref Item Count %d\n",
                   self->itemId, self->referenceItemCount);

            // copy cdsc item id to source image
            for (i = 0; i < self->referenceItemCount; i++) {
                for (j = 0; j < imageCount; j++) {
                    if (self->referenceItemArray[i] == imageArray[j].itemId) {
                        u32** target = metaArray[metaIndex].isExif ? &imageArray[j].exifRefs
                                                                   : &imageArray[j].xmpRefs;
                        u32* count = metaArray[metaIndex].isExif ? &imageArray[j].exifRefCount
                                                                 : &imageArray[j].xmpRefCount;
                        if (*count > 0)
                            *target = MP4LocalReAlloc(*target, (*count + 1) * sizeof(u32));
                        else
                            *target = MP4LocalCalloc(1, sizeof(u32));

                        *target[*count] = self->itemId;
                        (*count)++;
                        break;
                    }
                }
            }
            break;
        }
        case AuxiliaryAtomType: {
            if (imageArray == NULL)
                BAILWITHERROR(MP4InvalidMediaErr)

            // set auxiliary image to hidden
            for (i = 0; i < imageCount; i++) {
                if (self->itemId == imageArray[i].itemId) {
                    MP4MSG("apply reference auxl to itemId %d\n", self->itemId);
                    imageArray[i].hidden = TRUE;
                    break;
                }
            }
            if (i >= imageCount)
                BAILWITHERROR(MP4InvalidMediaErr)
            break;
        }
        default:
            break;
    }

bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;

    ReferenceChunkAtomPtr self = (ReferenceChunkAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    if (self->itemIdSize == 2) {
        GET16(itemId);
    } else {
        GET32(itemId);
    }

    GET16(referenceItemCount);

    if (self->referenceItemCount > 0) {
        u32 i;
        self->referenceItemArray = (u32*)MP4LocalCalloc(self->referenceItemCount, sizeof(u32));
        for (i = 0; i < self->referenceItemCount; i++) {
            u32 id;
            if (self->itemIdSize == 2) {
                GET16_V(id);
            } else {
                GET32_V(id);
            }
            self->referenceItemArray[i] = id;
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

MP4Err MP4CreateReferenceChunkAtom(ReferenceChunkAtomPtr* outAtom, u32 atomType) {
    MP4Err err;
    ReferenceChunkAtomPtr self;

    self = (ReferenceChunkAtomPtr)MP4LocalCalloc(1, sizeof(ReferenceChunkAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = atomType;
    self->name = "Reference Chunk Atom";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->setItemIdSize = setItemIdSize;
    self->apply = apply;
    /* Default setting */
    self->itemIdSize = 2;
    self->referenceItemCount = 0;
    self->referenceItemArray = 0;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
