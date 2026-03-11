/*
 ***********************************************************************
 * Copyright 2016 by Freescale Semiconductor, Inc.
 * Copyright 2025-2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ***********************************************************************
 */
#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4TrackEncryptionAtomPtr self;
    err = MP4NoErr;
    self = (MP4TrackEncryptionAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->default_key_constant_iv)
        MP4LocalFree(self->default_key_constant_iv);

    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    MP4TrackEncryptionAtomPtr self = (MP4TrackEncryptionAtomPtr)s;
    u8 defaultEntryptedByteBlock = 0;
    u8 defaultSkipByteBlock = 0;
    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    GETBYTES(3, default_IsEncrypted_bytes);

    self->default_IsEncrypted = (self->default_IsEncrypted_bytes[0] << 16) +
                                (self->default_IsEncrypted_bytes[1] << 8) +
                                self->default_IsEncrypted_bytes[2];

    self->origin_default_IsEncrypted = self->default_IsEncrypted;
    MP4MSG("MP4TrackEncryptionAtom default_IsEncrypted=%d", self->default_IsEncrypted);
    if (self->version == 1) {
        u32 pattern = self->default_IsEncrypted_bytes[1];
        defaultEntryptedByteBlock = pattern >> 4;
        defaultSkipByteBlock = (pattern & 0xf);
        if (0 == defaultEntryptedByteBlock && 0 == defaultSkipByteBlock)
            defaultEntryptedByteBlock = 1;

    } else if (self->default_IsEncrypted > 1)
        self->default_IsEncrypted = 1;

    GET8(default_IV_size);
    GETBYTES(16, default_KID);

    if (self->default_IsEncrypted == 0 && 0 != self->default_IV_size) {
        BAILWITHERROR(MP4BadParamErr)
    } else if (0 != self->default_IV_size && 8 != self->default_IV_size &&
               16 != self->default_IV_size)
        BAILWITHERROR(MP4BadParamErr)

    if (self->default_IsEncrypted != 0 && 0 == self->default_IV_size) {
        GET8(default_key_iv_Length);
        if (self->default_key_iv_Length != 8 && self->default_key_iv_Length != 16)
            BAILWITHERROR(MP4BadParamErr)

        self->default_key_constant_iv = MP4LocalMalloc(self->default_key_iv_Length);
        err = inputStream->readData(inputStream, (u32)self->default_key_iv_Length,
                                    self->default_key_constant_iv, NULL);
        if (err)
            goto bail;
        self->bytesRead += self->default_key_iv_Length;
    }

    self->default_EnctyptedByteBlock = defaultEntryptedByteBlock;
    self->default_SkipByteBlock = defaultSkipByteBlock;

    if (self->size > self->bytesRead) {
        u64 bytesToSkip = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(bytesToSkip);
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateTrackEncryptionAtom(MP4TrackEncryptionAtomPtr* outAtom) {
    MP4Err err;
    MP4TrackEncryptionAtomPtr self;

    self = (MP4TrackEncryptionAtomPtr)MP4LocalCalloc(1, sizeof(MP4TrackEncryptionAtom));
    TESTMALLOC(self)

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4TrackEncryptionAtomType;
    self->name = "Track Encryption Atom";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->default_key_constant_iv = NULL;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}