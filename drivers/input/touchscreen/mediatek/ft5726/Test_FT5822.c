/*
 * Copyright (C) 2012-2015, Focaltech Systems (R),All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/************************************************************************
* File Name: Test_FT5822.c
*
* Author: Software Development Team, AE
*
* Created: 2015-07-14
*
* Abstract: test item for FT5822\FT5626\FT5726\FT5826B
*
************************************************************************/
/*******************************************************************************
* Included header files
*******************************************************************************/
#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "Global.h"
#include "DetailThreshold.h"
#include "Test_FT5822.h"
#include "Config_FT5822.h"

/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/
enum WaterproofType {
	WT_NeedProofOnTest,
	WT_NeedProofOffTest,
	WT_NeedTxOnVal,

	WT_NeedRxOnVal,
	WT_NeedTxOffVal,
	WT_NeedRxOffVal,
};

/*******************************************************************************
* Static variables
*******************************************************************************/
static int m_RawData[TX_NUM_MAX][RX_NUM_MAX] = { {0, 0} };
static int m_iTempRawData[TX_NUM_MAX * RX_NUM_MAX] = { 0 };
static unsigned char m_ucTempData[TX_NUM_MAX * RX_NUM_MAX * 2] = { 0 };

static bool m_bV3TP;
static int RxLinearity[TX_NUM_MAX][RX_NUM_MAX] = { {0, 0} };
static int TxLinearity[TX_NUM_MAX][RX_NUM_MAX] = { {0, 0} };
static int m_DifferData[TX_NUM_MAX][RX_NUM_MAX] = { {0} };
static int m_absDifferData[TX_NUM_MAX][RX_NUM_MAX] = { {0} };

/* About Store Test Dat */
static char g_pStoreAllData[1024 * 80];

static char *g_pTmpBuff;
static char *g_pStoreMsgArea;
static int g_lenStoreMsgArea;
static char *g_pMsgAreaLine2;
static int g_lenMsgAreaLine2;
static char *g_pStoreDataArea;
static int g_lenStoreDataArea;
static unsigned char m_ucTestItemCode;
static int m_iStartLine;
static int m_iTestDataCount;
static char *g_pInfoBuff;
static char *g_pStoreInfoArea;
static int g_lenStoreInfoArea;
static char *g_pInfoArea;
static int g_lenInfoArea;

/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/
bool Rawresult = true;
bool ScapCbesult = true;
bool ScapRawresult = true;
bool Unifresult = true;

/*******************************************************************************
* Static function prototypes
*******************************************************************************/
/* Communication function */
static int StartScan(void);
static unsigned char ReadRawData(unsigned char Freq, unsigned char LineNum,
				 int ByteNum, int *pRevBuffer);
static unsigned char GetPanelRows(unsigned char *pPanelRows);
static unsigned char GetPanelCols(unsigned char *pPanelCols);
static unsigned char GetTxSC_CB(unsigned char index, unsigned char *pcbValue);
/* Common function */
static unsigned char GetRawData(void);
static unsigned char GetChannelNum(void);
/* about Test */
static void InitTest(void);
static void FinishTest(void);
static void Save_Test_Data(int iData[TX_NUM_MAX][RX_NUM_MAX], int iArrayIndex,
			   unsigned char Row, unsigned char Col,
			   unsigned char ItemCount);
static void InitStoreParamOfTestData(void);
static void MergeAllTestData(void);
static void Save_Info_Data(unsigned char *, int);
/* Others */
static void AllocateMemory(void);
static void FreeMemory(void);
/*static void ShowRawData(void);*/
static boolean GetTestCondition(int iTestType, unsigned char ucChannelValue);
static unsigned char GetChannelNumNoMapping(void);
static unsigned char SwitchToNoMapping(void);

/************************************************************************
* Name: FT5822_StartTest
* Brief:  Test entry. Determine which test item to test
* Input: none
* Output: none
* Return: Test Result, PASS or FAIL
***********************************************************************/
boolean FT5822_StartTest(void)
{
	bool bTestResult = true;
	bool bTempResult = 1;
	unsigned char ReCode = 0;
	unsigned char ucDevice = 0;
	int iItemCount = 0;

	/* 1. Init part */
	InitTest();
	/* 2. test item */
	if (0 == g_TestItemNum)
		bTestResult = false;

	for (iItemCount = 0; iItemCount < g_TestItemNum; iItemCount++) {
		m_ucTestItemCode = g_stTestItem[ucDevice][iItemCount].ItemCode;

		/* FT5822_ENTER_FACTORY_MODE */
		if (Code_FT5822_ENTER_FACTORY_MODE ==
		    g_stTestItem[ucDevice][iItemCount].ItemCode) {

			ReCode = FT5822_TestItem_EnterFactoryMode();

			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult =
				    RESULT_NG;

				break;	/* if this item FAIL, no longer test. */
			}
			g_stTestItem[ucDevice][iItemCount].TestResult =
				    RESULT_PASS;
		}

		/* FT5822_RAWDATA_TEST */
		if (Code_FT5822_RAWDATA_TEST ==
		    g_stTestItem[ucDevice][iItemCount].ItemCode) {

			ReCode = FT5822_TestItem_RawDataTest(&bTempResult);

			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult =
				    RESULT_NG;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult =
				    RESULT_PASS;
		}

		/* FT5822_SCAP_CB_TEST */
		if (Code_FT5822_SCAP_CB_TEST ==
		    g_stTestItem[ucDevice][iItemCount].ItemCode) {
			ReCode = FT5822_TestItem_SCapCbTest(&bTempResult);

			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult =
				    RESULT_NG;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult =
				    RESULT_PASS;
		}
		/* FT5822_SCAP_RAWDATA_TEST */
		if (Code_FT5822_SCAP_RAWDATA_TEST ==
		    g_stTestItem[ucDevice][iItemCount].ItemCode) {

			ReCode = FT5822_TestItem_SCapRawDataTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult =
				    RESULT_NG;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult =
				    RESULT_PASS;
		}

		/* FT5X22_PANELDIFFER_TEST */
		if (Code_FT5822_PANELDIFFER_TEST ==
		    g_stTestItem[ucDevice][iItemCount].ItemCode) {
			pr_info("Code_FT5822_PANELDIFFER_TEST\n");

			ReCode = FT5822_TestItem_PanelDifferTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult =
				    RESULT_NG;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult =
				    RESULT_PASS;
		}

		/* FT5X22_UNIFORMITY_TEST */
		if (Code_FT5822_UNIFORMITY_TEST ==
		    g_stTestItem[ucDevice][iItemCount].ItemCode) {

			ReCode = FT5822_TestItem_UniformityTest(&bTempResult);
			if (ERROR_CODE_OK != ReCode || (!bTempResult)) {
				bTestResult = false;
				g_stTestItem[ucDevice][iItemCount].TestResult =
				    RESULT_NG;
			} else
				g_stTestItem[ucDevice][iItemCount].TestResult =
				    RESULT_PASS;
		}
	}

	/* 3. End Part */
	/* Enter_WorkMode */
	for (iItemCount = 0; iItemCount <= 3; iItemCount++) {
		ReCode = EnterWork();
		if (ReCode == ERROR_CODE_OK) {
			pr_info("[focal] Enter_WorkMode OK! (%d)\n",
			       iItemCount);
			break;
		}
		SysDelay(50);
		pr_err("[focal] Enter_WorkMode FAIL! (%d)\n",
			       iItemCount);
	}

	FinishTest();
	/* 4. return result */
	return bTestResult;
}

/************************************************************************
* Name: InitTest
* Brief:  Init all param before test
* Input: none
* Output: none
* Return: none
***********************************************************************/
static void InitTest(void)
{
	/* Allocate pointer Memory */
	AllocateMemory();
	InitStoreParamOfTestData();
	/* show lib version */
	pr_info("[focal] %s\n", IC_TEST_VERSION);
}

/************************************************************************
* Name: FinishTest
* Brief:  Init all param before test
* Input: none
* Output: none
* Return: none
***********************************************************************/
static void FinishTest(void)
{
	/* Merge Test Result */
	MergeAllTestData();
	/* Release pointer memory */
	FreeMemory();
}

/************************************************************************
* Name: FT5822_get_test_data
* Brief:  get data of test result
* Input: none
* Output: pTestData, the returned buff
* Return: the length of test data. if length > 0, got data;else ERR.
***********************************************************************/
int FT5822_get_test_data(char *pTestData)
{
	if (NULL == pTestData)
		return -1;

	/* Msg + Data Area */
	memcpy(pTestData, g_pStoreAllData,
	       (g_lenStoreMsgArea + g_lenStoreDataArea));
	/*Add Info Area */
	memcpy(pTestData + (g_lenStoreMsgArea + g_lenStoreDataArea),
	       g_pStoreInfoArea, g_lenStoreInfoArea);

	return (g_lenStoreMsgArea + g_lenStoreDataArea + g_lenStoreInfoArea);
}

/************************************************************************
* Name: FT5822_TestItem_EnterFactoryMode
* Brief:  Check whether TP can enter Factory Mode, and do some thing
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT5822_TestItem_EnterFactoryMode(void)
{
	unsigned char ReCode = ERROR_CODE_INVALID_PARAM;
	int iRedo = 5;	/* retest times */
	int i;
	unsigned char chPattern = 0;

	SysDelay(150);
	for (i = 1; i <= iRedo; i++) {
		ReCode = EnterFactory();
		if (ERROR_CODE_OK != ReCode) {
			pr_err("Failed to Enter factory mode...\n");
			if (i < iRedo) {
				SysDelay(50);
				continue;
			}
		} else {
			break;
		}
	}
	SysDelay(300);

	if (ReCode != ERROR_CODE_OK)
		return ReCode;

	/* get channel numbers in factory mode */
	ReCode = GetChannelNum();

	/* set FIR,0: disable, 1: enable */
	/* theDevice.m_cHidDev[m_NumDevice]->WriteReg(REG_FIR, 0); */

	/* Determine whether the pattern is V3 */
	ReCode = ReadReg(REG_PATTERN_5422, &chPattern);

	if (chPattern == 1)
		m_bV3TP = true;
	else
		m_bV3TP = false;

	return ReCode;
}

/************************************************************************
* Name: FT5822_TestItem_RawDataTest
* Brief:  TestItem: RawDataTest. Check if MCAP RawData is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT5822_TestItem_RawDataTest(bool *bTestResult)
{
	unsigned char ReCode = 0;
	bool btmpresult = true;
	int RawDataMin;
	int RawDataMax;
	unsigned char ucFre;
	unsigned char strSwitch = 0;
	unsigned char OriginValue = 0xff;
	unsigned char FirValue = 0;
	int index = 0;
	int iRow, iCol;
	int iValue = 0;
	int iLen = 0;

	pr_info("\n\nTest Item: --------- Raw Data Test\n\n");
	g_lenInfoArea = 0;

	ReCode = EnterFactory();
	if (ReCode != ERROR_CODE_OK) {
		pr_err("\n\n Failed to Enter factory Mode. Error Code: %d",
		       ReCode);
		goto TEST_ERR;
	}
	/* Determine whether the pattern is V3 */
	 /* before mapping: 0x54=1; after mapping: 0x54=0;*/
	if (m_bV3TP) {
		ReCode = ReadReg(REG_MAPPING_SWITCH, &strSwitch);
		if (strSwitch != 0) {
			ReCode = WriteReg(REG_MAPPING_SWITCH, 0);
			if (ReCode != ERROR_CODE_OK)
				goto TEST_ERR;
		}
	}

	/* read raw data, 0x16 =0 (default) */
	ReCode = ReadReg(REG_NORMALIZE_TYPE, &OriginValue);
	if (ReCode != ERROR_CODE_OK)
		goto TEST_ERR;

	if (g_ScreenSetParam.isNormalize == Auto_Normalize) {
		pr_err("Auto_Normalize\n");

		if (OriginValue != 1) {
			ReCode = WriteReg(REG_NORMALIZE_TYPE, 0x01);
			if (ReCode != ERROR_CODE_OK)
				goto TEST_ERR;
		}

		pr_info("\n=========Set Frequecy High\n");

		ReCode = ReadReg(REG_FREQUENCY, &ucFre);
		if (ReCode != ERROR_CODE_OK)
			goto TEST_ERR;

		ReCode = WriteReg(REG_FREQUENCY, 0x81);
		if (ReCode != ERROR_CODE_OK)
			goto TEST_ERR;

		pr_info("\n=========FIR State: ON\n");

		ReCode = ReadReg(REG_FIR, &FirValue);
		if (ReCode != ERROR_CODE_OK)
			goto TEST_ERR;

		/*FIR OFF  0: disable, 1: enable */
		ReCode = WriteReg(REG_FIR, 1);
		if (ReCode != ERROR_CODE_OK)
			goto TEST_ERR;

		/*throw away three frames after reg value changed */
		for (index = 0; index < 3; ++index)
			ReCode = GetRawData();

		if (ReCode != ERROR_CODE_OK) {
			pr_err("\nGet Rawdata failed, Error Code: 0x%x",
			       ReCode);
			goto TEST_ERR;
		}
		/* ShowRawData(); */

		/* Add Info Area */
		iLen =
		    snprintf(g_pInfoBuff, INFOBUFF_SIZE,
				"\n<<Raw Data Test:Auto_Normalize\n");
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;

		/* To Determine RawData if in Range or not */
		for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; iRow++) {
			for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; iCol++) {
				if (g_stCfg_MCap_DetailThreshold.InvalidNode
				    [iRow][iCol] == 0)
					continue;	/*Invalid Node */

				RawDataMin =
				    g_stCfg_MCap_DetailThreshold.
				    RawDataTest_High_Min
				    [iRow][iCol];
				RawDataMax =
				    g_stCfg_MCap_DetailThreshold.
				    RawDataTest_High_Max
				    [iRow][iCol];
				iValue = m_RawData[iRow][iCol];

				if (iValue < RawDataMin ||
					iValue > RawDataMax) {
					btmpresult = false;
					pr_err
			("rawdata test failure.RawData[%d][%d]=%d (%d,%d)\n",
					     iRow + 1, iCol + 1, iValue,
					     RawDataMin, RawDataMax);
				#ifdef SHOW_INFO
					/* Add Info Area */
					iLen = snprintf(g_pInfoBuff,
						INFOBUFF_SIZE,
						"Raw[%d][%d]=%d(%d,%d)\n",
						iRow + 1, iCol + 1,
						iValue, RawDataMin,
						RawDataMax);
					memcpy(g_pInfoArea + g_lenInfoArea,
						g_pInfoBuff, iLen);
					g_lenInfoArea += iLen;
				#endif
				}
			}
		}

		Save_Test_Data(m_RawData, 0, g_ScreenSetParam.iTxNum,
			       g_ScreenSetParam.iRxNum, 2);
	} else {
		pr_err("non Auto_Normalize\n");

		if (OriginValue != 0) {
			ReCode = WriteReg(REG_NORMALIZE_TYPE, 0x00);
			if (ReCode != ERROR_CODE_OK)
				goto TEST_ERR;
		}

		ReCode = ReadReg(REG_FREQUENCY, &ucFre);
		if (ReCode != ERROR_CODE_OK)
			goto TEST_ERR;

		if (g_stCfg_FT5822_BasicThreshold.RawDataTest_SetLowFreq) {
			pr_info("\n=========Set Frequecy Low\n");
			ReCode = WriteReg(REG_FREQUENCY, 0x80);
			if (ReCode != ERROR_CODE_OK)
				goto TEST_ERR;

		/*FIR OFF  0: disable, 1: enable */
			pr_info("\n=========FIR State: OFF\n");
			ReCode = WriteReg(REG_FIR, 0);
			if (ReCode != ERROR_CODE_OK)
				goto TEST_ERR;

			SysDelay(100);

		/*throw away three frames after reg value changed */
			for (index = 0; index < 3; ++index)
				ReCode = GetRawData();

			if (ReCode != ERROR_CODE_OK) {
				pr_err("\nGet Rawdata failed, Error Code: 0x%x",
				       ReCode);
				goto TEST_ERR;
			}

			/* ShowRawData(); */

			/* Add Info Area */
			iLen =
				snprintf(g_pInfoBuff, INFOBUFF_SIZE,
					"\n<<Raw Data Test,non Auto_Normalize,LF\n");
			memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
			g_lenInfoArea += iLen;

			/*To Determine RawData if in Range or not */
			for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; iRow++) {
				for (iCol = 0; iCol < g_ScreenSetParam.iRxNum;
				     iCol++) {
						/*Invalid Node */
					if (g_stCfg_MCap_DetailThreshold.
						InvalidNode[iRow][iCol] == 0)
							continue;
					RawDataMin =
					    g_stCfg_MCap_DetailThreshold.
					    RawDataTest_High_Min
					    [iRow][iCol];
					RawDataMax =
					    g_stCfg_MCap_DetailThreshold.
					    RawDataTest_High_Max
					    [iRow][iCol];
					iValue = m_RawData[iRow][iCol];

					if (iValue < RawDataMin
					    || iValue > RawDataMax) {
						btmpresult = false;
						pr_err
		("rawdata test failure.RawData[%d][%d]=%d (%d,%d)\n",
						     iRow + 1, iCol + 1, iValue,
						     RawDataMin, RawDataMax);
				#ifdef SHOW_INFO
					/* Add Info Area */
					iLen = snprintf(g_pInfoBuff,
						INFOBUFF_SIZE,
						"Raw[%d][%d]=%d(%d,%d)\n",
						iRow + 1, iCol + 1,
						iValue, RawDataMin,
						RawDataMax);
					memcpy(g_pInfoArea + g_lenInfoArea,
						g_pInfoBuff, iLen);
					g_lenInfoArea += iLen;
				#endif
					}
				}
			}
			/*Save Test Data */
			Save_Test_Data(m_RawData, 0,
				       g_ScreenSetParam.iTxNum,
				       g_ScreenSetParam.iRxNum, 1);
		}

		if (g_stCfg_FT5822_BasicThreshold.RawDataTest_SetHighFreq) {
			pr_info("\n=========Set Frequecy High\n");
			ReCode = WriteReg(REG_FREQUENCY, 0x81);
			if (ReCode != ERROR_CODE_OK)
				goto TEST_ERR;

			/*FIR OFF  0: disable, 1: enable */
			pr_info("\n=========FIR State: OFF\n");

			ReCode = WriteReg(REG_FIR, 0);
			if (ReCode != ERROR_CODE_OK)
				goto TEST_ERR;
			SysDelay(100);

			/*throw away three frames after reg value changed */
			for (index = 0; index < 3; ++index)
				ReCode = GetRawData();

			if (ReCode != ERROR_CODE_OK) {
				pr_err("\nGet Rawdata failed, Error Code: 0x%x",
				       ReCode);
				if (ReCode != ERROR_CODE_OK)
					goto TEST_ERR;
			}

			/* ShowRawData(); */

			/* Add Info Area */
			iLen =
				snprintf(g_pInfoBuff, INFOBUFF_SIZE,
				"\n<<Raw Data Test,non Auto_Normalize,HF\n");
			memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
			g_lenInfoArea += iLen;

			/* To Determine RawData if in Range or not */
			for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; iRow++) {
				for (iCol = 0; iCol < g_ScreenSetParam.
					iRxNum; iCol++) {
						/* Invalid Node */
					if (g_stCfg_MCap_DetailThreshold.
						InvalidNode[iRow][iCol] == 0)
							continue;
					RawDataMin =
					    g_stCfg_MCap_DetailThreshold.
					    RawDataTest_High_Min
					    [iRow][iCol];
					RawDataMax =
					    g_stCfg_MCap_DetailThreshold.
					    RawDataTest_High_Max
					    [iRow][iCol];
					iValue = m_RawData[iRow][iCol];

					if (iValue < RawDataMin
					    || iValue > RawDataMax) {
						btmpresult = false;
						pr_err
			("rawdata test failure.RawData[%d][%d]=%d (%d,%d)\n",
						     iRow + 1, iCol + 1, iValue,
						     RawDataMin, RawDataMax);
					#ifdef SHOW_INFO
						/* Add Info Area */
						iLen =
							snprintf(g_pInfoBuff,
								INFOBUFF_SIZE,
						       "Raw[%d][%d]=%d(%d,%d)\n",
						       iRow + 1, iCol + 1,
						       iValue, RawDataMin,
						       RawDataMax);
					memcpy(g_pInfoArea + g_lenInfoArea,
							g_pInfoBuff, iLen);
						g_lenInfoArea += iLen;
					#endif
					}
				}
			}
			/* Save Test Data */
			Save_Test_Data(m_RawData, 0,
				       g_ScreenSetParam.iTxNum,
				       g_ScreenSetParam.iRxNum, 2);
		}
	}
	/* Restore register value */
	ReCode = WriteReg(REG_NORMALIZE_TYPE, OriginValue);
	if (ReCode != ERROR_CODE_OK)
		goto TEST_ERR;

	/* Restore mapping value of V3 pattern */
	if (m_bV3TP) {
		ReCode = WriteReg(REG_MAPPING_SWITCH, strSwitch);
		if (ReCode != ERROR_CODE_OK)
			goto TEST_ERR;
	}

	/* Result */
	if (btmpresult) {
		*bTestResult = true;
		pr_info("\n\nRawData Test PASS\n");
	} else {
		*bTestResult = false;
		pr_err("\n\nRawData Test FAIL\n");
	}

	/* Add Info Area */
	if (btmpresult) {
		iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
			"\nRawData Test PASS\n\n");
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;
	} else {
		iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
			"\nRawData Test FAIL\n\n");
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;
	}
	Save_Info_Data(g_pInfoArea, g_lenInfoArea);

	/* post-stage work */
	ReCode = WriteReg(REG_FREQUENCY, ucFre);
	SysDelay(100);
	if (ReCode != ERROR_CODE_OK)
		goto TEST_ERR;

	ReCode = WriteReg(REG_FIR, FirValue);
	SysDelay(100);
	if (ReCode != ERROR_CODE_OK)
		goto TEST_ERR;

	return ReCode;

TEST_ERR:
	*bTestResult = false;
	pr_err("\n\nRawData Test FAIL\n");

	/* Add Info Area */
	iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE, "\nRawData Test FAIL\n\n");
	memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
	g_lenInfoArea += iLen;

	return ReCode;
}

/************************************************************************
* Name: FT5822_TestItem_SCapRawDataTest
* Brief:  TestItem: SCapRawDataTest. Check if SCAP RawData is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT5822_TestItem_SCapRawDataTest(bool *bTestResult)
{
	int i = 0;
	int RawDataMin = 0;
	int RawDataMax = 0;
	int Value = 0;
	boolean bFlag = true;
	unsigned char ReCode = 0;
	boolean btmpresult = true;
	int iMax = 0;
	int iMin = 0;
	int iAvg = 0;
	int ByteNum = 0;
	unsigned char wc_value = 0;	/* waterproof channel value */
	unsigned char ucValue = 0;
	int iCount = 0;
	int ibiggerValue = 0;
	int iLen = 0;

	pr_info("\n\n Test Item: -------- Scap RawData Test\n\n");
	g_lenInfoArea = 0;

	/* 1.Preparatory work */
	/*in Factory Mode */
	ReCode = EnterFactory();
	if (ReCode != ERROR_CODE_OK) {
		pr_err("\n\n Failed to Enter factory Mode. Error Code: %d",
		       ReCode);
		goto TEST_ERR;
	}

	/* get waterproof channel setting,
	 * to check if Tx/Rx channel need to test */
	ReCode = ReadReg(REG_WATER_CHANNEL_SELECT, &wc_value);

	if (ReCode != ERROR_CODE_OK)
		goto TEST_ERR;

	/* If it is V3 pattern, Get Tx/Rx Num again */
	ReCode = SwitchToNoMapping();
	if (ReCode != ERROR_CODE_OK)
		goto TEST_ERR;

	/* 2.Get SCap Raw Data,
	 * Step:1.Start Scanning; 2. Read Raw Data */
	ReCode = StartScan();
	if (ReCode != ERROR_CODE_OK) {
		pr_err("Failed to Scan SCap RawData!\n");
		goto TEST_ERR;
	}

	for (i = 0; i < 3; i++) {
		memset(m_iTempRawData, 0, ARRAY_SIZE(m_iTempRawData));

		/* waterproof rawdata */
		ByteNum =
		    (g_ScreenSetParam.iTxNum + g_ScreenSetParam.iRxNum) * 2;
		ReCode = ReadRawData(0, 0xAC, ByteNum, m_iTempRawData);
		if (ReCode != ERROR_CODE_OK)
			goto TEST_ERR;

		memcpy(m_RawData[0 + g_ScreenSetParam.iTxNum], m_iTempRawData,
		       sizeof(int) * g_ScreenSetParam.iRxNum);
		memcpy(m_RawData[1 + g_ScreenSetParam.iTxNum],
		       m_iTempRawData + g_ScreenSetParam.iRxNum,
		       sizeof(int) * g_ScreenSetParam.iTxNum);

		/* non-waterproof rawdata */
		ByteNum =
		    (g_ScreenSetParam.iTxNum + g_ScreenSetParam.iRxNum) * 2;
		ReCode = ReadRawData(0, 0xAB, ByteNum, m_iTempRawData);
		if (ReCode != ERROR_CODE_OK)
			goto TEST_ERR;

		memcpy(m_RawData[2 + g_ScreenSetParam.iTxNum], m_iTempRawData,
		       sizeof(int) * g_ScreenSetParam.iRxNum);
		memcpy(m_RawData[3 + g_ScreenSetParam.iTxNum],
		       m_iTempRawData + g_ScreenSetParam.iRxNum,
		       sizeof(int) * g_ScreenSetParam.iTxNum);
	}

	/* 3. Judge */
	/* Waterproof ON */
	bFlag = GetTestCondition(WT_NeedProofOnTest, wc_value);

	/* Add Info Area */
	iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
		"\n<<Scap RawData Test>>\n");
	memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
	g_lenInfoArea += iLen;

	if (g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_SetWaterproof_ON
	    && bFlag) {
		iCount = 0;
		RawDataMin =
		    g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_ON_Min;
		RawDataMax =
		    g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_ON_Max;
		iMax = -m_RawData[0 + g_ScreenSetParam.iTxNum][0];
		iMin = 2 * m_RawData[0 + g_ScreenSetParam.iTxNum][0];
		iAvg = 0;
		Value = 0;

		bFlag = GetTestCondition(WT_NeedRxOnVal, wc_value);
		if (bFlag)
			pr_info("Judge Rx in Waterproof-ON:\n");

		/* Add Info Area */
		iLen =
		    snprintf(g_pInfoBuff, INFOBUFF_SIZE, "\nWaterproof ON\n");
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;

		for (i = 0; bFlag && i < g_ScreenSetParam.iRxNum; i++) {
			if (g_stCfg_MCap_DetailThreshold.InvalidNode_SC[0][i] == 0) {
				m_RawData[0 + g_ScreenSetParam.iTxNum][i] = 0;
				continue;
			}

			RawDataMin =
			    g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Min[0][i];
			RawDataMax =
			    g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Max[0][i];

			Value = m_RawData[0 + g_ScreenSetParam.iTxNum][i];
			iAvg += Value;

			/* find the Max value */
			if (iMax < Value)
				iMax = Value;
			/* fine the min value */
			if (iMin > Value)
				iMin = Value;
			if (Value > RawDataMax || Value < RawDataMin) {
				btmpresult = false;
				pr_err
				("Num=%d,Value=%d (%d,%d)\n",
				i + 1, Value, RawDataMin, RawDataMax);
				#ifdef SHOW_INFO
				/* Add Info Area */
				iLen =
				    snprintf(g_pInfoBuff, INFOBUFF_SIZE,
						"Rx%d=%d(%d,%d)\n",
					    i + 1, Value, RawDataMin,
					    RawDataMax);
				memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff,
				       iLen);
				g_lenInfoArea += iLen;
				#endif
			}
			iCount++;
		}

		bFlag = GetTestCondition(WT_NeedTxOnVal, wc_value);
		if (bFlag)
			pr_info("Judge Tx in Waterproof-ON:\n");

		for (i = 0; bFlag && i < g_ScreenSetParam.iTxNum; i++) {
			if (g_stCfg_MCap_DetailThreshold.InvalidNode_SC[1][i] == 0) {
				m_RawData[1 + g_ScreenSetParam.iTxNum][i] = 0;
				continue;
			}

			RawDataMin =
			    g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Min[1][i];
			RawDataMax =
			    g_stCfg_MCap_DetailThreshold.SCapRawDataTest_ON_Max[1][i];

			Value = m_RawData[1 + g_ScreenSetParam.iTxNum][i];
			iAvg += Value;
			/* find the Max value */
			if (iMax < Value)
				iMax = Value;
			/* fine the min value */
			if (iMin > Value)
				iMin = Value;
			if (Value > RawDataMax || Value < RawDataMin) {
				btmpresult = false;
				pr_err
				("Num=%d,Value=%d (%d,%d)\n",
				i + 1, Value, RawDataMin, RawDataMax);
				#ifdef SHOW_INFO
				/* Add Info Area */
				iLen =
				    snprintf(g_pInfoBuff, INFOBUFF_SIZE,
						"Tx%d=%d(%d,%d)\n",
					    i + 1, Value, RawDataMin,
					    RawDataMax);
				memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff,
				       iLen);
				g_lenInfoArea += iLen;
				#endif
			}
			iCount++;
		}

		if (0 == iCount) {
			iAvg = 0;
			iMax = 0;
			iMin = 0;
		} else
			iAvg = iAvg / iCount;

		pr_info
		("\nSCap RawData in Waterproof-ON,");
		pr_info
		(" Max:%d,Min:%d,Deviation:%d,Average:%d\n",
		iMax, iMin, iMax - iMin, iAvg);

		/* Add Info Area */
		iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
			       "\nMax:%d,Min:%d,Deviation:%d,Average:%d\n",
			       iMax, iMin, iMax - iMin, iAvg);
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;

		/* Save Test Data */
		ibiggerValue =
		    g_ScreenSetParam.iTxNum >
		    g_ScreenSetParam.
		    iRxNum ? g_ScreenSetParam.iTxNum : g_ScreenSetParam.iRxNum;

		Save_Test_Data(m_RawData, g_ScreenSetParam.iTxNum + 0, 2,
			       ibiggerValue, 1);
	}

	/* Waterproof OFF */
	bFlag = GetTestCondition(WT_NeedProofOffTest, wc_value);

	if (g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_SetWaterproof_OFF
	    && bFlag) {
		iCount = 0;
		RawDataMin =
		    g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_OFF_Min;
		RawDataMax =
		    g_stCfg_FT5822_BasicThreshold.SCapRawDataTest_OFF_Max;

		iMax = -m_RawData[2 + g_ScreenSetParam.iTxNum][0];
		iMin = 2 * m_RawData[2 + g_ScreenSetParam.iTxNum][0];
		iAvg = 0;
		Value = 0;

		bFlag = GetTestCondition(WT_NeedRxOffVal, wc_value);
		if (bFlag)
			pr_err("Judge Rx in Waterproof-OFF:\n");

		/* Add Info Area */
		iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
			       "\nWaterproof OFF\n");
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;

		for (i = 0; bFlag && i < g_ScreenSetParam.iRxNum; i++) {
			if (g_stCfg_MCap_DetailThreshold.InvalidNode_SC[0][i] == 0) {
				m_RawData[2 + g_ScreenSetParam.iTxNum][i] = 0;
				continue;
			}

			RawDataMin =
			    g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Min[0][i];
			RawDataMax =
			    g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Max[0][i];

			Value = m_RawData[2 + g_ScreenSetParam.iTxNum][i];
			iAvg += Value;

			if (iMax < Value)
				iMax = Value;

			if (iMin > Value)
				iMin = Value;

			if (Value > RawDataMax || Value < RawDataMin) {
				btmpresult = false;
				pr_err
				    ("Num=%d,Value=%d,range=(%d,%d):\n",
				     i + 1, Value, RawDataMin, RawDataMax);
			#ifdef SHOW_INFO
				/* Add Info Area */
				iLen =
				    snprintf(g_pInfoBuff, INFOBUFF_SIZE,
						"Rx%d=%d(%d,%d)\n",
					    i + 1, Value, RawDataMin,
					    RawDataMax);
				memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff,
				       iLen);
				g_lenInfoArea += iLen;
			#endif
			}
			iCount++;
		}

		bFlag = GetTestCondition(WT_NeedTxOffVal, wc_value);

		if (bFlag)
			pr_info("Judge Tx in Waterproof-OFF:\n");

		for (i = 0; bFlag && i < g_ScreenSetParam.iTxNum; i++) {
			if (g_stCfg_MCap_DetailThreshold.InvalidNode_SC[1][i] == 0) {
				m_RawData[3 + g_ScreenSetParam.iTxNum][i] = 0;
				continue;
			}

			Value = m_RawData[3 + g_ScreenSetParam.iTxNum][i];

			RawDataMin =
			    g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Min[1][i];
			RawDataMax =
			    g_stCfg_MCap_DetailThreshold.SCapRawDataTest_OFF_Max[1][i];

			iAvg += Value;

			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;

			if (Value > RawDataMax || Value < RawDataMin) {
				btmpresult = false;
				pr_err
				("Num=%d,Value=%d,range=(%d,%d):\n",
				i + 1, Value, RawDataMin, RawDataMax);
			#ifdef SHOW_INFO
				/* Add Info Area */
				iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
						"Tx%d=%d(%d,%d)\n",
					    i + 1, Value, RawDataMin,
					    RawDataMax);
				memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff,
				       iLen);
				g_lenInfoArea += iLen;
			#endif
			}
			iCount++;
		}

		if (0 == iCount) {
			iAvg = 0;
			iMax = 0;
			iMin = 0;
		} else
			iAvg = iAvg / iCount;

		pr_info
		    ("SCap RawData in Waterproof-OFF,\n");
		pr_info
		    (" Max : %d, Min: %d, Deviation: %d, Average: %d\n",
		     iMax, iMin, iMax - iMin, iAvg);

		/* Add Info Area */
		iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
			       "\nMax:%d,Min:%d,Deviation:%d,Average:%d\n",
			       iMax, iMin, iMax - iMin, iAvg);
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;

		/* Save Test Data */
		ibiggerValue =
		    g_ScreenSetParam.iTxNum >
		    g_ScreenSetParam.
		    iRxNum ? g_ScreenSetParam.iTxNum : g_ScreenSetParam.iRxNum;

		Save_Test_Data(m_RawData,
			       g_ScreenSetParam.iTxNum + 2, 2, ibiggerValue, 2);
	}

	/* 4. post-stage work */
	if (m_bV3TP) {
		ReCode = ReadReg(REG_MAPPING_SWITCH, &ucValue);
		if (0 != ucValue) {
			ReCode = WriteReg(REG_MAPPING_SWITCH, 0);
			SysDelay(10);
			if (ReCode != ERROR_CODE_OK) {
				pr_err("Failed to switch mapping type!\n ");
				btmpresult = false;
			}
		}

		/* SCap will use before mapping.
		  After the end of the test item,
		  MCap must transfer to after Mapping */
		GetChannelNum();
	}

	/* 5. Test Result */
	if (btmpresult) {
		*bTestResult = true;
		pr_info("\n\nSCap RawData Test PASS\n");
	} else {
		*bTestResult = false;
		pr_err("\n\nSCap RawData Test FAIL\n");
	}

	/* Add Info Area */
	if (btmpresult) {
		iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
			"\n\nSCap RawData Test PASS\n\n");
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;
	} else {
		iLen =
		    snprintf(g_pInfoBuff, INFOBUFF_SIZE,
			"\n\nSCap RawData Test FAIL\n\n");
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;
	}
	Save_Info_Data(g_pInfoArea, g_lenInfoArea);

	return ReCode;

TEST_ERR:
	*bTestResult = false;
	pr_err("\n\nSCap RawData Test FAIL\n");

	/* Add Info Area */
	iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
		"\n\nSCap RawData Test FAIL\n\n");
	memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
	g_lenInfoArea += iLen;
	Save_Info_Data(g_pInfoArea, g_lenInfoArea);

	return ReCode;
}

/************************************************************************
* Name: FT5822_TestItem_SCapCbTest
* Brief:  TestItem: SCapCbTest. Check if SCAP Cb is within the range.
* Input: none
* Output: bTestResult, PASS or FAIL
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char FT5822_TestItem_SCapCbTest(bool *bTestResult)
{
	int i, index, Value, CBMin, CBMax;
	boolean bFlag = true;
	unsigned char ReCode;
	boolean btmpresult = true;
	int iMax, iMin, iAvg;
	unsigned char wc_value = 0;
	unsigned char ucValue = 0;
	int iCount = 0;
	int ibiggerValue = 0;
	int iLen = 0;

	pr_info("\n\n Test Item: -----  Scap CB Test\n\n");
	g_lenInfoArea = 0;

	/* 1.Preparatory work */
	/* in Factory Mode */
	ReCode = EnterFactory();
	if (ReCode != ERROR_CODE_OK) {
		pr_err("\n\n Failed to Enter factory Mode. Error Code: %d",
		       ReCode);
		goto TEST_ERR;
	}

	/* get waterproof channel setting,
	 * to check if Tx/Rx channel need to test */
	ReCode = ReadReg(REG_WATER_CHANNEL_SELECT, &wc_value);
	if (ReCode != ERROR_CODE_OK)
		goto TEST_ERR;

	/* If it is V3 pattern, Get Tx/Rx Num again */
	bFlag = SwitchToNoMapping();
	if (bFlag) {
		pr_err("Failed to SwitchToNoMapping!\n");
		goto TEST_ERR;
	}

	/* 2.Get SCap Raw Data,
	 * Step:1.Start Scanning; 2. Read Raw Data */
	ReCode = StartScan();

	if (ReCode != ERROR_CODE_OK) {
		pr_err("Failed to Scan SCap RawData!\n");
		goto TEST_ERR;
	}

	pr_info
	    ("g_ScreenSetParam.iRxNum=%d , g_ScreenSetParam.iTxNum=%d\n",
	     g_ScreenSetParam.iRxNum, g_ScreenSetParam.iTxNum);

	for (i = 0; i < 3; i++) {
		memset(m_RawData, 0, ARRAY_SIZE(m_RawData));
		memset(m_ucTempData, 0, ARRAY_SIZE(m_ucTempData));

		/* waterproof CB */
		/* ScWorkMode: 1: waterproof, 0:non-waterproof */
		ReCode = WriteReg(REG_ScWorkMode, 1);
		ReCode = StartScan();

		ReCode = WriteReg(REG_ScCbAddrR, 0);
		ReCode =
		    GetTxSC_CB(g_ScreenSetParam.iTxNum +
			       g_ScreenSetParam.iRxNum + 128, m_ucTempData);

		for (index = 0; index < g_ScreenSetParam.iRxNum; ++index) {
			m_RawData[0 + g_ScreenSetParam.iTxNum][index] =
			    m_ucTempData[index];
		}

		for (index = 0; index < g_ScreenSetParam.iTxNum; ++index) {
			m_RawData[1 + g_ScreenSetParam.iTxNum][index] =
			    m_ucTempData[index + g_ScreenSetParam.iRxNum];
		}

		/* non-waterproof rawdata */
		/* ScWorkMode: 1: waterproof, 0:non-waterproof */
		ReCode = WriteReg(REG_ScWorkMode, 0);
		ReCode = StartScan();

		ReCode = WriteReg(REG_ScCbAddrR, 0);
		ReCode =
		    GetTxSC_CB(g_ScreenSetParam.iRxNum +
			       g_ScreenSetParam.iTxNum + 128, m_ucTempData);

		for (index = 0; index < g_ScreenSetParam.iRxNum; ++index) {
			m_RawData[2 + g_ScreenSetParam.iTxNum][index] =
			    m_ucTempData[index];
		}

		for (index = 0; index < g_ScreenSetParam.iTxNum; ++index) {
			m_RawData[3 + g_ScreenSetParam.iTxNum][index] =
			    m_ucTempData[index + g_ScreenSetParam.iRxNum];
		}

		if (ReCode != ERROR_CODE_OK)
			pr_err("Failed to Get SCap CB!\n");
	}

	if (ReCode != ERROR_CODE_OK)
		goto TEST_ERR;

	/* 3. Judge */
	/* Waterproof ON */
	bFlag = GetTestCondition(WT_NeedProofOnTest, wc_value);

	/* Add Info Area */
	iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE, "\n<<SCap Cb Test>>\n");
	memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
	g_lenInfoArea += iLen;

	if (g_stCfg_FT5822_BasicThreshold.SCapCbTest_SetWaterproof_ON
		&& bFlag) {
		pr_info("SCapCbTest in WaterProof On Mode:\n");

		/* Add Info Area */
		iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
			       "WaterProof ON\n");
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;

		iMax = -m_RawData[0 + g_ScreenSetParam.iTxNum][0];
		iMin = 2 * m_RawData[0 + g_ScreenSetParam.iTxNum][0];
		iAvg = 0;
		Value = 0;
		iCount = 0;

		bFlag = GetTestCondition(WT_NeedRxOnVal, wc_value);

		if (bFlag)
			pr_info("SCap CB_Rx:\n");

		for (i = 0; bFlag && i < g_ScreenSetParam.iRxNum; i++) {
			if (g_stCfg_MCap_DetailThreshold.InvalidNode_SC[0][i] ==
			    0)
				continue;

			CBMin =
			    g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Min[0]
			    [i];
			CBMax =
			    g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Max[0]
			    [i];

			Value = m_RawData[0 + g_ScreenSetParam.iTxNum][i];
			iAvg += Value;

			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > CBMax || Value < CBMin) {
				btmpresult = false;
				pr_err
				("Num=%d,Value=%d,range=(%d,%d)\n",
				i + 1, Value, CBMin, CBMax);
			#ifdef SHOW_INFO
				/* Add Info Area */
				iLen =
				    snprintf(g_pInfoBuff, INFOBUFF_SIZE,
					    "Rx%d=%d(%d,%d)\n",
					    i + 1, Value, CBMin, CBMax);
				memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff,
				       iLen);
				g_lenInfoArea += iLen;
			#endif
			}
			iCount++;
		}

		bFlag = GetTestCondition(WT_NeedTxOnVal, wc_value);

		if (bFlag)
			pr_info("SCap CB_Tx:\n");

		for (i = 0; bFlag && i < g_ScreenSetParam.iTxNum; i++) {
			if (g_stCfg_MCap_DetailThreshold.InvalidNode_SC[1][i] ==
			    0)
				continue;

			CBMin =
			    g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Min[1]
			    [i];
			CBMax =
			    g_stCfg_MCap_DetailThreshold.SCapCbTest_ON_Max[1]
			    [i];

			Value = m_RawData[1 + g_ScreenSetParam.iTxNum][i];
			iAvg += Value;

			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;

			if (Value > CBMax || Value < CBMin) {
				btmpresult = false;
				pr_err
				("Num=%d,Value=%d (%d,%d):\n",
					i + 1, Value, CBMin, CBMax);
			#ifdef SHOW_INFO
				/* Add Info Area */
				iLen =
				    snprintf(g_pInfoBuff, INFOBUFF_SIZE,
					    "Tx%d=%d(%d,%d)\n",
					    i + 1, Value, CBMin, CBMax);
				memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff,
				       iLen);
				g_lenInfoArea += iLen;
			#endif
			}
			iCount++;
		}

		if (0 == iCount) {
			iAvg = 0;
			iMax = 0;
			iMin = 0;
		} else {
			iAvg = iAvg / iCount;
		}
		pr_info
		    ("\n SCap CB in Waterproof-ON,");
		pr_info
		    (" Max : %d, Min: %d, Deviation: %d, Average: %d\n",
		     iMax, iMin, iMax - iMin, iAvg);

		/* Add Info Area */
		iLen = snprintf
		    (g_pInfoBuff, INFOBUFF_SIZE,
		     "\nMax:%d,Min:%d,Deviation:%d,Average:%d\n\n",
		     iMax, iMin, iMax - iMin, iAvg);
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;

		/* Save Test Data */
		ibiggerValue =
		    g_ScreenSetParam.iTxNum >
		    g_ScreenSetParam.
		    iRxNum ? g_ScreenSetParam.iTxNum : g_ScreenSetParam.iRxNum;

		Save_Test_Data(m_RawData, g_ScreenSetParam.iTxNum + 0, 2,
			       ibiggerValue, 1);
	}

	bFlag = GetTestCondition(WT_NeedProofOffTest, wc_value);

	if (g_stCfg_FT5822_BasicThreshold.
		SCapCbTest_SetWaterproof_OFF && bFlag) {

		/* Add Info Area */
		iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
			       "WaterProof OFF\n");
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;

		pr_info("SCapCbTest in WaterProof OFF Mode:\n");
		iMax = -m_RawData[2 + g_ScreenSetParam.iTxNum][0];
		iMin = 2 * m_RawData[2 + g_ScreenSetParam.iTxNum][0];
		iAvg = 0;
		Value = 0;
		iCount = 0;

		bFlag = GetTestCondition(WT_NeedRxOffVal, wc_value);
		if (bFlag)
			pr_info("SCap CB_Rx:\n");

		for (i = 0; bFlag && i < g_ScreenSetParam.iRxNum; i++) {
			if (g_stCfg_MCap_DetailThreshold.InvalidNode_SC[0][i] ==
			    0)
				continue;

			CBMin =
			    g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Min[0]
			    [i];
			CBMax =
			    g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Max[0]
			    [i];
			Value = m_RawData[2 + g_ScreenSetParam.iTxNum][i];
			iAvg += Value;

			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > CBMax || Value < CBMin) {
				btmpresult = false;
				pr_info
				("Num=%d,Value=%d,range=(%d,%d):\n",
				i + 1, Value, CBMin, CBMax);
			#ifdef SHOW_INFO
				/* Add Info Area */
				iLen =
				    snprintf(g_pInfoBuff, INFOBUFF_SIZE,
					    "Rx%d=%d(%d,%d)\n",
					    i + 1, Value, CBMin, CBMax);
				memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff,
				       iLen);
				g_lenInfoArea += iLen;
			#endif
			}
			iCount++;
		}

		bFlag = GetTestCondition(WT_NeedTxOffVal, wc_value);
		if (bFlag)
			pr_info("SCap CB_Tx:\n");

		for (i = 0; bFlag && i < g_ScreenSetParam.iTxNum; i++) {
			/* if( m_ScapInvalide[1][i] == 0 )      continue; */
			if (g_stCfg_MCap_DetailThreshold.InvalidNode_SC[1][i] ==
			    0)
				continue;

			CBMin =
			    g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Min[1]
			    [i];
			CBMax =
			    g_stCfg_MCap_DetailThreshold.SCapCbTest_OFF_Max[1]
			    [i];

			Value = m_RawData[3 + g_ScreenSetParam.iTxNum][i];

			iAvg += Value;
			if (iMax < Value)
				iMax = Value;
			if (iMin > Value)
				iMin = Value;
			if (Value > CBMax || Value < CBMin) {
				btmpresult = false;
				pr_info
				("Num=%d,Value=%d,range=(%d,%d)\n",
				i + 1, Value, CBMin, CBMax);
				#ifdef SHOW_INFO
				/* Add Info Area */
				iLen =
				    snprintf(g_pInfoBuff, INFOBUFF_SIZE,
					    "Tx%d=%d(%d,%d)\n",
					    i + 1, Value, CBMin, CBMax);
				memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff,
				       iLen);
				g_lenInfoArea += iLen;
				#endif
			}
			iCount++;
		}

		if (0 == iCount) {
			iAvg = 0;
			iMax = 0;
			iMin = 0;
		} else
			iAvg = iAvg / iCount;

		pr_info
		("\nSCap CB in Waterproof-OFF");
		pr_info
		(" Max:%d,Min:%d,Deviation:%d,Average:%d\n",
		iMax, iMin, iMax - iMin, iAvg);

		/* Add Info Area */
		iLen = snprintf
		    (g_pInfoBuff, INFOBUFF_SIZE,
		     "\nMax:%d,Min:%d,Deviation:%d,Average:%d\n\n",
		     iMax, iMin, iMax - iMin, iAvg);
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;

		/* Save Test Data */
		ibiggerValue =
		    g_ScreenSetParam.iTxNum >
		    g_ScreenSetParam.
		    iRxNum ? g_ScreenSetParam.iTxNum : g_ScreenSetParam.iRxNum;

		Save_Test_Data(m_RawData, g_ScreenSetParam.iTxNum + 2, 2,
			       ibiggerValue, 2);
	}

	/* 4. post-stage work */
	if (m_bV3TP) {
		ReCode = ReadReg(REG_MAPPING_SWITCH, &ucValue);
		if (0 != ucValue) {
			ReCode = WriteReg(REG_MAPPING_SWITCH, 0);
			SysDelay(10);
			if (ReCode != ERROR_CODE_OK) {
				pr_err("Failed to switch mapping type!\n ");
				btmpresult = false;
			}
		}

		/* SCap will use before mapping.
		  After the end of the test item,
		  MCap must transfer to after Mapping */
		GetChannelNum();
	}

	/* 5. Test Result */
	if (btmpresult) {
		*bTestResult = true;
		pr_info("\n\nSCap CB Test Test is OK!\n");
	} else {
		*bTestResult = false;
		pr_err("\n\nSCap CB Test Test is NG!\n");
	}

	/* Add Info Area */
	if (btmpresult) {
		iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
			"\nSCap CB Test PASS\n\n");
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;
	} else {
		iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
			"\nSCap CB Test FAIL\n\n");
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;
	}
	Save_Info_Data(g_pInfoArea, g_lenInfoArea);

	return ReCode;

TEST_ERR:
	*bTestResult = false;
	pr_err("\n\nSCap CB Test Test FAIL\n");

	/* Add Info Area */
	iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
		"\nSCap CB Test Test FAIL\n\n");
	memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
	g_lenInfoArea += iLen;

	Save_Info_Data(g_pInfoArea, g_lenInfoArea);

	return ReCode;
}

unsigned char FT5822_TestItem_PanelDifferTest(bool *bTestResult)
{
	int index = 0;
	int iRow = 0, iCol = 0;
	int iValue = 0;
	unsigned char ReCode = 0, strSwitch = -1;
	unsigned char OriginRawDataType = 0xff;
	unsigned char OriginFrequecy = 0xff;
	unsigned char OriginFirState = 0xff;
	bool btmpresult = true;
	int iMax, iMin;
	int maxValue = 0;
	int minValue = 32767;
	int AvgValue = 0;
	int InvalidNum = 0;
	int i = 0, j = 0;
	int iLen = 0;

	pr_info("\n\nTest Item: --------- Panel Differ Test\n\n");
	g_lenInfoArea = 0;

	ReCode = EnterFactory();
	SysDelay(20);
	if (ReCode != ERROR_CODE_OK) {
		pr_err("\n\n Failed to Enter factory Mode. Error Code: %d\n",
		       ReCode);
		goto TEST_ERR;
	}

	/* Add Info Area */
	iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE, "\n<<Differ Test>>\n");
	memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
	g_lenInfoArea += iLen;

	if (m_bV3TP) {
		ReCode = ReadReg(REG_MAPPING_SWITCH, &strSwitch);
		if (ReCode != ERROR_CODE_OK) {
			pr_err
			("\nRead REG_MAPPING_SWITCH error %d\n",
			ReCode);
			goto TEST_ERR;
		}

		if (strSwitch != 0) {
			ReCode = WriteReg(REG_MAPPING_SWITCH, 0);
			if (ReCode != ERROR_CODE_OK) {
				pr_err
				("\nWrite REG_MAPPING_SWITCH error %d\n",
				ReCode);
				goto TEST_ERR;
			}

			/* MUST get channel number again
			 * after write reg mapping switch. */
			ReCode = GetChannelNum();
			if (ReCode != ERROR_CODE_OK) {
				pr_err
				    ("\nGetChannelNum error. Error Code: %d\n",
				     ReCode);
				goto TEST_ERR;
			}

			ReCode = GetRawData();
		}
	}

	pr_info("\r\n===Set Auto Equalization:\r\n");
	ReCode = ReadReg(REG_NORMALIZE_TYPE, &OriginRawDataType);
	if (ReCode != ERROR_CODE_OK) {
		pr_err("Read REG_NORMALIZE_TYPE error.Error Code: %d\n",
		       ReCode);
		btmpresult = false;
		goto TEST_ERR;
	}

	if (OriginRawDataType != 0) {
		ReCode = WriteReg(REG_NORMALIZE_TYPE, 0x00);
		SysDelay(50);
		if (ReCode != ERROR_CODE_OK) {
			btmpresult = false;
			pr_err("Write reg REG_NORMALIZE_TYPE Failed.\n");
			goto TEST_ERR;
		}
	}

	pr_info("==Set Frequecy High\n");
	ReCode = ReadReg(REG_FREQUENCY, &OriginFrequecy);
	if (ReCode != ERROR_CODE_OK) {
		pr_err("Read reg 0x0A error. Error Code: %d\n", ReCode);
		btmpresult = false;
		goto TEST_ERR;
	}

	ReCode = WriteReg(REG_FREQUENCY, 0x81);
	SysDelay(10);
	if (ReCode != ERROR_CODE_OK) {
		btmpresult = false;
		pr_err("Write reg 0x0A Failed.\n");
		goto TEST_ERR;
	}

	pr_info("=========FIR State: OFF");
	ReCode = ReadReg(REG_FIR, &OriginFirState);
	if (ReCode != ERROR_CODE_OK) {
		pr_err("Read reg 0xFB error. Error Code: %d", ReCode);
		btmpresult = false;
		goto TEST_ERR;
	}

	ReCode = WriteReg(REG_FIR, 0);
	SysDelay(50);
	if (ReCode != ERROR_CODE_OK) {
		pr_err("Write reg 0xFB Failed.");
		btmpresult = false;
		goto TEST_ERR;
	}

	ReCode = GetRawData();

	for (index = 0; index < 4; ++index) {
		ReCode = GetRawData();
		if (ReCode != ERROR_CODE_OK) {
			pr_err("GetRawData Failed.");
			btmpresult = false;
			goto TEST_ERR;
		}
	}

	/* Differ value = RawData/10 */
	for (i = 0; i < g_ScreenSetParam.iTxNum; i++) {
		for (j = 0; j < g_ScreenSetParam.iRxNum; j++)
			m_DifferData[i][j] = m_RawData[i][j] / 10;
	}
#if 0
	/* To show value */
	pr_info("PannelDiffer:\n");
	for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; iRow++) {
		pr_info("Row%2d:", iRow + 1);
		for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; iCol++) {
			iValue = m_DifferData[iRow][iCol];
			pr_info("%4d,", iValue);
		}
		pr_info("\n");
	}
#endif

	/* To Determine  if in Range or not */
	for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; iRow++) {
		for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; iCol++) {
			if (g_stCfg_MCap_DetailThreshold.
			    InvalidNode[iRow][iCol] == 0)
				continue;	/* Invalid Node */

			iMin = g_stCfg_MCap_DetailThreshold.
						PanelDifferTest_Min[iRow][iCol];
			iMax = g_stCfg_MCap_DetailThreshold.
						PanelDifferTest_Max[iRow][iCol];

			iValue = m_DifferData[iRow][iCol];
			if (iValue < iMin || iValue > iMax) {
				btmpresult = false;
				pr_err("D[%d][%d]=%d(%d,%d)\n",
				       iRow + 1, iCol + 1, iValue, iMin, iMax);

#ifdef SHOW_INFO
				/* Add Info Area */
				iLen =
				    snprintf(g_pInfoBuff, INFOBUFF_SIZE,
					    "D[%d][%d]=%d(%d,%d)\n", iRow + 1,
					    iCol + 1, iValue, iMin, iMax);
				memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff,
				       iLen);
				g_lenInfoArea += iLen;
#endif
			}
		}
	}

	pr_info("PannelDiffer ABS:\n");
	for (i = 0; i < g_ScreenSetParam.iTxNum; i++) {
		for (j = 0; j < g_ScreenSetParam.iRxNum; j++) {
			pr_debug("%ld,", abs(m_DifferData[i][j]));
			m_absDifferData[i][j] = abs(m_DifferData[i][j]);

			if (3 ==
			    g_stCfg_MCap_DetailThreshold.InvalidNode[i][j] ||
			    0 ==
			    g_stCfg_MCap_DetailThreshold.InvalidNode[i][j]) {
				InvalidNum++;
				continue;
			}
			maxValue = max(maxValue, m_DifferData[i][j]);
			minValue = min(minValue, m_DifferData[i][j]);
			AvgValue += m_DifferData[i][j];
		}
	}
	Save_Test_Data(m_absDifferData, 0, g_ScreenSetParam.iTxNum,
		       g_ScreenSetParam.iRxNum, 1);

	AvgValue =
	    AvgValue / (g_ScreenSetParam.iTxNum * g_ScreenSetParam.iRxNum -
			InvalidNum);
	pr_info("\nPanelDiffer:Max: %d, Min: %d, Avg: %d\n", maxValue, minValue,
		AvgValue);

	/* Add Info Area */
	iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
		       "PanelDiffer:Max: %d, Min: %d, Avg: %d\n", maxValue,
		       minValue, AvgValue);
	memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
	g_lenInfoArea += iLen;

	ReCode = WriteReg(REG_NORMALIZE_TYPE, OriginRawDataType);
	ReCode = WriteReg(REG_FREQUENCY, OriginFrequecy);
	ReCode = WriteReg(REG_FIR, OriginFirState);

	/* Restore mapping value of V3 pattern */
	if (m_bV3TP) {
		ReCode = WriteReg(REG_MAPPING_SWITCH, strSwitch);
		if (ReCode != ERROR_CODE_OK) {
			pr_info("Failed to restore mapping type!\n");
			btmpresult = false;
		}
	}
	/* Result */
	if (btmpresult) {
		*bTestResult = true;
		pr_info("\n\nPanel Differ Test PASS!\n");
	} else {
		*bTestResult = false;
		pr_info("\n\nPanel Differ Test FAIL!\n");
	}

	/* Add Info Area */
	if (btmpresult) {
		iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
			"\n\nDiffer Test PASS\n\n");
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;
	} else {
		iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
			"\n\nDiffer Test FAIL\n\n");
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;
	}
	Save_Info_Data(g_pInfoArea, g_lenInfoArea);

	return ReCode;

TEST_ERR:
	*bTestResult = false;
	pr_info("\n\nDiffer Test FAIL!\n");

	/* Add Info Area */
	iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE, "\n\nDiffer Test FAIL\n\n");
	memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
	g_lenInfoArea += iLen;
	Save_Info_Data(g_pInfoArea, g_lenInfoArea);

	return ReCode;
}

/************************************************************************
* Name: GetPanelRows(Same function name as FT_MultipleTest)
* Brief:  Get row of TP
* Input: none
* Output: pPanelRows
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char GetPanelRows(unsigned char *pPanelRows)
{
	return ReadReg(REG_TX_NUM, pPanelRows);
}

/************************************************************************
* Name: GetPanelCols(Same function name as FT_MultipleTest)
* Brief:  get column of TP
* Input: none
* Output: pPanelCols
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char GetPanelCols(unsigned char *pPanelCols)
{
	return ReadReg(REG_RX_NUM, pPanelCols);
}

/************************************************************************
* Name: StartScan(Same function name as FT_MultipleTest)
* Brief:  Scan TP, do it before read Raw Data
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static int StartScan(void)
{
	unsigned char RegVal = 0;
	unsigned char times = 0;
	const unsigned char MaxTimes = 60;
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;

	ReCode = ReadReg(DEVIDE_MODE_ADDR, &RegVal);

	if (ReCode == ERROR_CODE_OK) {
		RegVal |= 0x80;	/* set MSB to 1 to enable scanning */
		ReCode = WriteReg(DEVIDE_MODE_ADDR, RegVal);
		if (ReCode == ERROR_CODE_OK) {
			/* wait the scan complete */
			while (times++ < MaxTimes) {
				SysDelay(8);	/* 8ms */
				ReCode = ReadReg(DEVIDE_MODE_ADDR, &RegVal);
				if (ReCode == ERROR_CODE_OK) {
					if ((RegVal >> 7) == 0)
						break;
				} else
					break;
			}

			if (times < MaxTimes)
				ReCode = ERROR_CODE_OK;
			else
				ReCode = ERROR_CODE_COMM_ERROR;
		}
	}

	return ReCode;
}

/************************************************************************
* Name: ReadRawData(Same function name as FT_MultipleTest)
* Brief:  read Raw Data
* Input: Freq(No longer used, reserved), LineNum, ByteNum
* Output: pRevBuffer
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char ReadRawData(unsigned char Freq, unsigned char LineNum,
			  int ByteNum, int *pRevBuffer)
{
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;
	unsigned char I2C_wBuffer[3];
	int i, iReadNum;
	unsigned short BytesNumInTestMode1 = 0;

	iReadNum = ByteNum / 342;
	if (0 != (ByteNum % 342))
		iReadNum++;

	if (ByteNum <= 342)
		BytesNumInTestMode1 = ByteNum;
	else
		BytesNumInTestMode1 = 342;

	/* Set row addr */
	ReCode = WriteReg(REG_LINE_NUM, LineNum);

	/* Read raw data */
	/* set begin address */
	I2C_wBuffer[0] = REG_RawBuf0;
	if (ReCode == ERROR_CODE_OK) {
		focal_msleep(10);
		ReCode =
		    Comm_Base_IIC_IO(I2C_wBuffer, 1, m_ucTempData,
				     BytesNumInTestMode1);
	}

	for (i = 1; i < iReadNum; i++) {
		if (ReCode != ERROR_CODE_OK)
			break;

		if (i == iReadNum - 1) {	/* last packet */
			focal_msleep(10);
			ReCode =
			    Comm_Base_IIC_IO(NULL, 0, m_ucTempData + 342 * i,
					     ByteNum - 342 * i);
		} else {
			focal_msleep(10);
			ReCode =
			    Comm_Base_IIC_IO(NULL, 0, m_ucTempData + 342 * i,
					     342);
		}
	}

	if (ReCode == ERROR_CODE_OK) {
		for (i = 0; i < (ByteNum >> 1); i++) {
			pRevBuffer[i] =
			    (m_ucTempData[i << 1] << 8) +
			    m_ucTempData[(i << 1) + 1];

			/* signed bit */
			/* if(pRevBuffer[i] & 0x8000)
			   {
			   pRevBuffer[i] -= 0xffff + 1;
			   } */
		}
	}

	return ReCode;
}

/************************************************************************
* Name: GetTxSC_CB(Same function name as FT_MultipleTest)
* Brief:  get CB of Tx SCap
* Input: index
* Output: pcbValue
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char GetTxSC_CB(unsigned char index, unsigned char *pcbValue)
{
	unsigned char ReCode = ERROR_CODE_OK;
	unsigned char wBuffer[4];

	if (index < 128) {	/* single reading */
		*pcbValue = 0;
		WriteReg(REG_ScCbAddrR, index);
		ReCode = ReadReg(REG_ScCbBuf0, pcbValue);
	} else {	/* sequential reading, length is index-128 */
		WriteReg(REG_ScCbAddrR, 0);
		wBuffer[0] = REG_ScCbBuf0;
		ReCode = Comm_Base_IIC_IO(wBuffer, 1, pcbValue, index - 128);
	}

	return ReCode;
}

/************************************************************************
* Name: AllocateMemory
* Brief:  Allocate pointer Memory
* Input: none
* Output: none
* Return: none
***********************************************************************/
static void AllocateMemory(void)
{
	/* New buff */
	g_pStoreMsgArea = NULL;
	if (NULL == g_pStoreMsgArea)
		g_pStoreMsgArea = kmalloc(STOREMSGAREA_SIZE, GFP_ATOMIC);

	g_pMsgAreaLine2 = NULL;
	if (NULL == g_pMsgAreaLine2)
		g_pMsgAreaLine2 = kmalloc(1024 * 8, GFP_ATOMIC);

	g_pStoreDataArea = NULL;
	if (NULL == g_pStoreDataArea)
		g_pStoreDataArea = kmalloc(1024 * 30, GFP_ATOMIC);

	g_pTmpBuff = NULL;
	if (NULL == g_pTmpBuff)
		g_pTmpBuff = kmalloc(TMPBUFF_SIZE, GFP_ATOMIC);

	g_pInfoBuff = NULL;
	if (NULL == g_pInfoBuff)
		g_pInfoBuff = kmalloc(INFOBUFF_SIZE, GFP_ATOMIC);

	g_pStoreInfoArea = NULL;
	if (NULL == g_pStoreInfoArea)
		g_pStoreInfoArea = kmalloc(1024 * 100, GFP_ATOMIC);

	g_pInfoArea = NULL;
	if (NULL == g_pInfoArea)
		g_pInfoArea = kmalloc(1024 * 80, GFP_ATOMIC);
}

/************************************************************************
* Name: FreeMemory
* Brief:  Release pointer memory
* Input: none
* Output: none
* Return: none
***********************************************************************/
static void FreeMemory(void)
{
	if (NULL != g_pStoreMsgArea)
		kfree(g_pStoreMsgArea);

	if (NULL != g_pMsgAreaLine2)
		kfree(g_pMsgAreaLine2);

	if (NULL != g_pStoreDataArea)
		kfree(g_pStoreDataArea);

	if (NULL != g_pInfoBuff)
		kfree(g_pInfoBuff);

	if (NULL != g_pStoreInfoArea)
		kfree(g_pStoreInfoArea);

	if (NULL != g_pInfoArea)
		kfree(g_pInfoArea);

	if (NULL != g_pTmpBuff)
		kfree(g_pTmpBuff);
}

/************************************************************************
* Name: InitStoreParamOfTestData
* Brief:  Init store param of test data
* Input: none
* Output: none
* Return: none
***********************************************************************/
static void InitStoreParamOfTestData(void)
{
	g_lenStoreMsgArea = 0;

	/* Msg Area, Add Line1 */
	g_lenStoreMsgArea +=
	    snprintf(g_pStoreMsgArea, STOREMSGAREA_SIZE,
			"ECC, 85, 170, IC Name, %s, IC Code, %x\n",
		    g_strIcName, g_ScreenSetParam.iSelectedIC);

	/* Line2 */
	g_lenMsgAreaLine2 = 0;

	/* Data Area */
	g_lenStoreDataArea = 0;

	/* Info Area */
	g_lenStoreInfoArea = 0;

	/* The Start Line of Data Area is 11 */
	m_iStartLine = 11;
	m_iTestDataCount = 0;
}

/************************************************************************
* Name: MergeAllTestData
* Brief:  Merge All Data of test result
* Input: none
* Output: none
* Return: none
***********************************************************************/
static void MergeAllTestData(void)
{
	int iLen = 0;

	/* Add the head part of Line2 */
	iLen = snprintf(g_pTmpBuff, TMPBUFF_SIZE,
		"TestItem, %d, ", m_iTestDataCount);
	memcpy(g_pStoreMsgArea + g_lenStoreMsgArea, g_pTmpBuff, iLen);
	g_lenStoreMsgArea += iLen;

	/* Add other part of Line2, except for "\n" */
	memcpy(g_pStoreMsgArea + g_lenStoreMsgArea, g_pMsgAreaLine2,
	       g_lenMsgAreaLine2);
	g_lenStoreMsgArea += g_lenMsgAreaLine2;

	/* Add Line3 ~ Line10 */
	iLen = snprintf(g_pTmpBuff, TMPBUFF_SIZE, "\n\n\n\n\n\n\n\n\n");
	memcpy(g_pStoreMsgArea + g_lenStoreMsgArea, g_pTmpBuff, iLen);
	g_lenStoreMsgArea += iLen;

	/* 1.Add Msg Area */
	memcpy(g_pStoreAllData, g_pStoreMsgArea, g_lenStoreMsgArea);

	/* 2.Add Data Area */
	if (0 != g_lenStoreDataArea) {
		memcpy(g_pStoreAllData + g_lenStoreMsgArea, g_pStoreDataArea,
		       g_lenStoreDataArea);
	}

	pr_info(
	"[focal] lenStoreMsgArea=%d,lenStoreDataArea=%d\n",
			g_lenStoreMsgArea, g_lenStoreDataArea);
}

static void Save_Info_Data(unsigned char *g_pInfo, int g_lenInfo)
{
	memcpy(g_pStoreInfoArea + g_lenStoreInfoArea, g_pInfo, g_lenInfo);
	g_lenStoreInfoArea += g_lenInfo;
}

/************************************************************************
* Name: Save_Test_Data
* Brief:  Storage format of test data
* Input: int iData[TX_NUM_MAX][RX_NUM_MAX],
*  int iArrayIndex, unsigned char Row,
* unsigned char Col, unsigned char ItemCount
* Output: none
* Return: none
***********************************************************************/
static void Save_Test_Data(int iData[TX_NUM_MAX][RX_NUM_MAX], int iArrayIndex,
			   unsigned char Row, unsigned char Col,
			   unsigned char ItemCount)
{
	int iLen = 0;
	int i = 0, j = 0;

	/* SaveMsg (ItemCode is enough,
	 * ItemName is not necessary, so set it to "NA".) */
	iLen =
	    snprintf(g_pTmpBuff, TMPBUFF_SIZE, "NA, %d, %d, %d, %d, %d, ",
		    m_ucTestItemCode, Row, Col, m_iStartLine, ItemCount);
	memcpy(g_pMsgAreaLine2 + g_lenMsgAreaLine2, g_pTmpBuff, iLen);
	g_lenMsgAreaLine2 += iLen;
	m_iStartLine += Row;
	m_iTestDataCount++;

	/* Save Data */
	for (i = 0 + iArrayIndex; i < Row + iArrayIndex; i++) {
		for (j = 0; j < Col; j++) {
			/* The Last Data of the Row, add "\n" */
			if (j == (Col - 1))
				iLen =
				    snprintf(g_pTmpBuff, TMPBUFF_SIZE,
						"%d,\n", iData[i][j]);
			else
				iLen = snprintf(g_pTmpBuff, TMPBUFF_SIZE,
					"%d, ", iData[i][j]);

			memcpy(g_pStoreDataArea + g_lenStoreDataArea,
			       g_pTmpBuff, iLen);
			g_lenStoreDataArea += iLen;
		}
	}
}

/************************************************************************
* Name: GetChannelNum
* Brief:  Get Channel Num(Tx and Rx)
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char GetChannelNum(void)
{
	unsigned char ReCode;
	unsigned char rBuffer[1];	/* new unsigned char; */

	/* m_strCurrentTestMsg = "Get Tx Num..."; */
	ReCode = GetPanelRows(rBuffer);

	if (ReCode == ERROR_CODE_OK) {
		g_ScreenSetParam.iTxNum = rBuffer[0];
		if (g_ScreenSetParam.iTxNum + 4 >
		    g_ScreenSetParam.iUsedMaxTxNum) {
			pr_err
			("Failed to get Tx number,Get num=%d,UsedMaxNum=%d\n",
			g_ScreenSetParam.iTxNum,
			   g_ScreenSetParam.iUsedMaxTxNum);
			return ERROR_CODE_INVALID_PARAM;
		}
		/* g_ScreenSetParam.iTxNum = 26; */
	} else {
		pr_err("Failed to get Tx number\n");
	}
	/* m_strCurrentTestMsg = "Get Rx Num..."; */

	ReCode = GetPanelCols(rBuffer);
	if (ReCode == ERROR_CODE_OK) {
		g_ScreenSetParam.iRxNum = rBuffer[0];

		if (g_ScreenSetParam.iRxNum > g_ScreenSetParam.iUsedMaxRxNum) {
			pr_err
		    ("Failed to get Rx number,Get num=%d,UsedMaxNum=%d\n",
		     g_ScreenSetParam.iRxNum,
		     g_ScreenSetParam.iUsedMaxRxNum);

			return ERROR_CODE_INVALID_PARAM;
		}
		/* g_ScreenSetParam.iRxNum = 28; */
	} else {
		pr_err("Failed to get Rx number\n");
	}

	return ReCode;
}

/************************************************************************
* Name: GetRawData
* Brief:  Get Raw Data of MCAP
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char GetRawData(void)
{
	unsigned char ReCode = ERROR_CODE_OK;
	int iRow = 0;
	int iCol = 0;
	/* Enter Factory Mode */
	ReCode = EnterFactory();
	if (ERROR_CODE_OK != ReCode) {
		pr_err("Failed to Enter Factory Mode...\n");
		return ReCode;
	}
	/* Check Num of Channel */
	if (0 == (g_ScreenSetParam.iTxNum + g_ScreenSetParam.iRxNum)) {
		ReCode = GetChannelNum();
		if (ERROR_CODE_OK != ReCode) {
			pr_err("Error Channel Num...\n");
			return ERROR_CODE_INVALID_PARAM;
		}
	}

	pr_info("Start Scan ...\n");
	ReCode = StartScan();
	if (ERROR_CODE_OK != ReCode) {
		pr_err("Failed to Scan ...\n");
		return ReCode;
	}

	/* Read RawData, Only MCAP */
	memset(m_RawData, 0, ARRAY_SIZE(m_RawData));

	ReCode =
	    ReadRawData(1, 0xAA,
			(g_ScreenSetParam.iTxNum * g_ScreenSetParam.iRxNum) * 2,
			m_iTempRawData);
	for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; iRow++) {
		for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; iCol++) {
			m_RawData[iRow][iCol] =
			    m_iTempRawData[iRow * g_ScreenSetParam.iRxNum +
					   iCol];
		}
	}
	return ReCode;
}

/************************************************************************
* Name: ShowRawData
* Brief:  Show RawData
* Input: none
* Output: none
* Return: none.
***********************************************************************/
#if 0
void ShowRawData(void)
{
	int iRow, iCol;

	/* Show RawData */
	for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; iRow++) {
		pr_info("\nTx%2d:  ", iRow + 1);
		for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; iCol++)
			pr_info("%5d    ", m_RawData[iRow][iCol]);
	}
}
#endif
/************************************************************************
* Name: GetChannelNumNoMapping
* Brief:  get Tx&Rx num from other Register
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char GetChannelNumNoMapping(void)
{
	unsigned char ReCode;
	unsigned char rBuffer[1];	/* new unsigned char; */

	pr_info("Get Tx Num...\n");
	ReCode = ReadReg(REG_TX_NOMAPPING_NUM, rBuffer);

	if (ReCode == ERROR_CODE_OK)
		g_ScreenSetParam.iTxNum = rBuffer[0];
	else
		pr_err("Failed to get Tx number\n");

	pr_info("Get Rx Num...\n");

	ReCode = ReadReg(REG_RX_NOMAPPING_NUM, rBuffer);

	if (ReCode == ERROR_CODE_OK)
		g_ScreenSetParam.iRxNum = rBuffer[0];
	else
		pr_err("Failed to get Rx number\n");

	return ReCode;
}

/************************************************************************
* Name: SwitchToNoMapping
* Brief:  If it is V3 pattern, Get Tx/Rx Num again
* Input: none
* Output: none
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
static unsigned char SwitchToNoMapping(void)
{
	unsigned char chPattern = -1;
	unsigned char ReCode = ERROR_CODE_OK;
	unsigned char RegData = -1;

	ReCode = ReadReg(REG_PATTERN_5422, &chPattern);

	/* 1: V3 Pattern */
	if (1 == chPattern) {
		RegData = -1;
		ReCode = ReadReg(REG_MAPPING_SWITCH, &RegData);

		if (1 != RegData) {
			/* 0-mapping 1-no mampping */
			ReCode = WriteReg(REG_MAPPING_SWITCH, 1);
			focal_msleep(20);
			GetChannelNumNoMapping();
		}
	}

	if (ReCode != ERROR_CODE_OK)
		pr_err("Switch To NoMapping Failed!\n");

	return ReCode;
}

/************************************************************************
* Name: GetTestCondition
* Brief:  Check whether Rx or TX need to test, in Waterproof ON/OFF Mode.
* Input: none
* Output: none
* Return: true: need to test; false: Not tested.
***********************************************************************/
static boolean GetTestCondition(int iTestType, unsigned char ucChannelValue)
{
	boolean bIsNeeded = false;

	switch (iTestType) {
	case WT_NeedProofOnTest:
		/*Bit5: 0: enable waterproof test;1: disable waterproof test */
		bIsNeeded = !(ucChannelValue & 0x20);
		break;

	case WT_NeedProofOffTest:
		/*Bit7: 0: enable detect in normal mode;
		 * 1: disable detect in normal mode */
		bIsNeeded = !(ucChannelValue & 0x80);
		break;

	case WT_NeedTxOnVal:
		/*Bit6: 0 : detect waterproof Rx+Tx, 1:detect one channel
		   Bit2: 0: detect waterproof Tx, 1: detect waterproof Rx */
		bIsNeeded = !(ucChannelValue & 0x40)
		    || !(ucChannelValue & 0x04);
		break;

	case WT_NeedRxOnVal:
		/*Bit6:  0 : detect waterproof Rx+Tx, 1:detect one channel
		   Bit2:  0: detect waterproof Tx, 1: detect waterproof Rx */
		bIsNeeded = !(ucChannelValue & 0x40)
		    || (ucChannelValue & 0x04);
		break;

	case WT_NeedTxOffVal:
		/*Bit1,Bit0:  00: normal mode Tx; 10: normal mode Rx+Tx */
		bIsNeeded = (0x00 == (ucChannelValue & 0x03))
		    || (0x02 == (ucChannelValue & 0x03));
		break;

	case WT_NeedRxOffVal:
		/*Bit1,Bit0:  01: normal mode Rx; 10: normal mode Rx+Tx */
		bIsNeeded = (0x01 == (ucChannelValue & 0x03))
		    || (0x02 == (ucChannelValue & 0x03));
		break;

	default:
		break;
	}

	return bIsNeeded;
}

/* value of high frequecy ¡¢FIR: 0 */
unsigned char FT5822_TestItem_UniformityTest(bool *bTestResult)
{
	unsigned char ReCode = ERROR_CODE_OK;
	bool btmpresult = true;
	unsigned char ucFre = 0;
	unsigned char FirValue = 0;
	int iMin = 100000;
	int iMax = -100000;
	int iDeviation = 0;
	int iRow = 0;
	int iCol = 0;
	int iUniform = 0;
	int index = 0;
	int iLen = 0;

	pr_info("\n\nTest Item: --------RawData Uniformity Test\n\n");
	g_lenInfoArea = 0;

	/*1.Preparatory work */
	/*in Factory Mode */
	ReCode = EnterFactory();
	if (ReCode != ERROR_CODE_OK) {
		pr_err("\n\nFailed to Enter factory Mode. Error Code:%d",
		       ReCode);
		goto TEST_END;
	}

	ReCode = ReadReg(REG_FREQUENCY, &ucFre);
	if (ReCode != ERROR_CODE_OK)
		goto TEST_END;

	/* High Frequecy */
	ReCode = WriteReg(REG_FREQUENCY, 0x81);
	SysDelay(100);
	if (ReCode != ERROR_CODE_OK)
		goto TEST_END;

	ReCode = ReadReg(REG_FIR, &FirValue);
	if (ReCode != ERROR_CODE_OK)
		goto TEST_END;

	/* FIR = 0 */
	ReCode = WriteReg(REG_FIR, 0);
	SysDelay(100);

	if (ReCode != ERROR_CODE_OK)
		goto TEST_END;

	/*throw away three frames after reg value changed */
	for (index = 0; index < 3; ++index)
		ReCode = GetRawData();

	/* Add Info Area */
	iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
		       "\n<<RawData Uniformity Test>>\n");
	memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
	g_lenInfoArea += iLen;

	if (g_stCfg_FT5822_BasicThreshold.Uniformity_CheckTx) {
		pr_info("\n\nCheck Tx Linearity\n\n");
		memset(TxLinearity, 0, ARRAY_SIZE(TxLinearity));
		for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; ++iRow) {
			for (iCol = 1; iCol < g_ScreenSetParam.iRxNum; ++iCol) {
				iDeviation =
				    focal_abs(m_RawData[iRow][iCol] -
					      m_RawData[iRow][iCol - 1]);
				iMax =
				    m_RawData[iRow][iCol] >
				    m_RawData[iRow][iCol -
						    1] ? m_RawData[iRow][iCol] :
				    m_RawData[iRow][iCol - 1];
				iMax = iMax ? iMax : 1;
				TxLinearity[iRow][iCol] =
				    100 * iDeviation / iMax;
			}
		}

		for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; ++iRow) {
			for (iCol = 1; iCol < g_ScreenSetParam.iRxNum; ++iCol) {
				if (0 ==
				    g_stCfg_MCap_DetailThreshold.InvalidNode
				    [iRow][iCol])
					continue;
				if (2 ==
				    g_stCfg_MCap_DetailThreshold.InvalidNode
				    [iRow][iCol])
					continue;

				if (TxLinearity[iRow][iCol] < MIN_HOLE_LEVEL
				    || TxLinearity[iRow][iCol] >
				    g_stCfg_FT5822_BasicThreshold.
				    Uniformity_Tx_Hole) {
					pr_err
	("Out Of Range,TxLinearity[%d,%d]=%d,Tx_Hole=%d.\n",
						iCol, iRow,
						TxLinearity[iRow][iCol],
						g_stCfg_FT5822_BasicThreshold.
						Uniformity_Tx_Hole);
					btmpresult = false;
				#ifdef SHOW_INFO
					/* Add Info Area */
					iLen = snprintf(g_pInfoBuff,
						INFOBUFF_SIZE,
						"T[%d][%d]=%d(%d,%d)\n",
						iCol, iRow,
						TxLinearity[iRow][iCol],
						MIN_HOLE_LEVEL,
						g_stCfg_FT5822_BasicThreshold.
						Uniformity_Tx_Hole);
					memcpy(g_pInfoArea + g_lenInfoArea,
					       g_pInfoBuff, iLen);
					g_lenInfoArea += iLen;
				#endif
				}
			}
		}

		/* Add Info Area */
		if (btmpresult) {
			iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
				       "\nTx Linearity is OK\n");
			memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
			g_lenInfoArea += iLen;
		} else {
			iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
				       "\nTx Linearity is NG!\n");
			memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
			g_lenInfoArea += iLen;
		}

		Save_Test_Data(TxLinearity, 0, g_ScreenSetParam.iTxNum,
			       g_ScreenSetParam.iRxNum, 1);
	}

	if (g_stCfg_FT5822_BasicThreshold.Uniformity_CheckRx) {
		pr_info("\n\nCheck Rx Linearity\n\n");

		for (iRow = 1; iRow < g_ScreenSetParam.iTxNum; ++iRow) {
			for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; ++iCol) {
				iDeviation =
				    focal_abs(m_RawData[iRow][iCol] -
					      m_RawData[iRow - 1][iCol]);
				iMax =
				    m_RawData[iRow][iCol] >
				    m_RawData[iRow -
					      1][iCol] ? m_RawData[iRow][iCol] :
				    m_RawData[iRow - 1][iCol];

				iMax = iMax ? iMax : 1;
				RxLinearity[iRow][iCol] =
				    100 * iDeviation / iMax;
			}
		}

		for (iRow = 1; iRow < g_ScreenSetParam.iTxNum; ++iRow) {
			for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; ++iCol) {
				if (0 ==
				    g_stCfg_MCap_DetailThreshold.InvalidNode
				    [iRow][iCol])
					continue;
				if (2 ==
				    g_stCfg_MCap_DetailThreshold.InvalidNode
				    [iRow][iCol])
					continue;

				if (RxLinearity[iRow][iCol] < MIN_HOLE_LEVEL
				    || RxLinearity[iRow][iCol] >
				    g_stCfg_FT5822_BasicThreshold.
				    Uniformity_Rx_Hole) {
					pr_err
	("Out Of Range,RxLinearity[%d][%d]=%d,Rx_Hole=%d\n",
					iCol, iRow,
					RxLinearity[iRow][iCol],
					g_stCfg_FT5822_BasicThreshold.
					Uniformity_Rx_Hole);
					btmpresult = false;
				#ifdef SHOW_INFO
					/* Add Info Area */
					iLen = snprintf(g_pInfoBuff,
						INFOBUFF_SIZE,
						"R[%d][%d]=%d(%d,%d)\n",
						iCol, iRow,
						RxLinearity[iRow][iCol],
						MIN_HOLE_LEVEL,
						g_stCfg_FT5822_BasicThreshold.
						Uniformity_Rx_Hole);
					memcpy(g_pInfoArea + g_lenInfoArea,
					       g_pInfoBuff, iLen);
					g_lenInfoArea += iLen;
				#endif
				}
			}
		}

		/* Add Info Area */
		if (btmpresult) {
			iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
				       "\nRx Linearity is OK!\n\n");
			memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
			g_lenInfoArea += iLen;
		} else {
			iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
				       "\nRx Linearity is NG!\n\n");
			memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
			g_lenInfoArea += iLen;
		}

		Save_Test_Data(RxLinearity, 0, g_ScreenSetParam.iTxNum,
			       g_ScreenSetParam.iRxNum, 2);
	}

	if (g_stCfg_FT5822_BasicThreshold.Uniformity_CheckMinMax) {
		pr_info("\n\nCheck Min/Max\n\n");
		iMin = 100000;
		iMax = -100000;

		for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; ++iRow) {
			for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; ++iCol) {
				if (0 ==
				    g_stCfg_MCap_DetailThreshold.InvalidNode
				    [iRow][iCol]) {
					continue;
				}

				if (2 ==
				    g_stCfg_MCap_DetailThreshold.InvalidNode
				    [iRow][iCol]) {
					continue;
				}

				if (iMin > m_RawData[iRow][iCol])
					iMin = m_RawData[iRow][iCol];
				if (iMax < m_RawData[iRow][iCol])
					iMax = m_RawData[iRow][iCol];
			}
		}

		iMax = !iMax ? 1 : iMax;
		iUniform = 100 * focal_abs(iMin) / focal_abs(iMax);
		pr_info("\n\n Min: %d, Max: %d, , Get Value of Min/Max: %d.\n",
		       iMin, iMax, iUniform);

		if (iUniform <
		    g_stCfg_FT5822_BasicThreshold.Uniformity_MinMax_Hole) {
			btmpresult = false;
			pr_err
			("\n\n MinMax Out Of Range, Set Value: %d.\n",
			g_stCfg_FT5822_BasicThreshold.Uniformity_MinMax_Hole
			);
		}

		/* Add Info Area */
		iLen = snprintf(g_pInfoBuff, INFOBUFF_SIZE,
			"Value of Min/Max=%d/%d=%d  (PASS >%d)\n",
			iMin, iMax, iUniform,
			g_stCfg_FT5822_BasicThreshold.Uniformity_MinMax_Hole
			);
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;
	}
	/* Restore the original frequency */
	ReCode = WriteReg(REG_FREQUENCY, ucFre);
	SysDelay(100);
	if (ReCode != ERROR_CODE_OK)
		goto TEST_END;

	/* restore FIR frequency*/
	ReCode = WriteReg(REG_FIR, FirValue);
	SysDelay(100);
	if (ReCode != ERROR_CODE_OK)
		goto TEST_END;

TEST_END:
	if (btmpresult && ReCode == ERROR_CODE_OK) {
		*bTestResult = true;
		pr_info("\n\n\n\nUniformity Test PASS\n\n");
	} else {
		*bTestResult = false;
		pr_err("\n\n\n\nUniformity Test FAIL\n\n");
	}

	/* Add Info Area */
	if (btmpresult) {
		iLen =
			snprintf(g_pInfoBuff, INFOBUFF_SIZE,
				"\nUniformity Test PASS\n\n");
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;
	} else {
		iLen =
			snprintf(g_pInfoBuff, INFOBUFF_SIZE,
				"\nUniformity Test FAIL\n\n");
		memcpy(g_pInfoArea + g_lenInfoArea, g_pInfoBuff, iLen);
		g_lenInfoArea += iLen;
	}
	Save_Info_Data(g_pInfoArea, g_lenInfoArea);

	return ReCode;
}
