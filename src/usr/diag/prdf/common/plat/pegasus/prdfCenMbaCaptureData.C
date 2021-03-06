/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/diag/prdf/common/plat/pegasus/prdfCenMbaCaptureData.C $ */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2013,2017                        */
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

/**
 *  @file prdfCenMbaCaptureData.C
 *  @brief Utility Functions to capture MBA data
 */

#include <prdfCenMbaCaptureData.H>

// Framwork includes
#include <iipServiceDataCollector.h>
#include <prdfDramRepairUsrData.H>
#include <prdfErrlUtil.H>
#include <prdfExtensibleChip.H>
#include <prdfRasServices.H>
#include <utilmem.H>
#include <UtilHash.H>

// Pegasus includes
#include <prdfCenDqBitmap.H>
#include <prdfCenMarkstore.H>
#include <prdfCenMbaDataBundle.H>
#include <prdfCenMembufDataBundle.H>

using namespace TARGETING;

namespace PRDF
{

using namespace PlatServices;

namespace CenMbaCaptureData
{

//------------------------------------------------------------------------------

void addMemChipletFirRegs( ExtensibleChip * i_membChip, CaptureData & io_cd )
{
    #define PRDF_FUNC "[CenMbaCaptureData::addMemChipletFirRegs] "

    int32_t l_rc = SUCCESS;

    do
    {
        if ( NULL == i_membChip )
        {
            PRDF_ERR( PRDF_FUNC "Given target is NULL" );
            break;
        }

        if ( TYPE_MEMBUF != getTargetType(i_membChip->GetChipHandle()) )
        {
            PRDF_ERR( PRDF_FUNC "Invalid target type: i_membChip=0x%08x",
                      i_membChip->GetId() );
            break;
        }

        SCAN_COMM_REGISTER_CLASS * cs_global, * re_global, * spa_global;
        cs_global  = i_membChip->getRegister("GLOBAL_CS_FIR");
        re_global  = i_membChip->getRegister("GLOBAL_RE_FIR");
        spa_global = i_membChip->getRegister("GLOBAL_SPA");
        l_rc  = cs_global->Read() | re_global->Read() | spa_global->Read();
        if ( SUCCESS != l_rc )
        {
            PRDF_ERR( PRDF_FUNC "Failed to read a GLOBAL register on "
                      "0x%08x", i_membChip->GetId() );
            break;
        }

        // If global bit 3 is not on, can't scom mem chiplets or mba's
        if( ! (cs_global->IsBitSet(3) ||
               re_global->IsBitSet(3) ||
               spa_global->IsBitSet(3)) )
        {
            break;
        }

        i_membChip->CaptureErrorData(io_cd,
                                     Util::hashString("MemChipletRegs"));

        CenMembufDataBundle * membdb = getMembufDataBundle( i_membChip );

        for ( uint32_t i = 0; i < MAX_MBA_PER_MEMBUF; i++ )
        {
            ExtensibleChip * mbaChip = membdb->getMbaChip(i);
            if ( NULL == mbaChip )
            {
                PRDF_ERR( PRDF_FUNC "MEM_CHIPLET registers indicated an "
                          "attention but no chip found: i_membChip=0x%08x "
                          "i=%d", i_membChip->GetId(), i );
                continue;
            }

            mbaChip->CaptureErrorData(io_cd,
                                      Util::hashString("MemChipletRegs") );
        }

    } while (0);

    #undef PRDF_FUNC
}

//------------------------------------------------------------------------------

void addEccData( TargetHandle_t i_mbaTrgt, errlHndl_t io_errl )
{
    CaptureData cd;

    // Add DRAM repairs data from hardware.
    captureDramRepairsData( i_mbaTrgt, cd );

    // Add DRAM repairs data from VPD.
    captureDramRepairsVpd( i_mbaTrgt, cd );

    ErrDataService::AddCapData( cd, io_errl );
}

//------------------------------------------------------------------------------

void captureDramRepairsData( TARGETING::TargetHandle_t i_mbaTrgt,
                             CaptureData & io_cd )
{
    #define PRDF_FUNC "[CenMbaCaptureData::captureDramRepairsData] "
    using namespace fapi; // for spare config

    int32_t rc = SUCCESS;
    DramRepairMbaData mbaData;

    mbaData.header.isSpareDram = false;
    std::vector<CenRank> masterRanks;

    do
    {
        rc = getMasterRanks( i_mbaTrgt, masterRanks );
        if ( SUCCESS != rc )
        {
            PRDF_ERR( PRDF_FUNC "getMasterRanks() failed" );
            break;
        }
        if( masterRanks.empty() )
        {
            PRDF_ERR( PRDF_FUNC "Master Rank list size is 0");
            break;;
        }
        uint8_t spareConfig = ENUM_ATTR_VPD_DIMM_SPARE_NO_SPARE;
        // check for spare DRAM. Port does not matter.
        // Also this configuration is same for all ranks on MBA.
        rc = getDimmSpareConfig( i_mbaTrgt, masterRanks[0], 0, spareConfig );
        if( SUCCESS != rc )
        {
            PRDF_ERR( PRDF_FUNC "getDimmSpareConfig() failed" );
            break;
        }

        if( ENUM_ATTR_VPD_DIMM_SPARE_NO_SPARE != spareConfig )
            mbaData.header.isSpareDram = true;

        // Iterate all ranks to get DRAM repair data
        for ( std::vector<CenRank>::iterator it = masterRanks.begin();
              it != masterRanks.end(); it++ )
        {
            // Get chip/symbol marks
            CenMark mark;
            rc = mssGetMarkStore( i_mbaTrgt, *it, mark );
            if ( SUCCESS != rc )
            {
                PRDF_ERR( PRDF_FUNC "mssGetMarkStore() Failed");
                continue;
            }

            // Get DRAM spares
            CenSymbol sp0, sp1, ecc;
            rc = mssGetSteerMux( i_mbaTrgt, *it, sp0, sp1, ecc );
            if ( SUCCESS != rc )
            {
                PRDF_ERR( PRDF_FUNC "mssGetSteerMux() failed");
                continue;
            }

            // Add data
            DramRepairRankData rankData = { (*it).getMaster(),
                                            mark.getCM().getSymbol(),
                                            mark.getSM().getSymbol(),
                                            sp0.getSymbol(),
                                            sp1.getSymbol(),
                                            ecc.getSymbol() };
            if ( rankData.valid() )
            {
                mbaData.rankDataList.push_back(rankData);
            }
        }
        // If MBA had some DRAM repair data, add header information
        if( mbaData.rankDataList.size() > 0 )
        {
            mbaData.header.rankCount = mbaData.rankDataList.size();
            mbaData.header.isX4Dram =  isDramWidthX4( i_mbaTrgt );
            UtilMem dramStream;
            dramStream << mbaData;

            #ifndef PPC
            // Fix endianness issues with non PPC machines.
            // This is a workaround. Though UtilMem takes care of endianness,
            // It seems with capture data its not working
            const size_t sz_word = sizeof(uint32_t);

            // Align data with 32 bit boundary
            for (uint32_t i = 0; i < ( dramStream.size()%sz_word ); i++)
            {
                uint8_t dummy = 0;
                dramStream << dummy;
            }
            for ( uint32_t i = 0; i < ( dramStream.size()/sz_word); i++ )
            {
                ((uint32_t*)dramStream.base())[i] =
                            htonl(((uint32_t*)dramStream.base())[i]);
            }
            #endif

            // Allocate space for the capture data.
            BitString dramRepairData ( ( dramStream.size() )*8,
                                               (CPU_WORD *) dramStream.base() );
            io_cd.Add( i_mbaTrgt, Util::hashString("DRAM_REPAIRS_DATA"),
                       dramRepairData );
        }
    }while(0);

    if( FAIL == rc )
        PRDF_ERR( PRDF_FUNC "Failed for MBA 0x%08X", getHuid( i_mbaTrgt ) );

    #undef PRDF_FUNC
}

//------------------------------------------------------------------------------

void captureDramRepairsVpd( TargetHandle_t i_mbaTrgt, CaptureData & io_cd )
{
    #define PRDF_FUNC "[captureDramRepairsVpd] "

    // Get the maximum capture data size.
    static const size_t sz_rank  = sizeof(uint8_t);
    static const size_t sz_entry = MBA_DIMMS_PER_RANK * DIMM_DQ_RANK_BITMAP_SIZE;
    static const size_t sz_word  = sizeof(CPU_WORD);
    int32_t rc = SUCCESS;

    do
    {
        std::vector<CenRank> masterRanks;
        rc = getMasterRanks( i_mbaTrgt, masterRanks );
        if ( SUCCESS != rc )
        {
            PRDF_ERR( PRDF_FUNC "getMasterRanks() failed" );
            break;
        }

        if( masterRanks.empty() )
        {
            PRDF_ERR( PRDF_FUNC "Master Rank list size is 0");
            break;
        }

        // Get the maximum capture data size.
        size_t sz_maxData = masterRanks.size() * (sz_rank + sz_entry);

        // Adjust the size for endianness.
        sz_maxData = ((sz_maxData + sz_word-1) / sz_word) * sz_word;

        // Initialize to 0.
        uint8_t capData[sz_maxData];
        memset( capData, 0x00, sz_maxData );

        // Iterate all ranks to get VPD data
        uint32_t idx = 0;
        for ( std::vector<CenRank>::iterator it = masterRanks.begin();
              it != masterRanks.end(); it++ )
        {
            CenDqBitmap bitmap;
            uint8_t rank = it->getMaster();

            if ( SUCCESS != getBadDqBitmap(i_mbaTrgt, *it, bitmap, true) )
            {
                PRDF_ERR( PRDF_FUNC "getBadDqBitmap() failed: MBA=0x%08x"
                          " rank=%d", getHuid(i_mbaTrgt), rank );
                continue; // skip this rank
            }

            if ( bitmap.badDqs() ) // make sure the data is non-zero
            {
                // Add the rank, then the entry data.
                capData[idx] = rank;              idx += sz_rank;
                memcpy(&capData[idx], bitmap.getData(), sz_entry);
                idx += sz_entry;
            }
        }

        if( 0 == idx ) break; // Nothing to capture

        // Fix endianness issues with non PPC machines.
        size_t sz_capData = idx;
        sz_capData = ((sz_capData + sz_word-1) / sz_word) * sz_word;
        for ( uint32_t i = 0; i < (sz_capData/sz_word); i++ )
            ((CPU_WORD*)capData)[i] = htonl(((CPU_WORD*)capData)[i]);

        // Add data to capture data.
        BitString bs ( sz_capData*8, (CPU_WORD *) &capData );
        io_cd.Add( i_mbaTrgt, Util::hashString("DRAM_REPAIRS_VPD"), bs );

    }while(0);

    if( FAIL == rc )
        PRDF_ERR( PRDF_FUNC "Failed for MBA 0x%08X", getHuid( i_mbaTrgt ) );

    #undef PRDF_FUNC
}

//------------------------------------------------------------------------------

void addExtMemMruData( const MemoryMru & i_memMru, errlHndl_t io_errl )
{
    #define PRDF_FUNC "[addExtMemMruData] "

    MemoryMruData::ExtendedData extMemMru ( i_memMru.toUint32() );

    do
    {
        int32_t l_rc = SUCCESS;

        TargetHandle_t mbaTrgt = i_memMru.getMbaTrgt();

        // Get the DRAM width.
        extMemMru.isX4Dram = isDramWidthX4( mbaTrgt ) ? 1 : 0;

        // Get the DIMM type.
        bool isBufDimm = false;
        l_rc = isMembufOnDimm( mbaTrgt, isBufDimm );
        if ( SUCCESS != l_rc )
        {
            PRDF_ERR( PRDF_FUNC "isMembufOnDimm() failed. MBA:0x%08x",
                      getHuid(mbaTrgt) );
            break;
        }
        extMemMru.isBufDimm = isBufDimm ? 1 : 0;

        if ( isBufDimm )
        {
            // Get the raw card type (CDIMMS only).
            CEN_SYMBOL::WiringType cardType = CEN_SYMBOL::WIRING_INVALID;
            l_rc = getMemBufRawCardType( mbaTrgt, cardType );
            if ( SUCCESS != l_rc )
            {
                PRDF_ERR( PRDF_FUNC "getMemBufRawCardType() failed. MBA:0x%08x",
                          getHuid(mbaTrgt) );
                break;
            }
            extMemMru.cardType = cardType;
        }
        else
        {
            // Get the 80-byte DQ map (ISDIMMS only). This is only needed if
            // the MemoryMru contains a single DIMM callout with a valid symbol.
            if ( i_memMru.getSymbol().isValid() )
            {
                TargetHandleList partList = i_memMru.getCalloutList();
                if ( 1         != partList.size()           ||
                     TYPE_DIMM != getTargetType(partList[0]) )
                {
                    PRDF_ERR( PRDF_FUNC "Symbol is valid but callout is not a "
                              "single DIMM." );
                    break;
                }

                errlHndl_t l_errl = getFapiDimmDqAttr( partList[0],
                                                   &(extMemMru.dqMapping[0]) );
                if ( NULL != l_errl )
                {
                    PRDF_ERR( PRDF_FUNC "getFapiDimmDqAttr() failed. "
                              "DIMM:0x%08x", getHuid(partList[0]) );
                    PRDF_COMMIT_ERRL( l_errl, ERRL_ACTION_REPORT );
                    break;
                }
            }
        }

        // If we reach this point, nothing failed and the data is valid.
        extMemMru.isValid = 1;

    } while (0);

    size_t sz_buf = sizeof(extMemMru);
    uint8_t buf[sz_buf]; memset( buf, 0x00, sz_buf );

    buf[0] = (extMemMru.mmMeld.u >> 24) & 0xff;
    buf[1] = (extMemMru.mmMeld.u >> 16) & 0xff;
    buf[2] = (extMemMru.mmMeld.u >>  8) & 0xff;
    buf[3] =  extMemMru.mmMeld.u        & 0xff;

    buf[4] = extMemMru.cardType;

    buf[5] = extMemMru.isBufDimm << 7 |
             extMemMru.isX4Dram  << 6 |
             extMemMru.isValid   << 5;

    memcpy( &buf[8], &extMemMru.dqMapping[0], sizeof(extMemMru.dqMapping) );

    // Add the extended MemoryMru to the error log.
    PRDF_ADD_FFDC( io_errl, buf, sz_buf, ErrlVer1, ErrlMruData );

    #undef PRDF_FUNC
}

//------------------------------------------------------------------------------

}  //end namespace MbaCaptureData

} // end namespace PRDF


