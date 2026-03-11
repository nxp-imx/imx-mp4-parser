/*
 ***********************************************************************
 * Copyright (c) 2005-2013, Freescale Semiconductor, Inc.
 *
 * Copyright 2017-2018, 2021, 2023, 2026 NXP
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
derivative works. Copyright (c) 1999, 2000.
*/
/*
    $Id: MP4Movies.c,v 1.57 2001/06/08 10:25:11 rtm Exp $
*/
//#define DEBUG
#include "MP4Movies.h"
#include "FileMappingObject.h"
#include "ItemTable.h"
#include "MJ2Atoms.h"
#include "MP4Atoms.h"
#include "MP4DataHandler.h"
#include "MP4Descriptors.h"
#include "MP4Impl.h"
#include "MP4InputStream.h"
#include "MovieTracks.h"

#ifdef macintosh
#include <OSUtils.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !(defined(__WINCE) || defined(WIN32))
#include <time.h>  // Wince doesn't support time method TLSbo80080
#else
#include <winbase.h>  //Instead use GetSystemTime method
#endif

MP4Err MP4DisposeMovie(MP4Movie theMovie) {
    GETMOOV(theMovie);

    MP4MSG("DisposeMovie ...\n");
    if (moov->inMemoryDataHandler) {
        MP4DataHandlerPtr dh = (MP4DataHandlerPtr)moov->inMemoryDataHandler;

        dh->close(dh);
    }

    SAFE_DESTROY(moov->inputStream);

    SAFE_DESTROY(moov->FreeSpaceAtom[0]);
    SAFE_DESTROY(moov->filetype); /* movie with BSAC audio tracks may not have the 'ftyp' atom.*/
    SAFE_DESTROY(moov->moovAtomPtr);
    SAFE_DESTROY(moov->mdat);
    SAFE_DESTROY(moov->sidx);
    SAFE_DESTROY(moov->mfra);

    SAFE_LOCAL_FREE(moov);

bail:
    TEST_RETURN(err);

    return err;
}

static MP4Err parseMovie(MP4Movie* theMovie, MP4PrivateMovieRecordPtr moov, int openMovieFlags) {
    MP4Err err = MP4NoErr;
    MP4AtomPtr anAtomPtr = NULL;
    int finished = 0;
    u32 ftyp_atom_size = 0;
    u32 fourcc; /* 4 character code, for file format detection */

    if ((moov == NULL) || (moov->inputStream == NULL)) {
        BAILWITHERROR(MP4BadParamErr);
    }

    *theMovie = (MP4Movie)moov; /* if failure, let the wrapper MP4ParserFreeAtom() to free internal
                                   data structures */

    // disable debug flag
    // moov->inputStream->stream_flags |= debug_flag;

    if (!(openMovieFlags & FILE_FLAG_NON_SEEKABLE)) {
        err = moov->inputStream->read32(moov->inputStream, &ftyp_atom_size, NULL);
        if (err)
            goto bail;

        MP4MSG("parseMovie ftyp size %d\n", ftyp_atom_size);

        err = moov->inputStream->read32(moov->inputStream, &fourcc, NULL);
        if (err)
            goto bail;

        MP4MSG("parseMovie fourcc %c%c%c%c\n", (fourcc >> 24) & 0xFF, (fourcc >> 16) & 0xFF,
               (fourcc >> 8) & 0xFF, fourcc & 0xFF);

        if ((MP4FileTypeAtomType != fourcc) &&
            (MP4MovieAtomType != fourcc && MP4MediaDataAtomType != fourcc) &&
            (MP4SkipAtomType != fourcc) &&
            (QTWideAtomType !=
             fourcc))  // fix 10.192.225.224:/nfsroot2/bitstreams/test_vectors_tmp/100_2600.MOV
        {
            BAILWITHERROR(MP4NoQTAtomErr);
        }

        /* TODO: check types other than AVI */
        moov->inputStream->file_offset =
                -8; /* modify relative offset, seek to the file beginning again */
        moov->inputStream->available += 8;
    }

    moov->moofOffset = 0;
    moov->fragmented = FALSE;

    /* begin parseing atoms sequentially */
    for (finished = 0; !finished;) {
        err = MP4ParseAtom(moov->inputStream, &anAtomPtr);
        if (err == MP4EOF) {
            err = MP4NoErr;
            MP4MSG("parseMovie finish for EOF\n");
            finished = 1;
            // break; /* ENGR56127: 'moov'atom ends with MP4EOF.Need to set pointer of 'moov'. */
        }
        if ((err && (MP4EOF != err)) || (!anAtomPtr)) {
            // for android steraming, if file size is larger than 4G, it could not get the file size
            // and we treat it as
            //  64GB, if avaiable size is larger than 1G, we think it already read to the end.
            if ((PARSER_READ_ERROR == err) && (moov->moovAtomPtr != NULL) &&
                (openMovieFlags & FILE_FLAG_READ_IN_SEQUENCE) &&
                (moov->inputStream->available > 0x40000000LL)) {
                err = MP4NoErr;
                MP4MSG("parseMovie finish for read error\n");
                finished = 1;
            }
            // in some cts cases data source doesn't provide file size, handle read error as end of
            // file
            if (PARSER_READ_ERROR == err && moov->isAvif == TRUE && moov->itemTable != NULL) {
                err = MP4NoErr;
                MP4MSG("avif format finish for read error \n");
                finished = 1;
            }
            goto bail;
        }
        if (anAtomPtr->type == MP4MovieAtomType) {
            if (moov->moovAtomPtr) {
                if (moov->inputStream->stream_flags & live_flag) {
                    MP4MSG("skip repeated moov in dash streaming\n");
                    anAtomPtr->destroy(anAtomPtr);
                } else {
                    // treat it as end of stream when meet another moov atom.
                    err = MP4NoErr;
                    MP4MSG("parseMovie finish for repeat moov\n");
                    finished = 1;
                }
            } else {
                MP4MovieAtomPtr m = (MP4MovieAtomPtr)anAtomPtr;
                err = m->setupTracks(m, moov);
                if (MP4NoErr == err && NULL != m->mvhd) {
                    moov->moovAtomPtr = anAtomPtr;
                } else {
                    err = MP4BadDataErr;
                    goto bail;
                }
            }
        } else if (anAtomPtr->type == MP4MediaDataAtomType) {
            if (moov->mdat != NULL) {
                anAtomPtr->destroy(moov->mdat); /* why? a file can have multiple 'mdat' */
                moov->mdat = NULL;
            }
            if (moov->mdat == NULL) {
                moov->mdat = anAtomPtr;
            }
            if (moov->inputStream->stream_flags & live_flag) {
                /* for live streaming, mdat should be the last atom to parse before playing */
                MP4MSG("parseMovie finish for get live mdat atom\n");
                finished = 1;
            }
        } else if (anAtomPtr->type == QTMetadataAtomType) {
            if (moov->meta == NULL) {
                MP4MetadataAtomPtr atom = (MP4MetadataAtomPtr)anAtomPtr;
                moov->meta = anAtomPtr;
                // save itemTable pointer in moov for easy to use in MP4Parser.c
                if (atom->itemTable != NULL)
                    moov->itemTable = atom->itemTable;
                if (moov->isHeif) {
                    MJ2FileTypeAtomPtr fileType = (MJ2FileTypeAtomPtr)moov->filetype;
                    if (fileType->hasMoovBox != TRUE && atom->itemTable != NULL &&
                        ((ItemTablePtr)atom->itemTable)->valid == TRUE)
                        finished = 1;
                }
            } else {
                anAtomPtr->destroy(anAtomPtr);
            }
        } else if (anAtomPtr->type == MJ2FileTypeAtomType) {
            MJ2FileTypeAtomPtr fileType = (MJ2FileTypeAtomPtr)anAtomPtr;
            moov->filetype = anAtomPtr;
            moov->isHeif = fileType->isHeif;
            moov->isAvif = fileType->isAvif;
        } else if (anAtomPtr->type == MP4FreeSpaceAtomType || anAtomPtr->type == QTWideAtomType) {
            anAtomPtr->destroy(anAtomPtr);
        } else if (anAtomPtr->type == MP4UserDataAtomType) {
            anAtomPtr->destroy(anAtomPtr);
        } else if (anAtomPtr->type == MP4SegmentIndexAtomType) {
            moov->sidx = anAtomPtr;
        } else if (anAtomPtr->type == MP4MovieFragmentAtomType) {
            if (moov->moofOffset == 0) {
                s64 offset = 0;
                MP4MovieFragmentAtomPtr self = (MP4MovieFragmentAtomPtr)anAtomPtr;
                self->getStartOffset(self, &offset);
                moov->moofOffset = offset;
                MP4MSG("parseMovie save moofOffset %lld\n", offset);
                moov->inputStream->stream_flags |= fragmented_flag;
                moov->fragmented = TRUE;
            }
            MP4MSG("parseMovie finish for get moof atom\n");
            finished = 1;
            if ((moov->inputStream->stream_flags & live_flag) && !moov->first_moof)
                moov->first_moof = anAtomPtr;  // save first moof because seeking back to read again
                                               // is not supported
            else
                anAtomPtr->destroy(anAtomPtr);
        } else {
            if (anAtomPtr != NULL && anAtomPtr->destroy != NULL)
                anAtomPtr->destroy(anAtomPtr);
            MP4MSG("unknown atom! type 0x%08x\n", anAtomPtr->type);
        }

        /* TODO: Save other top-level atoms */
        if (moov->inputStream->available == 0) {
            MP4MSG("parseMovie finish for input stream end\n");
            finished = 1;
        }
    }
    if (!(moov->inputStream->stream_flags & live_flag) && moov->fragmented) {
        u32 size;
        u32 type;
        s64 offset = 0;
        err = moov->inputStream->seekTo(moov->inputStream, -12, FSL_SEEK_END, "check mfro");
        if (err)
            BAILWITHERROR(MP4NoErr);
        err = moov->inputStream->read32(moov->inputStream, &type, NULL);
        err = moov->inputStream->read32(moov->inputStream, &size, NULL);
        err = moov->inputStream->read32(moov->inputStream, &size, NULL);
        MP4MSG("get read type=%x,size=%d\n", type, size);
        if (err)
            BAILWITHERROR(MP4NoErr);
        if (type == MP4MovieFragmentRandomAccessOffsetAtomType) {
            offset = 0 - (s64)size;
            err = moov->inputStream->seekTo(moov->inputStream, offset, FSL_SEEK_END, "check mfra");
            if (err)
                BAILWITHERROR(MP4NoErr);
            MP4MSG("seek offset=%lld\n", offset);
            err = MP4ParseAtom(moov->inputStream, &anAtomPtr);
            if (err)
                BAILWITHERROR(MP4NoErr);
            if (anAtomPtr->type == MP4MovieFragmentRandomAccessAtomType) {
                moov->mfra = anAtomPtr;
                MP4MSG("get mfra atom\n");
            }
        }
    }
bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4OpenMovieFile(MP4Movie* theMovie, void* fileMappingObject, uint32 openMovieFlags,
                        void* demuxer) {
    MP4Err err;
    MP4PrivateMovieRecordPtr moov = NULL;

    err = MP4NoErr;
    moov = (MP4PrivateMovieRecordPtr)MP4LocalCalloc(1, sizeof(MP4PrivateMovieRecord));
    (void)demuxer;
    TESTMALLOC(moov);

    moov->referenceCount = 1;
    moov->moovAtomPtr = NULL;

    err = MP4CreateFileMappingInputStream((struct FileMappingObjectRecord*)fileMappingObject,
                                          &moov->inputStream, openMovieFlags);
    if (err)
        goto bail;
    err = parseMovie(theMovie, moov, openMovieFlags);
    if (err)
        goto bail;

    // printf ("fileType = %ld\n", moov->fileType);

bail:
    TEST_RETURN(err);
    if (err) {
        MP4MSG("parseMovie return %d, moov %p, theMovie %p\n", err, moov, *theMovie);
        if (moov) {
            MP4DisposeMovie((MP4Movie)moov);
            moov = NULL;
        }
        *theMovie = NULL;
    }
    return err;
}

/* */

MP4Err MP4GetMovieTrackCount(MP4Movie theMovie, u32* outTrackCount) {
    GETMOVIEATOM(theMovie);
    // MP4MSG("movieAtom is %p", movieAtom);

    *outTrackCount = movieAtom->getTrackCount(movieAtom);
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMovieDuration(MP4Movie theMovie, u64* outDuration) {
    GETMOVIEHEADERATOM(theMovie);
    if (outDuration == 0) {
        BAILWITHERROR(MP4BadParamErr);
    }
    if (movieHeaderAtom == 0) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    err = movieAtom->calculateDuration(movieAtom);
    if (err)
        goto bail;
    *outDuration = movieHeaderAtom->duration;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMovieTimeScale(MP4Movie theMovie, u32* outTimeScale) {
    GETMOVIEHEADERATOM(theMovie);
    if (outTimeScale == 0) {
        BAILWITHERROR(MP4BadParamErr);
    }
    if (movieHeaderAtom == 0) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    *outTimeScale = movieHeaderAtom->timeScale;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMovieUserData(MP4Movie theMovie, MP4UserData* outUserData) {
    MP4UserData udta;
    GETMOVIEATOM(theMovie);
    if (outUserData == 0) {
        BAILWITHERROR(MP4BadParamErr);
    }
    udta = (MP4UserData)movieAtom->udta;
    if (movieAtom->udta == 0) {
        err = MP4NewUserData(&udta);
        if (err)
            goto bail;
        err = movieAtom->addAtom(movieAtom, (MP4AtomPtr)udta);
        if (err)
            goto bail;
    }
    *outUserData = (MP4UserData)udta;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMovieCreationTime(MP4Movie theMovie, u64* outTime) {
    GETMOVIEHEADERATOM(theMovie);
    if (outTime == 0) {
        BAILWITHERROR(MP4BadParamErr);
    }
    if (movieHeaderAtom == 0) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    *outTime = movieHeaderAtom->creationTime;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err makeESD(MP4Movie theMovie, u32 trackNumber, u64 cts, MP4SLConfig inSLConfig,
               MP4DescriptorPtr* outDesc) {
    MP4Err MP4GetMediaSampleDescIndex(MP4Media theMedia, u64 desiredTime,
                                      u32 * outSampleDescriptionIndex);
    MP4Err MP4GetMediaESD(MP4Media theMedia, u32 index, MP4ES_DescriptorPtr * outESD,
                          u32 * outDataReferenceIndex);
    MP4Err MP4CreateSLConfigDescriptor(u32 tag, u32 size, u32 bytesRead,
                                       MP4DescriptorPtr * outDesc);
    MP4Err MP4CreateES_Descriptor(u32 tag, u32 size, u32 bytesRead, MP4DescriptorPtr * outDesc);

    MP4Err err;
    MP4ES_DescriptorPtr esd;
    MP4ES_DescriptorPtr esdInFile;
    MP4SLConfigDescriptorPtr slconfig;
    MP4TrackReferenceTypeAtomPtr dpnd;
    MP4Track theTrack;
    MP4Media theMedia;
    u32 sampleDescIndex;

    err = MP4GetMovieTrack(theMovie, trackNumber, &theTrack);
    if (err)
        goto bail;
    err = MP4GetTrackMedia(theTrack, &theMedia);
    if (err)
        goto bail;
    err = MP4GetMediaSampleDescIndex(theMedia, cts, &sampleDescIndex);
    if (err)
        goto bail;
    err = MP4CreateES_Descriptor(MP4ES_DescriptorTag, 0, 0, (MP4DescriptorPtr*)&esd);
    if (err)
        goto bail;
    err = MP4GetMediaESD(theMedia, sampleDescIndex, &esdInFile, 0);
    if (err)
        goto bail;
    *esd = *esdInFile;
    esd->ESID = trackNumber;

    /* stream dependancy */
    err = MP4GetTrackReferenceType(theTrack, MP4StreamDependenceAtomType, &dpnd);
    if (err)
        goto bail;
    if (dpnd && (dpnd->trackIDCount))
        esd->dependsOnES = dpnd->trackIDs[0];

    /* JLF 11/00 */
    /* OCR dependancy */
    err = MP4GetTrackReferenceType(theTrack, MP4SyncTrackReferenceAtomType, &dpnd);
    if (err)
        goto bail;
    if (dpnd && (dpnd->trackIDCount))
        esd->OCRESID = dpnd->trackIDs[0];

    /*
        handle SLConfig
    */
    if (inSLConfig) {
        u32 movieTimeScale;
        u32 mediaTimeScale;

        slconfig = (MP4SLConfigDescriptorPtr)MP4LocalMalloc(sizeof(MP4SLConfigDescriptor));
        if (slconfig == NULL) {
            BAILWITHERROR(MP4NoMemoryErr);
        }
        *slconfig = *((MP4SLConfigDescriptorPtr)inSLConfig);
        err = MP4GetMovieTimeScale(theMovie, &movieTimeScale);
        if (err)
            goto bail;
        err = MP4GetMediaTimeScale(theMedia, &mediaTimeScale);
        if (err)
            goto bail;
        slconfig->timestampResolution = mediaTimeScale;
        slconfig->OCRResolution = movieTimeScale;
        slconfig->timeScale = mediaTimeScale;
        slconfig->AUDuration = mediaTimeScale;
        slconfig->CUDuration = mediaTimeScale;
        esd->slConfig = (MP4DescriptorPtr)slconfig;
    } else {
        slconfig = (MP4SLConfigDescriptorPtr)esd->slConfig;
        if (slconfig == NULL)
            BAILWITHERROR(MP4InvalidMediaErr);
        if (slconfig->predefined == SLConfigPredefinedMP4) {
            u32 movieTimeScale;
            u32 mediaTimeScale;

            err = MP4GetMovieTimeScale(theMovie, &movieTimeScale);
            if (err)
                goto bail;
            err = MP4GetMediaTimeScale(theMedia, &mediaTimeScale);
            if (err)
                goto bail;
            err = MP4CreateSLConfigDescriptor(MP4SLConfigDescriptorTag, 0, 0,
                                              (MP4DescriptorPtr*)&slconfig);
            if (err)
                goto bail;
            slconfig->predefined = 0;
            slconfig->useAccessUnitStartFlag = 0;
            slconfig->useAccessUnitEndFlag = 0;
            slconfig->useRandomAccessPointFlag = 1;
            slconfig->useRandomAccessUnitsOnlyFlag = 0;
            slconfig->usePaddingFlag = 0;
            slconfig->useTimestampsFlag = 1;
            slconfig->useIdleFlag = 0;
            slconfig->durationFlag = 0;
            slconfig->timestampResolution = mediaTimeScale;
            slconfig->OCRResolution = movieTimeScale;
            slconfig->timestampLength = 32;
            slconfig->OCRLength = 0;
            slconfig->AULength = 0;
            slconfig->instantBitrateLength = 0;
            slconfig->degradationPriorityLength = 0;
            slconfig->AUSeqNumLength = 0;
            slconfig->packetSeqNumLength = 5;
            slconfig->timeScale = mediaTimeScale;
            slconfig->AUDuration = mediaTimeScale;
            slconfig->CUDuration = mediaTimeScale;
            slconfig->startDTS = 0;
            slconfig->startCTS = 0;
            esd->slConfig = (MP4DescriptorPtr)slconfig;
        }
    }
    *outDesc = (MP4DescriptorPtr)esd;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMovieIODInlineProfileFlag(MP4Movie theMovie, u8* outFlag) {
    MP4InitialObjectDescriptorPtr iod;
    GETIODATOM(theMovie);

    if (outFlag == 0) {
        BAILWITHERROR(MP4BadParamErr);
    }
    if (iodAtom->ODSize == 0) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    iod = (MP4InitialObjectDescriptorPtr)iodAtom->descriptor;
    if (iod == 0) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    /* jlf 12/00: escape sequence for OD */
    if (iod->tag == MP4_OD_Tag)
        return MP4HasRootOD;

    *outFlag = (iod->inlineProfileFlag != 0);
bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4GetMovieProfilesAndLevels(MP4Movie theMovie, u8* outOD, u8* outScene, u8* outAudio,
                                    u8* outVisual, u8* outGraphics) {
    MP4InitialObjectDescriptorPtr iodDesc;
    GETIODATOM(theMovie);

    if (iodAtom->ODSize == 0) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    iodDesc = (MP4InitialObjectDescriptorPtr)iodAtom->descriptor;
    if (iodDesc == 0) {
        BAILWITHERROR(MP4InvalidMediaErr);
    }
    /* jlf 12/00: escape sequence for OD */
    if ((iodDesc->tag == MP4_OD_Tag) || (iodDesc->tag == MP4ObjectDescriptorTag))
        return MP4HasRootOD;

    if (outOD) {
        *outOD = (u8)iodDesc->OD_profileAndLevel;
    }
    if (outScene) {
        *outScene = (u8)iodDesc->scene_profileAndLevel;
    }
    if (outAudio) {
        *outAudio = (u8)iodDesc->audio_profileAndLevel;
    }
    if (outVisual) {
        *outVisual = (u8)iodDesc->visual_profileAndLevel;
    }
    if (outGraphics) {
        *outGraphics = (u8)iodDesc->graphics_profileAndLevel;
    }
bail:
    TEST_RETURN(err);
    return err;
}

MP4Err MP4GetMovieInitialObjectDescriptor(MP4Movie theMovie, MP4Handle outDescriptorH) {
    return MP4GetMovieInitialObjectDescriptorUsingSLConfig(theMovie, 0, outDescriptorH);
}

MP4Err MP4GetMovieInitialObjectDescriptorUsingSLConfig(MP4Movie theMovie, MP4SLConfig slconfig,
                                                       MP4Handle outDescriptorH) {
    MP4Err MP4CreateSLConfigDescriptor(u32 tag, u32 size, u32 bytesRead,
                                       MP4DescriptorPtr * outDesc);
    MP4Err MP4GetMediaESD(MP4Media theMedia, u32 index, MP4ES_DescriptorPtr * outESD,
                          u32 * outDataReferenceIndex);
    MP4Err MP4GetMovieObjectDescriptorUsingSLConfig(MP4Movie theMovie, MP4SLConfig slconfig,
                                                    MP4Handle outDescriptorH);

    MP4InitialObjectDescriptorPtr iodDesc;
    MP4LinkedList incDescriptors;
    GETIODATOM(theMovie);

    // if ( 21 ) // ranjeet
    if (iodAtom->ODSize)  //
    {
        u32 count;
        u32 i;
        u32 trackCount;

        /* jlf 11/00 : if this is an OD, extract directly the OD. */
        if (iodAtom->descriptor->tag == MP4_OD_Tag) {
            // printf("iodAtom->descriptor->tag = %d \n", iodAtom->descriptor->tag); // Ranjeet
            return MP4GetMovieObjectDescriptorUsingSLConfig(theMovie, slconfig, outDescriptorH);
        }

        //	    printf("iodAtom->descriptor->tag = %d \n", iodAtom->descriptor->tag); // Ranjeet

        err = MP4GetMovieTrackCount(theMovie, &trackCount);
        if (err)
            goto bail;
        iodDesc = (MP4InitialObjectDescriptorPtr)iodAtom->descriptor;
        err = MP4GetListEntryCount(iodDesc->ES_ID_IncDescriptors, &count);
        if (err)
            goto bail;

        /*
            get and rewrite ES_Descriptors, placing each in iodDesc
        */
        for (i = 0; i < count; i++) {
            MP4ES_ID_IncDescriptorPtr inc;
            MP4ES_DescriptorPtr esd;

            err = MP4GetListEntry(iodDesc->ES_ID_IncDescriptors, i, (char**)&inc);
            if (err)
                goto bail;
            err = makeESD(theMovie, inc->trackID, 0, slconfig, (MP4DescriptorPtr*)&esd);
            if (err)
                goto bail;
            /* add esd to iodDesc */
            err = iodDesc->addDescriptor((MP4DescriptorPtr)iodDesc, (MP4DescriptorPtr)esd);
            if (err)
                goto bail;
        }
        /*
          err = MP4DeleteLinkedList( iodDesc->ES_ID_IncDescriptors ); if (err) goto bail;
        */
        incDescriptors = iodDesc->ES_ID_IncDescriptors;
        iodDesc->ES_ID_IncDescriptors = NULL;
        iodDesc->tag = MP4InitialObjectDescriptorTag;
        err = iodDesc->calculateSize((MP4DescriptorPtr)iodDesc);
        if (err)
            goto bail;
        err = MP4SetHandleSize(outDescriptorH, iodDesc->size);
        if (err)
            goto bail;
        err = iodDesc->serialize((MP4DescriptorPtr)iodDesc, *outDescriptorH);
        if (err)
            goto bail;
        err = iodDesc->removeESDS((MP4DescriptorPtr)iodDesc);
        if (err)
            goto bail;
        iodDesc->ES_ID_IncDescriptors = incDescriptors;
        iodDesc->tag = MP4_IOD_Tag;
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMovieIndTrack(MP4Movie theMovie, u32 trackIndex, MP4Track* outTrack) {
    MP4AtomPtr aTrack;
    GETMOVIEATOM(theMovie);
    // MP4MSG("movieAtom is %p\n", movieAtom);
    //**total_tracks=movieAtom->getTrackCount(movieAtom);
    if ((trackIndex == 0) || (trackIndex > movieAtom->getTrackCount(movieAtom)))
        BAILWITHERROR(MP4BadParamErr)
    err = movieAtom->getIndTrack(movieAtom, trackIndex, &aTrack);
    if (err)
        goto bail;
    if (aTrack == NULL)
        BAILWITHERROR(MP4BadDataErr)
    *outTrack = (MP4Track)aTrack;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetMovieTrack(MP4Movie theMovie, u32 trackID, MP4Track* outTrack) {
    u32 i;
    u32 trackCount;

    MP4Err err;
    if (NULL == theMovie) {
        MP4MSG("MP4GetMovieTrack failed!\n");
        return MP4BadParamErr;
    }
    err = MP4GetMovieTrackCount(theMovie, &trackCount);
    if (err)
        goto bail;
    if (trackCount == 0) {
        BAILWITHERROR(MP4BadParamErr);
    }
    for (i = 1; i <= trackCount; i++) {
        MP4Track t;
        u32 id;
        err = MP4GetMovieIndTrack(theMovie, i, &t);
        if (err)
            goto bail;
        err = MP4GetTrackID(t, &id);
        if (err)
            goto bail;
        if (id == trackID) {
            *outTrack = t;
            break;
        } else if (i == trackCount)
            err = MP4BadParamErr;
    }
bail:
    TEST_RETURN(err);

    return err;
}

ISOErr MJ2GetMovieMatrix(ISOMovie theMovie, u32 outMatrix[9]) {
    GETMOVIEATOM(theMovie);
    if (outMatrix == NULL)
        BAILWITHERROR(MP4BadParamErr);
    assert(movieAtom->getMatrix);
    err = movieAtom->getMatrix(movieAtom, outMatrix);
bail:
    return err;
}

ISOErr MJ2GetMoviePreferredRate(ISOMovie theMovie, u32* outRate) {
    GETMOVIEATOM(theMovie);
    if (outRate == NULL)
        BAILWITHERROR(MP4BadParamErr);
    assert(movieAtom->getPreferredRate);
    err = movieAtom->getPreferredRate(movieAtom, outRate);
bail:
    return err;
}

ISOErr MJ2GetMoviePreferredVolume(ISOMovie theMovie, s16* outVolume) {
    GETMOVIEATOM(theMovie);
    if (outVolume == NULL)
        BAILWITHERROR(MP4BadParamErr);
    assert(movieAtom->getPreferredVolume);
    err = movieAtom->getPreferredVolume(movieAtom, outVolume);
bail:
    return err;
}

#if 0
MP4Err
MP4GetMovieInitialBIFSTrack( MP4Movie theMovie, MP4Track *outBIFSTrack )
{
	GETMOVIEATOM( theMovie );
	if ( moov->initialBIFS == NULL )
	{
		err = MP4NewMovieTrack( theMovie, MP4NewTrackIsVisual, &moov->initialBIFS ); if (err) goto bail;
	}
	*outBIFSTrack = moov->initialBIFS;
bail:
	TEST_RETURN( err );

	return err;
}

MP4_EXTERN ( MP4Err )
MP4SetMovieInitialBIFSTrack( MP4Movie theMovie, MP4Track theBIFSTrack )
{
	GETMOVIEATOM( theMovie );
	moov->initialBIFS = theBIFSTrack;
bail:
	TEST_RETURN( err );

	return err;
}

MP4Err
MP4SetMovieInitialODTrack( MP4Movie theMovie, MP4Track theODTrack )
{
	GETMOVIEATOM( theMovie );
	moov->initialOD = theODTrack;
bail:
	TEST_RETURN( err );

	return err;
}
#endif

#define MAC_EPOCH 2082758400;

#define WINCE_EPOCH 9561628800;

/*Added to offset time duration from
jan 1 1601(SystemTimeToFileTime)
to jan 1 1904 (GetDateTime) in case of Wince*/

MP4Err MP4GetCurrentTime(u64* outTime) {
    MP4Err err;
    u64 ret;
#if defined(__WINCE) || defined(WIN32)
    FILETIME* lpFileTime;
    SYSTEMTIME st;
#else
    unsigned long calctime;
#endif

    err = MP4NoErr;
    if (outTime == NULL)
        BAILWITHERROR(MP4BadParamErr)

#if defined(__WINCE) || defined(WIN32)  // TLSbo80080

    lpFileTime = (FILETIME*)MP4LocalMalloc(sizeof(FILETIME));

    // To extract current date & time into SYSTEMTIME structure

    GetSystemTime(&st);

    /* To  represent extracted  SystemTime	in seconds from
        jan 1 1601 */

    SystemTimeToFileTime(&st, lpFileTime);

    MP4LocalFree(lpFileTime);
    /*Represent the seconds as in MAC format*/

    ret = lpFileTime->dwHighDateTime;
    ret <<= 32;
    ret |= lpFileTime->dwLowDateTime;

    ret = (ret / 10000000);

    ret = ret - WINCE_EPOCH;

#elif macintosh
    GetDateTime(&calctime);
    ret = calctime;

#else  // elinux & WIN32

        {
            time_t t;
            t = time(NULL);
            calctime = t + MAC_EPOCH;
        }
    ret = calctime;

#endif  //__WINCE TLSbo80080

    *outTime = ret;

bail:
    TEST_RETURN(err);

    return err;
}

/* jlf 11/00: Handle ObjectDescriptor in the iods atom for exchange files */
MP4Err MP4GetMovieObjectDescriptorUsingSLConfig(MP4Movie theMovie, MP4SLConfig slconfig,
                                                MP4Handle outDescriptorH) {
    MP4Err MP4CreateSLConfigDescriptor(u32 tag, u32 size, u32 bytesRead,
                                       MP4DescriptorPtr * outDesc);
    MP4Err MP4GetMediaESD(MP4Media theMedia, u32 index, MP4ES_DescriptorPtr * outESD,
                          u32 * outDataReferenceIndex);

    MP4ObjectDescriptorPtr odDesc;
    MP4ObjectDescriptorPtr self;
    MP4LinkedList incDescriptors;
    GETIODATOM(theMovie);

    if (21) {
        u32 count;
        u32 i;
        u32 trackCount;

        err = MP4GetMovieTrackCount(theMovie, &trackCount);
        if (err)
            goto bail;
        odDesc = (MP4ObjectDescriptorPtr)iodAtom->descriptor;

        /* check that there's no ES_ID_Ref */
        err = MP4GetListEntryCount(odDesc->ES_ID_RefDescriptors, &count);
        if (err)
            goto bail;
        if (count)
            BAILWITHERROR(MP4BadDataErr);

        err = MP4GetListEntryCount(odDesc->ES_ID_IncDescriptors, &count);
        if (err)
            goto bail;

        /*
            get and rewrite ES_Descriptors, placing each in odDesc
        */
        for (i = 0; i < count; i++) {
            MP4ES_ID_IncDescriptorPtr inc;
            MP4ES_DescriptorPtr esd;

            err = MP4GetListEntry(odDesc->ES_ID_IncDescriptors, i, (char**)&inc);
            if (err)
                goto bail;
            err = makeESD(theMovie, inc->trackID, 0, slconfig, (MP4DescriptorPtr*)&esd);
            if (err)
                goto bail;
            /* add esd to odDesc */
            err = odDesc->addDescriptor((MP4DescriptorPtr)odDesc, (MP4DescriptorPtr)esd);
            if (err)
                goto bail;
        }
        incDescriptors = odDesc->ES_ID_IncDescriptors;
        odDesc->ES_ID_IncDescriptors = NULL;
        odDesc->tag = MP4ObjectDescriptorTag;
        err = odDesc->calculateSize((MP4DescriptorPtr)odDesc);
        if (err)
            goto bail;
        err = MP4SetHandleSize(outDescriptorH, odDesc->size);
        if (err)
            goto bail;
        err = odDesc->serialize((MP4DescriptorPtr)odDesc, *outDescriptorH);
        if (err)
            goto bail;

        self = odDesc;
        DESTROY_DESCRIPTOR_LIST(ESDescriptors);
        err = MP4MakeLinkedList(&odDesc->ESDescriptors);
        odDesc->ES_ID_IncDescriptors = incDescriptors;
        odDesc->tag = MP4_OD_Tag;
    }
bail:
    TEST_RETURN(err);

    return err;
}
