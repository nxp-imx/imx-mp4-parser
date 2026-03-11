
/************************************************************************
 * Copyright 2005-2006 by Freescale Semiconductor, Inc.
 * Copyright 2026 NXP
 ************************************************************************/

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
    $Id: MJ2Atoms.h,v 1.00 2000/08/08 07:58:05 tm Exp $
*/

#ifndef INCLUDED_MJ2ATOMS_H
#define INCLUDED_MJ2ATOMS_H

#ifndef INCLUDED_MP4ATOMS_H
#include "MP4Atoms.h"

#endif

enum {
    MJ2JPEG2000SignatureAtomType = MP4_FOUR_CHAR_CODE('j', 'P', ' ', ' '),
    MJ2FileTypeAtomType = MP4_FOUR_CHAR_CODE('f', 't', 'y', 'p'),
    MJ2JP2HeaderAtomType = MP4_FOUR_CHAR_CODE('j', 'p', '2', 'h'),
    MJ2ImageHeaderAtomType = MP4_FOUR_CHAR_CODE('i', 'h', 'd', 'r'),
    MJ2BitsPerComponentAtomType = MP4_FOUR_CHAR_CODE('b', 'p', 'c', 'c'),
    MJ2ColorSpecificationAtomType = MP4_FOUR_CHAR_CODE('c', 'o', 'l', 'r'),
    MJ2PaletteAtomType = MP4_FOUR_CHAR_CODE('p', 'c', 'l', 'r'),
    MJ2ChannelDefinitionAtomType = MP4_FOUR_CHAR_CODE('c', 'd', 'e', 'f'),
    MJ2ResolutionAtomType = MP4_FOUR_CHAR_CODE('r', 'e', 's', ' ')
};

enum {
    MJ2JPEG2000Signature = 0x0D0A870A, /* signature data for signature box */
    MJ2JPEG2000SignatureSize = 4
};

typedef struct MJ2JPEG2000SignatureAtom {
    MP4_BASE_ATOM
    u32 signature; /* must be MJ2JPEG2000Signature */
} MJ2JPEG2000SignatureAtom, *MJ2JPEG2000SignatureAtomPtr;

enum {
    MJ2JPEG2000Brand = MP4_FOUR_CHAR_CODE('j', 'p', '2', ' ') /* brand for JPEG-2000 */
};

typedef struct MJ2FileTypeAtom {
    MP4_BASE_ATOM

    u32 brand;              /* the brand of this file */
    u32 minorVersion;       /* the minor version of this file */
    u32 itemCount;          /* the number of items in the compatibility list */
    u32* compatibilityList; /* standards this file conforms to */
    u32 isHeif;             /* brand and compatibility list show heif format */
    u32 isAvif;             /* brand and compatibility list show avif format */
    u32 hasMoovBox;         /* heif format file with moov box */
} MJ2FileTypeAtom, *MJ2FileTypeAtomPtr;

typedef struct MJ2ImageHeaderAtom {
    MP4_BASE_ATOM

    u32 height;          /* height of the image */
    u32 width;           /* width of the image */
    u32 compCount;       /* number of components */
    u32 compBits;        /* bits per component */
    u32 compressionType; /* compression type */
    u32 colorspaceKnown; /* is the colorspace of the image data known? */
    u32 ip;              /* does this file contain intellectual property rights info? */
} MJ2ImageHeaderAtom, *MJ2ImageHeaderAtomPtr;

typedef struct MJ2BitsPerComponentAtom {
    MP4_BASE_ATOM

    ISOErr (*addbpcItem)(struct MJ2BitsPerComponentAtom* self, u8 bpc);
    u32 bpcCount;          /* the number of items in the bits per component list */
    u32* bitsPerComponent; /* bit depths of the corresponding components */
} MJ2BitsPerComponentAtom, *MJ2BitsPerComponentAtomPtr;

enum {
    MJ2EnumeratedColorSpace = 1, /* enumerated colorspace in enumCS field*/
    MJ2RestrictedICCProfile = 2  /* ICC profile in the profile field */
};

typedef struct MJ2ColorSpecificationAtom {
    MP4_BASE_ATOM

    u32 method;      /* the specification method; use constants defined above */
    u32 precedence;  /* the precedence; currently must be 0 */
    u32 approx;      /* the colorspace approximation; currently must be 0 */
    u32 enumCS;      /* enumerated colorspace; used only if method == MJ2EnumeratedColorSpace */
    char* profile;   /* ICC profile; used only if method == MJ2RestrictedICCProfile */
    u32 profileSize; /* size of data pointed to by profile */
} MJ2ColorSpecificationAtom, *MJ2ColorSpecificationAtomPtr;

typedef struct MJ2HeaderAtom {
    MP4_BASE_ATOM

    ISOErr (*addAtom)(struct MJ2HeaderAtom* self, MP4AtomPtr atom);
    ISOAtomPtr imageHeaderAtom;
    ISOAtomPtr bitsPerComponentAtom;
    ISOAtomPtr colorSpecificationAtom;
    ISOAtomPtr paletteAtom;
    ISOAtomPtr channelDefinitionAtom;
    ISOAtomPtr resolutionAtom;
    ISOLinkedList atomList;
} MJ2HeaderAtom, *MJ2HeaderAtomPtr;

/* public functions */
ISOErr MJ2CreateSignatureAtom(MJ2JPEG2000SignatureAtomPtr* outAtom);
ISOErr MJ2CreateFileTypeAtom(MJ2FileTypeAtomPtr* outAtom);
ISOErr MJ2CreateImageHeaderAtom(MJ2ImageHeaderAtomPtr* outAtom);
ISOErr MJ2CreateBitsPerComponentAtom(MJ2BitsPerComponentAtomPtr* outAtom);
ISOErr MJ2CreateColorSpecificationAtom(MJ2ColorSpecificationAtomPtr* outAtom);
/* To be supplied
ISOErr MJ2CreatePaletteAtom( MJ2PaletteAtomPtr *outAtom );
ISOErr MJ2CreateChannelDefinitionAtom( MJ2ChannelDefinitionAtomPtr *outAtom );
ISOErr MJ2CreateResolutionAtom( MJ2ResolutionAtomPtr *outAtom );
 */
ISOErr MJ2CreateHeaderAtom(MJ2HeaderAtomPtr* outAtom);

MP4Err MP4CreateD263Atom(MP4H263AtomPtr*);
MP4Err MP4CreateH263SampleEntryAtom(MP4H263SampleEntryAtomPtr*);
MP4Err MP4CreateAvccAtom(MP4AVCCAtomPtr*);
MP4Err MP4CreateAmrSampleEntryAtom(MP4AmrSampleEntryAtomPtr* outAtom, u32 type);
MP4Err MP4CreateDamrAtom(MP4DAMRAtomPtr*);
MP4Err MP4CreateMp3SampleEntryAtom(MP4Mp3SampleEntryAtomPtr* outAtom);
#endif
