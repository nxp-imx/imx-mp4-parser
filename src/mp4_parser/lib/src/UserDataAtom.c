/*
***********************************************************************
* Copyright (c) 2005-2015, Freescale Semiconductor, Inc.
*
* Copyright 2017-2018, 2024, 2026 NXP
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
    $Id: UserDataAtom.c,v 1.3 2000/07/04 09:28:29 francesc Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

typedef struct UserDataMapRecord {
    u32 atomType;
    MP4LinkedList atomList;
} UserDataMapRecord, *UserDataMapRecordPtr;

static MP4Err getEntryCount(struct MP4UserDataAtom* self, u32 userDataType, u32* outCount);
static MP4Err getIndType(struct MP4UserDataAtom* self, u32 typeIndex, u32* outType);
static MP4Err getItem(struct MP4UserDataAtom* self, MP4Handle userDataH, u32 userDataType,
                      u32 itemIndex);
static MP4Err getTypeCount(struct MP4UserDataAtom* self, u32* outCount);

static MP4Err findEntryForAtomType(MP4UserDataAtomPtr self, u32 atomType,
                                   UserDataMapRecordPtr* outRecord);

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4UserDataAtomPtr self;
    u32 i;
    u32 recordCount;
    u32 recordIndex;
    err = MP4NoErr;
    self = (MP4UserDataAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    /* engr94385: small udat atom */
    if (self->data) {
        MP4LocalFree(self->data);
        self->data = NULL;
    }

    err = MP4GetListEntryCount(self->recordList, &recordCount);
    if (err)
        goto bail;
    for (recordIndex = 0; recordIndex < recordCount; recordIndex++) {
        UserDataMapRecordPtr p;
        err = MP4GetListEntry(self->recordList, recordIndex, (char**)&p);
        if (err)
            goto bail;
        DESTROY_ATOM_LIST_V(p->atomList);
        MP4LocalFree(p);
    }
    err = MP4DeleteLinkedList(self->recordList);
    if (err)
        goto bail;

    SAFE_DESTROY(self->meta);

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err addUserData(struct MP4UserDataAtom* self, MP4Handle userDataH, u32 userDataType,
                          u32* outIndex) {
    MP4Err MP4CreateUserDataEntryAtom(MP4UserDataEntryAtomPtr * outAtom);

    MP4Err err;
    UserDataMapRecordPtr rec;
    MP4UserDataEntryAtomPtr atom;

    err = MP4CreateUserDataEntryAtom(&atom);
    if (err)
        goto bail;
    atom->type = userDataType;
    err = MP4GetHandleSize(userDataH, &atom->dataSize);
    if (err)
        goto bail;
    if (atom->dataSize > 0) {
        atom->data = (char*)MP4LocalMalloc(atom->dataSize);
        TESTMALLOC(atom->data)
        memcpy(atom->data, *userDataH, atom->dataSize);
    }
    err = findEntryForAtomType(self, userDataType, &rec);
    if (err)
        goto bail;
    if (rec == NULL) {
        rec = (UserDataMapRecordPtr)MP4LocalCalloc(1, sizeof(struct UserDataMapRecord));
        TESTMALLOC(rec)
        rec->atomType = atom->type;
        err = MP4MakeLinkedList(&rec->atomList);
        if (err)
            goto bail;
        err = MP4AddListEntry(rec, self->recordList);
        if (err)
            goto bail;
    }
    err = MP4AddListEntry(atom, rec->atomList);
    if (err)
        goto bail;
    if (outIndex != NULL) {
        err = MP4GetListEntryCount(rec->atomList, outIndex);
        if (err)
            goto bail;
    }
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err findEntryForAtomType(MP4UserDataAtomPtr self, u32 atomType,
                                   UserDataMapRecordPtr* outRecord) {
    MP4Err err;
    u32 i;
    u32 count;
    *outRecord = 0;
    err = MP4GetListEntryCount(self->recordList, &count);
    if (err)
        goto bail;
    for (i = 0; i < count; i++) {
        UserDataMapRecordPtr p;
        err = MP4GetListEntry(self->recordList, i, (char**)&p);
        if (err)
            goto bail;
        if ((p != 0) && (p->atomType == atomType)) {
            *outRecord = p;
            break;
        }
    }
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err getTypeCount(struct MP4UserDataAtom* self, u32* outCount) {
    MP4Err err;

    err = MP4GetListEntryCount(self->recordList, outCount);

    TEST_RETURN(err);
    return err;
}

static MP4Err getEntryCount(struct MP4UserDataAtom* self, u32 userDataType, u32* outCount) {
    MP4Err err;
    u32 i;
    u32 count;
    *outCount = 0;
    err = MP4GetListEntryCount(self->recordList, &count);
    if (err)
        goto bail;
    for (i = 0; i < count; i++) {
        UserDataMapRecordPtr p;
        err = MP4GetListEntry(self->recordList, i, (char**)&p);
        if (err)
            goto bail;
        if ((p != 0) && (p->atomType == userDataType) && (p->atomList != 0)) {
            err = MP4GetListEntryCount(p->atomList, outCount);
            if (err)
                goto bail;
            break;
        }
    }
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err getIndType(struct MP4UserDataAtom* self, u32 typeIndex, u32* outType) {
    MP4Err err;
    u32 count;
    UserDataMapRecordPtr p;
    *outType = 0;
    err = MP4GetListEntryCount(self->recordList, &count);
    if (err)
        goto bail;
    if (typeIndex > count) {
        BAILWITHERROR(MP4BadParamErr);
    }
    err = MP4GetListEntry(self->recordList, typeIndex - 1, (char**)&p);
    if (err)
        goto bail;
    *outType = p->atomType;
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err getItem(struct MP4UserDataAtom* self, MP4Handle userDataH, u32 userDataType,
                      u32 itemIndex) {
    MP4Err err;
    UserDataMapRecordPtr rec;
    // MP4UnknownAtomPtr atom;
    MP4UserDataEntryAtomPtr atom; /* ENGR114401 : not use unknown atom ptr */

    err = findEntryForAtomType(self, userDataType, &rec);
    if (err)
        goto bail;
    if (rec == 0) {
        BAILWITHERROR(MP4BadParamErr);
    }
    err = MP4GetListEntry(rec->atomList, itemIndex - 1, (char**)&atom);
    if (err)
        goto bail;
    err = atom->calculateSize((MP4AtomPtr)atom);
    if (err)
        goto bail;
    err = MP4SetHandleSize(userDataH, atom->dataSize);
    if (err)
        goto bail;
    memcpy(*userDataH, atom->data, atom->dataSize);
bail:
    TEST_RETURN(err);
    return err;
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

static MP4Err appendItemList(struct MP4UserDataAtom* self, MP4LinkedList list) {
    MP4Err err;
    UserDataMapRecordPtr rec;
    MP4UserDataEntryAtomPtr atom;
    u32 rec_count = 0, item_count = 0;

    if (NULL == list)
        BAILWITHERROR(MP4BadParamErr);

    err = MP4GetListEntryCount(self->recordList, &rec_count);
    if (err)
        goto bail;
    self->recordList->foundEntryNumber = 0;
    self->recordList->foundEntry = self->recordList->head;
    for (; (u32)self->recordList->foundEntryNumber < rec_count;) {
        UserDataRecordPtr record;

        rec = (UserDataMapRecordPtr)self->recordList->foundEntry->data;
        err = MP4GetListEntryCount(rec->atomList, &item_count);
        if (err)
            goto bail;

        err = findRecordForType(list, rec->atomType, &record);
        if (err)
            goto bail;
        if (NULL == record) {
            /* create a user data record */
            record = (UserDataRecordPtr)MP4LocalCalloc(1, sizeof(struct UserDataRecord));
            TESTMALLOC(record)
            record->type = rec->atomType;
            err = MP4MakeLinkedList(&record->list);
            if (err)
                goto bail;
            err = MP4AddListEntry(record, list);
            if (err)
                goto bail;
        }

        rec->atomList->foundEntryNumber = 0;
        rec->atomList->foundEntry = rec->atomList->head;

        for (; (u32)rec->atomList->foundEntryNumber < item_count;) {
            atom = (MP4UserDataEntryAtomPtr)rec->atomList->foundEntry->data;

            /* create a user data item */
            if (NULL != atom) {
                UserDataItemPtr item;
                item = (UserDataItemPtr)MP4LocalCalloc(1, sizeof(struct UserDataItem));
                TESTMALLOC(item)
                item->format = QT_WELL_KNOWN_DATA_TYPE_UTF_8;
                item->data = atom->data;
                item->size = atom->dataSize;
                err = MP4AddListEntry(item, record->list);
                if (err)
                    goto bail;
            }

            rec->atomList->foundEntryNumber++;
            rec->atomList->foundEntry = rec->atomList->foundEntry->link;
        }

        self->recordList->foundEntryNumber++;
        self->recordList->foundEntry = self->recordList->foundEntry->link;
    }
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err addAtom(MP4UserDataAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    UserDataMapRecordPtr rec;

    err = MP4NoErr;
    if (atom == NULL)
        BAILWITHERROR(MP4BadParamErr);

    if (QTMetadataAtomType == atom->type) {
        self->meta = atom;
    } else {
        err = findEntryForAtomType(self, atom->type, &rec);
        if (err)
            goto bail;
        if (rec == 0) {
            rec = (UserDataMapRecordPtr)MP4LocalCalloc(1, sizeof(struct UserDataMapRecord));
            TESTMALLOC(rec)
            rec->atomType = atom->type;
            err = MP4MakeLinkedList(&rec->atomList);
            if (err)
                goto bail;
            err = MP4AddListEntry(rec, self->recordList);
            if (err)
                goto bail;
        }
        err = MP4AddListEntry(atom, rec->atomList);
    }

bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    if (proto->size <
        16) { /* ENGR94385:
              'udta' atom is too small to hold user data list (a series of child atoms) */

        MP4Err err = MP4NoErr;
        long bytesToRead;
        MP4UserDataAtomPtr self = (MP4UserDataAtomPtr)s;

        err = MP4NoErr;
        if (self == NULL) {
            err = MP4BadParamErr;
            return err;
        }
        err = self->super->createFromInputStream(s, proto, (char*)inputStream);

        bytesToRead = (long)(self->size - self->bytesRead);
        if (bytesToRead > 0) {
            self->data = (char*)MP4LocalCalloc(1, bytesToRead);
            // TESTMALLOC( self->data )
            if (0 == self->data) {
                err = MP4NoMemoryErr;
                return err;
            }
            err = inputStream->readData(inputStream, bytesToRead, self->data,
                                        "small user data atom data");
            if (err)
                return err;
            self->bytesRead += bytesToRead;

            self->dataSize = bytesToRead;
        }
        return err;
    } else {
        /* otherwise, parse child items */
        PARSE_ATOM_LIST(MP4UserDataAtom)

    bail:
        TEST_RETURN(err);

        if (MP4NoQTAtomErr == err || MP4EOF == err) {
            /* ENGR94385:
            QT 'udta' atom data usually end with 4 bytes of 0 (0x00 00 00 00),
            after the last child atom in the data list.
            MP4 'udta' has not.
            So the program will find a atom of size 0 after finshing the last child atom
            when it reads the last 4 bytes, and this will cause an error 'MP4NoQTAtomErr',
            but we shall tolerate it. - Amanda */
            if (self->size == (4 + self->bytesRead)) {
                err = MP4NoErr;
            }
        }
        return err;
    }
}

MP4Err MP4CreateUserDataAtom(MP4UserDataAtomPtr* outAtom) {
    MP4Err err;
    MP4UserDataAtomPtr self;

    self = (MP4UserDataAtomPtr)MP4LocalCalloc(1, sizeof(MP4UserDataAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4UserDataAtomType;
    self->name = "user data";
    self->meta = NULL;
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    err = MP4MakeLinkedList(&self->recordList);
    if (err)
        goto bail;
    self->addUserData = addUserData;
    self->getEntryCount = getEntryCount;
    self->getIndType = getIndType;
    self->getItem = getItem;
    self->getTypeCount = getTypeCount;
    self->appendItemList = appendItemList;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
