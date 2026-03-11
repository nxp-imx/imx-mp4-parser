/************************************************************************
 * Copyright (c) 2014,2016, Freescale Semiconductor, Inc.
 * Copyright 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    u32 i;
    MP4MovieFragmentAtomPtr self;
    self = (MP4MovieFragmentAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)
    DESTROY_ATOM_LIST

    err = MP4DeleteLinkedList(self->trackList);
    if (err)
        goto bail;

    if (self->super)
        self->super->destroy(s);

bail:
    TEST_RETURN(err);

    return;
}

static MP4Err getStartOffset(MP4MovieFragmentAtomPtr self, s64* outOffset) {
    MP4Err err;

    err = MP4NoErr;
    if (outOffset == NULL)
        BAILWITHERROR(MP4BadParamErr);
    *outOffset = self->start_offset;
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err getEndOffset(MP4MovieFragmentAtomPtr self, s64* outOffset) {
    MP4Err err;

    err = MP4NoErr;
    if (outOffset == NULL)
        BAILWITHERROR(MP4BadParamErr);
    *outOffset = self->end_offset;
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err addAtom(MP4MovieFragmentAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    err = MP4NoErr;
    assert(atom);

    err = MP4AddListEntry(atom, self->atomList);
    if (err)
        goto bail;
    switch (atom->type) {
        case MP4TrackFragmentAtomType:
            err = MP4AddListEntry(atom, self->trackList);
            if (err)
                goto bail;
            break;
    }
bail:
    TEST_RETURN(err);

    return err;
}

static u32 getTrackCount(MP4MovieFragmentAtomPtr self) {
    MP4Err err = MP4NoErr;
    u32 trackCount = 0;

    if (self == NULL)
        return 0;

    err = MP4GetListEntryCount(self->trackList, &trackCount);
    if (err != MP4NoErr)
        trackCount = 0;

    return trackCount;
}

MP4Err getTrack(MP4MovieFragmentAtomPtr self, u32 TrackID, MP4AtomPtr* outTrack) {
    MP4Err err;

    err = MP4NoErr;
    if ((TrackID == 0) || (self == NULL) || (TrackID > getTrackCount(self)))
        BAILWITHERROR(MP4BadParamErr)
    err = MP4GetListEntry(self->trackList, TrackID - 1, (char**)outTrack);
    if (err)
        goto bail;
bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    MP4MovieFragmentAtomPtr ptr = (MP4MovieFragmentAtomPtr)s;
    MP4FileMappingInputStreamPtr stream = (MP4FileMappingInputStreamPtr)inputStream;

    if (ptr == NULL)
        return MP4BadParamErr;
    ptr->start_offset =
            MP4LocalGetCurrentFilePos((FILE*)(stream->ptr), stream->mapping->parser_context);
    ptr->start_offset -= 8;
    {
        PARSE_ATOM_LIST(MP4MovieFragmentAtom)

    bail:
        ptr->end_offset = ptr->start_offset + ptr->size;

        TEST_RETURN(err);

        return err;
    }
}

MP4Err MP4CreateMovieFragmentAtom(MP4MovieFragmentAtomPtr* outAtom) {
    MP4Err err;
    MP4MovieFragmentAtomPtr self;

    self = (MP4MovieFragmentAtomPtr)MP4LocalCalloc(1, sizeof(MP4MovieFragmentAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4MovieFragmentAtomType;
    self->name = "movie fragment";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->start_offset = 0;
    self->end_offset = 0;

    self->addAtom = addAtom;
    self->getStartOffset = getStartOffset;
    self->getEndOffset = getEndOffset;

    self->getTrackCount = getTrackCount;
    self->getTrack = getTrack;
    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;
    err = MP4MakeLinkedList(&self->trackList);
    if (err)
        goto bail;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
