/*
 ***********************************************************************
 * Copyright (c) 2005-2012, Freescale Semiconductor, Inc.
 * Copyright 2026 NXP
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
    $Id: MetadataAtom.c,v 1.1 2010/12/08 09:28:29 francesc Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "ItemTable.h"
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4MetadataAtomPtr self;
    u32 i;
    err = MP4NoErr;
    self = (MP4MetadataAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    DESTROY_ATOM_LIST

    if (self->itemTable != NULL) {
        deleteItemTable(self->itemTable);
        self->itemTable = NULL;
    }

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err addAtom(MP4MetadataAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    err = MP4NoErr;
    if (atom == NULL)
        BAILWITHERROR(MP4BadParamErr);
    err = MP4AddListEntry(atom, self->atomList);
    if (err)
        goto bail;
    switch (atom->type) {
        case MP4HandlerAtomType:
            if (NULL == self->handler)
                self->handler = atom;
            break;

        case QTMetadataItemKeysAtomType:
            if (NULL == self->itemKeys)
                self->itemKeys = atom;
            break;

        case QTMetadataItemListAtomType:
            if (NULL == self->itemList) {
                MP4MetadataItemListAtomPtr itemList;
                self->itemList = atom;
                itemList = (MP4MetadataItemListAtomPtr)atom;
                itemList->parentAtom = (MP4AtomPtr)self;
            }
            break;

        case UdtaID3V2Type:
            if (NULL == self->id32)
                self->id32 = atom;
            break;
    }

    if (atom->type == ItemLocationAtomType || atom->type == ItemInfoAtomType ||
        atom->type == ItemPropertiesAtomType || atom->type == PrimaryItemAtomType ||
        atom->type == ItemReferenceAtomType || atom->type == ItemDataAtomType) {
        ItemTablePtr itemTable;
        if (self->itemTable == NULL) {
            self->itemTable = (void*)MP4LocalMalloc(sizeof(ItemTable));
            memset(self->itemTable, 0, sizeof(ItemTable));
            itemTable = (ItemTablePtr)self->itemTable;
            itemTable->atomRequiredFlags = DEFAULT_REQUIRED;
            itemTable->atomAvailableFlags = 0;
            itemTable->inputStream = NULL;
            itemTable->valid = FALSE;
        }
        itemTable = (ItemTablePtr)self->itemTable;
        addAvailableAtom(itemTable, atom);

        if (conditionMatch(itemTable))
            buildImageItems(itemTable);
    }

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    MP4MetadataAtomPtr self = (MP4MetadataAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    /* check version and flag */
    // fix SR# 1-913890035
    // ref to ffmpeg, it doesn't care the version/flag
    if ((self->version != 0) || (self->flags != 0)) {
        /* Store data for backward seek operation. The position of data
         * in the buffer is equal to absolute value of file_offset minus 1 */
        if (inputStream->stream_flags & live_flag) {
            u8 pos;
            u32 value = (self->version << 24) + self->flags;
            if (inputStream->buf_mask) {
                MP4MSG("Live stream: There is data stored in the buffer, mask: %x\n",
                       inputStream->buf_mask);
            }

            MP4MSG("Live stream: Store data for seek: 0x%x\n", value);
            for (pos = 0; pos < 4; pos++) {
                inputStream->buf[pos] = value & 0xFF;
                value >>= 8;
            }
            inputStream->buf_mask = 0x0F;
        }

        self->version = 0;
        self->flags = 0;
        /* since QTFF not specify the 'meta' atom base class,
         * this metadata atom might be extension of base atom.
         * if so, roll back 4 bytes */
        inputStream->file_offset -= 4;
        inputStream->available += 4;
        self->bytesRead -= 4;
    }

    /* parse all the sub-atoms */
    while (self->bytesRead < self->size) {
        MP4AtomPtr atom;
        err = MP4ParseAtom((MP4InputStreamPtr)inputStream, &atom);
        if (err)
            goto bail;
        self->bytesRead += atom->size;
        err = addAtom(self, atom);
        if (err)
            goto bail;

        if (self->itemTable != NULL) {
            ItemTablePtr ptr = (ItemTablePtr)self->itemTable;
            if (ptr->inputStream == NULL)
                ptr->inputStream = inputStream;
        }
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateMetadataAtom(MP4MetadataAtomPtr* outAtom) {
    MP4Err err;
    MP4MetadataAtomPtr self;

    self = (MP4MetadataAtomPtr)MP4LocalCalloc(1, sizeof(MP4MetadataAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = QTMetadataAtomType;
    self->name = "metadata";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;
    self->handler = NULL;
    self->itemKeys = NULL;
    self->itemList = NULL;
    self->id32 = NULL;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
