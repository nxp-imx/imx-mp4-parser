/*
 ***********************************************************************
 * Copyright 2005-2006 by Freescale Semiconductor, Inc.
 * Copyright 2024, 2026 NXP
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
    $Id: MP4Descriptors.c,v 1.5 2000/07/04 09:28:29 francesc Exp $
*/
#include "MP4Descriptors.h"
#include "MP4Impl.h"
#include "MP4Movies.h"

#define GET_DESCRIPTOR_BYTE(varName, what)                 \
    err = inputStream->read8(inputStream, &varName, what); \
    if (err)                                               \
        goto bail;                                         \
    bytesRead++

MP4Err MP4CreateInitialObjectDescriptor(u32 tag, u32 size, u32 bytesRead,
                                        MP4DescriptorPtr* outDesc);
MP4Err MP4CreateES_Descriptor(u32 tag, u32 size, u32 bytesRead, MP4DescriptorPtr* outDesc);
MP4Err MP4CreateDefaultDescriptor(u32 tag, u32 size, u32 bytesRead, MP4DescriptorPtr* outDesc);
MP4Err MP4CreateDecoderConfigDescriptor(u32 tag, u32 size, u32 bytesRead,
                                        MP4DescriptorPtr* outDesc);
MP4Err MP4CreateES_ID_IncDescriptor(u32 tag, u32 size, u32 bytesRead, MP4DescriptorPtr* outDesc);
MP4Err MP4CreateES_ID_RefDescriptor(u32 tag, u32 size, u32 bytesRead, MP4DescriptorPtr* outDesc);
MP4Err MP4CreateSLConfigDescriptor(u32 tag, u32 size, u32 bytesRead, MP4DescriptorPtr* outDesc);
MP4Err MP4CreateObjectDescriptor(u32 tag, u32 size, u32 bytesRead, MP4DescriptorPtr* outDesc);

/*
ES_Descriptor (0x03)
|
|_ DecoderConfigDescriptor (0x04)
            |
            |_ DecSpecificInfoDescriptor (0x05)
               (Length 0 indicates this tag is empty and so no decoder specific info is available!)
|
|_ SLConfigDescriptorTag (0x06)

*/

MP4Err MP4ParseDescriptor(MP4InputStreamPtr inputStream, uint32 maxSize,
                          MP4DescriptorPtr* outDesc) {
    MP4Err err = MP4NoErr;
    u32 tag;
    u32 size;
    u32 val;
    MP4DescriptorPtr desc;
    u32 bytesRead;
    u32 sizeBytes; /* bytes used to encode the data length */

    if ((inputStream == NULL) || (outDesc == NULL))
        BAILWITHERROR(MP4BadParamErr)

    *outDesc = NULL;

    if (0 >= (int32)maxSize)
        goto bail;

    inputStream->msg(inputStream, "{");
    inputStream->indent++;
    bytesRead = 0;
    GET_DESCRIPTOR_BYTE(tag, "class tag");
    MP4MSG("tag %d, max size %d\n", tag, maxSize);

    /* SLConfigDescriptor (0X06) may be cut short! Only the tag field is present.
    An new atom "stts" is following it! */
    if (bytesRead >= maxSize) {
        MP4MSG("Warning! tag %d, is cut-short! No fields are after this tag!\n", tag);
        goto bail;
    }

    /* get size, 14496-1 section 14.3.3 expandable classes */
    size = 0;
    sizeBytes = 0;
    do {
        GET_DESCRIPTOR_BYTE(val, "size byte");
        sizeBytes++;
        size <<= 7;
        size |= val & 0x7F;
        // MP4MSG("descriptor tag  %ld, sizeBytes %ld, val %ld, size %ld\n", tag,  sizeBytes, val,
        // size);
    } while ((val & 0x80) && (bytesRead < maxSize));

    MP4MSG("\tDescriptor tag %d, data size %d, ", tag, size);
    size += (sizeBytes + 1); /* one byte for the tag field */
    MP4MSG("total size %d\n", size);

    if (size > maxSize) {
        MP4MSG("Warning! tag %d, is cut-short! Replace size expected %d by max size %d\n", tag,
               size, maxSize);
        size = maxSize;
    }

    switch (tag) {
        case MP4InitialObjectDescriptorTag:
        case MP4_IOD_Tag:
            err = MP4CreateInitialObjectDescriptor(MP4_IOD_Tag, size, bytesRead, &desc);
            if (err)
                goto bail;
            break;

        case MP4ES_DescriptorTag:
            err = MP4CreateES_Descriptor(tag, size, bytesRead, &desc);
            if (err)
                goto bail;
            break;

        case MP4DecoderConfigDescriptorTag:
            err = MP4CreateDecoderConfigDescriptor(tag, size, bytesRead, &desc);
            if (err)
                goto bail;
            break;

        case MP4ES_ID_IncDescriptorTag:
            err = MP4CreateES_ID_IncDescriptor(tag, size, bytesRead, &desc);
            if (err)
                goto bail;
            break;

        case MP4ES_ID_RefDescriptorTag:
            err = MP4CreateES_ID_RefDescriptor(tag, size, bytesRead, &desc);
            if (err)
                goto bail;
            break;

        case MP4SLConfigDescriptorTag:
            err = MP4CreateSLConfigDescriptor(tag, size, bytesRead, &desc);
            if (err)
                goto bail;
            break;

        case MP4ObjectDescriptorTag:
        case MP4_OD_Tag:
            err = MP4CreateObjectDescriptor(MP4ObjectDescriptorTag, size, bytesRead, &desc);
            if (err)
                goto bail;
            break;

        default:
            err = MP4CreateDefaultDescriptor(tag, size, bytesRead, &desc);
            if (err)
                goto bail;
            if (desc->name) {
                MP4LocalFree(desc->name);
                desc->name = NULL;
            }
            break;
    }
#ifndef COLDFIRE
//	sprintf( msgbuf, "descriptor is %s", desc->name );
//	inputStream->msg( inputStream, msgbuf );
#endif
    err = desc->createFromInputStream(desc, inputStream);
    if (err)
        goto bail;
    *outDesc = desc;
    inputStream->indent--;
    inputStream->msg(inputStream, "}");
bail:
    TEST_RETURN(err);

    return err;
}
