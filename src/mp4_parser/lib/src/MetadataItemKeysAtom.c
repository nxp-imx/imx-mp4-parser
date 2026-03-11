/*
 ***********************************************************************
 * Copyright (c) 2005-2015, Freescale Semiconductor, Inc.
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
    $Id: MetadataItemKeysAtom.c,v 1.1 2011/02/28 09:28:29 francesc Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

typedef struct KeyEntry {
    u32 keyNamespace;
    void* keyValue;
    u32 keySize;
} KeyEntry, *KeyEntryPtr;

typedef struct MdatKeyFourCC {
    char* mdatKey;
    u32 fourcc;
} MdatKeyFourCC, *MdatKeyFourCCPtr;

/* refer to section 'QuickTime Metadata Keys' in QTFF */
static MdatKeyFourCC g_mdatKeyTable[] = {
        {"com.apple.quicktime.album", UdtaAlbumType},
        {"com.apple.quicktime.artist", UdtaArtistType},
        {"com.apple.quicktime.artwork", UdtaCoverType},
        {"com.apple.quicktime.author", UdtaAuthorType},
        {"com.apple.quicktime.comment", UdtaCommentType},
        {"com.apple.quicktime.copyright", UdtaCopyrightType},
        {"com.apple.quicktime.creationdate", UdtaDateType},
        {"com.apple.quicktime.description", UdtaDescriptionType},
        {"com.apple.quicktime.director", UdtaDirectorType},
        {"com.apple.quicktime.title", UdtaTitleType},
        {"com.apple.quicktime.genre", UdtaGenreType},
        {"com.apple.quicktime.information", UdtaInformationType},
        {"com.apple.quicktime.keywords", UdtaKeywordType},
        //{"com.apple.quicktime.location.ISO6709",    -1},
        {"com.apple.quicktime.producer", UdtaProducerType},
        {"com.apple.quicktime.publisher", UdtaPublisherType},
        {"com.apple.quicktime.software", UdtaSoftwareType},
        {"com.apple.quicktime.year", UdtaYearType},
        {"com.apple.quicktime.collection.user", UdtaCollectionType},
        {"com.apple.quicktime.rating.user", UdtaRateType},
        {"com.android.version", UdtaAndroidVersionType},
        {"com.android.capture.fps", UdtaCaptureFpsType},
        {NULL, -1}};

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4MetadataItemKeysAtomPtr self;
    err = MP4NoErr;
    self = (MP4MetadataItemKeysAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->keyList) {
        u32 count, i;
        err = MP4GetListEntryCount(self->keyList, &count);
        if (MP4NoErr == err) {
            for (i = 0; i < count; i++) {
                KeyEntryPtr key;
                err = MP4GetListEntry(self->keyList, i, (char**)&key);
                if (err)
                    continue;
                if (key) {
                    if (key->keyValue)
                        MP4LocalFree(key->keyValue);
                    MP4LocalFree(key);
                }
            }
        }
        err = MP4DeleteLinkedList(self->keyList);
        if (err)
            err = MP4NoErr;
        self->keyList = NULL;
    }

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err getTypebyKeyIndex(MP4AtomPtr s, u32 index, u32* type) {
    MP4MetadataItemKeysAtomPtr self = (MP4MetadataItemKeysAtomPtr)s;
    KeyEntryPtr key = NULL;
    u32 i = 0;
    MP4Err err;

    if ((self == NULL) || (type == NULL))
        BAILWITHERROR(MP4BadParamErr)

    /* since key index is 1-based, minus 1 when get key from linked list */
    err = MP4GetListEntry(self->keyList, index - 1, (char**)(&key));
    if (err)
        goto bail;

    while (g_mdatKeyTable[i].mdatKey != NULL) {
        if ((u32)(-1) == g_mdatKeyTable[i].fourcc) {
            i++;
            continue;
        }

        if (!strncmp(g_mdatKeyTable[i].mdatKey, (char*)key->keyValue, key->keySize)) {
            *type = g_mdatKeyTable[i].fourcc;
            break;
        }

        i++;
    }

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err MP4ParseAtomUsingProtoKeys(MP4InputStreamPtr inputStream, u32 * protoKeys,
                                      u32 defaultAtom, MP4AtomPtr * outAtom);
    u32 i;
    MP4Err err;
    KeyEntryPtr key;
    MP4MetadataItemKeysAtomPtr self = (MP4MetadataItemKeysAtomPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32(entryCount);

    /* get all the keys in the loop and put them to a linked list */
    for (i = self->entryCount; i > 0; i--) {
        key = (KeyEntryPtr)MP4LocalCalloc(1, sizeof(struct KeyEntry));
        TESTMALLOC(key)

        GET32_V(key->keySize);
        key->keySize -= 8; /* sizeof(keySize) + sizeof(keyNamespace) */

        GET32_V(key->keyNamespace);

        key->keyValue = MP4LocalCalloc(1, key->keySize);
        GETBYTES_V(key->keySize, key->keyValue);

        err = MP4AddListEntry(key, self->keyList);
        if (err)
            goto bail;
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateMetadataItemKeysAtom(MP4MetadataItemKeysAtomPtr* outAtom) {
    MP4Err err;
    MP4MetadataItemKeysAtomPtr self;

    self = (MP4MetadataItemKeysAtomPtr)MP4LocalCalloc(1, sizeof(MP4MetadataItemKeysAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = QTMetadataItemKeysAtomType;
    self->name = "metadata item list";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    err = MP4MakeLinkedList(&self->keyList);
    if (err)
        goto bail;
    self->getTypebyKeyIndex = getTypebyKeyIndex;
    self->entryCount = 0;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
