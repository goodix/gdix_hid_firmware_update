/*
 * @Author: your name
 * @Date: 2020-12-30 17:42:32
 * @LastEditTime: 2021-05-12 10:31:46
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: \gdix_hid_firmware_update-master\gtp_util.h
 */
/*
 * Copyright (C) 2017 Goodix Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _GT_UTIL_H_
#define _GT_UTIL_H_
#include <stdio.h>
#define E_HID_PKG_INDEX 500
#define E_HID_PKG_LEN	501

#define GDIX_UPDATE_DEBUG
// #define GDIX_DBG_ARRY

extern bool pdebug;

#ifdef GDIX_UPDATE_DEBUG
#define gdix_info(fmt, ...)                                                    \
	do {                                                                       \
		if (pdebug)                                                            \
			fprintf(stdout, "[GDIX_INF][%s:%d]" fmt, __func__, __LINE__,       \
					##__VA_ARGS__);                                            \
	} while (0)
#define gdix_err(fmt, ...)                                                     \
	do {                                                                       \
		if (pdebug)                                                            \
			fprintf(stderr, "[GDIX_ERR][%s:%d]" fmt, __func__, __LINE__,       \
					##__VA_ARGS__);                                            \
	} while (0)
#else
#define gdix_info(fmt, ...)                                                    \
	do {                                                                       \
	} while (0)
#define gdix_err(fmt, ...)                                                     \
	do {                                                                       \
	} while (0)
#endif

#ifdef GDIX_UPDATE_DEBUG
#define gdix_dbg(fmt, ...)                                                     \
	do {                                                                       \
		if (pdebug)                                                            \
			fprintf(stdout, "[GDIX_DBG][%s:%d]" fmt, __func__, __LINE__,       \
					##__VA_ARGS__);                                            \
	} while (0)
#else
#define gdix_dbg(fmt, ...)                                                     \
	do {                                                                       \
	} while (0)
#endif

#ifdef GDIX_DBG_ARRY
#define gdix_dbg_array(buf, len)                                               \
	do {                                                                       \
		int i;                                                                 \
		unsigned char *a = buf;                                                \
		fprintf(stdout, "[GDIX_DEBUG_ARRAY][%s:%d]\n", __func__, __LINE__);    \
		fprintf(stdout, "[GDIX_DEBUG]");                                       \
		for (i = 0; i < (len); i++) {                                          \
			fprintf(stdout, "%02x ", (a)[i]);                                  \
			if ((i + 1) % 16 == 0)                                             \
				fprintf(stdout, "\n[GDIX_DEBUG]");                             \
		}                                                                      \
		fprintf(stdout, "\n");                                                 \
	} while (0)
#else
#define gdix_dbg_array(buf, len)                                               \
	do {                                                                       \
	} while (0)
#endif
#endif
