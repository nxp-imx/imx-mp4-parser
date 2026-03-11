/*
 ***********************************************************************
 * Copyright (c) 2005-2012, Freescale Semiconductor Inc.
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
    $Id: MJ2FileTypeAtom.c,v 1.0 2000/10/05 14:00:00 tm Exp $
*/
//#define DEBUG

#include <stdlib.h>
#include <string.h>
#include "MJ2Atoms.h"
#include "MP4Impl.h"

static void destroy(MP4AtomPtr s) {
    ISOErr err = ISONoErr;
    MJ2FileTypeAtomPtr self = (MJ2FileTypeAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(ISOBadParamErr)

    if (self->compatibilityList) {
        MP4LocalFree(self->compatibilityList);
        self->compatibilityList = NULL;
    }
    if (self->super)
        self->super->destroy(s);

bail:
    TEST_RETURN(err);

    return;
}

#define MAX_COMPATIBLE_LIST_SIZE (uint32)(128 * 1024)

static ISOErr createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    ISOErr err;
    u32 items = 0;
    u32 bytesToRead;
    MP4LinkedList brands;
    u32 count;
    s32 i;
    bool mif1 = FALSE;
    bool heic = FALSE;
    MJ2FileTypeAtomPtr self = (MJ2FileTypeAtomPtr)s;

    err = ISONoErr;
    if (self == NULL)
        BAILWITHERROR(ISOBadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32(brand);
    GET32(minorVersion);
    if ((self->brand & 0xFFFF0000) == 0x33670000)
        inputStream->stream_flags |= flag_3gp;

    bytesToRead = (u32)(self->size - self->bytesRead);
    if (bytesToRead < sizeof(u32)) /* there must be at least one item in the compatibility list */
        BAILWITHERROR(ISOBadDataErr)

    /*	if ( self->compatibilityList )
            free( self->compatibilityList );   */         // Ranjeet. Free.

    // fix SR# 1-916328976
    if (bytesToRead > MAX_COMPATIBLE_LIST_SIZE) {
        BAILWITHERROR(ISOBadDataErr)
    }

    self->compatibilityList = (u32*)MP4LocalCalloc(1, bytesToRead);
    TESTMALLOC(self->compatibilityList);

    while (bytesToRead > 0) {
        if (bytesToRead < sizeof(u32)) /* we need to read a full u32 */
            BAILWITHERROR(ISOBadDataErr)

        GET32(compatibilityList[items]);
        items++;
        bytesToRead = (u32)(self->size - self->bytesRead);
    }

    self->itemCount = items;

    err = MP4MakeLinkedList(&brands);
    if (err)
        goto bail;
    err = MP4AddListEntry(&self->brand, brands);
    if (err)
        goto bail;
    for (i = 0; i < (s32)self->itemCount; i++) {
        err = MP4AddListEntry(&self->compatibilityList[i], brands);
        if (err)
            goto bail;
    }
    err = MP4GetListEntryCount(brands, &count);
    if (err)
        goto bail;
    for (i = count - 1; i >= 0; i--) {
        u32* brand;
        err = MP4GetListEntry(brands, i, (char**)&brand);
        if (err)
            goto bail;
        if (*brand == MP4_FOUR_CHAR_CODE('m', 'i', 'f', '1')) {
            mif1 = TRUE;
            err = MP4RemoveListEntry(brands, i, NULL);
            if (err)
                goto bail;
        } else if (*brand == MP4_FOUR_CHAR_CODE('h', 'e', 'i', 'c')) {
            heic = TRUE;
            err = MP4RemoveListEntry(brands, i, NULL);
            if (err)
                goto bail;
        } else if (*brand == MP4_FOUR_CHAR_CODE('a', 'v', 'i', 'f') ||
                   *brand == MP4_FOUR_CHAR_CODE('a', 'v', 'i', 's')) {
            self->isAvif = TRUE;
            MP4MSG("isAvif\n");
            err = MP4RemoveListEntry(brands, i, NULL);
            if (err)
                goto bail;
        }
    }

    if (mif1 && heic) {
        self->isHeif = TRUE;
        MP4MSG("isHeif\n");
    }

    err = MP4GetListEntryCount(brands, &count);
    if (err)
        goto bail;
    if ((self->isHeif || self->isAvif) && count > 0) {
        MP4MSG("%s format file with other tracks!\n", self->isHeif ? "Heif" : "Avif");
        self->hasMoovBox = TRUE;
    }
    err = MP4DeleteLinkedList(brands);
    if (err)
        goto bail;

bail:
    TEST_RETURN(err);

    return err;
}

ISOErr MJ2CreateFileTypeAtom(MJ2FileTypeAtomPtr* outAtom) {
    ISOErr err;
    MJ2FileTypeAtomPtr self;

    self = (MJ2FileTypeAtomPtr)MP4LocalCalloc(1, sizeof(MJ2FileTypeAtom));
    TESTMALLOC(self);

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->type = MJ2FileTypeAtomType;
    self->name = "JPEG 2000 file type atom";
    self->destroy = destroy;
    self->createFromInputStream = (cisfunc)createFromInputStream;

    self->brand = MJ2JPEG2000Brand;
    self->minorVersion = (u32)0;
    self->itemCount = (u32)0;
    /*self->compatibilityList		= (u32 *) MP4LocalCalloc( 1, sizeof( u32 ) );
    TESTMALLOC( self->compatibilityList );

    self->compatibilityList[0]	= MJ2JPEG2000Brand;*/
    self->itemCount = (u32)1;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
