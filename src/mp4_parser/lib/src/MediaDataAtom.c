/*
 ***********************************************************************
 * Copyright 2005-2006 by Freescale Semiconductor, Inc.
 *
 * Copyright 2017-2018, 2026 NXP
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
    $Id: MediaDataAtom.c,v 1.3 2000/05/17 08:01:30 francesc Exp $
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4MediaDataAtomPtr self;
    self = (MP4MediaDataAtomPtr)s;
    if (self->data) {
        MP4LocalFree(self->data);
        self->data = NULL;
    }
    if (self->super)
        self->super->destroy(s);
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4Err err;
    u64 bytesToSkip;
    MP4MediaDataAtomPtr self = (MP4MediaDataAtomPtr)s;

#ifdef FSL_MP4_PARSER_DBG_TIME
#if defined(__WINCE) || defined(WIN32)
    u32 start_time = timeGetTime();
    u32 end_time;
#else
    struct timezone time_zone;
    struct timeval start_time;
    struct timeval end_time;
    gettimeofday(&start_time, &time_zone);
#endif
#endif

    err = MP4NoErr;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    err = self->super->createFromInputStream(s, proto, (char*)inputStream);

    if (!(inputStream->stream_flags & live_flag)) {
        bytesToSkip = s->size - s->bytesRead;
        MP4MSG("mdat size: %lld, bytes read: %lld, to skip %lld\n", (u64)s->size, s->bytesRead,
               bytesToSkip);

        SKIPBYTES_FORWARD(bytesToSkip);
    }
    /* else */
    /* for live steaming, don't skip since we should avoid seeking */

bail:
    TEST_RETURN(err);

    if (err && self && self->data) {
        MP4LocalFree(self->data);
        self->data = NULL;
    }

#ifdef FSL_MP4_PARSER_DBG_TIME
#if defined(__WINCE) || defined(WIN32)
    end_time = timeGetTime();
    MP4DbgLog("\ntime for mdat: %ld ms\n\n", end_time - start_time);
    MP4DbgLog("start %ld ms, end %ld ms\n\n", start_time, end_time);
#else
    gettimeofday(&end_time, &time_zone);
    if (end_time.tv_usec >= start_time.tv_usec) {
        MP4DbgLog("mdat: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec,
                  end_time.tv_usec - start_time.tv_usec);
    } else {
        MP4DbgLog("mdat: %ld sec, %ld us\n", end_time.tv_sec - start_time.tv_sec - 1,
                  end_time.tv_usec + 1000000000 - start_time.tv_usec);
    }

    MP4DbgLog("start %ld sec, %ld us\n\n", start_time.tv_sec, start_time.tv_usec);
#endif
#endif

    return err;
}

MP4Err MP4CreateMediaDataAtom(MP4MediaDataAtomPtr* outAtom) {
    MP4Err err;
    MP4MediaDataAtomPtr self;

    self = (MP4MediaDataAtomPtr)MP4LocalCalloc(1, sizeof(MP4MediaDataAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4MediaDataAtomType;
    self->name = "media data";
    self->destroy = destroy;
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->data = NULL;

    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
