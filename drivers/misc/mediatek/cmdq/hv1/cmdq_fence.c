/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "mtk_sync.h"
#include "cmdq_core.h"
#include "cmdq_fence.h"
/*
**	globle var for fence
*/
#define MAX_FENCE_NUM 20

struct fenceArrayStruct {
	struct fence_data fenceArray[MAX_FENCE_NUM];	/* max 20 fence acquired in CMDQ */

	/* support operation */
	int32_t (*insertFenceToArray)(struct fenceArrayStruct *pFenceArray, struct fence_data const *);
	int32_t (*removeFenceFromArray)(struct fenceArrayStruct *pFenceArray, uint32_t fenceFd);
	struct fence_data *(*getFenceFromArray)(struct fenceArrayStruct *pFenceArray, uint32_t fenceFd);
};

struct fence_data *getFenceFromArray(struct fenceArrayStruct *pFenceArray, uint32_t fenceFd)
{
	int i = 0;

	for (i = 0; i < MAX_FENCE_NUM; i++) {
		if (fenceFd == pFenceArray->fenceArray[i].fence)
			break;
	}

	if (i == MAX_FENCE_NUM) {
		CMDQ_ERR("can't find fenceFd in fenceArray\n");
		for (i = 0; i < MAX_FENCE_NUM; i++) {
			CMDQ_ERR("fence[%d]: value[%d], name[%s], fd[%d]\n",
				i, pFenceArray->fenceArray[i].value,
				pFenceArray->fenceArray[i].name,
				pFenceArray->fenceArray[i].fence);
		}
		return NULL;
	}

	return &(pFenceArray->fenceArray[i]);
}

int32_t insertFenceToArray(struct fenceArrayStruct *pFenceArray, struct fence_data const *pFenceData)
{
	int32_t i = 0;


	/* check if array is full */
	for (i = 0; i < MAX_FENCE_NUM; i++) {
		if (-1 == pFenceArray->fenceArray[i].fence)
			break;
	}

	if (MAX_FENCE_NUM == i) {
		CMDQ_ERR("fence array is full, dump all fence in fenceArray\n");
		for (i = 0; i < MAX_FENCE_NUM; i++) {
			CMDQ_ERR("fence[%d]: value[%d], name[%s], fd[%d]\n",
				i, pFenceArray->fenceArray[i].value,
				pFenceArray->fenceArray[i].name,
				pFenceArray->fenceArray[i].fence);
		}
		return -1;
	}


	/* copy pFenceData to fenceArray[i] */
	pFenceArray->fenceArray[i].fence = pFenceData->fence;
	pFenceArray->fenceArray[i].value = pFenceData->value;
	strncpy(pFenceArray->fenceArray[i].name, pFenceData->name, sizeof(pFenceArray->fenceArray[i].name));


	return 0;
}

int32_t removeFenceFromArray(struct fenceArrayStruct *pFenceArray, uint32_t fenceValue)
{

	int32_t i = 0;

	/* remove all fence which fence.value is smaller than  fenceValue*/

	for (i = 0; i < MAX_FENCE_NUM; i++) {
		if ((0 != pFenceArray->fenceArray[i].value) &&
			(pFenceArray->fenceArray[i].value <= fenceValue)) {

			CMDQ_MSG("remove fence name[%s] value[%d] fd[%d] releaseFenceValue[%d]\n",
				pFenceArray->fenceArray[i].name,
				pFenceArray->fenceArray[i].value,
				pFenceArray->fenceArray[i].fence,
				fenceValue);

			pFenceArray->fenceArray[i].value = 0;
			pFenceArray->fenceArray[i].fence = -1;
			memset(pFenceArray->fenceArray[i].name, 0, sizeof(pFenceArray->fenceArray[i].name));
		}
	}

	return 0;
}

void cmdqFenceInitFenceArrayStruct(struct fenceArrayStruct *pFenceArray)
{
	int32_t i = 0;

	for (i = 0; i < MAX_FENCE_NUM; i++) {
		pFenceArray->fenceArray[i].value = 0;
		memset(pFenceArray->fenceArray[i].name, 0, sizeof(pFenceArray->fenceArray[i].name));
		pFenceArray->fenceArray[i].fence = -1;
	}

	pFenceArray->insertFenceToArray = insertFenceToArray;
	pFenceArray->removeFenceFromArray = removeFenceFromArray;
	pFenceArray->getFenceFromArray = getFenceFromArray;
}



struct cmdqFenceManageStruct {
	struct sw_sync_timeline *timeline;
	uint32_t timelineNum;
	uint32_t fenceNum;
	struct fenceArrayStruct fenceArray;
} cmdqFence;


int32_t cmdqFenceGetFence(struct cmdqFenceStruct *pFence)
{
	struct fence_data fenceData;
	int status = 0;

	if (NULL == pFence) {
		CMDQ_ERR("error, invalid param pFence is NULL\n");
		return -1;
	}

	do {
		fenceData.fence = MTK_FB_INVALID_FENCE_FD;
		fenceData.value = ++(cmdqFence.fenceNum);
		if (cmdqFence.timelineNum >= cmdqFence.fenceNum) {
			CMDQ_ERR("ERROR: fenceNum is smaller than timeline\n");
			status = -1;
			break;
		}

		snprintf(fenceData.name, CMDQ_FENCE_NAME_MAX_LEN, "cmdqFence-%d-%d",
			cmdqFence.timelineNum,
			cmdqFence.fenceNum);

		status = fence_create(cmdqFence.timeline, &fenceData);
		if (0 > status) {
			CMDQ_ERR("create fence error status[%d]\n", status);
			break;
		}

		/* insert fence to fenceArray */
		status = cmdqFence.fenceArray.insertFenceToArray(&(cmdqFence.fenceArray), &fenceData);
		if (status) {
			CMDQ_ERR("store fence to fenceArray failed, may have fence leak\n");
			status = 0;		/* may not a error */
		}

		pFence->fenceFd = fenceData.fence;
		pFence->fenceValue = fenceData.value;
		strncpy(pFence->name, fenceData.name, CMDQ_FENCE_NAME_MAX_LEN - 1);
		CMDQ_MSG("get fence fd[%d] value[%d] name[%s]\n", fenceData.fence, fenceData.value, fenceData.name);
	} while (0);

	return status;
}

int32_t cmdqFenceReleaseFence(struct cmdqFenceStruct fence)
{

	struct fence_data *pFenceData = NULL;
	uint32_t fenceValue = 0;
	int32_t inc = 0;
	int32_t status = 0;

	do {

		CMDQ_MSG("release fence: fd[%d] fenceValue[%d] name[%s] timeline[%d]\n",
			fence.fenceFd, fence.fenceValue, fence.name, cmdqFence.timelineNum);

		if (fence.fenceValue <= cmdqFence.timelineNum) {
			/* fence is already released */
			break;
		}

		pFenceData = cmdqFence.fenceArray.getFenceFromArray(&(cmdqFence.fenceArray), fence.fenceFd);
		if (NULL == pFenceData) {
			CMDQ_ERR("can't find fence Fd[%d] in fenceArray, fenceValue[%d], timeline[%d]\n",
					fence.fenceFd, fence.fenceValue, cmdqFence.timelineNum);
			status = -1;
			break;
		}

		/* release fence */
		fenceValue = pFenceData->value;
		cmdqFence.fenceArray.removeFenceFromArray(&(cmdqFence.fenceArray), fenceValue);

		inc = fenceValue - cmdqFence.timelineNum;
		if (1 != inc)
			CMDQ_MSG("release more than one fence[%d] in one time\n", inc);

		while (inc--) {
			timeline_inc(cmdqFence.timeline, 1);
			cmdqFence.timelineNum += 1;
		}


	} while (0);

	return status;
}


int32_t cmdqFenceCreateTimeLine(void)
{
	struct sw_sync_timeline *timeline = NULL;

	/* init cmdqFence */
	memset(&cmdqFence, 0, sizeof(struct cmdqFenceManageStruct));
	cmdqFenceInitFenceArrayStruct(&(cmdqFence.fenceArray));

	timeline = timeline_create("cmdq_fence_timeline");
	if (NULL == timeline) {
		CMDQ_ERR("create cmdq_fence_timeline failed");
		return -1;
	}

	cmdqFence.timeline = timeline;
	return 0;
}


