/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/import/chips/p9/procedures/hwp/nest/p9_chiplet_scominit.H $ */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2015,2016                        */
/* [+] International Business Machines Corp.                              */
/*                                                                        */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/*                                                                        */
/* IBM_PROLOG_END_TAG                                                     */
///
/// @file p9_chiplet_scominit.H
///
/// @brief SCOM inits to all chiplets (sans Quad)
///

//
// *HWP HW Owner : Joe McGill <jmcgill@us.ibm.com>
// *HWP FW Owner : Thi N. Tran <thi@us.ibm.com>
// *HWP Team : Nest
// *HWP Level : 2
// *HWP Consumed by : HB
//

#ifndef _P9_CHIPLET_SCOMINIT_H_
#define _P9_CHIPLET_SCOMINIT_H_

//------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------
#include <fapi2.H>

//------------------------------------------------------------------------------
// Structure definitions
//------------------------------------------------------------------------------

typedef fapi2::ReturnCode (*p9_chiplet_scominit_FP_t)(const fapi2::Target<fapi2::TARGET_TYPE_PROC_CHIP>&);

//------------------------------------------------------------------------------
// Function prototypes
//------------------------------------------------------------------------------

extern "C"
{

/// @brief SCOM inits to all chiplets (sans Quad)
///
/// @param[in] i_target Reference to processor chip target
/// @return fapi2::ReturnCode. FAPI2_RC_SUCCESS if success, else error code.
    fapi2::ReturnCode p9_chiplet_scominit(const fapi2::Target<fapi2::TARGET_TYPE_PROC_CHIP>& i_target);

} // extern "C"

#endif // _P9_CHIPLET_SCOMINIT_H_

