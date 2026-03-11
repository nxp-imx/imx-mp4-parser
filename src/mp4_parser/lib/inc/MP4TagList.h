/************************************************************************
 * Copyright 2016 by Freescale Semiconductor, Inc.
 * Copyright 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ************************************************************************/
#ifndef MP4_TAT_LIST_H
#define MP4_TAT_LIST_H

#include "MP4OSMacros.h"
#include "fsl_parser.h"

TrackExtTagList* MP4CreateTagList();
MP4Err MP4AddTag(TrackExtTagList* list, uint32 index, uint32 type, uint32 size, u8* data);
MP4Err MP4DeleteTagList(TrackExtTagList* list);

#endif