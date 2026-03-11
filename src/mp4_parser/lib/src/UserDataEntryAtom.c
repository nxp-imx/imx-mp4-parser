/*
 ***********************************************************************
 * Copyright (c) 2005-2012, Freescale Semiconductor, Inc.
 * Copyright 2017, 2026 NXP
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
    $Id: UserDataEntryAtom.c,v 1.2 2000/05/17 08:01:32 francesc Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "Charset.h"
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4UserDataEntryAtomPtr self;
    self = (MP4UserDataEntryAtomPtr)s;
    if (self->data) {
        MP4LocalFree(self->data);
        self->data = NULL;
    }
    if (self->super)
        self->super->destroy(s);
}

static MP4Err serialize(struct MP4Atom* s, char* buffer) {
    MP4Err err = MP4NoErr;
    MP4UserDataEntryAtomPtr self = (MP4UserDataEntryAtomPtr)s;

    err = MP4SerializeCommonBaseAtomFields(s, buffer);
    if (err)
        goto bail;
    buffer += self->bytesWritten;

    PUT16(stringSize);
    PUT16(languageCode);

    if (self->dataSize && self->data) {
        PUTBYTES(self->data, self->dataSize);
    }
    assert(self->bytesWritten == self->size);
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err calculateSize(struct MP4Atom* s) {
    MP4Err err = MP4NoErr;
    MP4UserDataEntryAtomPtr self = (MP4UserDataEntryAtomPtr)s;

    err = MP4CalculateBaseAtomFieldSize(s);
    if (err)
        goto bail;
    self->size += 4 + self->dataSize;
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err = MP4NoErr;
    long bytesToRead;
    MP4UserDataEntryAtomPtr self = (MP4UserDataEntryAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);

    /* warning: for entry type not start with ascii 169 (0xa9), its content can be different */
    GET16(stringSize);
    GET16(languageCode);

    bytesToRead = (long)(s->size - s->bytesRead);
    if (bytesToRead > 0) {
        self->data = (char*)MP4LocalCalloc(1, bytesToRead);
        TESTMALLOC(self->data)

        GETBYTES(bytesToRead, data);
        self->dataSize = bytesToRead;

        /* check if the string is UTF-8 */
        if (MP4StringisUTF8(self->data, self->dataSize)) {
            MP4MSG("Got a UTF-8 string\n");
            goto bail;
        }

        /* FIXME: is no UTF-8 string. we assume it is (extended) ascii */
        MP4MSG("WARNING: The string is not UTF-8. Assume it is extended ASCII\n");
        {
            char* asciiData = self->data;
            u32 asciiDataSize = self->dataSize;

            self->dataSize = self->dataSize * 2;
            self->data = (char*)MP4LocalCalloc(1, self->dataSize);
            TESTMALLOC(self->data)

            err = MP4ConvertASCIItoUTF8((uint8*)asciiData, asciiDataSize, (uint8*)self->data,
                                        &(self->dataSize));
        }
    }

bail:
    TEST_RETURN(err);

    if (err && self && self->data) {
        MP4LocalFree(self->data);
        self->data = NULL;
    }
    return err;
}

MP4Err MP4CreateUserDataEntryAtom(MP4UserDataEntryAtomPtr* outAtom) {
    MP4Err err;
    MP4UserDataEntryAtomPtr self;

    self = (MP4UserDataEntryAtomPtr)MP4LocalCalloc(1, sizeof(MP4UserDataEntryAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;

    self->name = "user data atom entry";
    self->destroy = destroy;
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->data = NULL;
    self->calculateSize = calculateSize;
    self->serialize = serialize;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
