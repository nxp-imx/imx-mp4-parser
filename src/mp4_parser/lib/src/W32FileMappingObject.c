/*
 ***********************************************************************
 * Copyright (c) 2005-2012,2016, Freescale Semiconductor, Inc.
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
    $Id: W32FileMappingObject.c,v 1.6 1999/11/20 08:07:49 mc Exp $
*/
#include "W32FileMappingObject.h"
#include <string.h>
#include "MP4OSMacros.h"
#ifndef COLDFIRE

#if !(defined(__WINCE) || defined(WIN32))  // Wince doesn't support Stat method TLSbo80080
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif  //__WINCE

#endif
#include <stdio.h>
#include <stdlib.h>
//#include "MP4UtilityFile.h"

static MP4Err doClose(struct FileMappingObjectRecord* s);
static MP4Err doOpen(struct FileMappingObjectRecord* s, const char* filename);
static MP4Err isYourFile(struct FileMappingObjectRecord* s, const char* filename, u32* outSameFile);

static MP4Err destroy(struct FileMappingObjectRecord* s) {
    MP4Err err;
    Win32FileMappingObject self = (Win32FileMappingObject)s;

    err = MP4NoErr;

    if (self->fd) {
        MP4MSG("Destroy file mapping object & close fd %p\n", self->fd);
        MP4LocalFileClose(self->fd, self->parser_context);
        self->fd = NULL;
    }

    if (self->fileName) {
        err = doClose(s);
        if (err)
            goto bail;
    }
    MP4LocalFree(s);
bail:
    return err;
}

static MP4Err doOpen(struct FileMappingObjectRecord* s, const char* filename) {
    MP4Err err;
    u64 file_size;

    Win32FileMappingObject self = (Win32FileMappingObject)s;
    err = MP4NoErr;

    /* Holds the Handle of the File. */
    self->fd = (FILE*)MP4LocalFileOpen(filename, (const u8*)"rb", self->parser_context);
    if (NULL == self->fd)
        BAILWITHERROR(PARSER_FILE_OPEN_ERROR)

    file_size = MP4LocalFileSize(self->fd, self->parser_context);
    MP4MSG("file size: %lld\n", file_size);

    if ((s64)file_size <= 0) {
        // can not get correct file size, set it to 64GB
        MP4MSG("warning: file size <= 0 (%lld), set it to the max\n", (s64)file_size);
        file_size = 0x1000000000ULL;
    }

    self->size = file_size;
#ifdef MALLOC_FILE_SIZE
    // File of the current file. ranjeet
    self->data = (char*)MP4LocalMalloc(self->size);

    if (self->data == NULL)
        BAILWITHERROR(MP4IOErr);

    MP4LocalReadFile(self->data, 1, self->size, self->fd) != self->size)

#else
    self->data = (char*)self->fd;
#endif

bail:
    if(err)
        MP4LocalFileClose(self->fd, self->parser_context);

    return err;
}

static MP4Err doClose(struct FileMappingObjectRecord* s) {
    MP4Err err;
    Win32FileMappingObject self = (Win32FileMappingObject)s;

    err = MP4NoErr;

    if (self->fileName) {
        MP4LocalFree(self->fileName);
        self->fileName = NULL;
    }
    /*
        if(self->data){
            MP4LocalFree(self->data);
            self->data = NULL;
        }
    */

    return err;
}

static MP4Err isYourFile(struct FileMappingObjectRecord* s, const char* filename,
                         u32* outSameFile) {
    MP4Err err;
    Win32FileMappingObject self = (Win32FileMappingObject)s;
    int result;
    err = MP4NoErr;
#if defined(_MSC_VER)
    result = _stricmp(self->fileName, filename);
#else
    result = strcmp(self->fileName, filename);
#endif
    *outSameFile = result ? 0 : 1;
    return err;
}

MP4Err MP4CreateWin32FileMappingObject(char* filename, FileMappingObject* outObject,
                                       void* demuxer) {
    MP4Err err;
    Win32FileMappingObject self;

    err = MP4NoErr;
    self = (Win32FileMappingObject)MP4LocalCalloc(1, sizeof(Win32FileMappingObjectRecord));
    TESTMALLOC(self);
    self->destroy = destroy;
    self->open = doOpen;
    self->close = doClose;
    self->isYourFile = isYourFile;
    self->parser_context = demuxer;
    err = doOpen((FileMappingObject)self, filename);
    if (err)
        goto bail;
    *outObject = (FileMappingObject)self;
bail:
    TEST_RETURN(err);
    if (err) {
        if (self)
            MP4LocalFree(self);
    }

    return err;
}

MP4Err MP4CreateFileMappingObject(char* urlString, FileMappingObject* outObject, void* demuxer) {
    char* pathname = urlString;

    return MP4CreateWin32FileMappingObject(pathname, outObject, demuxer);
}

MP4Err MP4AssertFileExists(char* pathName) {
    MP4Err err;
#if !(defined(__WINCE) || defined(WIN32))
#ifndef COLDFIRE
    struct stat buf;

    err = stat(pathName, &buf);
    if (err) {
        BAILWITHERROR(MP4FileNotFoundErr);
    }
#else
    asm(" halt");
#endif

bail:
    return err;

#else  //__WINCE TLSbo80080
    /*Wince doesn't support stat method*/

    FILE* fp = fopen(pathName, "r");
    err = MP4NoErr;

    if (fp == NULL)
        err = MP4FileNotFoundErr;

    fclose(fp);
    return err;

#endif  //__WINCE TLSbo80080
}
