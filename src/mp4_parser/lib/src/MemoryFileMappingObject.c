/*
 ***********************************************************************
 * Copyright 2005-2006 by Freescale Semiconductor, Inc.
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
    $Id: MemoryFileMappingObject.c,v 1.10 2001/06/06 18:09:59 rtm Exp $
*/
#include "FileMappingObject.h"

static MP4Err doClose(struct FileMappingObjectRecord* s);
static MP4Err doOpen(struct FileMappingObjectRecord* s, const char* filename);
static MP4Err isYourFile(struct FileMappingObjectRecord* s, const char* filename, u32* outSameFile);

static MP4Err destroy(struct FileMappingObjectRecord* s) {
    MP4Err err;

    MP4LocalFree(s);

    err = MP4NoErr;
    return err;
}

static MP4Err doOpen(struct FileMappingObjectRecord* s, const char* filename) {
    MP4Err err;
    err = MP4NoErr;
    if (filename != NULL)
        BAILWITHERROR(MP4BadParamErr);

    (void)s;
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err doClose(struct FileMappingObjectRecord* s) {
    MP4Err err;
    err = MP4NoErr;

    (void)s;
    return err;
}

static MP4Err isYourFile(struct FileMappingObjectRecord* s, const char* filename,
                         u32* outSameFile) {
    MP4Err err;
    err = MP4NoErr;
    *outSameFile = (filename == NULL);
    (void)s;
    return err;
}

MP4Err MP4CreateMemoryFileMappingObject(char* src, u32 size, FileMappingObject* outObject) {
    MP4Err err;
    FileMappingObject self;

    err = MP4NoErr;
    self = (FileMappingObject)MP4LocalCalloc(1, sizeof(FileMappingObjectRecord));
    TESTMALLOC(self);
    self->destroy = destroy;
    self->open = doOpen;
    self->close = doClose;
    self->isYourFile = isYourFile;
    self->data = src;
    self->size = size;
    *outObject = (FileMappingObject)self;
bail:
    TEST_RETURN(err);

    return err;
}
