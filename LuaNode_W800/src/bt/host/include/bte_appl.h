/******************************************************************************
 *
 *  Copyright (C) 2002-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/******************************************************************************
 *
 *  This is the interface file for the bte application task
 *
 ******************************************************************************/

#pragma once

typedef struct
{
    #if ((BLE_INCLUDED == TRUE) && (SMP_INCLUDED == TRUE))
    uint8_t   ble_auth_req;
    uint8_t   ble_io_cap;
    uint8_t   ble_init_key;
    uint8_t   ble_resp_key;
    uint8_t   ble_max_key_size;
    #endif
} tBTE_APPL_CFG;

extern tBTE_APPL_CFG bte_appl_cfg;
