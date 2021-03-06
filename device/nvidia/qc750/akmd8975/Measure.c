/******************************************************************************
 *
 * $Id: Measure.c 61 2011-03-31 06:58:14Z yamada.rj $
 *
 * -- Copyright Notice --
 *
 * Copyright (c) 2009 Asahi Kasei Microdevices Corporation, Japan
 * All Rights Reserved.
 *
 * This software program is proprietary program of Asahi Kasei Microdevices
 * Corporation("AKM") licensed to authorized Licensee under Software License
 * Agreement (SLA) executed between the Licensee and AKM.
 *
 * Use of the software by unauthorized third party, or use of the software
 * beyond the scope of the SLA is strictly prohibited.
 *
 * -- End Asahi Kasei Microdevices Copyright Notice --
 *
 ******************************************************************************/
#include "AKCommon.h"
#include "AK8975Driver.h"
#include "Measure.h"
#include "TestLimit.h"
#include "FileIO.h"
#include "DispMessage.h"
#include "misc.h"


#define MAG_ACQ_FLAG_POS	0
#define ACC_ACQ_FLAG_POS	1
#define ORI_ACQ_FLAG_POS	2
#define MAG_MES_FLAG_POS	4
#define MAG_INT_FLAG_POS	5
#define ACC_MES_FLAG_POS	6
#define ACC_INT_FLAG_POS	7
#define SETTING_FLAG_POS	8


static FORM_CLASS* g_form = NULL;

/*!
 This function open formation status device.
 @return Return 0 on success. Negative value on fail.
 */
static int16 openForm(void){
	if (g_form != NULL) {
		if (g_form->open != NULL) {
			return g_form->open();
		}
	}
	// If function is not set, return success.
	return 0;
}

/*!
 This function close formation status device.
 @return None.
 */
static void closeForm(void){
	if (g_form != NULL) {
		if (g_form->close != NULL) {
			g_form->close();
		}
	}
}

/*!
 This function check formation status
 @return The index of formation.
 */
static int16 checkForm(void){
	if (g_form != NULL) {
		if (g_form->check != NULL) {
			return g_form->check();
		}
	}
	// If function is not set, return default value.
	return 0;
}

/*!
 This function registers the callback function.
 @param[in] 
 */
void RegisterFormClass(FORM_CLASS* pt) {
	g_form = pt;
}

/*!
 Initialize #AK8975PRMS structure. At first, 0 is set to all parameters. 
 After that, some parameters, which should not be 0, are set to specific
 value. Some of initial values can be customized by editing the file
 \c "CustomerSpec.h".
 @param[out] prms A pointer to #AK8975PRMS structure.
 */
void InitAK8975PRMS(AK8975PRMS* prms)
{
	// Set 0 to the AK8975PRMS structure.
	memset(prms, 0, sizeof(AK8975PRMS));
	
	// Sensitivity
	prms->m_hs.u.x = AKSC_HSENSE_TARGET;
	prms->m_hs.u.y = AKSC_HSENSE_TARGET;
	prms->m_hs.u.z = AKSC_HSENSE_TARGET;
	
	// HDOE
	prms->m_hdst = AKSC_HDST_UNSOLVED;
	
	// (m_hdata is initialized with AKSC_InitDecomp8975)
	prms->m_hnave = CSPEC_HNAVE;
	prms->m_dvec.u.x = CSPEC_DVEC_X;
	prms->m_dvec.u.y = CSPEC_DVEC_Y;
	prms->m_dvec.u.z = CSPEC_DVEC_Z;
}

/*!
 Fill #AK8975PRMS structure with default value.
 @param[out] prms A pointer to #AK8975PRMS structure.
 */
void SetDefaultPRMS(AK8975PRMS* prms)
{
	int16 i;
	// Set parameter to HDST, HO, HREF
	for (i = 0; i < CSPEC_NUM_FORMATION; i++) {
		prms->HSUC_HDST[i] = AKSC_HDST_UNSOLVED;
		prms->HSUC_HO[i].u.x = 0;
		prms->HSUC_HO[i].u.y = 0;
		prms->HSUC_HO[i].u.z = 0;
		prms->HFLUCV_HREF[i].u.x = 0;
		prms->HFLUCV_HREF[i].u.y = 0;
		prms->HFLUCV_HREF[i].u.z = 0;
	}
}

/*!
 Get interval from device driver. This function will not resolve dependencies.
 Dependencies will be resolved in Sensor HAL.
 @param[out] mag_acq Magnetometer acquisition timing.
 @param[out] acc_acq Accelerometer acquisition timing.
 @param[out] ori_acq Orientation sensor acquisition timing.
 @param[out] mag_mes Magnetometer measurement timing.
 @param[out] acc_mes Accelerometer measurement timing.
 @param[out] hdoe_interval HDOE decimator.
 */
int16 GetInterval(
	AKMD_LOOP_TIME* mag_acq,
	AKMD_LOOP_TIME* acc_acq,
	AKMD_LOOP_TIME* ori_acq,
	AKMD_LOOP_TIME* mag_mes,
	AKMD_LOOP_TIME* acc_mes,
	int16* hdoe_dec)
{
	/* Magnetometer, Accelerometer, Orientation */
	/* Delay is in nano second unit. */
	/* Negative value means the sensor is disabled.*/
	int64_t delay[3];

	if (AKD_GetDelay(delay) < 0) {
		return AKRET_PROC_FAIL;
	}
	AKMDATA(AKMDATA_GETINTERVAL,"delay=%lld,%lld,%lld\n",
		delay[0], delay[1], delay[2]);

	/* Magnetmeter's frequency should be discrete value */
	if ((0 <= delay[0]) && (delay[0] <= AKMD_MAG_MIN_INTERVAL)) {
		delay[0] = AKMD_MAG_MIN_INTERVAL;
	}
#ifdef AKMD_ACC_COMBINED
	/* Accelerometer's interval limit */
	if ((0 <= delay[1]) && (delay[1] <= AKMD_ACC_MIN_INTERVAL)) {
		delay[1] = AKMD_ACC_MIN_INTERVAL;
	}
#else
	/* Always disabled */
	delay[1] = -1;
#endif
	/* Orientation sensor's interval limit */
	if ((0 <= delay[2]) && (delay[2] <= AKMD_ORI_MIN_INTERVAL)) {
		delay[2] = AKMD_ORI_MIN_INTERVAL;
	}
	/* update */
	if ((delay[0] != mag_acq->interval) ||
		(delay[1] != acc_acq->interval) ||
		(delay[2] != ori_acq->interval)) {
		mag_acq->duration = mag_acq->interval = delay[0];
		acc_acq->duration = acc_acq->interval = delay[1];
		ori_acq->duration = ori_acq->interval = delay[2];

		if (ori_acq->interval < 0) {
			/* NO relation between orientation sensor */
			acc_mes->interval = acc_acq->interval;
			mag_mes->interval = mag_acq->interval;
			acc_mes->duration = 0;
			mag_mes->duration = 0;
		} else {
			if ((acc_acq->interval >= 0) && (acc_acq->interval
											 < ori_acq->interval)) {
				acc_mes->interval = acc_acq->interval;
				acc_mes->duration = 0;
			} else {
				acc_mes->interval = ori_acq->interval;
				acc_mes->duration = 0;
			}
			if ((mag_acq->interval >= 0) && (mag_acq->interval
											 < ori_acq->interval)) {
				mag_mes->interval = mag_acq->interval;
				mag_mes->duration = 0;
			} else {
				mag_mes->interval = ori_acq->interval;
				mag_mes->duration = 0;
			}
		}
		// Adjust frequency for HDOE
		if (0 <= (mag_mes->interval)) {
			GetHDOEDecimator(&(mag_mes->interval), hdoe_dec);
		}
		AKMDATA(AKMDATA_GETINTERVAL,
				 "%s:\n"
				 "  AcqInterval(M,A,O)=%lld,%lld,%lld\n"
				 "  MesInterval(M,A)=%lld,%lld\n", __FUNCTION__,
				 mag_acq->interval, acc_acq->interval, ori_acq->interval,
				 mag_mes->interval, acc_mes->interval);
	}
	
	return AKRET_PROC_SUCCEED;
}

/*!
 Calculate loop duration
 @return If it is time to fire the event, the return value is 1, otherwise 0.
 @param[in,out] tm An event.
 @param[in] execTime The time to execute main loop for one time.
 @param[in,out] minDuration The minimum sleep time in all events.
 */
int SetLoopTime(
	AKMD_LOOP_TIME* tm,
	int64_t execTime,
	int64_t* minDuration)
{
	int ret = 0;
	if (tm->interval >= 0) {
		tm->duration -= execTime;
		if (tm->duration <= AKMD_LOOP_MARGIN) {
			tm->duration = tm->interval;
			ret = 1;
		} else if (tm->duration < *minDuration) {
			*minDuration = tm->duration;
		}
	}
	return ret;
}

/*!
 Read hard coded value (Fuse ROM) from AK8975. Then set the read value to 
 calculation parameter.
 @return If parameters are read successfully, the return value is 
 #AKRET_PROC_SUCCEED. Otherwise the return value is #AKRET_PROC_FAIL. No 
 error code is reserved to show which operation has failed.
 @param[out] prms A pointer to #AK8975PRMS structure.
 */
int16 ReadAK8975FUSEROM(AK8975PRMS* prms)
{
	BYTE    i2cData[6];
	
	// Set to PowerDown mode
	if (AKD_SetMode(AK8975_MODE_POWERDOWN) != AKD_SUCCESS) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}
	
	// Set to FUSE ROM access mode
	if (AKD_SetMode(AK8975_MODE_FUSE_ACCESS) != AKD_SUCCESS) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}
	
	// Read values. ASAX, ASAY, ASAZ
	if (AKD_RxData(AK8975_FUSE_ASAX, i2cData, 3) != AKD_SUCCESS) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}
	prms->m_asa.u.x = (int16)i2cData[0];
	prms->m_asa.u.y = (int16)i2cData[1];
	prms->m_asa.u.z = (int16)i2cData[2];

	AKMDEBUG(DBG_LEVEL2, "%s: asa(dec)=%d,%d,%d\n", __FUNCTION__,
			 prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z);

	// Set keywords for SmartCompassLibrary certification
	prms->m_key[2] = (int16)i2cData[0];
	prms->m_key[3] = (int16)i2cData[1];
	prms->m_key[4] = (int16)i2cData[2];
	
	// Set to PowerDown mode 
	if (AKD_SetMode(AK8975_MODE_POWERDOWN) != AKD_SUCCESS) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}
	
	// Set keywords for SmartCompassLibrary certification
	if (AKD_RxData(AK8975_REG_WIA, i2cData, 1) != AKD_SUCCESS) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}
	prms->m_key[0] = CSPEC_CI_AK_DEVICE;
	prms->m_key[1] = (int16)i2cData[0];
	strncpy(prms->m_licenser, CSPEC_CI_LICENSER, AKSC_CI_MAX_CHARSIZE);
	strncpy(prms->m_licensee, CSPEC_CI_LICENSEE, AKSC_CI_MAX_CHARSIZE);

	AKMDEBUG(DBG_LEVEL2, "%s: key=%d, licenser=%s, licensee=%s\n", 
			 __FUNCTION__, prms->m_key[1], prms->m_licenser, prms->m_licensee);

	return AKRET_PROC_SUCCEED;
}


/*!
 Set initial values to registers of AK8975. Then initialize algorithm 
 parameters.
 @return If parameters are read successfully, the return value is 
 #AKRET_PROC_SUCCEED. Otherwise the return value is #AKRET_PROC_FAIL. No 
 error code is reserved to show which operation has failed.
 @param[in,out] prms A pointer to a #AK8975PRMS structure.
 */
int16 InitAK8975_Measure(AK8975PRMS* prms)
{
	// Set to PowerDown mode
	if (AKD_SetMode(AK8975_MODE_POWERDOWN) != AKD_SUCCESS) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}

	prms->m_form = checkForm();

	// Restore the value when succeeding in estimating of HOffset. 
	prms->m_ho   = prms->HSUC_HO[prms->m_form]; 
	prms->m_hdst = prms->HSUC_HDST[prms->m_form];

	// Initialize the decompose parameters
	AKSC_InitDecomp8975(prms->m_hdata);

	// Initialize HDOE parameters
	AKSC_InitHDOEProcPrmsS3(
							&prms->m_hdoev,
							1,
							&prms->m_ho,
							prms->m_hdst
							);

	AKSC_InitHFlucCheck(
						&(prms->m_hflucv),
						&(prms->HFLUCV_HREF[prms->m_form]),
						HFLUCV_TH
						);

	// Reset counter
	prms->m_cntSuspend = 0;
	prms->m_callcnt = 0;

	return AKRET_PROC_SUCCEED;
}

/*!
 Set initial values to registers of AK8975. Then initialize algorithm 
 parameters.
 @return Currently, this function always return #AKRET_PROC_SUCCEED. 
 @param[in,out] prms A pointer to a #AK8975PRMS structure.
 */
int16 SwitchFormation(AK8975PRMS* prms)
{
	prms->m_form = checkForm();
	
	// Restore the value when succeeding in estimating of HOffset. 
	prms->m_ho   = prms->HSUC_HO[prms->m_form]; 
	prms->m_hdst = prms->HSUC_HDST[prms->m_form];
	
	// Initialize the decompose parameters
	AKSC_InitDecomp8975(prms->m_hdata);
	
	// Initialize HDOE parameters
	AKSC_InitHDOEProcPrmsS3(
							&prms->m_hdoev,
							1,
							&prms->m_ho,
							prms->m_hdst
							);
	
	AKSC_InitHFlucCheck(
						&(prms->m_hflucv),
						&(prms->HFLUCV_HREF[prms->m_form]),
						HFLUCV_TH
						);
	
	// Reset counter
	prms->m_cntSuspend = CSPEC_CNTSUSPEND_SNG;
	prms->m_callcnt = 0;
	
	return AKRET_PROC_SUCCEED;
}


/*!
 Execute "Onboard Function Test" (includes "START" and "END" command).
 @retval 1 The test is passed successfully.
 @retval -1 The test is failed.
 @retval 0 The test is aborted by kind of system error.
 @param[in] prms A pointer to a #AK8975PRMS structure.
 */
int16 FctShipmntTest_Body(AK8975PRMS* prms)
{
	int16 pf_total = 1;
	
	//***********************************************
	//    Reset Test Result
	//***********************************************
	TEST_DATA(NULL, "START", 0, 0, 0, &pf_total);
	
	//***********************************************
	//    Step 1 to 2
	//***********************************************
	pf_total = FctShipmntTestProcess_Body(prms);

	//***********************************************
	//    Judge Test Result
	//***********************************************
	TEST_DATA(NULL, "END", 0, 0, 0, &pf_total);
	
	return pf_total;
}

/*!
 Execute "Onboard Function Test" (NOT includes "START" and "END" command).
 @retval 1 The test is passed successfully.
 @retval -1 The test is failed.
 @retval 0 The test is aborted by kind of system error.
 @param[in] prms A pointer to a #AK8975PRMS structure.
 */
int16 FctShipmntTestProcess_Body(AK8975PRMS* prms)
{
	int16   pf_total;  //p/f flag for this subtest
	BYTE    i2cData[16];
	int16   hdata[3];
	int16   asax;
	int16   asay;
	int16   asaz;
	
	//***********************************************
	//  Reset Test Result
	//***********************************************
	pf_total = 1;
	
	//***********************************************
	//  Step1
	//***********************************************
	
	// Set to PowerDown mode 
	if (AKD_SetMode(AK8975_MODE_POWERDOWN) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
	
	// When the serial interface is SPI,
	// write "00011011" to I2CDIS register(to disable I2C,).
	if(CSPEC_SPI_USE == 1){
		i2cData[0] = 0x1B;
		if (AKD_TxData(AK8975_REG_I2CDIS, i2cData, 1) != AKD_SUCCESS) {
			AKMERROR;
			return 0;
		}
	}
	
	// Read values from WIA to ASTC.
	if (AKD_RxData(AK8975_REG_WIA, i2cData, 13) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
	
	// TEST
	TEST_DATA(TLIMIT_NO_RST_WIA,  TLIMIT_TN_RST_WIA,  (int16)i2cData[0],  TLIMIT_LO_RST_WIA,  TLIMIT_HI_RST_WIA,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_INFO, TLIMIT_TN_RST_INFO, (int16)i2cData[1],  TLIMIT_LO_RST_INFO, TLIMIT_HI_RST_INFO, &pf_total);
	TEST_DATA(TLIMIT_NO_RST_ST1,  TLIMIT_TN_RST_ST1,  (int16)i2cData[2],  TLIMIT_LO_RST_ST1,  TLIMIT_HI_RST_ST1,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HXL,  TLIMIT_TN_RST_HXL,  (int16)i2cData[3],  TLIMIT_LO_RST_HXL,  TLIMIT_HI_RST_HXL,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HXH,  TLIMIT_TN_RST_HXH,  (int16)i2cData[4],  TLIMIT_LO_RST_HXH,  TLIMIT_HI_RST_HXH,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HYL,  TLIMIT_TN_RST_HYL,  (int16)i2cData[5],  TLIMIT_LO_RST_HYL,  TLIMIT_HI_RST_HYL,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HYH,  TLIMIT_TN_RST_HYH,  (int16)i2cData[6],  TLIMIT_LO_RST_HYH,  TLIMIT_HI_RST_HYH,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HZL,  TLIMIT_TN_RST_HZL,  (int16)i2cData[7],  TLIMIT_LO_RST_HZL,  TLIMIT_HI_RST_HZL,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HZH,  TLIMIT_TN_RST_HZH,  (int16)i2cData[8],  TLIMIT_LO_RST_HZH,  TLIMIT_HI_RST_HZH,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_ST2,  TLIMIT_TN_RST_ST2,  (int16)i2cData[9],  TLIMIT_LO_RST_ST2,  TLIMIT_HI_RST_ST2,  &pf_total);
	TEST_DATA(TLIMIT_NO_RST_CNTL, TLIMIT_TN_RST_CNTL, (int16)i2cData[10], TLIMIT_LO_RST_CNTL, TLIMIT_HI_RST_CNTL, &pf_total);
	// i2cData[11] is BLANK.
	TEST_DATA(TLIMIT_NO_RST_ASTC, TLIMIT_TN_RST_ASTC, (int16)i2cData[12], TLIMIT_LO_RST_ASTC, TLIMIT_HI_RST_ASTC, &pf_total);
	
	// Read values from I2CDIS.
	if (AKD_RxData(AK8975_REG_I2CDIS, i2cData, 1) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
	if(CSPEC_SPI_USE == 1){
		TEST_DATA(TLIMIT_NO_RST_I2CDIS, TLIMIT_TN_RST_I2CDIS, (int16)i2cData[0], TLIMIT_LO_RST_I2CDIS_USESPI, TLIMIT_HI_RST_I2CDIS_USESPI, &pf_total);
	}else{
		TEST_DATA(TLIMIT_NO_RST_I2CDIS, TLIMIT_TN_RST_I2CDIS, (int16)i2cData[0], TLIMIT_LO_RST_I2CDIS_USEI2C, TLIMIT_HI_RST_I2CDIS_USEI2C, &pf_total);
	}
	
	// Set to FUSE ROM access mode
	if (AKD_SetMode(AK8975_MODE_FUSE_ACCESS) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
	
	// Read values from ASAX to ASAZ
	if (AKD_RxData(AK8975_FUSE_ASAX, i2cData, 3) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
	asax = (int16)i2cData[0];
	asay = (int16)i2cData[1];
	asaz = (int16)i2cData[2];
	
	// TEST
	TEST_DATA(TLIMIT_NO_ASAX, TLIMIT_TN_ASAX, asax, TLIMIT_LO_ASAX, TLIMIT_HI_ASAX, &pf_total);
	TEST_DATA(TLIMIT_NO_ASAY, TLIMIT_TN_ASAY, asay, TLIMIT_LO_ASAY, TLIMIT_HI_ASAY, &pf_total);
	TEST_DATA(TLIMIT_NO_ASAZ, TLIMIT_TN_ASAZ, asaz, TLIMIT_LO_ASAZ, TLIMIT_HI_ASAZ, &pf_total);
	
	// Read values. CNTL
	if (AKD_RxData(AK8975_REG_CNTL, i2cData, 1) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
	
	// Set to PowerDown mode 
	if (AKD_SetMode(AK8975_MODE_POWERDOWN) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
	
	// TEST
	TEST_DATA(TLIMIT_NO_WR_CNTL, TLIMIT_TN_WR_CNTL, (int16)i2cData[0], TLIMIT_LO_WR_CNTL, TLIMIT_HI_WR_CNTL, &pf_total);

	
	//***********************************************
	//  Step2
	//***********************************************
	
	// Set to SNG measurement pattern (Set CNTL register) 
	if (AKD_SetMode(AK8975_MODE_SNG_MEASURE) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
	
	// Wait for DRDY pin changes to HIGH.
	// Get measurement data from AK8975
	// ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + ST2
	// = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 = 8 bytes
	if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
	
	hdata[0] = (int16)((((uint16)(i2cData[2]))<<8)+(uint16)(i2cData[1]));
	hdata[1] = (int16)((((uint16)(i2cData[4]))<<8)+(uint16)(i2cData[3]));
	hdata[2] = (int16)((((uint16)(i2cData[6]))<<8)+(uint16)(i2cData[5]));
	
	// TEST
	TEST_DATA(TLIMIT_NO_SNG_ST1, TLIMIT_TN_SNG_ST1, (int16)i2cData[0], TLIMIT_LO_SNG_ST1, TLIMIT_HI_SNG_ST1, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HX, TLIMIT_TN_SNG_HX, hdata[0], TLIMIT_LO_SNG_HX, TLIMIT_HI_SNG_HX, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HY, TLIMIT_TN_SNG_HY, hdata[1], TLIMIT_LO_SNG_HY, TLIMIT_HI_SNG_HY, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HZ, TLIMIT_TN_SNG_HZ, hdata[2], TLIMIT_LO_SNG_HZ, TLIMIT_HI_SNG_HZ, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_ST2, TLIMIT_TN_SNG_ST2, (int16)i2cData[7], TLIMIT_LO_SNG_ST2, TLIMIT_HI_SNG_ST2, &pf_total);
	
	// Generate magnetic field for self-test (Set ASTC register)
	i2cData[0] = 0x40;
	if (AKD_TxData(AK8975_REG_ASTC, i2cData, 1) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
	
	// Set to Self-test mode (Set CNTL register)
	if (AKD_SetMode(AK8975_MODE_SELF_TEST) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
	
	// Wait for DRDY pin changes to HIGH.
	// Get measurement data from AK8975
	// ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + ST2
	// = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 = 8Byte
	if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
		
	// TEST
	TEST_DATA(TLIMIT_NO_SLF_ST1, TLIMIT_TN_SLF_ST1, (int16)i2cData[0], TLIMIT_LO_SLF_ST1, TLIMIT_HI_SLF_ST1, &pf_total);
	
	hdata[0] = (int16)((((uint16)(i2cData[2]))<<8)+(uint16)(i2cData[1]));
	hdata[1] = (int16)((((uint16)(i2cData[4]))<<8)+(uint16)(i2cData[3]));
	hdata[2] = (int16)((((uint16)(i2cData[6]))<<8)+(uint16)(i2cData[5]));
	
	// TEST
	TEST_DATA(
			  TLIMIT_NO_SLF_RVHX, 
			  TLIMIT_TN_SLF_RVHX, 
			  (hdata[0])*((asax - 128)*0.5f/128.0f + 1),
			  TLIMIT_LO_SLF_RVHX,
			  TLIMIT_HI_SLF_RVHX,
			  &pf_total
			  );
	
	TEST_DATA(
			  TLIMIT_NO_SLF_RVHY,
			  TLIMIT_TN_SLF_RVHY,
			  (hdata[1])*((asay - 128)*0.5f/128.0f + 1),
			  TLIMIT_LO_SLF_RVHY,
			  TLIMIT_HI_SLF_RVHY,
			  &pf_total
			  );
	
	TEST_DATA(
			  TLIMIT_NO_SLF_RVHZ,
			  TLIMIT_TN_SLF_RVHZ,
			  (hdata[2])*((asaz - 128)*0.5f/128.0f + 1),
			  TLIMIT_LO_SLF_RVHZ,
			  TLIMIT_HI_SLF_RVHZ,
			  &pf_total
			  );
	
	// TEST
	TEST_DATA(TLIMIT_NO_SLF_ST2, TLIMIT_TN_SLF_ST2, (int16)i2cData[7], TLIMIT_LO_SLF_ST2, TLIMIT_HI_SLF_ST2, &pf_total);
	
	// Set to Normal mode for self-test.
	i2cData[0] = 0x00;
	if (AKD_TxData(AK8975_REG_ASTC, i2cData, 1) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
	
	return pf_total;
}

/*!
 This is the main routine of measurement.
 @param[in,out] prms A pointer to a #AK8975PRMS structure.
 */
void MeasureSNGLoop(AK8975PRMS* prms)
{
	BYTE    i2cData[AKSC_BDATA_SIZE];
	int16   bData[AKSC_BDATA_SIZE];  // Measuring block data
	int16   ret;
	int16   i;
	int16	hdoe_interval;

	/* Magnetic interval */
	AKMD_LOOP_TIME mag_acq = { -1, 0 };
	/* Acceleration interval */
	AKMD_LOOP_TIME acc_acq = { -1, 0 };
	/* Orientation interval */
	AKMD_LOOP_TIME ori_acq = { -1, 0 };
	/* Magnetic acquisition interval */
	AKMD_LOOP_TIME mag_mes = { -1, 0 };
	/* Acceleration acquisition interval */
	AKMD_LOOP_TIME acc_mes = { -1, 0 };
	/* Magnetic measurement interval */
	AKMD_LOOP_TIME mag_int = { AK8975_MEASUREMENT_TIME, 0 };
	/* Setting interval */
	AKMD_LOOP_TIME setting = { AKMD_SETTING_INTERVAL, 0 };

	/* 0x001: Magnetic execute flag (data output) */
	/* 0x002: Acceleration execute flag (data output) */
	/* 0x004: Orientation execute flag (data output) */
	/* 0x010: Magnetic measurement flag */
	/* 0x020: Magnetic interrupt flag */
	/* 0x040: Acceleration measurement flag */
	/* 0x080: Acceleration interrupt flag */
	/* 0x100: Setting execute flag */
	uint16 exec_flags;
	
	struct timespec currTime = { 0, 0 }; /* Current time */
	struct timespec lastTime = { 0, 0 }; /* Previous time */
	int64_t execTime; /* Time between two points */
	int64_t minVal; /* The minimum duration to the next event */
	int measuring = 0; /* The value is 1, if while measuring. */

	if (openForm() < 0) {
		AKMERROR;
		return;
	}

	/* Get initial interva */
	if (GetInterval(&mag_acq, &acc_acq, &ori_acq,
					&mag_mes, &acc_mes,
					&hdoe_interval) != AKRET_PROC_SUCCEED) {
		AKMERROR;
		goto MEASURE_SNG_END;
	}
	
	/* Initialize */
	if(InitAK8975_Measure(prms) != AKD_SUCCESS){
		goto MEASURE_SNG_END;
	}

	/* Beginning time */
	if (clock_gettime(CLOCK_REALTIME, &currTime) < 0) {
		AKMERROR;
		goto MEASURE_SNG_END;
	}

	while(g_stopRequest != AKKEY_STOP_MEASURE){
		exec_flags = 0;
		minVal = 1000000000; /*1sec*/

		/* Copy the last time */
		lastTime = currTime;
		
		/* Get current time */
		if (clock_gettime(CLOCK_REALTIME, &currTime) < 0) {
			AKMERROR;
			break;
		}
		
		/* Calculate the difference */
		execTime = CalcDuration(&currTime, &lastTime);
		
		AKMDATA(AKMDATA_EXECTIME,
				"Executing(%6.2f)\n", (double)execTime / 1000000.0);

		/* Subtract the differential time from each event. 
		 If subtracted value is negative turn event flag on. */
		exec_flags |= (SetLoopTime(&setting, execTime, &minVal)
					   << (SETTING_FLAG_POS));

		exec_flags |= (SetLoopTime(&mag_acq, execTime, &minVal)
					   << (MAG_ACQ_FLAG_POS));
		
		exec_flags |= (SetLoopTime(&acc_acq, execTime, &minVal)
					   << (ACC_ACQ_FLAG_POS));
		
		exec_flags |= (SetLoopTime(&ori_acq, execTime, &minVal)
					   << (ORI_ACQ_FLAG_POS));
		
		exec_flags |= (SetLoopTime(&acc_mes, execTime, &minVal)
					   << (ACC_MES_FLAG_POS));

		/* Magnetometer needs special care. While the device is
		 under measuring, measurement start flag should not be turned on.*/
		if (mag_mes.interval >= 0) {
			mag_mes.duration -= execTime;
			if (!measuring) {
				/* Not measuring */
				if (mag_mes.duration <= AKMD_LOOP_MARGIN) {
					exec_flags |= (1 << (MAG_MES_FLAG_POS));
				} else if (mag_mes.duration < minVal) {
					minVal = mag_mes.duration;
				}
			} else {
				/* While measuring */
				mag_int.duration -= execTime;
				/* NO_MARGIN! */
				if (mag_int.duration <= 0) {
					exec_flags |= (1 << (MAG_INT_FLAG_POS));
				} else if (mag_int.duration < minVal) {
					minVal = mag_int.duration;
				}
			}
		}
		
		/* If all flag is off, go sleep */
		if (exec_flags == 0) {
			AKMDATA(AKMDATA_EXECTIME, "Sleeping(%6.2f)...\n",
					(double)minVal / 1000000.0);
			if (minVal > 0) {
				struct timespec doze = { 0, 0 };
				doze = int64_to_timespec(minVal);
				nanosleep(&doze, NULL);
			}
		} else {
			AKMDATA(AKMDATA_EXECFLAG, "ExecFlags=0x%04X\n", exec_flags);
			
			if (exec_flags & (1 << (MAG_MES_FLAG_POS))) {
				/* Set to SNG measurement pattern (Set CNTL register) */
				if (AKD_SetMode(AK8975_MODE_SNG_MEASURE) != AKD_SUCCESS) {
					AKMERROR;
					break;
				}
				mag_mes.duration = mag_mes.interval;
				mag_int.duration = mag_int.interval;
				measuring = 1;
			}
			
			if (exec_flags & (1 << (MAG_INT_FLAG_POS))) {
				/* Get magnetometer measurement data */
				/* ST1 + HXL,HXH + HYL,HYH + HZL,HZH + ST2 = 8 Byte */
				if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) {
					AKMERROR;
					break;
				}
				// Copy to local variable
				AKMDATA(AKMDATA_BDATA, "bData(Hex)=");
				for(i=0; i<AKSC_BDATA_SIZE; i++){
					bData[i] = i2cData[i];
					AKMDATA(AKMDATA_BDATA, "%02x,", bData[i]);
				}
				AKMDATA(AKMDATA_BDATA, "\n");

				ret = MeasuringEventProcess(
											bData,
											prms,
											checkForm(),
											hdoe_interval);
				
				// Check the return value
				if(ret == AKRET_PROC_SUCCEED) {
					;/* Do nothing */
				} else if(ret == AKRET_FORMATION_CHANGED) {
					SwitchFormation(prms);
				} else if(ret == AKRET_DATA_READERROR) {
					AKMDEBUG(DBG_LEVEL2, "Data read error occurred.\n");
				} else if(ret == AKRET_DATA_OVERFLOW) {
					AKMDEBUG(DBG_LEVEL2, "Data overflow occurred.\n");
				} else if(ret == AKRET_HFLUC_OCCURRED) {
					AKMDEBUG(DBG_LEVEL2, "AKSC_HFlucCheck did not return 1.\n");
				} else {
					LOGE("MeasuringEventProcess has failed.\n");
					break;
				}
				measuring = 0;
			}

			if (exec_flags & (1 << (ACC_MES_FLAG_POS))) {
				/* Get accelerometer data */
				if (AKD_GetAccelerationData(prms->m_avec.v) != AKD_SUCCESS) {
					AKMERROR;
					break;
				}

				AKMDATA(AKMDATA_AVEC, "acc(dec)=%d,%d,%d\n",
						prms->m_avec.u.x, prms->m_avec.u.y, prms->m_avec.u.z);
			}

			if (exec_flags & (1 << (ORI_ACQ_FLAG_POS))) {
				/* Calculate direction angle */
				if (CalcDirection(prms) != AKRET_PROC_SUCCEED) {
					AKMERROR;
				}
			}
		}
		if (exec_flags & 0x07) {
			/* If any ACQ flag is on, report the data to device driver */
			Disp_MeasurementResultHook(prms, (uint16)(exec_flags & 0x07));
		}
		
		if (exec_flags & (1 << (SETTING_FLAG_POS))) {
			/* Get measurement interval from device driver */
			GetInterval(&mag_acq, &acc_acq, &ori_acq, 
						&mag_mes, &acc_mes,
						&hdoe_interval);
		}
	}
	
MEASURE_SNG_END:
	// Set to PowerDown mode 
	if (AKD_SetMode(AK8975_MODE_POWERDOWN) != AKD_SUCCESS) {
		AKMERROR;
	}

	closeForm();
}



/*!
 SmartCompass main calculation routine. This function will be processed 
 when INT pin event is occurred.
 @retval AKRET_
 @param[in] bData An array of register values which holds,
 ST1, HXL, HXH, HYL, HYH, HZL, HZH and ST2 value respectively.
 @param[in,out] prms A pointer to a #AK8975PRMS structure.
 @param[in] curForm The index of hardware position which represents the 
 index when this function is called.
 @param[in] hDecimator HDOE will execute once while this function is called 
 this number of times.
 */
int16 MeasuringEventProcess(
	const int16	bData[],
	AK8975PRMS*	prms,
	const int16	curForm,
	const int16	hDecimator)
{
	int16vec have;
	int16    dor, derr, hofl;
	int16    isOF;
	int16    aksc_ret;
	int16    hdSucc;
	
	dor = 0;
	derr = 0;
	hofl = 0;
	
	// Decompose one block data into each Magnetic sensor's data
	aksc_ret = AKSC_Decomp8975(
							   bData,
							   prms->m_hnave,
							   &prms->m_asa,
							   prms->m_hdata,
							   &prms->m_hn,
							   &have,
							   &dor,
							   &derr,
							   &hofl
							   );
	
	if (aksc_ret == 0) {
		AKMDUMP("AKSC_Decomp8975 failed.\n"
				"  ST1=0x%02X, ST2=0x%02X\n"
				"  XYZ(HEX)=%02X,%02X,%02X,%02X,%02X,%02X\n"
				"  asa(dec)=%d,%d,%d\n", 
				bData[0], bData[7],
				bData[1], bData[2], bData[3], bData[4], bData[5], bData[6], 
				prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z);
		return AKRET_PROC_FAIL;
	}
	
	// Check the formation change
	if(prms->m_form != curForm){
		prms->m_form = curForm;
		return AKRET_FORMATION_CHANGED;
	}
	
	if(derr == 1){
		return AKRET_DATA_READERROR;
	}
	
	if(prms->m_cntSuspend > 0){
		prms->m_cntSuspend--;
	}
	else {
		// Detect a fluctuation of magnetic field.
		isOF = AKSC_HFlucCheck(&(prms->m_hflucv), &(prms->m_hdata[0]));
		
		if(hofl == 1){
			// Set a HDOE level as "HDST_UNSOLVED" 
			AKSC_SetHDOELevel(
							  &prms->m_hdoev,
							  &prms->m_ho,
							  AKSC_HDST_UNSOLVED,
							  1
							  );
			prms->m_hdst = AKSC_HDST_UNSOLVED;
			return AKRET_DATA_OVERFLOW;
		}
		else if(isOF == 1){
			// Set a HDOE level as "HDST_UNSOLVED" 
			AKSC_SetHDOELevel(
							  &prms->m_hdoev,
							  &prms->m_ho,
							  AKSC_HDST_UNSOLVED,
							  1
							  );
			prms->m_hdst = AKSC_HDST_UNSOLVED;
			return AKRET_HFLUC_OCCURRED;
		}
		else {
			prms->m_callcnt--;
			if(prms->m_callcnt <= 0){
				//Calculate Magnetic sensor's offset by DOE
				hdSucc = AKSC_HDOEProcessS3(
											prms->m_licenser,
											prms->m_licensee,
											prms->m_key,
											&prms->m_hdoev,
											prms->m_hdata,
											prms->m_hn,
											&prms->m_ho,
											&prms->m_hdst
											);
				
				if(hdSucc == AKSC_CERTIFICATION_DENIED){
					AKMERROR;
					return AKRET_PROC_FAIL;
				}
				if(hdSucc > 0){
					prms->HSUC_HO[prms->m_form] = prms->m_ho;
					prms->HSUC_HDST[prms->m_form] = prms->m_hdst;
					prms->HFLUCV_HREF[prms->m_form] = prms->m_hflucv.href;
				}
				
				prms->m_callcnt = hDecimator;
			}
		}
	}
	
	// Subtract offset and normalize magnetic field vector.
	aksc_ret = AKSC_VNorm(
						  &have,
						  &prms->m_ho,
						  &prms->m_hs,
						  AKSC_HSENSE_TARGET,
						  &prms->m_hvec
						  );
	if (aksc_ret == 0) {
		AKMDUMP("AKSC_VNorm failed.\n"
				"  have=%6d,%6d,%6d  ho=%6d,%6d,%6d  hs=%6d,%6d,%6d\n",
				have.u.x, have.u.y, have.u.z,
				prms->m_ho.u.x, prms->m_ho.u.y, prms->m_ho.u.z,
				prms->m_hs.u.x, prms->m_hs.u.y, prms->m_hs.u.z);
		return AKRET_PROC_FAIL;
	}

	//Convert layout from sensor to Android by using PAT number.
	// Magnetometer
	ConvertCoordinate(prms->m_layout, &prms->m_hvec);

#ifdef AKMD_ACC_COMBINED
	// Accelerometer
	ConvertCoordinate(prms->m_layout, &prms->m_avec);
#endif

	return AKRET_PROC_SUCCEED;
}

/*!
 Calculate Yaw, Pitch, Roll angle.
 m_hvec and m_avec should be Android coordination.
 @return Always return #AKRET_PROC_SUCCEED.
 @param[in,out] prms A pointer to a #AK8975PRMS structure.
 */
int16 CalcDirection(AK8975PRMS* prms)
{
	/* Conversion matrix from Android to SmartCompass coordination */
	const I16MATRIX hlayout = {{
								 0, 1, 0,
								-1, 0, 0,
								 0, 0, 1}};
	const I16MATRIX alayout = {{
								 0,-1, 0,
								 1, 0, 0,
								 0, 0,-1}};

	int16    preThe;

	preThe = prms->m_theta;
	prms->m_ds3Ret = AKSC_DirectionS3(
									  prms->m_licenser,
									  prms->m_licensee,
									  prms->m_key,
									  &prms->m_hvec,
									  &prms->m_avec,
									  &prms->m_dvec,
									  &hlayout,
									  &alayout,
									  &prms->m_theta,
									  &prms->m_delta,
									  &prms->m_hr,
									  &prms->m_hrhoriz,
									  &prms->m_ar,
									  &prms->m_phi180,
									  &prms->m_phi90,
									  &prms->m_eta180,
									  &prms->m_eta90,
									  &prms->m_mat
									  );
	
	prms->m_theta =	AKSC_ThetaFilter(
									 prms->m_theta,
									 preThe,
									 THETAFILTER_SCALE
									 );

	if(prms->m_ds3Ret == AKSC_CERTIFICATION_DENIED){
		AKMERROR;
		return AKRET_PROC_FAIL;
	}

	if(prms->m_ds3Ret != 3) {
		AKMDUMP("AKSC_Direction6D failed (0x%02x).\n"
				"  hvec=%d,%d,%d  avec=%d,%d,%d  dvec=%d,%d,%d\n",
				prms->m_ds3Ret,
				prms->m_hvec.u.x, prms->m_hvec.u.y, prms->m_hvec.u.z,
				prms->m_avec.u.x, prms->m_avec.u.y, prms->m_avec.u.z,
				prms->m_dvec.u.x, prms->m_dvec.u.y, prms->m_dvec.u.z);
	}
	/* Convert Yaw, Pitch, Roll angle to Android coordinate system */
	/* Actually, only Roll angle is opposite */
	if(prms->m_ds3Ret & 0x02){
		prms->m_eta180 = -prms->m_eta180;
		prms->m_eta90  = -prms->m_eta90;
	}

	return AKRET_PROC_SUCCEED;
}

