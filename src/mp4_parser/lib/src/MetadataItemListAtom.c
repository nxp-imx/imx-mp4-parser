/*
 ***********************************************************************
 * Copyright (c) 2005-2013, Freescale Semiconductor, Inc.
 *
 * Copyright 2017-2018, 2026 NXP
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
    $Id: MetadataItemListAtom.c,v 1.1 2010/12/08 09:28:29 francesc Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

#define ITUNES_MEAN "com.apple.iTunes"
#define ENCODER_DELAY_NAME "iTunSMPB"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4MetadataItemListAtomPtr self;
    u32 i;
    err = MP4NoErr;
    self = (MP4MetadataItemListAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    DESTROY_ATOM_LIST

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err findRecordForType(MP4LinkedList list, u32 type, UserDataRecordPtr* outRecord) {
    MP4Err err;
    u32 i;
    u32 count;
    *outRecord = NULL;
    err = MP4GetListEntryCount(list, &count);
    if (err)
        goto bail;
    for (i = 0; i < count; i++) {
        UserDataRecordPtr p;
        err = MP4GetListEntry(list, i, (char**)&p);
        if (err)
            goto bail;
        if ((p != 0) && (p->type == type)) {
            *outRecord = p;
            break;
        }
    }
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err appendItemList(MP4MetadataItemListAtomPtr self, MP4LinkedList list) {
    MP4Err err;
    MP4MetadataAtomPtr meta;
    MP4MetadataItemKeysAtomPtr itemKeys;
    MP4MetadataItemAtomPtr metaItemAtom;
    MP4ValueAtomPtr valueAtom;
    u32 atom_count = 0;

    if (NULL == list)
        BAILWITHERROR(MP4BadParamErr);

    err = MP4GetListEntryCount(self->atomList, &atom_count);
    if (err)
        goto bail;
    self->atomList->foundEntryNumber = 0;
    self->atomList->foundEntry = self->atomList->head;
    for (; (u32)self->atomList->foundEntryNumber < atom_count;) {
        UserDataRecordPtr record;
        u32 type;

        metaItemAtom = (MP4MetadataItemAtomPtr)self->atomList->foundEntry->data;
        if (NULL == metaItemAtom) {
            continue;
        }

        self->atomList->foundEntryNumber++;
        self->atomList->foundEntry = self->atomList->foundEntry->link;

        type = metaItemAtom->type;

        /* if metadata item keys atom exists, get metadata item type from it */
        meta = (MP4MetadataAtomPtr)(self->parentAtom);
        itemKeys = (MP4MetadataItemKeysAtomPtr)(meta->itemKeys);
        if (NULL != itemKeys) {
            u32 index = metaItemAtom->type;
            if ((index > 0) && (index <= itemKeys->entryCount)) {
                err = itemKeys->getTypebyKeyIndex((MP4AtomPtr)itemKeys, index, &type);
                /* failed to get fourcc type, drop it and get the next */
                if ((err) || ((type > 0) && (type <= itemKeys->entryCount))) {
                    continue;
                }
            }
        }

        // reset the type to be another type because there are more than one '----' atom
        if (MP4AudioInfoAtomType == type) {
            MP4MeanAtomPtr meanAtom = (MP4MeanAtomPtr)metaItemAtom->mean;
            MP4NameAtomPtr nameAtom = (MP4NameAtomPtr)metaItemAtom->dataName;
            if (meanAtom != NULL && nameAtom != NULL && meanAtom->data != NULL &&
                nameAtom->data != NULL &&
                (!strncmp(ITUNES_MEAN, meanAtom->data, meanAtom->dataSize)) &&
                (!strncmp(ENCODER_DELAY_NAME, nameAtom->data, nameAtom->dataSize))) {
                type = FSLiTunSMPBAtomType;
            }
        }
        err = findRecordForType(list, type, &record);
        if (err)
            goto bail;
        if (NULL == record) {
            /* create a user data record */
            record = (UserDataRecordPtr)MP4LocalCalloc(1, sizeof(struct UserDataRecord));
            TESTMALLOC(record)
            record->type = type;
            err = MP4MakeLinkedList(&record->list);
            if (err)
                goto bail;
            err = MP4AddListEntry(record, list);
            if (err)
                goto bail;
        }

        valueAtom = (MP4ValueAtomPtr)metaItemAtom->data;
        if (NULL != valueAtom) {
            UserDataItemPtr item;
            item = (UserDataItemPtr)MP4LocalCalloc(1, sizeof(struct UserDataItem));
            TESTMALLOC(item)
            item->format = valueAtom->valueType;
            item->data = valueAtom->data;
            item->size = valueAtom->dataSize;
            err = MP4AddListEntry(item, record->list);
            if (err)
                goto bail;
        }
    }

bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err addAtom(MP4MetadataItemListAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    err = MP4NoErr;
    if (atom == NULL)
        BAILWITHERROR(MP4BadParamErr);
    err = MP4AddListEntry(atom, self->atomList);
    if (err)
        goto bail;

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err MP4ParseAtomUsingProtoList(MP4InputStreamPtr inputStream, u32 * protoList,
                                      u32 defaultAtom, MP4AtomPtr * outAtom);
    u32 MP4EmptyProtos[] = {0};
    MP4Err err;
    MP4MetadataItemListAtomPtr self = (MP4MetadataItemListAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    while (self->bytesRead < self->size) {
        MP4AtomPtr atom;
        err = MP4ParseAtomUsingProtoList(inputStream, MP4EmptyProtos, QTMetadataItemAtomType,
                                         &atom);
        if (err)
            goto bail;
        self->bytesRead += atom->size;
        err = addAtom(self, atom);
        if (err)
            goto bail;
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateMetadataItemListAtom(MP4MetadataItemListAtomPtr* outAtom) {
    MP4Err err;
    MP4MetadataItemListAtomPtr self;

    self = (MP4MetadataItemListAtomPtr)MP4LocalCalloc(1, sizeof(MP4MetadataItemListAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = QTMetadataItemListAtomType;
    self->name = "metadata item list";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;
    self->appendItemList = appendItemList;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
