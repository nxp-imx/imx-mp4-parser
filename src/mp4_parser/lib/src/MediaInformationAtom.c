/*
 ***********************************************************************
 * Copyright (c) 2011-2013, Freescale Semiconductor, Inc.
 * Copyright 2017, 2020-2021, 2026 NXP
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
    $Id: MediaInformationAtom.c,v 1.5 2000/06/05 12:06:56 francesc Exp $
*/

#include <stdlib.h>
#include <string.h>
#include "MP4Atoms.h"
#include "MP4DataHandler.h"

static MP4Err closeDataHandler(MP4AtomPtr s);

static void destroy(MP4AtomPtr s) {
    MP4Err err;
    MP4MediaInformationAtomPtr self;
    u32 i;

    err = MP4NoErr;
    self = (MP4MediaInformationAtomPtr)s;

    if (self == NULL)
        BAILWITHERROR(MP4BadParamErr)

    err = closeDataHandler(s); /* close the data handler */

    if (self->atomList) {
        u32 atomListSize;
        err = MP4GetListEntryCount(self->atomList, &atomListSize);
        if (err)
            goto bail;

        for (i = 0; i < atomListSize; i++) {
            MP4AtomPtr a;
            err = MP4GetListEntry(self->atomList, i, (char**)&a);
            if (err)
                goto bail;
            if (a)
                a->destroy(a);
        }
        err = MP4DeleteLinkedList(self->atomList);
        if (err)
            goto bail;
        self->atomList = NULL;
    }

    if (self->super)
        self->super->destroy(s);

bail:
    TEST_RETURN(err);

    return;
}

static MP4Err addAtom(MP4MediaInformationAtomPtr self, MP4AtomPtr atom) {
    MP4Err err;
    err = MP4NoErr;
    err = MP4AddListEntry(atom, self->atomList);
    if (err)
        goto bail;
    switch (atom->type) {
        case MP4MediaHeaderAtomType:
        case MP4VideoMediaHeaderAtomType:
        case MP4SoundMediaHeaderAtomType:
        case MP4HintMediaHeaderAtomType:
            if (self->mediaHeader)
                BAILWITHERROR(MP4BadDataErr)
            self->mediaHeader = atom;
            break;

        case MP4DataInformationAtomType:
            if (self->dataInformation)
                BAILWITHERROR(MP4BadDataErr)
            self->dataInformation = atom;
            break;

        case MP4SampleTableAtomType:
            if (self->sampleTable)
                BAILWITHERROR(MP4BadDataErr)
            self->sampleTable = atom;
            break;
    }
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err testDataEntry(struct MP4MediaInformationAtom* self, u32 dataEntryIndex) {
    MP4Err err;
    MP4DataInformationAtomPtr dinf;
    MP4DataReferenceAtomPtr dref;
    MP4DataEntryAtomPtr dataEntryAtom;

    err = MP4NoErr;
    if ((self == NULL) || (dataEntryIndex == 0))
        BAILWITHERROR(MP4BadParamErr)
    if (self->dataHandler && (self->dataEntryIndex == dataEntryIndex)) {
        /* already open */
    } else {
        dinf = (MP4DataInformationAtomPtr)self->dataInformation;
        if (dinf == NULL)
            BAILWITHERROR(MP4InvalidMediaErr)
        dref = (MP4DataReferenceAtomPtr)dinf->dataReference;
        if (dref == NULL)
            BAILWITHERROR(MP4InvalidMediaErr)
        if (dataEntryIndex > dref->getEntryCount(dref))
            BAILWITHERROR(MP4BadParamErr)
        err = dref->getEntry(dref, dataEntryIndex, &dataEntryAtom);
        if (err)
            goto bail;
        if (dataEntryAtom == NULL)
            BAILWITHERROR(MP4InvalidMediaErr)

        // unknown atom in dref atom, do not read the url to get file name
        if (dataEntryAtom->type != MP4DataEntryURLAtomType &&
            dataEntryAtom->type != MP4DataEntryURNAtomType)
            dataEntryAtom->flags = 1;

        err = MP4PreflightDataHandler(self->inputStream, dataEntryAtom);
        if (err)
            goto bail;
    }
bail:
    TEST_RETURN(err);

    return err;
}

/* Guido : inserted to clean-up resources */
static MP4Err closeDataHandler(MP4AtomPtr s) {
    MP4Err err;
    MP4MediaInformationAtomPtr self;
    MP4DataInformationAtomPtr dinf;
    MP4DataReferenceAtomPtr dref;
    MP4DataEntryAtomPtr dataEntryAtom = NULL;

    err = MP4NoErr;
    if (s == NULL)
        BAILWITHERROR(MP4BadParamErr)

    self = (MP4MediaInformationAtomPtr)s;
    dinf = (MP4DataInformationAtomPtr)self->dataInformation;
    if (dinf == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);
    dref = (MP4DataReferenceAtomPtr)dinf->dataReference;
    if (dref == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);

    /* If no Access Unit was requested, dataEntryIndex was not initialized (remains 0)
    and no data handler will be created */
    // MP4MSG("dataEntryIndex %d, data handler %p... \n", self->dataEntryIndex, self->dataHandler);
    if (self->dataEntryIndex > 0) {
        /* only if at least one sample has been read, the dataEntryIndex will not be zero */
        err = dref->getEntry(dref, self->dataEntryIndex, &dataEntryAtom);
        if (err)
            goto bail;
        if (dataEntryAtom == NULL)
            BAILWITHERROR(MP4InvalidMediaErr)
    }

    if (self->dataHandler) {
        /* Amanda: do NOT use MP4DisposeDataHandler, because it assume data handler is shared by all
        tracks. But now each track has its own data handler .
        TODO: Further check under streaming case. */

        MP4DataHandlerPtr dhlr;
        dhlr = (MP4DataHandlerPtr)self->dataHandler;
        if (dhlr != NULL)
            dhlr->close(dhlr);

        // err = MP4DisposeDataHandler( self->dataHandler, dataEntryAtom ); if (err) goto bail;
        self->dataHandler = NULL;
    }
    self->dataEntryIndex = 0;

bail:
    TEST_RETURN(err);
    return err;
}

static MP4Err openDataHandler(MP4AtomPtr s, u32 dataEntryIndex) {
    MP4Err err;
    MP4MediaInformationAtomPtr self;
    MP4DataInformationAtomPtr dinf;
    MP4DataReferenceAtomPtr dref;
    MP4DataEntryAtomPtr dataEntryAtom;

    // MP4MSG("\tCreate data handler, data entry index %d\n", dataEntryIndex);
    err = MP4NoErr;
    if ((s == NULL) || (dataEntryIndex == 0))
        BAILWITHERROR(MP4BadParamErr)
    self = (MP4MediaInformationAtomPtr)s;
    if (self->dataHandler && (self->dataEntryIndex == dataEntryIndex)) {
        /* desired one is already open */
    } else {
        if (self->dataHandler) {
            /* close the current one */
            /* Guido : changed because I have now a function to close cleanly */
            // MP4MSG("\tClose data handler of index %d, create new one for index %d\n",
            // self->dataEntryIndex, dataEntryIndex);
            err = closeDataHandler((MP4AtomPtr)self);
            if (err)
                goto bail;
            /*	MP4DataHandlerPtr dh = (MP4DataHandlerPtr) self->dataHandler;
            if ( dh->close )
            {
                dh->close( dh );
                self->dataHandler = NULL;
            }
            */
        }
        dinf = (MP4DataInformationAtomPtr)self->dataInformation;
        if (dinf == NULL)
            BAILWITHERROR(MP4InvalidMediaErr);
        dref = (MP4DataReferenceAtomPtr)dinf->dataReference;
        if (dref == NULL)
            BAILWITHERROR(MP4InvalidMediaErr);
        if (dataEntryIndex > dref->getEntryCount(dref))
            BAILWITHERROR(MP4BadParamErr)
        err = dref->getEntry(dref, dataEntryIndex, &dataEntryAtom);
        if (err)
            goto bail;
        if (dataEntryAtom == NULL)
            BAILWITHERROR(MP4InvalidMediaErr)

        err = MP4CreateDataHandler(self->inputStream, dataEntryAtom,
                                   (struct MP4DataHandler**)&self->dataHandler);
        if (err)
            goto bail;
        self->dataEntryIndex = dataEntryIndex;
    }

bail:
    TEST_RETURN(err);

    return err;
}

/* sample description entry types */
u32 MP4SampleEntryProtos[] = {
        MP4MPEGSampleEntryAtomType, MP4VisualSampleEntryAtomType, MP4AudioSampleEntryAtomType,
        MP4Av1SampleEntryAtomType, MP4AvcSampleEntryAtomType, MP4HevcSampleEntryAtomType,
        MP4Hev1SampleEntryAtomType, MP4H263SampleEntryAtomType, MP4DivxSampleEntryAtomType,
        AmrNBSampleEntryAtomType, AmrWBSampleEntryAtomType,
        // MP4RtpSampleEntryAtomType, /* for hint track */
        MP4Mp3SampleEntryAtomType, MP4Mp3CbrSampleEntryAtomType, MuLawAudioSampleEntryAtomType,
        MP4AC3SampleEntryAtomType, MP4EC3SampleEntryAtomType, MP4AC4SampleEntryAtomType,
        MJPEGFormatASampleEntryAtomType, MJPEGFormatBSampleEntryAtomType,
        MJPEG2000SampleEntryAtomType, SVQ3SampleEntryAtomType, JPEGSampleEntryAtomType,
        QTSowtSampleEntryAtomType,   /* for apple little endian uncompressed (sowt) audio */
        QTTwosSampleEntryAtomType,   /* for apple big endian uncompressed (twos) audio */
        ImaAdpcmSampleEntryAtomType, /* IMA 4:1 */
        QTH263SampleEntryAtomType, H263SampleEntryAtomType, RawAudioSampleEntryAtomType,
        TimedTextEntryType, /* 3GPP timed text */
        MP4MSAdpcmEntryAtomType, MP4ProtectedVideoSampleEntryAtomType,
        MP4ProtectedAudioSampleEntryAtomType, MP4MetadataSampleEntryAtomType,
        MP4ALACSampleEntryAtomType, MP4OpusSampleEntryAtomType, MP4FlacSampleEntryAtomType,
        MPEGHA1SampleEntryAtomType, MPEGHM1SampleEntryAtomType, DVAVSampleEntryAtomType,
        DVA1SampleEntryAtomType, DVHESampleEntryAtomType, DVH1SampleEntryAtomType,
        DAV1SampleEntryAtomType, MP4Vp9SampleEntryAtomType, MP4ApvSampleEntryAtomType, 0};

static MP4Err getMediaDuration(struct MP4MediaInformationAtom* self, u64* outDuration) {
    MP4Err err;
    MP4SampleTableAtomPtr stbl;
    err = MP4NoErr;

    if (self->sampleTable == NULL)
        BAILWITHERROR(MP4InvalidMediaErr);
    stbl = (MP4SampleTableAtomPtr)self->sampleTable;
    err = stbl->calculateDuration(stbl, outDuration);
bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream) {
    PARSE_ATOM_LIST(MP4MediaInformationAtom)
    self->inputStream = inputStream;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4CreateMediaInformationAtom(MP4MediaInformationAtomPtr* outAtom) {
    MP4Err err;
    MP4MediaInformationAtomPtr self;

    self = (MP4MediaInformationAtomPtr)MP4LocalCalloc(1, sizeof(MP4MediaInformationAtom));
    TESTMALLOC(self)

    err = MP4CreateBaseAtom((MP4AtomPtr)self);
    if (err)
        goto bail;
    self->type = MP4MediaInformationAtomType;
    self->name = "media information";
    self->createFromInputStream = (cisfunc)createFromInputStream;
    self->destroy = destroy;
    self->closeDataHandler = closeDataHandler;
    self->openDataHandler = openDataHandler;
    self->getMediaDuration = getMediaDuration;
    err = MP4MakeLinkedList(&self->atomList);
    if (err)
        goto bail;
    self->testDataEntry = testDataEntry;
    *outAtom = self;
bail:
    TEST_RETURN(err);

    return err;
}
