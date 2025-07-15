/* SPDX-License-Identifier: BSD-2-Clause */
/***********************************************************************
* Copyright (c) 2025 - 2025, Huawei Technologies Co., Ltd.
 * All rights reserved.
 ***********************************************************************/
#ifdef HAVE_CONFIG_H
#include "config.h" // IWYU pragma: keep
#endif

#include <inttypes.h>         // for PRIx32
#include <stdlib.h>           // for exit
#include <string.h>           // for memcpy, strlen

#include "tss2_common.h"      // for TSS2_RC_SUCCESS, UINT32, TSS2_RC
#include "tss2_sys.h"         // for Tss2_Sys_FlushContext, Tss2_Sys_Create
#include "tss2_tpm2_types.h"  // for TPM2B_PUBLIC, TPMT_PUBLIC, TPM2B_ECC_POINT...

#define LOGMODULE test
#include "sys-util.h"         // for TSS2_RETRY_EXP
#include "test.h"             // for test_invoke
#include "util/log.h"         // for LOG_ERROR, LOG_INFO

#define EXIT_SKIP 77

/**
 * This program contains integration test for ECC asymmetric encrypt and
 * decrypt use case that has SYSs Tss2_Sys_CreatePrimary,
 * Tss2_Sys_Create, Tss2_Sys_Load, Tss2_Sys_ECC_Encrypt and
 * Tss2_Sys_ECC_Decrypt. First, the program creates the object and load
 * it in TPM. Then, it performs encryption based on the loaded
 * object. The object will be verified if it is encrypted.
 * If the verification is passed, it performs decryption and the
 * program will check if the decrypted value is the same as
 * the value before encryption.
 */
int
test_invoke (TSS2_SYS_CONTEXT *sys_context)
{
    TSS2_RC rc;
    TPM2B_SENSITIVE_CREATE  in_sensitive;
    TPM2B_PUBLIC            in_public = {0};
    TPM2B_DATA              outside_info = {0,};
    TPML_PCR_SELECTION      creation_pcr;
    TPM2B_NAME name = {sizeof(TPM2B_NAME)-2,};
    TPM2B_PRIVATE out_private = {sizeof(TPM2B_PRIVATE)-2,};
    TPM2B_PUBLIC out_public = {0,};
    TPM2B_CREATION_DATA creation_data = {0,};
    TPM2B_DIGEST creation_hash = {sizeof(TPM2B_DIGEST)-2,};
    TPMT_TK_CREATION creation_ticket = {0,};
    TPM2_HANDLE loaded_sym_handle;
    TPM2_HANDLE sym_handle;
    const char plainText[] = "plainText message";
    TPMT_KDF_SCHEME in_scheme;
    TPM2B_MAX_BUFFER input_plainText = {sizeof(TPM2B_MAX_BUFFER)-2,};
    TPM2B_MAX_BUFFER out_plainText = {sizeof(TPM2B_MAX_BUFFER)-2,};
    TPM2B_ECC_POINT c1 = {sizeof(TPM2B_ECC_POINT)-2,};
    TPM2B_MAX_BUFFER c2 = {sizeof(TPM2B_MAX_BUFFER)-2,};
    TPM2B_DIGEST c3 = {sizeof(TPM2B_DIGEST)-2,};

    TSS2L_SYS_AUTH_RESPONSE sessions_data_out;
    TSS2L_SYS_AUTH_COMMAND sessions_data = {
        .count = 1,
        .auths = {{.sessionHandle = TPM2_RH_PW,
            .nonce={.size=0},
            .hmac={.size=0}}}};

    in_sensitive.size = 0;
    in_sensitive.sensitive.userAuth.size = 0;
    in_sensitive.sensitive.data.size = 0;

    in_public.publicArea.type = TPM2_ALG_ECC;
    in_public.publicArea.nameAlg = TPM2_ALG_SHA256;
    *(UINT32 *)&(in_public.publicArea.objectAttributes) = 0;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_RESTRICTED;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_USERWITHAUTH;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_DECRYPT;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_FIXEDTPM;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_FIXEDPARENT;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_SENSITIVEDATAORIGIN;

    in_public.publicArea.authPolicy.size = 0;

    in_public.publicArea.parameters.eccDetail.symmetric.algorithm = TPM2_ALG_AES;
    in_public.publicArea.parameters.eccDetail.symmetric.keyBits.aes = 128;
    in_public.publicArea.parameters.eccDetail.symmetric.mode.aes = TPM2_ALG_CFB;
    in_public.publicArea.parameters.eccDetail.scheme.scheme = TPM2_ALG_NULL;
    in_public.publicArea.parameters.eccDetail.curveID = TPM2_ECC_NIST_P256;
    in_public.publicArea.parameters.eccDetail.kdf.scheme = TPM2_ALG_NULL;

    in_public.publicArea.unique.ecc.x.size = 0;
    in_public.publicArea.unique.ecc.y.size = 0;

    outside_info.size = 0;
    creation_pcr.count = 0;
    out_public.size = 0;
    creation_data.size = 0;

    LOG_INFO("ECC Asymmetric Encryption and Decryption Tests started.");
    rc = Tss2_Sys_CreatePrimary(sys_context, TPM2_RH_OWNER, &sessions_data, &in_sensitive, &in_public, &outside_info, &creation_pcr, &sym_handle, &out_public, &creation_data, &creation_hash, &creation_ticket, &name, &sessions_data_out);
    if (rc != TPM2_RC_SUCCESS) {
        LOG_ERROR("CreatePrimary FAILED! Response Code : 0x%x", rc);
        exit(1);
    }
    LOG_INFO("New ECC key successfully created.  Handle: 0x%8.8x", sym_handle);

    in_public.publicArea.type = TPM2_ALG_ECC;
    in_public.publicArea.parameters.eccDetail.symmetric.algorithm = TPM2_ALG_NULL;
    in_public.publicArea.parameters.eccDetail.scheme.scheme = TPM2_ALG_NULL;
    in_public.publicArea.parameters.eccDetail.curveID = TPM2_ECC_NIST_P256;
    in_public.publicArea.parameters.eccDetail.kdf.scheme = TPM2_ALG_NULL;
    in_public.publicArea.unique.ecc.x.size = 0;
    in_public.publicArea.unique.ecc.y.size = 0;

    /* First clear attributes bit field. */
    *(UINT32 *)&(in_public.publicArea.objectAttributes) = 0;
    in_public.publicArea.objectAttributes &= ~TPMA_OBJECT_RESTRICTED;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_USERWITHAUTH;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_DECRYPT;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_SIGN_ENCRYPT;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_FIXEDTPM;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_FIXEDPARENT;
    in_public.publicArea.objectAttributes |= TPMA_OBJECT_SENSITIVEDATAORIGIN;

    outside_info.size = 0;
    out_public.size = 0;
    creation_data.size = 0;
    sessions_data.auths[0].hmac.size = 0;

    rc = TSS2_RETRY_EXP (Tss2_Sys_Create(sys_context, sym_handle, &sessions_data, &in_sensitive, &in_public, &outside_info, &creation_pcr, &out_private, &out_public, &creation_data, &creation_hash, &creation_ticket, &sessions_data_out));
    if (rc != TPM2_RC_SUCCESS) {
        LOG_ERROR("Create FAILED! Response Code : 0x%x", rc);
        exit(1);
    }
    rc = Tss2_Sys_Load(sys_context, sym_handle, &sessions_data, &out_private, &out_public, &loaded_sym_handle, &name, &sessions_data_out);
    if (rc != TPM2_RC_SUCCESS) {
        LOG_ERROR("Load FAILED! Response Code : 0x%x", rc);
        exit(1);
    }
    LOG_INFO( "Loaded ECC key handle:  %8.8x", loaded_sym_handle );

    input_plainText.size = strlen(plainText);
    memcpy(input_plainText.buffer, plainText, input_plainText.size);
    in_scheme.scheme = TPM2_ALG_KDF2;
    in_scheme.details.kdf2.hashAlg = TPM2_ALG_SHA256;

    rc = Tss2_Sys_ECC_Encrypt(sys_context, loaded_sym_handle, 0, &input_plainText, &in_scheme, &c1, &c2, &c3, 0);
    goto_if_error(rc, "Error: ECC_Encrypt", error);

    LOG_INFO("ECC Encrypt successful.");

    rc = Tss2_Sys_ECC_Decrypt(sys_context, loaded_sym_handle, &sessions_data, &c1, &c2, &c3, &in_scheme, &out_plainText, &sessions_data_out);
    goto_if_error(rc, "Error: ECC_Decrypt", error);

    LOG_INFO("ECC Decrypt successful.");

    /* Verify decrypted message matches original */
    if (out_plainText.size != input_plainText.size ||
        memcmp(out_plainText.buffer, input_plainText.buffer, out_plainText.size) != 0) {
        LOG_ERROR("Decrypted message does not match original message!");
        exit(1);
    }
    LOG_INFO("Message verification successful.");

    LOG_INFO("ECC Asymmetric Encryption and Decryption Test Passed!");

    rc = Tss2_Sys_FlushContext(sys_context, sym_handle);
    if (rc != TSS2_RC_SUCCESS) {
        LOG_ERROR("Tss2_Sys_FlushContext failed with 0x%"PRIx32, rc);
        return 99; /* fatal error */
    }
    rc = Tss2_Sys_FlushContext(sys_context, loaded_sym_handle);
    if (rc != TSS2_RC_SUCCESS) {
        LOG_ERROR("Tss2_Sys_FlushContext failed with 0x%"PRIx32, rc);
        return 99; /* fatal error */
    }
    return 0;
error:
    (void)Tss2_Sys_FlushContext(sys_context, sym_handle);
    (void)Tss2_Sys_FlushContext(sys_context, loaded_sym_handle);
    /* If the TPM doesn't support it return skip */
    if ((rc == TPM2_RC_COMMAND_CODE) ||
        (rc == (TPM2_RC_COMMAND_CODE | TSS2_RESMGR_RC_LAYER)) ||
        (rc == (TPM2_RC_COMMAND_CODE | TSS2_RESMGR_TPM_RC_LAYER)))
        return EXIT_SKIP;
    else
        return EXIT_FAILURE;
}
