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
    $Id: MP4UserData.c,v 1.2 2000/03/04 07:58:05 mc Exp $
*/
#include "MP4Atoms.h"
#include "MP4Movies.h"

extern MP4Err MP4CreateAtom(u32 atomType, MP4AtomPtr* outAtom, MP4InputStreamPtr inputStream);

MP4_EXTERN(MP4Err)

MP4AddUserData(MP4UserData theUserData, MP4Handle dataH, u32 userDataType, u32* outIndex) {
    MP4Err err;
    MP4UserDataAtomPtr udta;
    err = MP4NoErr;
    if ((theUserData == NULL) || (dataH == NULL) || (userDataType == 0)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    udta = (MP4UserDataAtomPtr)theUserData;
    if (udta->addUserData) {
        err = udta->addUserData(udta, dataH, userDataType, outIndex);
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4_EXTERN(MP4Err)

MP4GetUserDataEntryCount(MP4UserData theUserData, u32 userDataType, u32* outCount) {
    MP4Err err;
    MP4UserDataAtomPtr udta;
    err = MP4NoErr;
    if ((theUserData == NULL) || (outCount == NULL) || (userDataType == 0)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    udta = (MP4UserDataAtomPtr)theUserData;
    if (udta->getEntryCount == NULL) {
        BAILWITHERROR(MP4BadParamErr);
    }
    err = udta->getEntryCount(udta, userDataType, outCount);
bail:
    TEST_RETURN(err);

    return err;
}

MP4_EXTERN(MP4Err)

MP4GetIndUserDataType(MP4UserData theUserData, u32 typeIndex, u32* outType) {
    MP4Err err;
    MP4UserDataAtomPtr udta;
    err = MP4NoErr;
    if ((theUserData == NULL) || (outType == NULL) || (typeIndex == 0)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    udta = (MP4UserDataAtomPtr)theUserData;
    if (udta->getIndType == NULL) {
        BAILWITHERROR(MP4BadParamErr);
    }
    err = udta->getIndType(udta, typeIndex, outType);
bail:
    TEST_RETURN(err);

    return err;
}

MP4_EXTERN(MP4Err)

MP4GetUserDataTypeCount(MP4UserData theUserData, u32* outCount) {
    MP4Err err;
    MP4UserDataAtomPtr udta;
    err = MP4NoErr;
    if ((theUserData == NULL) || (outCount == NULL)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    udta = (MP4UserDataAtomPtr)theUserData;
    if (udta->getTypeCount == NULL) {
        BAILWITHERROR(MP4BadParamErr);
    }
    err = udta->getTypeCount(udta, outCount);
bail:
    TEST_RETURN(err);

    return err;
}

MP4_EXTERN(MP4Err)

MP4GetUserDataItem(MP4UserData theUserData, MP4Handle dataH, u32 userDataType, u32 itemIndex) {
    MP4Err err;
    MP4UserDataAtomPtr udta;
    err = MP4NoErr;
    if ((theUserData == NULL) || (dataH == NULL) || (userDataType == 0) || (itemIndex == 0)) {
        BAILWITHERROR(MP4BadParamErr);
    }
    udta = (MP4UserDataAtomPtr)theUserData;
    if (udta->getItem == NULL) {
        BAILWITHERROR(MP4BadParamErr);
    }
    err = udta->getItem(udta, dataH, userDataType, itemIndex);
bail:
    TEST_RETURN(err);

    return err;
}

MP4_EXTERN(MP4Err)

MP4NewUserData(MP4UserData* outUserData) {
    MP4Err err;
    MP4AtomPtr udta;
    err = MP4NoErr;
    err = MP4CreateAtom(MP4UserDataAtomType, &udta, NULL);
    if (err)
        goto bail;
    *outUserData = (MP4UserData)udta;
bail:
    TEST_RETURN(err);

    return err;
}
