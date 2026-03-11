/*
 ***********************************************************************
 * Copyright 2023-2024, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#ifndef ITEM_TABLE_H
#define ITEM_TABLE_H

#include "MP4InputStream.h"

typedef struct CleanAperture {
    u32 widthN;
    u32 widthD;
    u32 heightN;
    u32 heightD;
    u32 horizOffN;
    u32 horizOffD;
    u32 vertOffN;
    u32 vertOffD;
} CleanAperture;

typedef struct ImageItem {
    u32 type;
    u32 itemId;
    bool hidden;

    u32 rows;
    u32 columns;
    u32 width;
    u32 height;
    s32 rotation;
    u64 offset;
    u32 size;

    u32 csdSize;
    u8* hvcc;
    u8* av1c;
    u32 iccSize;
    u8* icc;
    bool seenClap;
    CleanAperture clap;

    bool isThumbnail;  // this image is thumbnail of another image

    u32 thumbnailCount;
    u32* thumbnails;

    u32 dimgRefCount;
    u32* dimgRefs;

    u32 exifRefCount;
    u32* exifRefs;

    u32 xmpRefCount;
    u32* xmpRefs;

    u32 nextTileIndex;
} ImageItem, *ImageItemPtr;

typedef struct ExternalMetaItem {
    u32 itemId;
    u64 offset;
    u32 size;
    bool isExif;
} ExternalMetaItem, *ExternalMetaItemPtr;

bool isGrid(struct ImageItem* pItem);
MP4Err getNextTileItemId(struct ImageItem* pItem, u32* nextTileItemId, bool reset);

#define IPRP_BIT (1 << 0)
#define ILOC_BIT (1 << 1)
#define PITM_BIT (1 << 2)
#define IINF_BIT (1 << 3)
#define IDAT_BIT (1 << 4)
#define IREF_BIT (1 << 5)
#define SET_FLAG_BIT(FLAG, BIT) FLAG |= BIT
#define CLEAR_FLAG_BIT(FLAG, BIT) FLAG &= ~BIT
#define DEFAULT_REQUIRED IPRP_BIT | ILOC_BIT | PITM_BIT | IINF_BIT

typedef struct ItemTable {
    MP4InputStreamPtr inputStream;

    ImageItemPtr images;
    u32 imageCount;

    ExternalMetaItemPtr metas;
    u32 metaCount;

    u32* displayables;  // store indexes in images array
    u32 displayCount;

    bool valid;
    u32 atomAvailableFlags;
    u32 atomRequiredFlags;
    u32 primaryItemId;

    void* infoAtom;
    void* locAtom;
    void* propAtom;
    void* primAtom;
    void* refAtom;
    void* dataAtom;
} ItemTable, *ItemTablePtr;

void addAvailableAtom(ItemTablePtr itemTable, void* anAtomPtr);
bool conditionMatch(ItemTablePtr itemTable);
MP4Err buildImageItems(ItemTablePtr itemTable);
void deleteItemTable(ItemTablePtr itemTable);
u32 countDisplayable(ItemTablePtr itemTable);
MP4Err getDisplayImageInfo(ItemTablePtr tb, u32 displayIndex, AVStreamPtr stream);
MP4Err findNextImage(ItemTablePtr tb, u32 displayIndex, u32* nextImage);
MP4Err findThumbnailImage(ItemTablePtr tb, u32 displayIndex, u32* nextImage);
MP4Err readImageContent(ItemTablePtr tb, AVStreamPtr stream, u32* dataSize);

#endif
