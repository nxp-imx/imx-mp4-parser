/*
 ***********************************************************************
 * Copyright (c) 2005-2012, Freescale Semiconductor, Inc.
 * Copyright 2017, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */

#include <stdlib.h>
#include <string.h>
#include "Charset.h"
#include "ID3ParserCore.h"
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4UserDataID3v2AtomPtr self;
    u32 i;
    err = MP4NoErr;
    self = (MP4UserDataID3v2AtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    DESTROY_ATOM_LIST

    if (self->data) {
        MP4LocalFree(self->data);
        self->data = NULL;
    }

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static void destroyUnknownAtom(MP4AtomPtr s) {
    MP4UnknownAtomPtr self;
    self = (MP4UnknownAtomPtr)s;
    if (self->data) {
        MP4LocalFree(self->data);
        self->data = NULL;
    }
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

static MP4Err addAtom(MP4UserDataID3v2AtomPtr self, MP4AtomPtr atom) {
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

static MP4Err appendItemList(struct MP4UserDataID3v2Atom* self, MP4LinkedList list) {
    MP4Err err;
    MP4UnknownAtomPtr unknownAtom;
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

        unknownAtom = (MP4UnknownAtomPtr)self->atomList->foundEntry->data;
        if (NULL == unknownAtom) {
            continue;
        }

        self->atomList->foundEntryNumber++;
        self->atomList->foundEntry = self->atomList->foundEntry->link;

        type = unknownAtom->type;

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

        if (NULL != unknownAtom->data) {
            UserDataItemPtr item;
            item = (UserDataItemPtr)MP4LocalCalloc(1, sizeof(struct UserDataItem));
            TESTMALLOC(item)
            item->data = unknownAtom->data;
            item->size = unknownAtom->dataSize;
            if (UdtaCoverType != unknownAtom->type)
                item->format = QT_WELL_KNOWN_DATA_TYPE_UTF_8;
            else
                item->format = QT_WELL_KNOWN_DATA_TYPE_JPEG;
            err = MP4AddListEntry(item, record->list);
            if (err)
                goto bail;
        }
    }

bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    long bytesToRead;
    MP4UserDataID3v2AtomPtr self = (MP4UserDataID3v2AtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);

    if (self->size > 14) {
        u64 bytesToSkip = 6;
        SKIPBYTES_FORWARD(bytesToSkip);
    }

    bytesToRead = (long)(s->size - s->bytesRead);

    if (bytesToRead > 0) {
        uint32 dataSize;
        char* buffer;
        ID3 id3;
        char* data;
        char* mime;
        uint32 i = 0;
        extern ParserMemoryOps g_memOps;

        typedef struct tagMap {
            const char* key;
            const char* tag1;
            const char* tag2;
            uint32 type;
        } Map;

        static const Map g_kMap[] = {{"album", "TALB", "TAL", UdtaAlbumType},
                                     {"artist", "TPE1", "TP1", UdtaArtistType},
                                     {"band", "TPE2", "TP2", UdtaAlbumArtistType},
                                     {"composer", "TCOM", "TCM", UdtaComposerType},
                                     {"genre", "TCON", "TCO", UdtaGenreType},
                                     {"title", "TIT2", "TT2", UdtaTitleType},
                                     {"year", "TYER", "TYE", UdtaYearType},
                                     {"tracknumber", "TRCK", "TRK", UdtaCDTrackNumType},
                                     {"discnumber", "TPOS", "TPA", 0x0},
                                     {"copyright", "TCOP", NULL, 0x0}};
        static const uint32 g_kNumMapEntries = sizeof(g_kMap) / sizeof(g_kMap[0]);

        self->data = (char*)MP4LocalCalloc(1, bytesToRead);
        TESTMALLOC(self->data)

        GETBYTES(bytesToRead, data);
        self->dataSize = bytesToRead;

        buffer = self->data;

        ID3CoreInit(&id3, &g_memOps, TRUE);
        if (ID3V2Parse(&id3, buffer)) {
            if (id3.mSize > self->dataSize)
                BAILWITHERROR(MP4BadParamErr)

            for (i = 0; i < g_kNumMapEntries; ++i) {
                Iterator it;
                char* s = NULL;

                IteratorInit(&it, &id3, g_kMap[i].tag1);
                if (Miss(&it) && g_kMap[i].tag2) {
                    IteratorExit(&it);
                    IteratorInit(&it, &id3, g_kMap[i].tag2);
                }

                if (Miss(&it)) {
                    IteratorExit(&it);
                    continue;
                }

                FetchFrameVal(&it, &s, FALSE);
                IteratorExit(&it);

                if (s && g_kMap[i].type != 0x0) {
                    MP4UnknownAtomPtr atom;
                    atom = (MP4UnknownAtomPtr)MP4LocalCalloc(1, sizeof(MP4UnknownAtom));
                    atom->type = g_kMap[i].type;
                    atom->dataSize = strlen(s);
                    atom->data = MP4LocalCalloc(1, atom->dataSize + 1);
                    atom->destroy = destroyUnknownAtom;
                    memcpy(atom->data, s, atom->dataSize);
                    err = addAtom(self, (MP4AtomPtr)atom);
                    if (err)
                        goto bail;
                }
            }

            data = (void*)GetArtWork(&id3, &dataSize, &mime);
            if (data) {
                MP4UnknownAtomPtr atom;
                atom = (MP4UnknownAtomPtr)MP4LocalCalloc(1, sizeof(MP4UnknownAtom));
                atom->type = UdtaCoverType;
                atom->dataSize = dataSize;
                atom->data = MP4LocalCalloc(1, atom->dataSize);
                atom->destroy = destroyUnknownAtom;
                memcpy(atom->data, data, atom->dataSize);
                MP4LocalFree(mime);

                err = addAtom(self, (MP4AtomPtr)atom);
                if (err)
                    goto bail;
            }
        }
        ID3CoreExit(&id3);
    }

bail:
    TEST_RETURN(err);

    if (err && self && self->data) {
        MP4LocalFree(self->data);
        self->data = NULL;
    }
    return err;
}

MP4Err MP4CreateID3v2UserDataAtom(MP4UserDataID3v2AtomPtr* outAtom) {
    MP4Err err;
    MP4UserDataID3v2AtomPtr self;

    self = (MP4UserDataID3v2AtomPtr)MP4LocalCalloc(1, sizeof(MP4UserDataID3v2Atom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->name = "user data id3v2 atom entry";
    self->destroy = destroy;
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->data = NULL;
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
