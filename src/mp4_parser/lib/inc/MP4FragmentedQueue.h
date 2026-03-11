/************************************************************************
 * Copyright (c) 2014, Freescale Semiconductor, Inc.
 * Copyright 2017, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ************************************************************************/

#ifndef MP4_FRAGMENTED_QUEUQ
#define MP4_FRAGMENTED_QUEUQ

#include "MP4Atoms.h"
#include "MP4OSMacros.h"

typedef struct _MP4FragmentedQueue MP4FragmentedQueue;

MP4Err MP4CreateFragmentQueue(MP4FragmentedQueue** queue);

MP4Err MP4AddSamples(MP4FragmentedQueue* queue, MP4TrackFragmentSample* sample, u32 size);
MP4Err MP4AddOneSample(MP4FragmentedQueue* queue, MP4TrackFragmentSample* sample);

MP4Err MP4GetOneSample(MP4FragmentedQueue* queue, MP4TrackFragmentSample* sample);
MP4Err MP4GetQueueSize(MP4FragmentedQueue* queue, u32* size);

MP4Err MP4ClearQueue(MP4FragmentedQueue* queue);

MP4Err MP4GetHeadSampleOffset(MP4FragmentedQueue* queue, u64* offset);
MP4Err MP4UpdateSampleDRMInfo(MP4FragmentedQueue* queue, u32 sampleIndex,
                              MP4TrackFragmentSample* srcSample);
#endif
