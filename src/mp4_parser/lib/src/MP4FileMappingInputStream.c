/*
***********************************************************************
* Copyright (c) 2005-2014, Freescale Semiconductor, Inc.
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
    $Id: MP4FileMappingInputStream.c,v 1.3 2000/05/17 08:01:29 francesc Exp $
*/
//#define DEBUG
#include "MP4InputStream.h"
#include "MP4Impl.h"
#include <stdlib.h>
#include <string.h>


#define CHECK_AVAIL(bytes)         \
    if (self->available < bytes) { \
        err = MP4IOErr;            \
        goto bail;                 \
    }  // Char *outData Ranjeet;
static MP4Err readData(struct MP4InputStreamRecord* s, u32 bytes, void* outData, char* msg);

static void doIndent(struct MP4InputStreamRecord* self) {
#ifndef COLDFIRE
    u32 i;
    for (i = 0; i < self->indent; i++) fprintf(stdout, "    ");
#endif
}

static void msg(struct MP4InputStreamRecord* self, char* msg) {
#ifndef COLDFIRE
    if (self->stream_flags & debug_flag) {
        doIndent(self);
        fprintf(stdout, "%s\n", msg);
    }
#endif
}

static void destroy(struct MP4InputStreamRecord* s) {
    MP4LocalFree(s);
}

static FileMappingObject getFileMappingObject(struct MP4InputStreamRecord* s) {
    MP4FileMappingInputStreamPtr self = (MP4FileMappingInputStreamPtr)s;
    return self->mapping;
}

static MP4Err read8(struct MP4InputStreamRecord* s, u32* outVal, char* msg) {
    MP4Err err;
    u8 val;
    MP4FileMappingInputStreamPtr self = (MP4FileMappingInputStreamPtr)s;
    err = MP4NoErr;
    err = readData(s, 1, (char*)&val, NULL);
    if (err)
        goto bail;
    *outVal = val;
#ifndef COLDFIRE
    if (msg && self->debugging) {
        doIndent(s);
        fprintf(stdout, "%s = %d\n", msg, val);
    }
#endif

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err read16(struct MP4InputStreamRecord* s, u32* outVal, char* msg) {
    u8 a;
    int val;
    MP4Err err;
    MP4FileMappingInputStreamPtr self = (MP4FileMappingInputStreamPtr)s;
    err = MP4NoErr;
    //	CHECK_AVAIL( 2 )
    err = readData(s, 1, (char*)&a, NULL);
    if (err)
        goto bail;
    val = a << 8;
    err = readData(s, 1, (char*)&a, NULL);
    if (err)
        goto bail;
    val |= a;
    *outVal = val;
#ifndef COLDFIRE
    if (msg && (self->stream_flags & debug_flag)) {
        doIndent(s);
        fprintf(stdout, "%s = %d\n", msg, val);
    }
#endif

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err read32(struct MP4InputStreamRecord* s, u32* outVal, char* msg) {
    int hw;
    int lw;
    MP4Err err;
    MP4FileMappingInputStreamPtr self = (MP4FileMappingInputStreamPtr)s;
    err = MP4NoErr;
    CHECK_AVAIL(4)
    err = read16(s, (u32*)&hw, NULL);
    if (err)
        goto bail;
    err = read16(s, (u32*)&lw, NULL);
    if (err)
        goto bail;
    *outVal = (u32)(hw << 16) | lw;

#ifndef COLDFIRE
    if (msg && (self->stream_flags & debug_flag)) {
        doIndent(s);
        fprintf(stdout, "%s = %d\n", msg, (int)(*outVal));
    }
#endif
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err readData(struct MP4InputStreamRecord* s, u32 bytes, void* outData, char* msg) {
    MP4Err err;
    MP4FileMappingInputStreamPtr self = (MP4FileMappingInputStreamPtr)s;

    err = MP4NoErr;
    if (bytes) /* Amanda: For safety, "outData" may point to a fake memory of size 0 */
    {
        CHECK_AVAIL(bytes)
#ifdef MALLOC_FILE_SIZE
        memcpy(outData, self->ptr, bytes);
        self->ptr += bytes;
#else
        {
            if (self->file_offset) {
                if (self->stream_flags & live_flag) {
                    if (self->file_offset > 0 && self->file_offset < 1024 * 1024) {
                        (void)MP4LocalGetCurrentFilePos((FILE*)self->ptr,
                                                        self->mapping->parser_context);
                        char* temp = (char*)MP4LocalMalloc(self->file_offset);
                        TESTMALLOC(temp);
                        MP4MSG("inputStream readData(%d): live mode seek offset %lld at pos %lld\n",
                               bytes, self->file_offset, pos);
                        MP4LocalReadFile(temp, sizeof(char), self->file_offset, (FILE*)self->ptr,
                                         self->mapping->parser_context);
                        MP4LocalFree(temp);
                    } else {
                        if (self->file_offset < 0) {
                            /* Read the data from the buffer for backward seek operation */
                            u32 buf_size = sizeof(self->buf) / sizeof(self->buf[0]);

                            if ((self->file_offset + buf_size) >= 0) {
                                u32 pos = (u32)(llabs(self->file_offset) - 1);

                                if ((self->buf_mask & (1 << pos)) && bytes == 1) {
                                    *((u8*)outData) = self->buf[pos];
                                    MP4MSG("inputStream readData(%d): live mode, seek offset: "
                                           "%lld, data: 0x%x, mask: 0x%x\n",
                                           bytes, self->file_offset, *((u8*)outData),
                                           self->buf_mask);
                                    self->buf[pos] = 0;
                                    self->file_offset += bytes;
                                    self->buf_mask &= ~(1 << pos);
                                    goto done;
                                }
                            }
                        }
                        MP4MSG("!!! inputStream readData(%d): live mode seek offset %lld "
                               "unsupported !!!\n",
                               bytes, self->file_offset);
                    }
                } else {
                    MP4LocalSeekFile((FILE*)self->ptr, self->file_offset, SEEK_CUR,
                                     self->mapping->parser_context);
                }
                self->file_offset = 0;
            }

            MP4LocalReadFile(outData, sizeof(char), bytes, (FILE*)self->ptr,
                             self->mapping->parser_context);
        }
#endif
    done:
        self->available -= bytes;
#ifndef COLDFIRE
        if (msg && (self->stream_flags & debug_flag)) {
            doIndent(s);
            fprintf(stdout, "%s = [%d bytes of data]\n", msg, (int)bytes);
        }
#endif
    }

bail:
    TEST_RETURN(err);

    return err;
}

static s64 checkAvailableBytes(struct MP4InputStreamRecord* s, int64 bytesRequested) {
    MP4FileMappingInputStreamPtr self = (MP4FileMappingInputStreamPtr)s;
    s64 bytesAvailable;

    bytesAvailable = MP4LocalCheckAvailableBytes((FILE*)self->ptr, bytesRequested,
                                                 self->mapping->parser_context);
    return bytesAvailable;
}

MP4Err seekTo(struct MP4InputStreamRecord* s, s64 offset, u32 flag, char* msg) {
    MP4Err err;
    s64 pos;
    s64 filesize = 0;
    s64 seekTo = 0;
    MP4FileMappingInputStreamPtr self = (MP4FileMappingInputStreamPtr)s;
    err = MP4NoErr;
    filesize = MP4LocalFileSize(self->ptr, self->mapping->parser_context);
    seekTo = (s64)offset;
    if (SEEK_SET == flag) {
        if (filesize > 0 && seekTo > filesize) {
            err = MP4BadParamErr;
            goto bail;
        }
    }
    pos = MP4LocalGetCurrentFilePos((FILE*)self->ptr, self->mapping->parser_context);
    MP4MSG("inputStream seekTo: flag %d offset %lld, cur pos %lld\n", flag, offset, pos);

    if (self->stream_flags & live_flag) {
        if ((flag == SEEK_SET && offset - pos > 1024 * 1024) ||
            (flag == SEEK_SET && offset < pos) || (flag == SEEK_END) ||
            (flag == SEEK_CUR && offset < 0)) {
            MP4MSG("!!! inputStream seekTo: live mode seek flag %d offset %lld unsupported !!!\n",
                   flag, offset);
        } else {
            u32 dataLen = 0;
            if (flag == SEEK_SET)
                dataLen = offset - pos;
            else if (flag == SEEK_CUR)
                dataLen = offset;
            if (dataLen > 0) {
                char* temp = (char*)MP4LocalMalloc(dataLen);
                TESTMALLOC(temp);
                MP4LocalReadFile(temp, sizeof(char), dataLen, (FILE*)self->ptr,
                                 self->mapping->parser_context);
                MP4LocalFree(temp);
            }
        }
    } else {
        MP4LocalSeekFile((FILE*)self->ptr, seekTo, flag, self->mapping->parser_context);
    }

    self->file_offset = 0;
    self->available = filesize - seekTo;
#ifndef COLDFIRE
    if (msg && (self->stream_flags & debug_flag)) {
        doIndent(s);
        fprintf(stdout, "%s = %lld\n", msg, offset);
    }
#endif
bail:
    TEST_RETURN(err);
    return err;
}

static s64 getFilePos(struct MP4InputStreamRecord* s, char* msg) {
    MP4FileMappingInputStreamPtr self = (MP4FileMappingInputStreamPtr)s;
    s64 pos;

    pos = MP4LocalGetCurrentFilePos((FILE*)self->ptr, self->mapping->parser_context);

    if (msg && (self->stream_flags & debug_flag)) {
        doIndent(s);
        fprintf(stdout, "%s = %lld\n", msg, pos);
    }

    return pos;
}

MP4Err MP4CreateFileMappingInputStream(struct FileMappingObjectRecord* mapping,
                                       MP4InputStreamPtr* outStream, uint32 flag) {
    MP4Err err;
    MP4FileMappingInputStreamPtr is;

    err = MP4NoErr;
    is = (MP4FileMappingInputStreamPtr)MP4LocalCalloc(1, sizeof(MP4FileMappingInputStream));
    TESTMALLOC(is)
    is->available = mapping->size;
    is->base = mapping->data;
    is->ptr = mapping->data;
    is->destroy = destroy;
    is->read8 = read8;
    is->read16 = read16;
    is->read32 = read32;
    is->readData = readData;
    is->getFileMappingObject = getFileMappingObject;
    is->mapping = mapping;
    is->msg = msg;
    is->stream_flags = 0x0;
    if ((flag & FILE_FLAG_NON_SEEKABLE) && (flag & FILE_FLAG_READ_IN_SEQUENCE)) {
        is->stream_flags |= live_flag;
    }
    if (!(FILE_FLAG_READ_IN_SEQUENCE & flag)) {
        is->stream_flags |= memOptimization_flag;
    }
    is->checkAvailableBytes = checkAvailableBytes;
    is->seekTo = seekTo;
    is->getFilePos = getFilePos;
    memset(is->buf, 0, sizeof(is->buf) / sizeof(is->buf[0]));
    is->buf_mask = 0;
    *outStream = (MP4InputStreamPtr)is;
bail:
    return err;
}
