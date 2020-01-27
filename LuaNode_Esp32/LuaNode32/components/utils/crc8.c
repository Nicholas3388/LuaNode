/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*
 * Table computation:
 *
 * void
 * gen_small_table(uint8_t poly)
 * {
 *      int i;
 *	int j;
 *	uint8_t curr;
 *
 *	for (i = 0; i < 16; i++) {
 *		curr = i;
 *
 *		for (j = 0; j < 8; j++)  {
 *			if ((curr & 0x80) != 0) {
 *				curr = (curr << 1) ^ poly;
 *			} else {
 *				curr <<= 1;
 *			}
 *		}
 *
 *		small_table[i] = curr;
 *
 *		printf("0x%x, ", small_table[i]);
 *	}
 *	printf("\n");
 *}
 */

#include "crc8.h"

static uint8_t crc8_small_table[16] = {0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15,
                                       0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d};

uint8_t crc8_init(void) { return 0xff; }

uint8_t crc8_calc(uint8_t val, void *buf, int cnt)
{
    int i;
    uint8_t *p = buf;

    for (i = 0; i < cnt; i++)
    {
        val ^= p[i];
        val = (val << 4) ^ crc8_small_table[val >> 4];
        val = (val << 4) ^ crc8_small_table[val >> 4];
    }
    return val;
}
