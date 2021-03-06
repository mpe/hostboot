/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/secureboot/base/securerommgr.C $                      */
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
#include <secureboot/service.H>
#include <secureboot/secure_reasoncodes.H>
#include <sys/mmio.h>
#include <kernel/pagemgr.H>
#include <limits.h>
#include <targeting/common/commontargeting.H>
#include <targeting/common/targetservice.H>
#include <devicefw/driverif.H>
#include <errl/errlentry.H>
#include <errl/errlmanager.H>
#include "../common/securetrace.H"
#include <kernel/bltohbdatamgr.H>

#include "securerommgr.H"
#include <secureboot/settings.H>
#include <config.h>
#include <console/consoleif.H>

// Quick change for unit testing
//#define TRACUCOMP(args...)  TRACFCOMP(args)
#define TRACUCOMP(args...)

namespace SECUREBOOT
{

/**
 * @brief Initialize Secure Rom by loading it into memory and
 *        retrieving Hash Keys
 */
errlHndl_t initializeSecureRomManager(void)
{
    return Singleton<SecureRomManager>::instance().initialize();
}

/**
 * @brief Verify Signed Container
 */
errlHndl_t verifyContainer(void * i_container, const sha2_hash_t* i_hwKeyHash)
{
    errlHndl_t l_errl = nullptr;

    // @TODO RTC:170136 remove isValid check
    if(Singleton<SecureRomManager>::instance().isValid())
    {
        l_errl = Singleton<SecureRomManager>::instance().
                                       verifyContainer(i_container,i_hwKeyHash);
    }

    return l_errl;
}

/**
 * @brief Hash Signed Blob
 *
 */
void hashBlob(const void * i_blob, size_t i_size, SHA512_t o_buf)
{
    // @TODO RTC:170136 remove isValid check
    if(Singleton<SecureRomManager>::instance().isValid())
    {
        return Singleton<SecureRomManager>::instance().
                                               hashBlob(i_blob, i_size, o_buf);
    }
}

/**
 * @brief Hash concatenation of 2 Blobs
 *
 */
void hashConcatBlobs(const blobPair_t &i_blobs, SHA512_t o_buf)
{
    // @TODO RTC:170136 remove isValid check
    if(Singleton<SecureRomManager>::instance().isValid())
    {
        return Singleton<SecureRomManager>::instance().
                                                hashConcatBlobs(i_blobs, o_buf);
    }
}

/*
 * @brief  Externally available hardware keys' hash retrieval function
 */
void getHwKeyHash(sha2_hash_t o_hash)
{
    // @TODO RTC:170136 remove isValid check
    if(Singleton<SecureRomManager>::instance().isValid())
    {
        return Singleton<SecureRomManager>::instance().getHwKeyHash(o_hash);
    }
}

}; //end SECUREBOOT namespace

/********************
 Public Methods
 ********************/

// allow external methods to access g_trac_secure
using namespace SECUREBOOT;

/**
 * @brief Initialize Secure Rom by loading it into memory and
 *        getting Hash Keys
 */
errlHndl_t SecureRomManager::initialize()
{
    TRACDCOMP(g_trac_secure,ENTER_MRK"SecureRomManager::initialize()");

    errlHndl_t l_errl = nullptr;
    uint32_t l_rc = 0;

    do{
        // @TODO RTC:170136 terminate in initialize if the securebit is on
        //       and code is not valid. Remove all isValid() checks in rest of
        //       SecureRomManager.
        // Check if secureboot data is valid.
        iv_secureromValid = g_BlToHbDataManager.isValid();
        if (!iv_secureromValid)
        {
            // The Secure ROM has already been initialized
            TRACFCOMP(g_trac_secure,"SecureRomManager::initialize(): SecureROM invalid, skipping functionality");

#ifdef CONFIG_CONSOLE
            CONSOLE::displayf(SECURE_COMP_NAME, "SecureROM invalid - skipping functionality");
#endif

            // Can skip the rest of this function
            break;
        }

        TRACFCOMP(g_trac_secure,"SecureRomManager::initialize(): SecureROM valid, enabling functionality");
#ifdef CONFIG_CONSOLE
        CONSOLE::displayf(SECURE_COMP_NAME, "SecureROM valid - enabling functionality");
#endif

        // Check to see if ROM has already been initialized
        if (iv_securerom != nullptr)
        {
            // The Secure ROM has already been initialized
            TRACUCOMP(g_trac_secure,"SecureRomManager::initialize(): Already "
                      "Loaded: iv_securerom=%p", iv_securerom);

            // Can skip the rest of this function
            break;
        }

        // ROM code starts at the end of the reserved page
        iv_securerom = g_BlToHbDataManager.getSecureRom();

        // invalidate icache to make sure that bootrom code in memory is used
        size_t l_icache_invalid_size = (g_BlToHbDataManager.getPreservedSize() /
                                        sizeof(uint64_t));

        mm_icache_invalidate(const_cast<void*>(iv_securerom),
                             l_icache_invalid_size);

        // Make this address space executable
        uint64_t l_access_type = EXECUTABLE;
        l_rc = mm_set_permission( const_cast<void*>(iv_securerom),
                                  g_BlToHbDataManager.getPreservedSize(),
                                  l_access_type);

        if (l_rc != 0)
        {
            TRACFCOMP(g_trac_secure,EXIT_MRK"SecureRomManager::initialize():"
            " Fail from mm_set_permission(EXECUTABLE): l_rc=0x%x, ptr=%p, "
            "size=0x%x, access=0x%x", l_rc, iv_securerom,
            g_BlToHbDataManager.getPreservedSize(), EXECUTABLE);

            /*@
             * @errortype
             * @moduleid     SECUREBOOT::MOD_SECURE_ROM_INIT
             * @reasoncode   SECUREBOOT::RC_SET_PERMISSION_FAIL_EXE
             * @userdata1    l_rc
             * @userdata2    iv_securerom
             * @devdesc      mm_set_permission(EXECUTABLE) failed for Secure ROM
             * @custdesc     A problem occurred during the IPL of the system.
             */
            l_errl = new ERRORLOG::ErrlEntry(
                                   ERRORLOG::ERRL_SEV_UNRECOVERABLE,
                                   SECUREBOOT::MOD_SECURE_ROM_INIT,
                                   SECUREBOOT::RC_SET_PERMISSION_FAIL_EXE,
                                   TO_UINT64(l_rc),
                                   reinterpret_cast<uint64_t>(iv_securerom),
                                   true /*Add HB Software Callout*/ );

            l_errl->collectTrace(SECURE_COMP_NAME,ERROR_TRACE_SIZE);
            break;

        }

        /***************************************************************/
        /*  Retrieve HW Hash Keys From The System                      */
        /***************************************************************/
        SecureRomManager::getHwKeyHash();

        TRACFCOMP(g_trac_secure,INFO_MRK"SecureRomManager::initialize(): SUCCESSFUL:"
        " iv_securerom=%p", iv_securerom);

    }while(0);

    TRACDCOMP(g_trac_secure,EXIT_MRK"SecureRomManager::initialize() - %s",
              ((nullptr == l_errl) ? "No Error" : "With Error") );

    return l_errl;
}

/**
 * @brief Verify Container against system hash keys
 */
errlHndl_t SecureRomManager::verifyContainer(void * i_container,
                                      const sha2_hash_t* i_hwKeyHash)
{
    TRACDCOMP(g_trac_secure,ENTER_MRK"SecureRomManager::verifyContainer(): "
              "i_container=%p", i_container);


    errlHndl_t  l_errl = nullptr;
    uint64_t    l_rc   = 0;

    do{

        // Check if secureboot data is valid.
        if (!iv_secureromValid)
        {
            // Can skip the rest of this function
            break;
        }

        // Check to see if ROM has already been initialized
        // This should have been done early in IPL so assert if this
        // is not the case as system is in a bad state
        assert(iv_securerom != nullptr);


        // Declare local input struct
        ROM_hw_params l_hw_parms;

        // Clear/zero-out the struct since we want 0 ('zero') values for
        // struct elements my_ecid, entry_point and log
        memset(&l_hw_parms, 0, sizeof(ROM_hw_params));

        // Now set hw_key_hash, which is of type sha2_hash_t, to iv_key_hash
        if (i_hwKeyHash == nullptr)
        {
            // Use current hw hash key
            memcpy (&l_hw_parms.hw_key_hash, iv_key_hash, sizeof(sha2_hash_t));
        }
        else
        {
            // Use custom hw hash key
            memcpy (&l_hw_parms.hw_key_hash, i_hwKeyHash, sizeof(sha2_hash_t));
        }

        /*******************************************************************/
        /* Call ROM_verify() function via an assembly call                 */
        /*******************************************************************/

        // Set startAddr to ROM_verify() function at an offset of Secure ROM
        uint64_t l_rom_verify_startAddr =
                                reinterpret_cast<uint64_t>(iv_securerom)
                                + g_BlToHbDataManager.getBranchtableOffset()
                                + ROM_VERIFY_FUNCTION_OFFSET;

        TRACUCOMP(g_trac_secure,"SecureRomManager::verifyContainer(): "
                  " Calling ROM_verify() via call_rom_verify: l_rc=0x%x, "
                  "l_hw_parms.log=0x%x (&l_hw_parms=%p) addr=%p (iv_d_p=%p)",
                  l_rc, l_hw_parms.log, &l_hw_parms, l_rom_verify_startAddr,
                 iv_securerom);

        ROM_container_raw* l_container = reinterpret_cast<ROM_container_raw*>(i_container);
        l_rc = call_rom_verify(reinterpret_cast<void*>
                               (l_rom_verify_startAddr),
                               l_container,
                               &l_hw_parms);


        TRACUCOMP(g_trac_secure,"SecureRomManager::verifyContainer(): "
                  "Back from ROM_verify() via call_rom_verify: l_rc=0x%x, "
                  "l_hw_parms.log=0x%x (&l_hw_parms=%p) addr=%p (iv_d_p=%p)",
                   l_rc, l_hw_parms.log, &l_hw_parms, l_rom_verify_startAddr,
                   iv_securerom);



        if (l_rc != 0)
        {
            TRACFCOMP(g_trac_secure,ERR_MRK"SecureRomManager::verifyContainer():"
            " ROM_verify() FAIL: l_rc=0x%x, l_hw_parms.log=0x%x "
            "addr=%p (iv_d_p=%p)", l_rc, l_hw_parms.log,
            l_rom_verify_startAddr, iv_securerom);

            /*@
             * @errortype
             * @severity     ERRL_SEV_UNRECOVERABLE
             * @moduleid     SECUREBOOT::MOD_SECURE_ROM_VERIFY
             * @reasoncode   SECUREBOOT::RC_ROM_VERIFY
             * @userdata1    l_rc
             * @userdata2    l_hw_parms.log
             * @devdesc      ROM_verify() Call Failed
             * @custdesc     Failure to verify authenticity of software.
             */
            l_errl = new ERRORLOG::ErrlEntry(ERRORLOG::ERRL_SEV_UNRECOVERABLE,
                                         SECUREBOOT::MOD_SECURE_ROM_VERIFY,
                                         SECUREBOOT::RC_ROM_VERIFY,
                                         l_rc,
                                         l_hw_parms.log,
                                         true /*Add HB Software Callout*/ );
            // Callout code to force a rewrite of the contents
            //@todo RTC:93870 - Define new callout for verification fail

            l_errl->collectTrace(SECURE_COMP_NAME,ERROR_TRACE_SIZE);
            break;

        }

    }while(0);


    TRACDCOMP(g_trac_secure,EXIT_MRK"SecureRomManager::verifyContainer() - %s",
             ((nullptr == l_errl) ? "No Error" : "With Error") );

    return l_errl;
}


/**
 * @brief Hash Blob
 */
void SecureRomManager::hashBlob(const void * i_blob, size_t i_size, SHA512_t o_buf) const
{

    TRACDCOMP(g_trac_secure,INFO_MRK"SecureRomManager::hashBlob()");

    // Check if secureboot data is valid.
    if (iv_secureromValid)
    {
        // Check to see if ROM has already been initialized
        // This should have been done early in IPL so assert if this
        // is not the case as system is in a bad state
        assert(iv_securerom != nullptr);

        // Set startAddr to ROM_SHA512() function at an offset of Secure ROM
        uint64_t l_rom_SHA512_startAddr =
                                    reinterpret_cast<uint64_t>(iv_securerom)
                                    + g_BlToHbDataManager.getBranchtableOffset()
                                    + SHA512_HASH_FUNCTION_OFFSET;

        call_rom_SHA512(reinterpret_cast<void*>(l_rom_SHA512_startAddr),
                        reinterpret_cast<const sha2_byte*>(i_blob),
                        i_size,
                        reinterpret_cast<sha2_hash_t*>(o_buf));

        TRACUCOMP(g_trac_secure,"SecureRomManager::hashBlob(): "
                  "call_rom_SHA512: blob=%p size=0x%X addr=%p (iv_d_p=%p)",
                   i_blob, i_size, l_rom_SHA512_startAddr,
                   iv_securerom);
    }

    TRACDCOMP(g_trac_secure,EXIT_MRK"SecureRomManager::hashBlob()");
}

/**
 * @brief Hash concatenation of N Blobs
 */
void SecureRomManager::hashConcatBlobs(const blobPair_t &i_blobs,
                                      SHA512_t o_buf) const
{
    // Check if secureboot data is valid.
    if (iv_secureromValid)
    {
        std::vector<uint8_t> concatBuf;
        for (const auto &it : i_blobs)
        {
            assert(it.first != nullptr, "BUG! In SecureRomManager::hashConcatBlobs(), "
                "User passed in nullptr blob pointer");
            const uint8_t* const blob =  static_cast<const uint8_t*>(it.first);
            const auto blobSize = it.second;
            concatBuf.insert(concatBuf.end(), blob, blob + blobSize);
        }

        // Call hash blob on new concatenated buffer
        hashBlob(concatBuf.data(),concatBuf.size(),o_buf);
    }
}

bool SecureRomManager::isValid()
{
    return iv_secureromValid;
}

/********************
 Internal Methods
 ********************/

/**
 * @brief Retrieves HW Keys from the system
 */
void SecureRomManager::getHwKeyHash()
{
    // Check if secureboot data is valid.
    if (iv_secureromValid)
    {
        iv_key_hash  = reinterpret_cast<const sha2_hash_t*>(
                                           g_BlToHbDataManager.getHwKeysHash());
    }
}

/**
 * @brief  Retrieve the internal hardware keys' hash from secure ROM object.
 */
void SecureRomManager::getHwKeyHash(sha2_hash_t o_hash)
{
    // Check if secureboot data is valid.
    if (iv_secureromValid)
    {
        memcpy(o_hash, iv_key_hash, sizeof(sha2_hash_t));
    }
}
