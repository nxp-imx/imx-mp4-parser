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
    $Id: MP4Handle.c,v 1.4 2000/11/30 14:18:32 rtm Exp $
*/
#include <stdlib.h>
#include "MP4Movies.h"

typedef struct {
    char* data;
    u32 signature;
    u32 size;
    u32 allocatedSize;
} handleStruct, *actualHandle;

#define HANDLE_SIGNATURE 0x1234

MP4_EXTERN(MP4Err)

MP4NewHandle(u32 handleSize, MP4Handle* outHandle) {
    MP4Err err;
    MP4Handle ret;
    actualHandle h;
    ret = NULL;
    err = MP4NoErr;
    if (outHandle == NULL) {
        err = MP4BadParamErr;
        goto bail;
    }
    h = (actualHandle)MP4LocalCalloc(1, sizeof(handleStruct));
    if (h != NULL) {
        if (handleSize)
            h->data = (char*)MP4LocalCalloc(1, handleSize);
        if (handleSize && h->data == NULL) {
            MP4LocalFree(h);
            err = MP4NoMemoryErr;
            goto bail;
        } else {
            h->signature = HANDLE_SIGNATURE;
            h->size = handleSize;
            h->allocatedSize = handleSize;
            ret = (MP4Handle)h;
        }
    }
    *outHandle = ret;
bail:
    return err;
}

MP4_EXTERN(MP4Err)

MP4SetHandleSizeLocal(MP4Handle theHandle, u32 requestedSize) {
    MP4Err err;
    actualHandle h = (actualHandle)theHandle;
    err = MP4NoErr;
    if (h == NULL || (h->signature != HANDLE_SIGNATURE))
        err = MP4BadParamErr;
    else {
        if (h->allocatedSize >= requestedSize) {
            h->size = requestedSize;
        } else {
#if 0
			char *p = NULL;
			
			if ( h->data == NULL)
				p = (char *)MP4LocalMalloc( requestedSize );
			else
				p = (char *)MP4LocalReAlloc( h->data, requestedSize );
			if ( p )

			{
				h->data = p;
				h->size = requestedSize;
				h->allocatedSize = requestedSize;
			}
			else
				err = MP4NoMemoryErr;
#else
            h->size = requestedSize;
            h->allocatedSize = requestedSize;
#endif
        }
    }
    return err;
}

MP4_EXTERN(MP4Err)

MP4GetHandleSize(MP4Handle theHandle, u32* outSize) {
    MP4Err err;
    actualHandle h = (actualHandle)theHandle;

    err = MP4NoErr;

    if ((h == NULL) || (h->signature != HANDLE_SIGNATURE) || (outSize == NULL)) {
        err = MP4BadParamErr;
        goto bail;
    }
    *outSize = h->size;
bail:
    return err;
}

MP4_EXTERN(MP4Err)

MP4SetHandleSize(MP4Handle theHandle, u32 requestedSize) {
    MP4Err err;
    actualHandle h = (actualHandle)theHandle;
    err = MP4NoErr;
    if (h == NULL || (h->signature != HANDLE_SIGNATURE))
        err = MP4BadParamErr;
    else {
        if (h->allocatedSize >= requestedSize) {
            h->size = requestedSize;
        } else {
            char* p = NULL;

            if (h->data == NULL)
                p = (char*)MP4LocalMalloc(requestedSize);
            else
                p = (char*)MP4LocalReAlloc(h->data, requestedSize);
            if (p) {
                h->data = p;
                h->size = requestedSize;
                h->allocatedSize = requestedSize;
            } else
                err = MP4NoMemoryErr;
        }
    }
    return err;
}

MP4_EXTERN(MP4Err)

MP4DisposeHandle(MP4Handle theHandle) {
    MP4Err err;
    actualHandle h = (actualHandle)theHandle;
    err = MP4NoErr;
    if (h == NULL || (h->signature != HANDLE_SIGNATURE))
        err = MP4BadParamErr;
    else {
        if (h->data) {
            MP4LocalFree(h->data);
            h->data = NULL;
        }
        MP4LocalFree(h);
    }
    return err;
}
