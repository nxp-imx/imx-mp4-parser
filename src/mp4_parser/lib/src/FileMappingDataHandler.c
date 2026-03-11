/*
 ***********************************************************************
 * Copyright (c) 2005-2012, Freescale Semiconductor, Inc.
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
    $Id: FileMappingDataHandler.c,v 1.9 1999/11/19 07:49:44 mc Exp $
*/
//#define DEBUG

#include "FileMappingDataHandler.h"
#include <stdlib.h>
#include <string.h>
#include "MP4DataHandler.h"
#include "MP4Impl.h"
#include "MP4Movies.h"
#include "MP4OSMacros.h"

static MP4Err createCanonicalPathName(struct MP4InputStreamRecord* inputStream,
                                      MP4DataEntryAtomPtr dataEntry, MP4Handle outPathNameH);

static MP4Err createCanonicalPathName(struct MP4InputStreamRecord* inputStream,
                                      MP4DataEntryAtomPtr dataEntry, MP4Handle outPathNameH) {
    MP4Err err;
    char* fname;
    if ((dataEntry == NULL) || (dataEntry->flags == 1) || (outPathNameH == NULL)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    err = MP4NoErr;

    fname = dataEntry->location;
    if (NULL == fname) {
        BAILWITHERROR(MP4BadParamErr);
    }
    if ((strncmp(fname, "file://", 7) == 0) || (strncmp(fname, "file|//", 7) == 0)) {
        /* absolute URL */
        err = MP4SetHandleSize(outPathNameH, strlen(fname + 6));
        if (err)
            goto bail;
        strcpy(*outPathNameH, fname + 7);
    } else {
        /* a relative file name */
        char* parent = NULL;
        if (inputStream) {
            parent = inputStream->getFileMappingObject(inputStream)->parent;
        }
        if (parent) {
            /* prepend parent path */
            err = MP4SetHandleSize(outPathNameH, (strlen(parent) + strlen(fname) + 1));
            if (err)
                goto bail;
            strcpy(*outPathNameH, parent); /* parent includes platform-specific path separator */
            strcat(*outPathNameH, fname);
        } else {
            /* bare: open relative to cwd */
            err = MP4SetHandleSize(outPathNameH, (strlen(fname) + 1));
            if (err)
                goto bail;
            strcat(*outPathNameH, fname);
        }
    }

bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err doOpen(struct MP4DataHandler* s, struct MP4InputStreamRecord* inputStream,
                     MP4DataEntryAtomPtr dataEntry) {
    MP4Err err;
    MP4FileMappingDataHandlerPtr self = (MP4FileMappingDataHandlerPtr)s;
    MP4Handle pathH = NULL;

    err = MP4NoErr;
    if (dataEntry->flags == 1) {
        /* self-contained: get from movie */
        if (inputStream == NULL)
            BAILWITHERROR(MP4BadParamErr)
        self->mappingObject = inputStream->getFileMappingObject(inputStream);
        if (self->mappingObject == NULL)
            BAILWITHERROR(MP4IOErr)

    } else {
        err = MP4NewHandle(1024, &pathH);
        if (err)
            goto bail;
        err = createCanonicalPathName(inputStream, dataEntry, pathH);
        if (err)
            goto bail;
        err = MP4CreateFileMappingObject(*pathH, &self->mappingObject,
                                         self->mappingObject->parser_context);
        if (err)
            goto bail;
        err = MP4DisposeHandle(pathH);
        if (err)
            goto bail;
    }
bail:
    if (pathH) {
        MP4DisposeHandle(pathH);
    }
    TEST_RETURN(err);

    return err;
}

static MP4Err doClose(struct MP4DataHandler* s) {
    MP4Err err = MP4NoErr;
    MP4FileMappingDataHandlerPtr self = (MP4FileMappingDataHandlerPtr)s;

    // fix ENGR00214496.
    // the mappingObject is already close and freed in the MP4DeleteParser()
    //   self->fileMappingObject->close( self->fileMappingObject );
    //   self->fileMappingObject->destroy( self->fileMappingObject );

    // So no need to close here.
    // Also the fileMappingObject is the goble resource in the parser handle,
    // should not be closed in a single track.

    //	err = self->mappingObject->close( self->mappingObject );

    MP4LocalFree(self);
    return err;
}

static MP4Err copyData(struct MP4DataHandler* s, u64 offset, char* dst, u32 bytes) {
    MP4Err err;
    MP4FileMappingDataHandlerPtr self = (MP4FileMappingDataHandlerPtr)s;

    err = MP4NoErr;
    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    if (self->mappingObject) {
#ifdef MALLOC_FILE_SIZE
        char* src = self->mappingObject->data + offset;
        memcpy(dst, src, bytes);
#else
        {
            u64 cur = MP4LocalGetCurrentFilePos((FILE*)self->mappingObject->data,
                                                self->mappingObject->parser_context);
            if (self->inputStream->stream_flags & live_flag) {
                if ((offset > cur) && (offset - cur < 1024 * 1024)) {  // use read to replace seek
                    char* temp = (char*)MP4LocalMalloc(offset - cur);
                    TESTMALLOC(temp);
                    MP4MSG("dataHandler copyData(%d): live mode seek %lld => %lld \n", bytes, cur,
                           offset);
                    MP4LocalReadFile(temp, sizeof(char), offset - cur,
                                     (FILE*)self->mappingObject->data,
                                     self->mappingObject->parser_context);
                    MP4LocalFree(temp);
                    self->inputStream->file_offset = 0;
                } else if (offset == cur) {
                    // do nothing
                } else {
                    MP4MSG("!!! dataHandler copyData(%d): live mode seek %lld => %lld unsupported "
                           "!!!\n",
                           bytes, cur, offset);
                }
            } else
                MP4LocalSeekFile((FILE*)self->mappingObject->data, offset, SEEK_SET,
                                 self->mappingObject->parser_context);
            /* Warning: never directly set absolute file offset to context! The lib shall not modify
             * the context*/
            MP4LocalReadFile(dst, sizeof(char), bytes, (FILE*)self->mappingObject->data,
                             self->mappingObject->parser_context);
        }
#endif
    }

bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4CreateFileMappingDataHandler(MP4InputStreamPtr inputStream, MP4DataEntryAtomPtr dataEntry,
                                       MP4DataHandlerPtr* outDataHandler) {
    MP4Err err;
    MP4FileMappingDataHandlerPtr self;

    err = MP4NoErr;

    self = (MP4FileMappingDataHandlerPtr)MP4LocalCalloc(1, sizeof(MP4FileMappingDataHandler));
    TESTMALLOC(self);
    self->inputStream = inputStream;
    self->copyData = copyData;
    self->entryAtom = dataEntry;
    self->open = doOpen;
    self->close = doClose;
    err = doOpen((MP4DataHandlerPtr)self, inputStream, dataEntry);
    *outDataHandler = (MP4DataHandlerPtr)self;

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4DisposeFileMappingDataHandler(MP4DataHandlerPtr dataHandler,
                                        MP4DataEntryAtomPtr dataEntry) {
    MP4Err err;

    err = MP4NoErr;
    if (dataEntry->flags == 1) {
        /* contained in the same file as the moov atom:
        do not close the file
        Amanda: But each track will have its own data hander, shall not check this flag*/
        return err;
    }
    err = doClose(dataHandler);
    // MP4LocalFree(dataHandler); /* double free. doClose() free this memory.*/

    TEST_RETURN(err);
    return err;
}

MP4Err MP4PreflightFileMappingDataHandler(MP4InputStreamPtr inputStream,
                                          MP4DataEntryAtomPtr dataEntry) {
    MP4Err err;
    MP4Handle pathH = NULL;
    err = MP4NewHandle(1024, &pathH);
    if (err)
        goto bail;
    err = createCanonicalPathName(inputStream, dataEntry, pathH);
    if (err)
        goto bail;
    err = MP4AssertFileExists(*pathH);
    if (err)
        goto bail;
bail:
    if (pathH) {
        MP4DisposeHandle(pathH);
    }
    TEST_RETURN(err);
    return err;
}
