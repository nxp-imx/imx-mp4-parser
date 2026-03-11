/*
 ***********************************************************************
 * Copyright 2023-2024, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */
//#define DEBUG

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"
#include "MP4Impl.h"

#define FOURCC(hex) hex >> 24, (hex & 0x00FF0000) >> 16, (hex & 0x0000FF00) >> 8, hex & 0xFF

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    InfoEntryAtomPtr self;
    err = MP4NoErr;
    self = (InfoEntryAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    u32 c;
    MP4Err err = MP4NoErr;
    int index = 0;
    int itemIdSize;

    InfoEntryAtomPtr self = (InfoEntryAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    if (self->version < 2)
        BAILWITHERROR(MP4InvalidMediaErr)

    itemIdSize = self->version == 2 ? 2 : 4;

    if (itemIdSize == 2) {
        GET16(itemId);
    } else {
        GET32(itemId);
    }

    GET16(protectionIndex);
    GET32(itemType);
    self->hidden = self->flags & 1;

    while (self->size > self->bytesRead) {
        GET8_V(c);
        self->itemName[index++] = (char)c;
        if (c == 0)
            break;
    }

    if (self->itemType == MP4_FOUR_CHAR_CODE('m', 'i', 'm', 'e')) {
        char tempStr[256];
        index = 0;
        while (self->size > self->bytesRead) {
            GET8_V(c);
            tempStr[index++] = c;
            if (c == 0)
                break;
        }
        if (index > 1)
            self->contentType = strdup(tempStr);

        index = 0;
        while (self->size > self->bytesRead) {
            GET8_V(c);
            tempStr[index++] = c;
            if (c == 0)
                break;
        }
        if (index > 1)
            self->contentEncoding = strdup(tempStr);

    } else if (self->itemType == MP4_FOUR_CHAR_CODE('u', 'r', 'i', ' ')) {
        char tempStr[256];
        index = 0;
        while (self->size > self->bytesRead) {
            GET8_V(c);
            tempStr[index++] = c;
            if (c == 0)
                break;
        }
        if (index > 1)
            self->uriType = strdup(tempStr);
    }

    MP4MSG("info item id %d type %c%c%c%c hidden %d\n", self->itemId, FOURCC(self->itemType),
           self->hidden);
    if (self->size > self->bytesRead) {
        u64 redundance_size = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(redundance_size);
    }

bail:
    TEST_RETURN(err);
    return err;
}

static bool isXmp(struct InfoEntryAtom* self) {
    return self->itemType == MP4_FOUR_CHAR_CODE('m', 'i', 'm', 'e') &&
           strcmp(self->contentType, "application/rdf+xml") == 0;
}

static bool isExif(struct InfoEntryAtom* self) {
    return self->itemType == MP4_FOUR_CHAR_CODE('E', 'x', 'i', 'f');
}

static bool isGrid(struct InfoEntryAtom* self) {
    return self->itemType == MP4_FOUR_CHAR_CODE('g', 'r', 'i', 'd');
}

static bool isSample(struct InfoEntryAtom* self) {
    return self->itemType == MP4_FOUR_CHAR_CODE('a', 'v', '0', '1') ||
           self->itemType == MP4_FOUR_CHAR_CODE('h', 'v', 'c', '1');
}

MP4Err MP4CreateInfoEntryAtom(InfoEntryAtomPtr* outAtom, u32 atomType) {
    MP4Err err;
    InfoEntryAtomPtr self;

    self = (InfoEntryAtomPtr)MP4LocalCalloc(1, sizeof(InfoEntryAtom));
    TESTMALLOC(self);

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = atomType;
    self->name = "Info Entry Atom";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->isXmp = isXmp;
    self->isExif = isExif;
    self->isGrid = isGrid;
    self->isSample = isSample;
    /* Default setting */
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
