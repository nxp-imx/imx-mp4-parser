/************************************************************************
 * Copyright (c) 2014,2016 Freescale Semiconductor, Inc.
 * Copyright 2017, 2020, 2024, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ************************************************************************/

#include "MP4OSMacros.h"

#include <stdlib.h>
#include <string.h>
#include "MP4FragmentedQueue.h"
#include "MP4Impl.h"

struct _MP4FragmentedQueue {
    u32 size;
    u32 num;
    MP4TrackFragmentSample* data;
};

#define INC_NUM 256

MP4Err MP4CreateFragmentQueue(MP4FragmentedQueue** queue) {
    MP4Err err;
    MP4FragmentedQueue* self;

    err = MP4NoErr;
    self = (MP4FragmentedQueue*)MP4LocalCalloc(1, sizeof(MP4FragmentedQueue));
    TESTMALLOC(self)

    self->num = 0;

    self->data = NULL;
    self->size = 0;

    *queue = self;

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4AddOneSample(MP4FragmentedQueue* queue, MP4TrackFragmentSample* sample) {
    MP4Err err;

    u32 old_num = 0;
    u32 new_num = 0;

    if (queue == NULL || sample == NULL) {
        err = MP4BadParamErr;
        goto bail;
    }

    err = MP4NoErr;

    old_num = queue->num;

    new_num = old_num + 1;
    if (new_num < queue->size) {
        memcpy(queue->data + old_num, sample, sizeof(MP4TrackFragmentSample));
    } else {
        u32 old_size = queue->size;
        MP4TrackFragmentSample* old_data = queue->data;
        queue->data = MP4LocalCalloc((old_size + INC_NUM), sizeof(MP4TrackFragmentSample));
        TESTMALLOC(queue->data)
        if (old_data) {
            memcpy(queue->data, old_data, old_size * sizeof(MP4TrackFragmentSample));

            MP4LocalFree(old_data);
        }

        queue->size += INC_NUM;
        memcpy(queue->data + old_num, sample, sizeof(MP4TrackFragmentSample));

        if (queue->size * sizeof(MP4TrackFragmentSample) > (2 * 1024 * 1024)) {
            MP4MSG("MP4AddOneSample fragmented queue size=%lld! \n",
                   (long long)queue->size * sizeof(MP4TrackFragmentSample));
        }
    }

    queue->num = new_num;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4AddSamples(MP4FragmentedQueue* queue, MP4TrackFragmentSample* sample, u32 size) {
    MP4Err err;

    u32 old_num = 0;
    u32 new_num = 0;
    err = MP4NoErr;

    if (queue == NULL || sample == NULL || size == 0) {
        err = MP4BadParamErr;
        goto bail;
    }

    old_num = queue->num;

    new_num = old_num + size;
    if (new_num < queue->size) {
        memcpy(queue->data + (old_num - 1), sample, size * sizeof(MP4TrackFragmentSample));
    } else {
        u32 old_size = queue->size;
        MP4TrackFragmentSample* old_data = queue->data;
        queue->data = MP4LocalCalloc((old_size + size), sizeof(MP4TrackFragmentSample));
        TESTMALLOC(queue->data)
        memcpy(queue->data, old_data, old_size);

        MP4LocalFree(old_data);

        queue->size += size;
        memcpy(queue->data + (old_num - 1), sample, size * sizeof(MP4TrackFragmentSample));

        // if(queue->size * sizeof(MP4TrackFragmentSample) > (2*1024*1024))
        //     MP4MSG("MP4AddOneSample fragmented queue size=%d! \n",queue->size *
        //     sizeof(MP4TrackFragmentSample));
    }

    queue->num = new_num;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4UpdateSampleDRMInfo(MP4FragmentedQueue* queue, u32 sampleIndex,
                              MP4TrackFragmentSample* srcSample) {
    MP4Err err = MP4NoErr;
    MP4TrackFragmentSample* targetSample = NULL;

    if (queue == NULL || srcSample == NULL) {
        err = MP4BadParamErr;
        goto bail;
    }

    if (sampleIndex >= queue->size) {
        err = MP4BadParamErr;
        goto bail;
    }

    targetSample = queue->data + sampleIndex;

    if (srcSample->drm_flags & FRAGMENT_SAMPLE_DRM_FLAG_FULL) {
        targetSample->clearsSize = 0;
        targetSample->encryptedSize = targetSample->size;
        memcpy(targetSample->iv, srcSample->iv, 16);
        targetSample->drm_flags = FRAGMENT_SAMPLE_DRM_FLAG_FULL;
    } else if ((srcSample->drm_flags & FRAGMENT_SAMPLE_DRM_FLAG_SUB) &&
               srcSample->clearsSize < FRAGMENT_SAMPLE_DRM_MAX_SUBSAMPLE &&
               srcSample->encryptedSize < FRAGMENT_SAMPLE_DRM_MAX_SUBSAMPLE &&
               0 == targetSample->clearsSize) {
        targetSample->clearsSize = srcSample->clearsSize;
        targetSample->encryptedSize = srcSample->encryptedSize;
        memcpy(targetSample->clearArray, srcSample->clearArray,
               srcSample->clearsSize * sizeof(u32));
        memcpy(targetSample->encryptedArray, srcSample->encryptedArray,
               srcSample->encryptedSize * sizeof(u32));
        memcpy(targetSample->iv, srcSample->iv, 16);
        targetSample->drm_flags = FRAGMENT_SAMPLE_DRM_FLAG_SUB;
    }

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetOneSample(MP4FragmentedQueue* queue, MP4TrackFragmentSample* out_sample) {
    MP4Err err;
    err = MP4NoErr;

    if (queue == NULL || out_sample == NULL) {
        err = MP4BadParamErr;
        goto bail;
    }

    if (queue->num > 0) {
        memcpy(out_sample, queue->data, sizeof(MP4TrackFragmentSample));

        memmove(queue->data, queue->data + 1, (queue->num - 1) * sizeof(MP4TrackFragmentSample));

        queue->num--;
    }
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetQueueSize(MP4FragmentedQueue* queue, u32* size) {
    MP4Err err;
    err = MP4NoErr;
    if (queue == NULL || size == NULL) {
        err = MP4BadParamErr;
        goto bail;
    }

    *size = queue->num;
bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4ClearQueue(MP4FragmentedQueue* queue) {
    MP4Err err;
    err = MP4NoErr;

    if (queue == NULL) {
        err = MP4BadParamErr;
        goto bail;
    }
    if (queue->data)
        MP4LocalFree(queue->data);
    queue->num = 0;

    queue->data = NULL;
    queue->size = 0;

bail:
    TEST_RETURN(err);

    return err;
}

MP4Err MP4GetHeadSampleOffset(MP4FragmentedQueue* queue, u64* offset) {
    MP4Err err;
    MP4TrackFragmentSample sample;
    err = MP4NoErr;

    if (queue == NULL || offset == NULL) {
        err = MP4BadParamErr;
        goto bail;
    }
    if (queue->num > 0) {
        sample = queue->data[0];
        *offset = sample.offset;
    } else {
        err = MP4EOF;
    }
bail:
    TEST_RETURN(err);
    return err;
}
