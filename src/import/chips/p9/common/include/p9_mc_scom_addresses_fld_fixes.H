/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: chips/p9/common/include/p9_mc_scom_addresses_fld_fixes.H $    */
/*                                                                        */
/* IBM CONFIDENTIAL                                                       */
/*                                                                        */
/* EKB Project                                                            */
/*                                                                        */
/* COPYRIGHT 2015,2016                                                    */
/* [+] International Business Machines Corp.                              */
/*                                                                        */
/*                                                                        */
/* The source code for this program is not published or otherwise         */
/* divested of its trade secrets, irrespective of what has been           */
/* deposited with the U.S. Copyright Office.                              */
/*                                                                        */
/* IBM_PROLOG_END_TAG                                                     */
///
/// @file mc_scom_addresses_fld_fixes.H
/// @brief The *scom_addresses_fld.H files are generated form figtree,
///        but the figree can be wrong.  This file is included in
///        *_scom_addresses_fld.H and allows incorrect constants to be
///        fixed manually.
///
// *HWP HWP Owner: Ben Gass <bgass@us.ibm.com>
// *HWP FW Owner: ? <?>
// *HWP Team: SAO
// *HWP Level: 1
// *HWP Consumed by: FSP:HB:HS:OCC:SBE:CME:SGPE:PGPE:FPPE:IPPE

#ifndef __P9_MC_SCOM_ADDRESSES_FLD_FIXES_H
#define __P9_MC_SCOM_ADDRESSES_FLD_FIXES_H

//Example
//Copy the whole line from the *scom_addresses_fld.H file.  Then add FIX in front of REG
//and add another paramter that is the new value you want.
//
//FIXREG64_FLD( PU_ALTD_CMD_REG_FBC_WITH_TM_QUIESCE, 24, SH_UNT, SH_ACS_SCOM, SH_FLD_FBC_WITH_TM_QUIESCE,
//              12);

static const uint64_t SH_FLD_COMMAND_LIST_TIMEOUT_SPEC = 99990000;

REG64_FLD( MCS_MCFIR_COMMAND_LIST_TIMEOUT_SPEC                     , 9   , SH_UNT_MCS      , SH_ACS_SCOM2_OR ,
           SH_FLD_COMMAND_LIST_TIMEOUT_SPEC );

REG64_FLD( MCA_DDRPHY_DP16_SYSCLK_PR0_P0_0_01_ENABLE               , 48  , SH_UNT_MCA      , SH_ACS_SCOM_RW  ,
           0 );

REG64_FLD( MCA_DDRPHY_WC_RTT_WL_SWAP_ENABLE_P0                     , 48  , SH_UNT_MCA      , SH_ACS_SCOM_RW  ,
           0 );
REG64_FLD( MCA_DDRPHY_WC_RTT_WR_CTL_SWAP_ENABLE_P0                 , 49  , SH_UNT_MCA      , SH_ACS_SCOM_RW  ,
           0 );

REG64_FLD( MCBIST_MCBCFGQ_CFG_MCBIST_CFG_FORCE_PAUSE_AFTER_RANK    , 34  , SH_UNT_MCA      , SH_ACS_SCOM_RW  ,
           0 );

#endif