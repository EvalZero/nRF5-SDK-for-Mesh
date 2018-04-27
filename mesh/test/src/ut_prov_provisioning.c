/* Copyright (c) 2010 - 2017, Nordic Semiconductor ASA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <uECC.h>
#include <unity.h>

#include "event.h"
#include "log.h"
#include "packet_mgr.h"
#include "prov_pdu.h"
#include "rand.h"
#include "timer.h"
#include "utils.h"

#include "nrf_mesh_prov.h"

#include "prov_provisionee.h"
#include "prov_provisioner.h"

/* Provisioner public key in little endian format: */
#define PROVISIONER_PUBKEY_LE \
    { \
        0x2c, 0x31, 0xa4, 0x7b, 0x57, 0x79, 0x80, 0x9e, 0xf4, 0x4c, 0xb5, 0xea, 0xaf, 0x5c, 0x3e, 0x43, \
        0xd5, 0xf8, 0xfa, 0xad, 0x4a, 0x87, 0x94, 0xcb, 0x98, 0x7e, 0x9b, 0x03, 0x74, 0x5c, 0x78, 0xdd, \
        0x91, 0x95, 0x12, 0x18, 0x38, 0x98, 0xdf, 0xbe, 0xcd, 0x52, 0xe2, 0x40, 0x8e, 0x43, 0x87, 0x1f, \
        0xd0, 0x21, 0x10, 0x91, 0x17, 0xbd, 0x3e, 0xd4, 0xea, 0xf8, 0x43, 0x77, 0x43, 0x71, 0x5d, 0x4f  \
    }
/* Provisioner private key in little endian format: */
#define PROVISIONER_PRIVKEY_LE \
    { \
        0x06, 0xa5, 0x16, 0x69, 0x3c, 0x9a, 0xa3, 0x1a, 0x60, 0x84, 0x54, 0x5d, 0x0c, 0x5d, 0xb6, 0x41, \
        0xb4, 0x85, 0x72, 0xb9, 0x72, 0x03, 0xdd, 0xff, 0xb7, 0xac, 0x73, 0xf7, 0xd0, 0x45, 0x76, 0x63  \
    }

/* Provisionee public key in little endian format: */
#define PROVISIONEE_PUBKEY_LE \
    { \
        0xf4, 0x65, 0xe4, 0x3f, 0xf2, 0x3d, 0x3f, 0x1b, 0x9d, 0xc7, 0xdf, 0xc0, 0x4d, 0xa8, 0x75, 0x81, \
        0x84, 0xdb, 0xc9, 0x66, 0x20, 0x47, 0x96, 0xec, 0xcf, 0x0d, 0x6c, 0xf5, 0xe1, 0x65, 0x00, 0xcc, \
        0x02, 0x01, 0xd0, 0x48, 0xbc, 0xbb, 0xd8, 0x99, 0xee, 0xef, 0xc4, 0x24, 0x16, 0x4e, 0x33, 0xc2, \
        0x01, 0xc2, 0xb0, 0x10, 0xca, 0x6b, 0x4d, 0x43, 0xa8, 0xa1, 0x55, 0xca, 0xd8, 0xec, 0xb2, 0x79  \
    }
/* Provisionee private key in little enian format: */
#define PROVISIONEE_PRIVKEY_LE \
    { \
        0x52, 0x9a, 0xa0, 0x67, 0x0d, 0x72, 0xcd, 0x64, 0x97, 0x50, 0x2e, 0xd4, 0x73, 0x50, 0x2b, 0x03, \
        0x7e, 0x88, 0x03, 0xb5, 0xc6, 0x08, 0x29, 0xa5, 0xa3, 0xca, 0xa2, 0x19, 0x50, 0x55, 0x30, 0xba  \
    }

/* Shared secret derived from the keys. */
#define SHARED_SECRET_LE \
    { \
        0xab, 0x85, 0x84, 0x3a, 0x2f, 0x6d, 0x88, 0x3f, 0x62, 0xe5, 0x68, 0x4b, 0x38, 0xe3, 0x07, 0x33, \
        0x5f, 0xe6, 0xe1, 0x94, 0x5e, 0xcd, 0x19, 0x60, 0x41, 0x05, 0xc6, 0xf2, 0x32, 0x21, 0xeb, 0x69  \
    }

/* Random numbers for test_provisioning(): */
#define TEST_PROVISIONING_RANDOM_NUMBERS \
    { \
        0x8b, 0x19, 0xac, 0x31, 0xd5, 0x8b, 0x12, 0x4c, 0x94, 0x62, 0x09, 0xb5, 0xdb, 0x10, 0x21, 0xb9, /* Provisioner random number */ \
        0x55, 0xa2, 0xa2, 0xbc, 0xa0, 0x4c, 0xd3, 0x2f, 0xf6, 0xf3, 0x46, 0xbd, 0x0a, 0x0c, 0x1a, 0x3a  /* Provisionee random number */ \
    }

/* Sample messages from the Mesh Profile Specification v1.0: */
#define TEST_INVITE_PDU { 0x00, 0x00 }
#define TEST_CAPABILITIES_PDU { 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
#define TEST_START_PDU { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 }
#define TEST_PUBKEY_PDU_PROVISIONER { 0x03, \
    0x2c, 0x31, 0xa4, 0x7b, 0x57, 0x79, 0x80, 0x9e, 0xf4, 0x4c, 0xb5, 0xea, 0xaf, 0x5c, 0x3e, 0x43, \
    0xd5, 0xf8, 0xfa, 0xad, 0x4a, 0x87, 0x94, 0xcb, 0x98, 0x7e, 0x9b, 0x03, 0x74, 0x5c, 0x78, 0xdd, \
    0x91, 0x95, 0x12, 0x18, 0x38, 0x98, 0xdf, 0xbe, 0xcd, 0x52, 0xe2, 0x40, 0x8e, 0x43, 0x87, 0x1f, \
    0xd0, 0x21, 0x10, 0x91, 0x17, 0xbd, 0x3e, 0xd4, 0xea, 0xf8, 0x43, 0x77, 0x43, 0x71, 0x5d, 0x4f }
#define TEST_PUBKEY_PDU_PROVISIONEE { 0x03, \
    0xf4, 0x65, 0xe4, 0x3f, 0xf2, 0x3d, 0x3f, 0x1b, 0x9d, 0xc7, 0xdf, 0xc0, 0x4d, 0xa8, 0x75, 0x81, \
    0x84, 0xdb, 0xc9, 0x66, 0x20, 0x47, 0x96, 0xec, 0xcf, 0x0d, 0x6c, 0xf5, 0xe1, 0x65, 0x00, 0xcc, \
    0x02, 0x01, 0xd0, 0x48, 0xbc, 0xbb, 0xd8, 0x99, 0xee, 0xef, 0xc4, 0x24, 0x16, 0x4e, 0x33, 0xc2, \
    0x01, 0xc2, 0xb0, 0x10, 0xca, 0x6b, 0x4d, 0x43, 0xa8, 0xa1, 0x55, 0xca, 0xd8, 0xec, 0xb2, 0x79 }
#define TEST_CONFIRMATION_PDU_PROVISIONER { 0x05, 0xb3, 0x8a, 0x11, 0x4d, 0xfd, 0xca, 0x1f, 0xe1, 0x53, 0xbd, 0x2c, 0x1e, 0x0d, 0xc4, 0x6a, 0xc2 }
#define TEST_CONFIRMATION_PDU_PROVISIONEE { 0x05, 0xee, 0xba, 0x52, 0x1c, 0x19, 0x6b, 0x52, 0xcc, 0x2e, 0x37, 0xaa, 0x40, 0x32, 0x9f, 0x55, 0x4e }
#define TEST_RANDOM_PDU_PROVISIONER { 0x06, 0x8b, 0x19, 0xac, 0x31, 0xd5, 0x8b, 0x12, 0x4c, 0x94, 0x62, 0x09, 0xb5, 0xdb, 0x10, 0x21, 0xb9 }
#define TEST_RANDOM_PDU_PROVISIONEE { 0x06, 0x55, 0xa2, 0xa2, 0xbc, 0xa0, 0x4c, 0xd3, 0x2f, 0xf6, 0xf3, 0x46, 0xbd, 0x0a, 0x0c, 0x1a, 0x3a }
#define TEST_DATA_PDU { \
    0x07, 0xd0, 0xbd, 0x7f, 0x4a, 0x89, 0xa2, 0xff, 0x62, 0x22, 0xaf, 0x59, 0xa9, 0x0a, 0x60, 0xad, 0x58, \
    0xac, 0xfe, 0x31, 0x23, 0x35, 0x6f, 0x5c, 0xec, 0x29, 0x73, 0xe0, 0xec, 0x50, 0x78, 0x3b, 0x10, 0xc7 }
#define TEST_COMPLETE_PDU { 0x08 }

/* Sample data from the Mesh Profile Specification v1.0: */
#define DATA_NETKEY  { 0xef, 0xb2, 0x25, 0x5e, 0x64, 0x22, 0xd3, 0x30, 0x08, 0x8e, 0x09, 0xbb, 0x01, 0x5e, 0xd7, 0x07 }
#define DATA_NETKEY_INDEX 0x0567
#define DATA_FLAGS_IV_UPDATE 0
#define DATA_FLAGS_KEY_REFRESH 0
#define DATA_IVINDEX 0x01020304
#define DATA_ADDRESS 0x0b0c

#define PROVISIONEE_NUM_COMPONENTS 1

/********** Mesh Assertion Handling **********/
nrf_mesh_assertion_handler_t m_assertion_handler;
void nrf_mesh_assertion_handler(uint32_t pc)
{
    TEST_FAIL_MESSAGE("Mesh assertion triggered");
}


/********** Test Initialization and Finalization ***********/

void setUp(void)
{
}

void tearDown(void)
{

}

/********** Test Cases **********/

void test_provisioning()
{
    TEST_IGNORE_MESSAGE("UNIMPLEMENTED TEST");
}
