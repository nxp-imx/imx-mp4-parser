
/*
 ***********************************************************************
 * Copyright 2023-2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause

 ***********************************************************************
 */
//#define DEBUG

#include "ItemTable.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"
#include "MP4Impl.h"
#include "MP4ParserPriv.h"

extern ParserOutputBufferOps g_outputBufferOps;

#ifndef INT32_MAX
#define INT32_MAX 0x7fffffff
#endif

ImageItemPtr searchImageItem(ItemTablePtr tb, u32 imageId) {
    u32 i;
    for (i = 0; i < tb->imageCount; i++) {
        if (tb->images[i].itemId == imageId)
            return tb->images + i;
    }
    return NULL;
}

bool isGrid(struct ImageItem* pItem) {
    return pItem->type == MP4_FOUR_CHAR_CODE('g', 'r', 'i', 'd');
}

MP4Err getNextTileItemId(struct ImageItem* pItem, uint32_t* nextTileItemId, bool reset) {
    if (reset == TRUE) {
        pItem->nextTileIndex = 0;
    }
    if (pItem->nextTileIndex >= pItem->dimgRefCount) {
        return MP4EOF;
    }
    *nextTileItemId = pItem->dimgRefs[pItem->nextTileIndex++];
    return MP4NoErr;
}

void addAvailableAtom(ItemTablePtr tb, void* anAtomPtr) {
    MP4AtomPtr atom = (MP4AtomPtr)anAtomPtr;

    switch (atom->type) {
        case ItemLocationAtomType: {
            ItemLocationAtomPtr ptr = (ItemLocationAtomPtr)anAtomPtr;
            if (ptr->hasConstructMethod1)
                SET_FLAG_BIT(tb->atomRequiredFlags, IDAT_BIT);
            SET_FLAG_BIT(tb->atomAvailableFlags, ILOC_BIT);
            tb->locAtom = anAtomPtr;
            break;
        }
        case ItemInfoAtomType: {
            ItemInfoAtomPtr ptr = (ItemInfoAtomPtr)anAtomPtr;
            if (ptr->needIref)
                SET_FLAG_BIT(tb->atomRequiredFlags, IREF_BIT);
            SET_FLAG_BIT(tb->atomAvailableFlags, IINF_BIT);
            tb->infoAtom = anAtomPtr;
            break;
        }
        case ItemPropertiesAtomType:
            SET_FLAG_BIT(tb->atomAvailableFlags, IPRP_BIT);
            tb->propAtom = anAtomPtr;
            break;
        case PrimaryItemAtomType:
            SET_FLAG_BIT(tb->atomAvailableFlags, PITM_BIT);
            tb->primAtom = anAtomPtr;
            break;
        case ItemReferenceAtomType:
            SET_FLAG_BIT(tb->atomAvailableFlags, IREF_BIT);
            tb->refAtom = anAtomPtr;
            break;
        case ItemDataAtomType:
            SET_FLAG_BIT(tb->atomAvailableFlags, IDAT_BIT);
            tb->dataAtom = anAtomPtr;
            break;
        default:
            break;
    }

    MP4MSG("available %x, required %x\n", tb->atomAvailableFlags, tb->atomRequiredFlags);
    return;
}

bool conditionMatch(ItemTablePtr tb) {
    return ((~tb->atomAvailableFlags & tb->atomRequiredFlags) == 0) && (tb->valid == FALSE);
}

MP4Err buildImageItems(ItemTablePtr tb) {
    MP4Err err = MP4NoErr;
    u32 imageIndex = 0;
    u32 metaIndex = 0;
    u64 idatOffset = 0;
    u32 idatSize = 0;
    u32 refCount = 0;
    u32 i;
    bool foundPrimary;

    PrimaryItemAtomPtr primAtom = (PrimaryItemAtomPtr)tb->primAtom;
    ItemInfoAtomPtr infoAtom = (ItemInfoAtomPtr)tb->infoAtom;
    ItemLocationAtomPtr locAtom = (ItemLocationAtomPtr)tb->locAtom;
    ItemReferenceAtomPtr refAtom = (ItemReferenceAtomPtr)tb->refAtom;
    ItemPropertiesAtomPtr prop = (ItemPropertiesAtomPtr)tb->propAtom;
    ItemPropertyAssociationAtomPtr association = (ItemPropertyAssociationAtomPtr)prop->ipmaAtom;
    ItemPropertyContainerAtomPtr container = (ItemPropertyContainerAtomPtr)prop->ipcoAtom;

    ItemDataAtomPtr dataAtom = (ItemDataAtomPtr)tb->dataAtom;
    MP4MSG("buildImageItems\n");

    if (dataAtom != NULL) {
        idatOffset = dataAtom->offset;
        idatSize = dataAtom->dataSize;
    }

    // get image count and meta count
    for (i = 0; i < infoAtom->infoEntryCount; i++) {
        InfoEntryAtomPtr info;
        err = MP4GetListEntry(infoAtom->atomList, i, (char**)&info);
        if (err)
            goto bail;
        if (info->isExif(info) || info->isXmp(info))
            tb->metaCount++;
        else if (info->isGrid(info) || info->isSample(info))
            tb->imageCount++;
    }

    // create image and meta array
    if (tb->imageCount > 0) {
        tb->images = (ImageItemPtr)MP4LocalCalloc(tb->imageCount, sizeof(ImageItem));
        memset(tb->images, 0, tb->imageCount * sizeof(ImageItem));
    }
    if (tb->metaCount > 0) {
        tb->metas = (ExternalMetaItemPtr)MP4LocalCalloc(tb->metaCount, sizeof(ExternalMetaItem));
        memset(tb->metas, 0, tb->metaCount * sizeof(ExternalMetaItem));
    }

    // initialize image and meta array
    for (i = 0; i < infoAtom->infoEntryCount; i++) {
        InfoEntryAtomPtr info;
        u32 itemId;
        u64 offset;
        u32 size;
        MP4Err err;
        err = MP4GetListEntry(infoAtom->atomList, i, (char**)&info);
        if (err)
            goto bail;

        if (!info->isGrid(info) && !info->isSample(info) && !info->isExif(info) &&
            !info->isXmp(info)) {
            continue;
        }
        itemId = info->itemId;
        locAtom->getLoc(locAtom, itemId, &offset, &size, idatOffset, idatSize);

        if (info->isExif(info) || info->isXmp(info)) {
            if ((info->isExif(info) && size > 4) || (info->isXmp(info) && size > 0)) {
                tb->metas[metaIndex].isExif = info->isExif(info);
                tb->metas[metaIndex].offset = offset;
                tb->metas[metaIndex].size = size;
                tb->metas[metaIndex].itemId = itemId;
                MP4MSG("add meta[%d]: itemId %d isExif %d\n", metaIndex, itemId,
                       tb->metas[metaIndex].isExif);
                metaIndex++;
            }
        } else if (info->isGrid(info)) {
            u8 buf[12];
            s64 pos;
            if (size < 8 || size > 12) {
                MP4MSG("incorrect grid data size %d from iLoc\n", size);
                continue;
            }
            pos = tb->inputStream->getFilePos(tb->inputStream, NULL);
            tb->inputStream->seekTo(tb->inputStream, offset, SEEK_SET, NULL);
            err = tb->inputStream->readData(tb->inputStream, size, buf, NULL);
            if (err)
                goto bail;
            tb->inputStream->seekTo(tb->inputStream, pos, SEEK_SET, NULL);
            tb->images[imageIndex].rows = buf[2] + 1;
            tb->images[imageIndex].columns = buf[3] + 1;
            tb->images[imageIndex].itemId = itemId;
            tb->images[imageIndex].type = info->itemType;
            MP4MSG("adding grid: itemId %d, rows %d, columns %d\n", itemId,
                   tb->images[imageIndex].rows, tb->images[imageIndex].columns);
            imageIndex++;
        } else {
            tb->images[imageIndex].offset = offset;
            tb->images[imageIndex].size = size;
            tb->images[imageIndex].itemId = itemId;
            tb->images[imageIndex].type = info->itemType;
            MP4MSG("adding image: itemId %d\n", itemId);
            imageIndex++;
        }
    }

    tb->imageCount = imageIndex;
    tb->metaCount = metaIndex;

    // attach property to image items
    for (i = 0; i < association->entryCount; i++) {
        u32 j;
        AssociationEntryPtr assoEntry = association->entryArray + i;
        ImageItemPtr image = searchImageItem(tb, assoEntry->itemId);
        if (image == NULL)
            continue;
        for (j = 0; j < assoEntry->propCount; j++) {
            u32 propIndex = assoEntry->propIndexArray[j];
            MP4MSG("attaching property %d to itemId %d\n", propIndex, image->itemId);
            propIndex--;  // index in ipma is 1-based, convert to 0-based
            container->attachTo(container, propIndex, image);
        }
    }

    if (refAtom != NULL) {
        MP4MSG("apply references ...\n");
        err = MP4GetListEntryCount(refAtom->atomList, &refCount);
        if (err) {
            MP4MSG("MP4GetListEntryCount refAtom fail\n");
            goto bail;
        }
        MP4MSG("refCount %d\n", refCount);
        for (i = 0; i < refCount; i++) {
            ReferenceChunkAtomPtr ref;
            err = MP4GetListEntry(refAtom->atomList, i, (char**)&ref);
            if (err) {
                MP4MSG("MP4GetListEntry %d from refAtom fail\n", i);
                goto bail;
            }
            err = ref->apply(ref, tb->images, tb->imageCount, tb->metas, tb->metaCount);
            if (err) {
                MP4MSG("apply reference %d fail\n", i);
                goto bail;
            }
        }
    }

    tb->primaryItemId = primAtom->itemId;
    MP4MSG("primaryItemId %d\n", tb->primaryItemId);
    foundPrimary = FALSE;
    tb->displayables = (u32*)MP4LocalCalloc(tb->imageCount, sizeof(u32));

    // initialize displayable images
    for (i = 0; i < tb->imageCount; i++) {
        ImageItemPtr image = tb->images + i;
        bool isPrimary = image->itemId == tb->primaryItemId;
        MP4MSG("image %d id %d hidden %d\n", i, image->itemId, image->hidden);

        if (isPrimary)
            foundPrimary = TRUE;
        if (isPrimary || !image->hidden)
            tb->displayables[tb->displayCount++] = i;
    }

    if (tb->displayCount <= 0)
        BAILWITHERROR(MP4InvalidMediaErr)

    MP4MSG("found %d displayables\n", tb->displayCount);

    if (!foundPrimary)
        tb->primaryItemId = (tb->images + tb->displayables[0])->itemId;

    tb->valid = TRUE;
bail:
    TEST_RETURN(err);
    if (err) {
        MP4MSG("buildImageItems fail\n");
    }
    return err;
}

void deleteItemTable(ItemTablePtr tb) {
    u32 i;
    if (tb->images) {
        for (i = 0; i < tb->imageCount; i++) {
            ImageItemPtr image = tb->images + i;
            if (image->hvcc)
                MP4LocalFree(image->hvcc);
            if (image->icc)
                MP4LocalFree(image->icc);
            if (image->av1c)
                MP4LocalFree(image->av1c);
            if (image->thumbnails)
                MP4LocalFree(image->thumbnails);
            if (image->dimgRefs)
                MP4LocalFree(image->dimgRefs);
            if (image->exifRefs)
                MP4LocalFree(image->exifRefs);
            if (image->xmpRefs)
                MP4LocalFree(image->xmpRefs);
        }
        MP4LocalFree(tb->images);
    }

    if (tb->metas)
        MP4LocalFree(tb->metas);

    if (tb->displayables)
        MP4LocalFree(tb->displayables);

    MP4LocalFree(tb);
}

static int32 indexOfId(ImageItemPtr array, u32 count, u32 id) {
    u32 i;
    for (i = 0; i < count; i++) {
        if (array[i].itemId == id)
            return i;
    }
    return -1;
}

static int32 indexOfMetaId(ExternalMetaItemPtr array, u32 count, u32 id) {
    u32 i;
    for (i = 0; i < count; i++) {
        if (array[i].itemId == id)
            return i;
    }
    return -1;
}

bool convertCleanApertureToCrop(uint32 width, uint32 height, CleanAperture clap, Crop* pCrop) {
    float left = 0;
    float top = 0;
    float right = 0;
    float bottom = 0;

    if (clap.widthD == 0 || clap.heightD == 0 || clap.horizOffD == 0 || clap.vertOffD == 0 ||
        clap.widthN > INT32_MAX || clap.heightN > INT32_MAX || clap.horizOffN > INT32_MAX ||
        clap.vertOffN > INT32_MAX || width > INT32_MAX || height > INT32_MAX) {
        return FALSE;
    }

    float centerX = (float)width / 2;
    float centerY = (float)height / 2;
    float clapWidth = (float)clap.widthN / clap.widthD;
    float clapHeight = (float)clap.heightN / clap.heightD;
    float halfW = clapWidth / 2;
    float halfH = clapHeight / 2;
    float clapHorizOff = (float)clap.horizOffN / clap.horizOffD;
    float clapVertOff = (float)clap.vertOffN / clap.vertOffD;

    MP4MSG("centerX %f centerY %f halfW %f halfH %f\n", centerX, centerY, halfW, halfH);

    left = centerX - halfW + clapHorizOff;
    top = centerY - halfH + clapVertOff;
    right = left + clapWidth;
    bottom = top + clapHeight;

    MP4MSG("converting: left %f top %f right %f bottom %f\n", left, top, right, bottom);

    pCrop->left = (uint32)roundf(left);
    pCrop->top = (uint32)roundf(top);
    pCrop->right = (uint32)roundf(right);
    pCrop->bottom = (uint32)roundf(bottom);

    if (pCrop->right > width || pCrop->bottom > height) {
        return FALSE;
    }
    return TRUE;
}

MP4Err getDisplayImageInfo(ItemTablePtr tb, u32 displayIndex, AVStreamPtr stream) {
    MP4Err err = MP4NoErr;
    u32 imageIndex;
    ImageItemPtr image;
    ImageItemPtr tile = NULL;
    s32 tileItemIndex = -1;

    MP4MSG("getDisplayImageInfo %d\n", displayIndex);

    if (!tb->valid)
        BAILWITHERROR(MP4InvalidMediaErr)

    if (displayIndex >= tb->displayCount)
        BAILWITHERROR(MP4BadParamErr)

    imageIndex = tb->displayables[displayIndex];
    image = tb->images + imageIndex;

    if (isGrid(image)) {
        if (!image->dimgRefs)
            BAILWITHERROR(MP4InvalidMediaErr)
        stream->imageInfo.rows = image->rows;
        stream->imageInfo.columns = image->columns;

        tileItemIndex = indexOfId(tb->images, tb->imageCount, image->dimgRefs[0]);
        if (tileItemIndex < 0)
            BAILWITHERROR(MP4InvalidMediaErr)
        MP4MSG("isGrid tileItem id %d index %u\n", image->dimgRefs[0], tileItemIndex);

        tile = tb->images + tileItemIndex;
        stream->imageInfo.isGrid = TRUE;
        stream->imageInfo.tileWidth = tile->width;
        stream->imageInfo.tileHeight = tile->height;
        MP4MSG("isGrid rows %d columns %d source w/h %d/%d\n", stream->imageInfo.rows,
               stream->imageInfo.columns, tile->width, tile->height);
    }

    if (image->itemId == tb->primaryItemId) {
        stream->imageInfo.isDefault = TRUE;

        if (image->exifRefCount > 0) {
            int32 exifIndex = indexOfMetaId(tb->metas, tb->metaCount, image->exifRefs[0]);
            MP4MSG("exifIndex %d, id %d\n", exifIndex, image->exifRefs[0]);
            if (exifIndex >= 0) {
                u32 tiffOffset, exifOffset;
                ExternalMetaItemPtr meta = tb->metas + exifIndex;
                MP4MSG("exif offset %lld, size %d\n", (long long)meta->offset, meta->size);

                tb->inputStream->seekTo(tb->inputStream, meta->offset, SEEK_SET, NULL);
                err = tb->inputStream->read32(tb->inputStream, &tiffOffset, NULL);
                if (err)
                    goto bail;
                MP4MSG("read tiffOffset %lld\n", (long long)tiffOffset);
                if (meta->size <= 4 || tiffOffset > meta->size - 4) {
                    MP4MSG("exif size error\n");
                    BAILWITHERROR(MP4InvalidMediaErr)
                }
                exifOffset = tiffOffset - 2;
                stream->imageInfo.exifOffset = meta->offset + exifOffset;
                stream->imageInfo.exifSize = meta->size - exifOffset;
            }
        }

        if (image->xmpRefCount > 0) {
            int32 xmpIndex = indexOfMetaId(tb->metas, tb->metaCount, image->xmpRefs[0]);
            MP4MSG("xmpIndex %d, id %d\n", xmpIndex, image->xmpRefs[0]);
            if (xmpIndex >= 0) {
                ExternalMetaItemPtr meta = tb->metas + xmpIndex;
                MP4MSG("xmp offset %lld, size %d\n", (long long)meta->offset, meta->size);
                stream->imageInfo.xmpOffset = meta->offset;
                stream->imageInfo.xmpSize = meta->size;
            }
        }
    }

    stream->videoInfo.frameWidth = image->width;
    stream->videoInfo.frameHeight = image->height;
    stream->videoInfo.frameRotationDegree = image->rotation;

    if (image->thumbnailCount > 0) {
        ImageItemPtr thumbnail;
        u32 thumbnailId = image->thumbnails[0];
        int32 thumbnailItemIndex = indexOfId(tb->images, tb->imageCount, thumbnailId);
        if (thumbnailItemIndex < 0)
            BAILWITHERROR(MP4InvalidMediaErr)
        thumbnail = tb->images + thumbnailItemIndex;
        stream->imageInfo.thumbnailWidth = thumbnail->width;
        stream->imageInfo.thumbnailHeight = thumbnail->height;
        stream->imageInfo.thumbnailCsdSize = thumbnail->csdSize;
        stream->imageInfo.thumbnailHvcc = thumbnail->hvcc;
        stream->imageInfo.thumbnailAv1c = thumbnail->av1c;
    }

    if (isGrid(image)) {
        // point image to first tile
        if (tile)
            image = tile;
        imageIndex = tileItemIndex;
    }

    if (image->hvcc != NULL) {
        stream->imageInfo.csdSize = image->csdSize;
        stream->imageInfo.hvcc = image->hvcc;
    } else if (image->av1c != NULL) {
        stream->imageInfo.csdSize = image->csdSize;
        stream->imageInfo.av1c = image->av1c;
    }

    if (image->icc != NULL) {
        stream->imageInfo.iccSize = image->iccSize;
        stream->imageInfo.icc = image->icc;
    }

    if (image->seenClap) {
        Crop c;
        if (TRUE == convertCleanApertureToCrop(image->width, image->height, image->clap, &c))
            stream->imageInfo.crop = c;
        MP4MSG("crop: left %d top %d right %d bottom %d\n", c.left, c.top, c.right, c.bottom);
    }

    MP4MSG("w/h/r %d/%d/%d isDefault %d\n", image->width, image->height, image->rotation,
           stream->imageInfo.isDefault);
    MP4MSG("csd size %d, hvcc %p, av1c %p, iccSize %d, icc %p\n", image->csdSize, image->hvcc,
           image->av1c, image->iccSize, image->icc);

bail:
    TEST_RETURN(err);
    return err;
}

MP4Err findNextImage(ItemTablePtr tb, u32 displayIndex, u32* nextImage) {
    MP4Err err = MP4NoErr;
    u32 imageIndex;
    ImageItemPtr image;
    MP4MSG("getNextImage displayIndex %d", displayIndex);

    if (!tb->valid)
        BAILWITHERROR(MP4InvalidMediaErr)

    if (displayIndex >= tb->displayCount)
        BAILWITHERROR(MP4BadParamErr)

    imageIndex = tb->displayables[displayIndex];
    image = tb->images + imageIndex;

    // reset tile index to start reading from first one
    if (isGrid(image))
        image->nextTileIndex = 0;

    *nextImage = imageIndex;

bail:
    TEST_RETURN(err);
    return err;
}

MP4Err findThumbnailImage(ItemTablePtr tb, u32 displayIndex, u32* nextImage) {
    MP4Err err = MP4NoErr;
    u32 imageIndex;
    ImageItemPtr image;
    MP4MSG("getThumbnailImage displayIndex %d", displayIndex);

    if (!tb->valid)
        BAILWITHERROR(MP4InvalidMediaErr)

    if (displayIndex >= tb->displayCount)
        BAILWITHERROR(MP4BadParamErr)

    imageIndex = tb->displayables[displayIndex];
    image = tb->images + imageIndex;

    if (image->thumbnailCount > 0 && image->thumbnails != NULL) {
        u32 thumbnailId = image->thumbnails[0];
        s32 thumbnailIndex = indexOfId(tb->images, tb->imageCount, thumbnailId);
        if (thumbnailIndex < 0)
            BAILWITHERROR(MP4EOF)
        *nextImage = thumbnailIndex;
    }

bail:
    TEST_RETURN(err);
    return err;
}

MP4Err readImageContent(ItemTablePtr tb, AVStreamPtr stream, u32* dataSize) {
    MP4Err err = MP4NoErr;
    ImageItemPtr image;
    u32 bufferSize;
    u64 offset;
    s32 imageIndex = stream->currentReadingImageIndex;

    if (imageIndex < 0)
        BAILWITHERROR(MP4EOF)

    image = tb->images + imageIndex;

    MP4MSG("readImage index %d itemId %d\n", imageIndex, image->itemId);

    if (isGrid(image)) {
        u32 tileItemId;
        s32 tileImageIndex;
        MP4MSG("grid tileIndex %d, dimgCount %d\n", image->nextTileIndex, image->dimgRefCount);

        if (image->nextTileIndex >= image->dimgRefCount)
            BAILWITHERROR(MP4EOF)

        tileItemId = image->dimgRefs[image->nextTileIndex++];
        tileImageIndex = indexOfId(tb->images, tb->imageCount, tileItemId);
        MP4MSG("tileItemId %d tileImageIndex %d\n", tileItemId, tileImageIndex);
        if (tileImageIndex < 0)
            BAILWITHERROR(MP4EOF)

        // switch image to current tile item
        image = tb->images + tileImageIndex;

        MP4MSG("switch to tileItem, imageIndex %d itemId %d\n", tileImageIndex, image->itemId);
    } else {
        // only grid image can read content multi-times
        // other type image clean reading index at first reading, then it returns EOF at second
        // reading
        stream->currentReadingImageIndex = -1;
    }

    bufferSize = image->size;
    offset = image->offset;

    MP4MSG("offset %lld size %d", (long long)offset, bufferSize);

    stream->sampleBuffer = MP4LocalRequestBuffer(stream->streamIndex, &bufferSize,
                                                 &stream->bufferContext, stream->appContext);

    if (bufferSize < image->size)
        BAILWITHERROR(MP4NoMemoryErr)

    err = tb->inputStream->seekTo(tb->inputStream, offset, SEEK_SET, NULL);
    if (err)
        goto bail;

    err = tb->inputStream->readData(tb->inputStream, bufferSize, stream->sampleBuffer, NULL);
    if (err)
        goto bail;

    *dataSize = bufferSize;
bail:
    TEST_RETURN(err);
    return err;
}

u32 countDisplayable(ItemTablePtr tb) {
    if (!tb)
        return 0;

    return tb->valid ? tb->displayCount : 0;
}
