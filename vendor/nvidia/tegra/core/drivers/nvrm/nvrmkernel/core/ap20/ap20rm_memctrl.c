/*
 * Copyright (c) 2008 - 2010 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvrm_init.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvrm_module.h"
#include "ap20/aremc.h"
#include "ap20/armc.h"
#include "ap20/arahb_arbc.h"
#include "nvrm_drf.h"
#include "nvrm_hwintf.h"
#include "nvrm_structure.h"

NvError NvRmPrivAp20McErrorMonitorStart(NvRmDeviceHandle hRm);
void NvRmPrivAp20McErrorMonitorStop(NvRmDeviceHandle hRm);
void NvRmPrivAp20SetupMc(NvRmDeviceHandle hRm);
static void McErrorIntHandler(void* args);
void
McStatAp20_Start(
        NvRmDeviceHandle rm,
        NvU32 client_id_0,
        NvU32 client_id_1,
        NvU32 llc_client_id);
void
McStatAp20_Stop(
        NvRmDeviceHandle rm,
        NvU32 *client_0_cycles,
        NvU32 *client_1_cycles,
        NvU32 *llc_client_cycles,
        NvU32 *llc_client_clocks,
        NvU32 *mc_clocks);

static NvOsInterruptHandle s_McInterruptHandle = NULL;

void McErrorIntHandler(void* args)
{
    NvU32 RegVal;
    NvU32 IntStatus;
    NvU32 IntClear = 0;
    NvRmDeviceHandle hRm = (NvRmDeviceHandle)args;
    
    IntStatus = NV_REGR(hRm,
        NVRM_MODULE_ID( NvRmPrivModuleID_MemoryController, 0 ),
        MC_INTSTATUS_0);

    if ( NV_DRF_VAL(MC, INTSTATUS, SECURITY_VIOLATION_INT, IntStatus) )
    {
        IntClear |= NV_DRF_DEF(MC, INTSTATUS, SECURITY_VIOLATION_INT, SET);
        RegVal = NV_REGR(hRm,
            NVRM_MODULE_ID( NvRmPrivModuleID_MemoryController, 0 ),
            MC_SECURITY_VIOLATION_ADR_0);
        NvOsDebugPrintf("SECURITY_VIOLATION DecErrAddress=0x%x ", RegVal);
        RegVal = NV_REGR(hRm,
            NVRM_MODULE_ID( NvRmPrivModuleID_MemoryController, 0 ),
            MC_SECURITY_VIOLATION_STATUS_0);
        NvOsDebugPrintf("SECURITY_VIOLATION DecErrStatus=0x%x ", RegVal);
    }
    if ( NV_DRF_VAL(MC, INTSTATUS, DECERR_EMEM_OTHERS_INT, IntStatus) )
    {
        IntClear |= NV_DRF_DEF(MC, INTSTATUS, DECERR_EMEM_OTHERS_INT, SET);
        RegVal = NV_REGR(hRm,
            NVRM_MODULE_ID( NvRmPrivModuleID_MemoryController, 0 ),
            MC_DECERR_EMEM_OTHERS_ADR_0);
        NvOsDebugPrintf("EMEM DecErrAddress=0x%x ", RegVal);
        RegVal = NV_REGR(hRm,
            NVRM_MODULE_ID( NvRmPrivModuleID_MemoryController, 0 ),
            MC_DECERR_EMEM_OTHERS_STATUS_0);
        NvOsDebugPrintf("EMEM DecErrStatus=0x%x ", RegVal);
    }
    if ( NV_DRF_VAL(MC, INTSTATUS, INVALID_GART_PAGE_INT, IntStatus) )
    {
        IntClear |= NV_DRF_DEF(MC, INTSTATUS, INVALID_GART_PAGE_INT, SET);
        RegVal = NV_REGR(hRm,
            NVRM_MODULE_ID( NvRmPrivModuleID_MemoryController, 0 ),
            MC_GART_ERROR_ADDR_0);
        NvOsDebugPrintf("GART DecErrAddress=0x%x ", RegVal);
        RegVal = NV_REGR(hRm,
            NVRM_MODULE_ID( NvRmPrivModuleID_MemoryController, 0 ),
            MC_GART_ERROR_REQ_0);
        NvOsDebugPrintf("GART DecErrStatus=0x%x ", RegVal);
    }
    
    NV_ASSERT(!"MC Decode Error ");
    // Clear the interrupt.
    NV_REGW(hRm, NVRM_MODULE_ID( NvRmPrivModuleID_MemoryController, 0 ),
        MC_INTSTATUS_0, IntClear);
    NvRmInterruptDone(s_McInterruptHandle);
}

NvError NvRmPrivAp20McErrorMonitorStart(NvRmDeviceHandle hRm)
{
    NvU32 val;
    NvU32 IrqList;
    NvError e = NvSuccess;
    NvOsInterruptHandler handler;
    
    if (s_McInterruptHandle == NULL)
    {
        // Install an interrupt handler.
        handler = McErrorIntHandler;
        IrqList = NvRmGetIrqForLogicalInterrupt(hRm,
                      NvRmPrivModuleID_MemoryController, 0);
        NV_CHECK_ERROR( NvRmInterruptRegister(hRm, 1, &IrqList,  &handler, 
            hRm, &s_McInterruptHandle, NV_TRUE) );
        // Enable Dec Err interrupts in memory Controller.
        val = NV_DRF_DEF(MC, INTMASK, SECURITY_VIOLATION_INTMASK, UNMASKED) |
              NV_DRF_DEF(MC, INTMASK, DECERR_EMEM_OTHERS_INTMASK, UNMASKED) |
              NV_DRF_DEF(MC, INTMASK, INVALID_GART_PAGE_INTMASK, UNMASKED);
        NV_REGW(hRm, NVRM_MODULE_ID( NvRmPrivModuleID_MemoryController, 0 ),
            MC_INTMASK_0, val);
    }
    return e;
}

void NvRmPrivAp20McErrorMonitorStop(NvRmDeviceHandle hRm)
{
    NvRmInterruptUnregister(hRm, s_McInterruptHandle);
    s_McInterruptHandle = NULL;
}

/* This function sets some performance timings for Mc & Emc.  Numbers are from
 * the Arch team.
 *
 */
void NvRmPrivAp20SetupMc(NvRmDeviceHandle hRm)
{
    NvU32   reg, mask;
    reg = NV_REGR(hRm, NVRM_MODULE_ID( NvRmPrivModuleID_MemoryController, 0 ),
        MC_LOWLATENCY_CONFIG_0);
    mask = NV_DRF_DEF(MC, LOWLATENCY_CONFIG, MPCORER_LL_CTRL, ENABLE) |
           NV_DRF_DEF(MC, LOWLATENCY_CONFIG, MPCORER_LL_SEND_BOTH, ENABLE);
    if ( mask != (reg & mask) )
        NV_ASSERT(!"MC LL Path not enabled!");
    // For AP20, no need to program any MC timeout registers here. Default 
    // values should be good enough.
}
void
McStatAp20_Start(
        NvRmDeviceHandle rm,
        NvU32 client_id_0,
        NvU32 client_id_1,
        NvU32 llc_client_id)
{
    
    NvRmModuleID emem_id = NVRM_MODULE_ID(
        NvRmPrivModuleID_ExternalMemoryController, 0 );
    NvRmModuleID mem_id = NVRM_MODULE_ID(
        NvRmPrivModuleID_MemoryController, 0 );
    NvU32 emc_ctrl = 
      (AREMC_STAT_CONTROL_MODE_BANDWIDTH << AREMC_STAT_CONTROL_MODE_SHIFT) |
      (AREMC_STAT_CONTROL_EVENT_QUALIFIED << AREMC_STAT_CONTROL_EVENT_SHIFT) |
      (AREMC_STAT_CONTROL_CLIENT_TYPE_MPCORER << 
         AREMC_STAT_CONTROL_CLIENT_TYPE_SHIFT) |  // default is MPCORE Read client (only client)
      (AREMC_STAT_CONTROL_FILTER_CLIENT_ENABLE << 
         AREMC_STAT_CONTROL_FILTER_CLIENT_SHIFT) |
      (AREMC_STAT_CONTROL_FILTER_ADDR_DISABLE << 
         AREMC_STAT_CONTROL_FILTER_ADDR_SHIFT);
    
    NvU32 mc_filter_client_0 = (ARMC_STAT_CONTROL_FILTER_CLIENT_ENABLE <<
                   ARMC_STAT_CONTROL_FILTER_CLIENT_SHIFT);

    NvU32 mc_filter_client_1 = (ARMC_STAT_CONTROL_FILTER_CLIENT_ENABLE <<
                   ARMC_STAT_CONTROL_FILTER_CLIENT_SHIFT);

    //NvOsDebugPrintf(" AP20 ID 0 = %d and ID 1 = %d \n", client_id_0, client_id_1);

    if (client_id_0 == 0xffffffff)
    {
        mc_filter_client_0 = (ARMC_STAT_CONTROL_FILTER_CLIENT_DISABLE <<
                   ARMC_STAT_CONTROL_FILTER_CLIENT_SHIFT);
        client_id_0 = 0;
    }

    if (client_id_1 == 0xffffffff)
    {
        mc_filter_client_1 = (ARMC_STAT_CONTROL_FILTER_CLIENT_DISABLE <<
                   ARMC_STAT_CONTROL_FILTER_CLIENT_SHIFT);
        client_id_1 = 0;
    }

    if(llc_client_id == 1)
        emc_ctrl |= AREMC_STAT_CONTROL_CLIENT_TYPE_MPCORER << 
                    AREMC_STAT_CONTROL_CLIENT_TYPE_SHIFT; 
                    // overwrite with MPCore read
        NV_REGW(rm, emem_id, EMC_STAT_CONTROL_0, 
                NV_DRF_DEF(EMC, STAT_CONTROL, LLMC_GATHER,DISABLE));
        NV_REGW(rm, emem_id, EMC_STAT_LLMC_CLOCK_LIMIT_0, 0xffffffff);
        NV_REGW(rm, emem_id, EMC_STAT_LLMC_CONTROL_0_0, emc_ctrl);
        NV_REGW(rm, emem_id, EMC_STAT_CONTROL_0, 
                NV_DRF_DEF(EMC, STAT_CONTROL, LLMC_GATHER, CLEAR));
        NV_REGW(rm, emem_id, EMC_STAT_CONTROL_0, 
                NV_DRF_DEF(EMC, STAT_CONTROL, LLMC_GATHER, ENABLE));
        NV_REGW(rm, mem_id, MC_STAT_CONTROL_0, 
                NV_DRF_DEF(MC, STAT_CONTROL, EMC_GATHER, DISABLE));
        NV_REGW(rm, mem_id, MC_STAT_EMC_CLOCK_LIMIT_0, 0xffffffff);
        NV_REGW(rm, mem_id, MC_STAT_EMC_CONTROL_0_0,
                (ARMC_STAT_CONTROL_MODE_BANDWIDTH << 
                   ARMC_STAT_CONTROL_MODE_SHIFT) |
                (client_id_0 << ARMC_STAT_CONTROL_CLIENT_ID_SHIFT) |
                (ARMC_STAT_CONTROL_EVENT_QUALIFIED << 
                   ARMC_STAT_CONTROL_EVENT_SHIFT) |
                mc_filter_client_0 |
                (ARMC_STAT_CONTROL_FILTER_ADDR_DISABLE << 
                   ARMC_STAT_CONTROL_FILTER_ADDR_SHIFT) |
                (ARMC_STAT_CONTROL_FILTER_PRI_DISABLE <<
                   ARMC_STAT_CONTROL_FILTER_PRI_SHIFT) |
                (ARMC_STAT_CONTROL_FILTER_COALESCED_DISABLE << 
                   ARMC_STAT_CONTROL_FILTER_COALESCED_SHIFT));
        NV_REGW(rm, mem_id, MC_STAT_EMC_CONTROL_1_0,
                (ARMC_STAT_CONTROL_MODE_BANDWIDTH <<
                   ARMC_STAT_CONTROL_MODE_SHIFT) |
                (client_id_1 << ARMC_STAT_CONTROL_CLIENT_ID_SHIFT) |
                (ARMC_STAT_CONTROL_EVENT_QUALIFIED <<
                   ARMC_STAT_CONTROL_EVENT_SHIFT) |
                mc_filter_client_1 |
                (ARMC_STAT_CONTROL_FILTER_ADDR_DISABLE <<
                   ARMC_STAT_CONTROL_FILTER_ADDR_SHIFT) |
                (ARMC_STAT_CONTROL_FILTER_PRI_DISABLE <<
                   ARMC_STAT_CONTROL_FILTER_PRI_SHIFT) |
                (ARMC_STAT_CONTROL_FILTER_COALESCED_DISABLE <<
                   ARMC_STAT_CONTROL_FILTER_COALESCED_SHIFT));

        NV_REGW(rm, mem_id, MC_STAT_CONTROL_0, 
                NV_DRF_DEF(MC, STAT_CONTROL, EMC_GATHER, CLEAR));
        NV_REGW(rm, mem_id, MC_STAT_CONTROL_0,
                NV_DRF_DEF(MC, STAT_CONTROL, EMC_GATHER, ENABLE));
}

void
McStatAp20_Stop(
        NvRmDeviceHandle rm,
        NvU32 *client_0_cycles,
        NvU32 *client_1_cycles,
        NvU32 *llc_client_cycles,
        NvU32 *llc_client_clocks,
        NvU32 *mc_clocks)
{
    *llc_client_cycles = NV_REGR(rm,
        NVRM_MODULE_ID( NvRmPrivModuleID_ExternalMemoryController, 0 ),
        EMC_STAT_LLMC_COUNT_0_0);
    *llc_client_clocks = NV_REGR(rm,
        NVRM_MODULE_ID( NvRmPrivModuleID_ExternalMemoryController, 0 ),
        EMC_STAT_LLMC_CLOCKS_0);
    *client_0_cycles = NV_REGR(rm,
        NVRM_MODULE_ID( NvRmPrivModuleID_MemoryController, 0 ),
        MC_STAT_EMC_COUNT_0_0);
    *client_1_cycles = NV_REGR(rm,
        NVRM_MODULE_ID( NvRmPrivModuleID_MemoryController, 0 ),
        MC_STAT_EMC_COUNT_1_0);
    *mc_clocks = NV_REGR(rm,
        NVRM_MODULE_ID( NvRmPrivModuleID_MemoryController, 0 ),
        MC_STAT_EMC_CLOCKS_0);
}
