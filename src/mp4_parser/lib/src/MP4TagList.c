/************************************************************************
 * Copyright 2016 by Freescale Semiconductor, Inc.
 *
 * Copyright 2017-2018, 2026 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 ************************************************************************/

#include "MP4TagList.h"

TrackExtTagList* MP4CreateTagList() {
    TrackExtTagList* out = NULL;

    out = (TrackExtTagList*)MP4LocalCalloc(1, sizeof(TrackExtTagList));
    if (out) {
        out->num = 0;
        out->m_ptr = NULL;
        return out;
    } else
        return NULL;
}

MP4Err MP4AddTag(TrackExtTagList* list, uint32 index, uint32 type, uint32 size, u8* data) {
    TrackExtTagItem* curr = NULL;
    TrackExtTagItem* temp = NULL;
    if (list == NULL || data == NULL)
        return -1;

    temp = (TrackExtTagItem*)MP4LocalCalloc(1, sizeof(TrackExtTagItem));
    if (temp == NULL)
        return -1;

    temp->index = index;
    temp->type = type;
    temp->size = size;
    temp->data = data;
    temp->nextItemPtr = NULL;

    if (list->m_ptr == NULL) {
        list->m_ptr = temp;
    } else {
        curr = list->m_ptr;
        while (curr != NULL && curr->nextItemPtr != NULL) {
            curr = curr->nextItemPtr;
        }
        curr->nextItemPtr = temp;
    }

    list->num++;

    return 0;
}

MP4Err MP4DeleteTagList(TrackExtTagList* list) {
    TrackExtTagItem* curr = NULL;
    TrackExtTagItem* temp = NULL;
    if (list == NULL)
        return -1;

    curr = list->m_ptr;
    while (curr != NULL) {
        temp = curr;
        curr = curr->nextItemPtr;
        MP4LocalFree(temp);
        list->num--;
    }

    MP4LocalFree(list);

    return 0;
}
