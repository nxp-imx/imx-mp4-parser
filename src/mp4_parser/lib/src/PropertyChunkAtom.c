/*
 ***********************************************************************
 * Copyright 2023-2024, 2026 NXP
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
    PropertyChunkAtomPtr self;
    err = MP4NoErr;
    self = (PropertyChunkAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->data)
        MP4LocalFree(self->data);

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err attachTo(PropertyChunkAtomPtr self, void* imageItem) {
    MP4Err err = MP4NoErr;
    char typeString[8];
    ImageItemPtr image = (ImageItemPtr)imageItem;
    if (image == NULL)
        BAILWITHERROR(MP4BadParamErr)

    switch (self->type) {
        case HEVCConfigurationAtomType:
            image->hvcc = (u8*)MP4LocalMalloc(self->dataSize);
            memcpy(image->hvcc, self->data, self->dataSize);
            image->csdSize = self->dataSize;
            MP4MSG(" <- hvcc size %d\n", image->csdSize);
            break;
        case MP4Av1CAtomType:
            image->av1c = (u8*)MP4LocalMalloc(self->dataSize);
            memcpy(image->av1c, self->data, self->dataSize);
            image->csdSize = self->dataSize;
            MP4MSG(" <- av1c size %d\n", image->csdSize);
            break;
        case ImageSpatialExtentsAtomType:
            image->width = self->width;
            image->height = self->height;
            MP4MSG(" <- ispe %dx%d\n", image->width, image->height);
            break;
        case ImageRotationAtomType:
            image->rotation = self->rotation * 90;
            MP4MSG(" <- irot %d\n", image->rotation);
            break;
        case CleanApertureAtomType:
            if (self->seenClap) {
                image->seenClap = TRUE;
                image->clap.widthN = self->clapWidthN;
                image->clap.widthD = self->clapWidthD;
                image->clap.heightN = self->clapHeightN;
                image->clap.heightD = self->clapHeightD;
                image->clap.horizOffN = self->clapHorizOffN;
                image->clap.horizOffD = self->clapHorizOffD;
                image->clap.vertOffN = self->clapVertOffN;
                image->clap.vertOffD = self->clapVertOffD;
                MP4MSG(" <- clap w %d/%d h %d/%d hOff %d/%d vOff %d/%d\n", image->clap.widthN,
                       image->clap.widthD, image->clap.heightN, image->clap.heightD,
                       image->clap.horizOffN, image->clap.horizOffD, image->clap.vertOffN,
                       image->clap.vertOffD);
            }
            break;
        case QTColorParameterAtomType:
            if (self->dataSize > 0) {
                image->icc = (u8*)MP4LocalMalloc(self->dataSize);
                memcpy(image->icc, self->data, self->dataSize);
            }
            image->iccSize = self->dataSize;
            MP4MSG(" <- icc size %d\n", image->iccSize);
            break;
        default:
            MP4TypeToString(self->type, typeString);
            MP4MSG(" <- %s unsupported\n", typeString);
            break;
    }
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    u32 val;
    u32 clapValues[8];
    int i;

    PropertyChunkAtomPtr self = (PropertyChunkAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    switch (self->type) {
        case HEVCConfigurationAtomType:
        case MP4Av1CAtomType:
            if (self->size > self->bytesRead) {
                self->dataSize = self->size - self->bytesRead;
                self->data = (u8*)MP4LocalCalloc(self->dataSize, sizeof(u8));
                TESTMALLOC(self->data)
                GETBYTES(self->dataSize, data);
                self->bytesRead += self->dataSize;
            }
            break;
        case ImageSpatialExtentsAtomType:
            // skip version&flags because ispe is fullAtom
            err = inputStream->read32(inputStream, &val, NULL);
            if (err)
                goto bail;
            self->bytesRead += 4;
            GET32(width);
            GET32(height);
            break;
        case ImageRotationAtomType:
            GET8_V(val);
            self->rotation = val & 0x3;
            break;
        case CleanApertureAtomType:
            self->seenClap = 1;
            for (i = 0; i < 8; ++i) {
                GET32_V(clapValues[i]);
            }
            self->clapWidthN = clapValues[0];
            self->clapWidthD = clapValues[1];
            self->clapHeightN = clapValues[2];
            self->clapHeightD = clapValues[3];
            self->clapHorizOffN = clapValues[4];
            self->clapHorizOffD = clapValues[5];
            self->clapVertOffN = clapValues[6];
            self->clapVertOffD = clapValues[7];
            break;
        case QTColorParameterAtomType:
            GET32(colorType);
            if (self->colorType == MP4_FOUR_CHAR_CODE('n', 'c', 'l', 'x'))
                break;
            else if ((self->colorType != MP4_FOUR_CHAR_CODE('r', 'I', 'C', 'C')) &&
                     (self->colorType != MP4_FOUR_CHAR_CODE('p', 'r', 'o', 'f')))
                err = MP4InvalidMediaErr;
            else if (self->size > self->bytesRead) {
                self->dataSize = self->size - self->bytesRead;
                self->data = (u8*)MP4LocalCalloc(self->dataSize, sizeof(u8));
                TESTMALLOC(self->data)
                GETBYTES(self->dataSize, data);
                self->bytesRead += self->dataSize;
            }
            break;
        default:
            break;
    }

    if (self->size > self->bytesRead) {
        u64 redundance_size = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(redundance_size);
    }

bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4CreatePropertyChunkAtom(PropertyChunkAtomPtr* outAtom, u32 atomType) {
    MP4Err err;
    PropertyChunkAtomPtr self;

    self = (PropertyChunkAtomPtr)MP4LocalCalloc(1, sizeof(PropertyChunkAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = atomType;
    self->name = "Property Chunk Atom";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->attachTo = attachTo;
    /* Default setting */

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
