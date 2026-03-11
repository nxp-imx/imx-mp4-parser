/*
***********************************************************************
* Copyright (c) 2016, Freescale Semiconductor, Inc.
*
* Copyright 2017-2018, 2026 NXP
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************
*/
#include <stdlib.h>
#include "MP4Atoms.h"
#include "MP4Descriptors.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4PSSHAtomPtr self;
    err = MP4NoErr;
    self = (MP4PSSHAtomPtr)s;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->data) {
        MP4LocalFree(self->data);
        self->data = NULL;
    }
    if (self->totalData) {
        MP4LocalFree(self->totalData);
        self->totalData = NULL;
    }
    if (self->super)
        self->super->destroy(s);
bail:
    TEST_RETURN(err);

    return;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    MP4PSSHAtomPtr self = (MP4PSSHAtomPtr)s;
    u64 bytesToRead;
    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);
    if (err)
        goto bail;

    bytesToRead = s->size - s->bytesRead;
    if (bytesToRead < 20)
        BAILWITHERROR(MP4BadParamErr)

    GETBYTES(16, uid);
    GET32(dataLen);

    bytesToRead = s->size - s->bytesRead;

    if (self->dataLen > bytesToRead)
        BAILWITHERROR(MP4BadParamErr)

    self->data = (void*)MP4LocalCalloc(1, self->dataLen);
    TESTMALLOC(self->data)

    GETBYTES(self->dataLen, data);

    self->totalSize = self->dataLen + 20;
    self->totalData = (void*)MP4LocalCalloc(1, self->totalSize);
    TESTMALLOC(self->totalData)
    memcpy(self->totalData, &self->uid[0], 16);
    memcpy((u8*)self->totalData + 16, &self->dataLen, 4);
    memcpy((u8*)self->totalData + 20, self->data, self->dataLen);

    if (self->size > self->bytesRead) {
        bytesToRead = self->size - self->bytesRead;
        SKIPBYTES_FORWARD(bytesToRead);
    }

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err appendPssh(struct MP4PSSHAtom* self, struct MP4PSSHAtom* inAtom) {
    MP4Err err = MP4NoErr;
    void* temp = NULL;
    u32 tempSize = 0;
    if (self == NULL || inAtom == NULL)
        BAILWITHERROR(MP4BadParamErr)

    if (self->totalData == NULL || inAtom->totalData == NULL)
        BAILWITHERROR(MP4BadParamErr)

    tempSize = self->totalSize + inAtom->totalSize;
    temp = (void*)MP4LocalCalloc(1, tempSize);
    TESTMALLOC(temp)
    memcpy(temp, self->totalData, self->totalSize);
    memcpy((u8*)temp + self->totalSize, inAtom->totalData, inAtom->totalSize);
    if (self->totalData) {
        MP4LocalFree(self->totalData);
        self->totalData = temp;
        self->totalSize = tempSize;
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateProtectionSystemSpecificHeaderAtom(MP4PSSHAtomPtr* outAtom) {
    MP4Err err;
    MP4PSSHAtomPtr self;

    self = (MP4PSSHAtomPtr)MP4LocalCalloc(1, sizeof(MP4PSSHAtom));
    TESTMALLOC(self);

    err = MP4CreateFullAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4ProtectionSystemSpecificHeaderAtomType;
    self->name = "pssh";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->appendPssh = appendPssh;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
