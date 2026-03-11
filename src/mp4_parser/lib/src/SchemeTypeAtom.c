/*
 ***********************************************************************
 * Copyright 2016 by Freescale Semiconductor, Inc.
 *
 * Copyright 2017-2018, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */
#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

#define SCHEME_MODE_CBC 2
#define SCHEME_MODE_CTR 1

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4SchemeTypeAtomPtr self;
    err = MP4NoErr;
    self = (MP4SchemeTypeAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->uri) {
        MP4LocalFree(self->uri);
        self->uri = NULL;
    }
    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    MP4SchemeTypeAtomPtr self = (MP4SchemeTypeAtomPtr)s;
    u32 uriLen = 0;
    u64 bytesToRead;
    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GET32(scheme_type);
    GET32(scheme_version);

    switch (self->scheme_type) {
        case MP4SchemeTypeCBC1:
            self->mode = SCHEME_MODE_CBC;
            break;
        case MP4SchemeTypeCBCS:
            self->mode = SCHEME_MODE_CBC;
            self->bSubsampleEncryption = TRUE;
            break;
        case MP4SchemeTypeCENC:
            self->mode = SCHEME_MODE_CTR;
            break;
        case MP4SchemeTypeCENS:
            self->mode = SCHEME_MODE_CTR;
            self->bSubsampleEncryption = TRUE;
            break;
        default:
            self->mode = 0;
            break;
    }
    if (self->flags & 0x000001) {
        if (self->bytesRead < self->size)
            uriLen = self->size - self->bytesRead;
        if (uriLen > 0) {
            self->uri = (void*)MP4LocalCalloc(1, uriLen);
            TESTMALLOC(self->uri)
            GETBYTES(uriLen, uri);
        }
    }

    if (self->size > self->bytesRead) {
        bytesToRead = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(bytesToRead);
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateSchemeTypeAtom(MP4SchemeTypeAtomPtr* outAtom) {
    MP4Err err;
    MP4SchemeTypeAtomPtr self;

    self = (MP4SchemeTypeAtomPtr)MP4LocalCalloc(1, sizeof(MP4SchemeTypeAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4SchemeTypeAtomType;
    self->name = "Scheme Type Atom";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->uri = NULL;
    self->mode = 0;
    self->bSubsampleEncryption = FALSE;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
