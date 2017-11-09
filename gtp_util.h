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

#define E_HID_PKG_INDEX 500
#define E_HID_PKG_LEN	501

#define GDIX_UPDATE_DEBUG
#define GDIX_DBG_ARRY

#define gdix_info(fmt, arg...)  fprintf(stdout, "[GDIX_INFO][%s:%d]" fmt, __func__, __LINE__, ##arg)
#define gdix_err(fmt, arg...)  fprintf(stderr, "[GDIX_ERROR][%s:%d]" fmt, __func__, __LINE__, ##arg)

#ifdef GDIX_UPDATE_DEBUG
#define gdix_dbg(fmt, arg...)  fprintf(stdout, "[GDIX_DEBUG][%s:%d]" fmt, __func__, __LINE__, ##arg)
#else
#define gdix_dbg(fmt, arg...) do{}while(0)
#endif

#ifdef GDIX_DBG_ARRY
#define gdix_dbg_array(buf, len) do{\
		int i;\
		unsigned char *a = buf;\
		fprintf(stdout,"[GDIX_DEBUG_ARRAY][%s:%d]\n",__func__, __LINE__);\
		fprintf(stdout,"[GDIX_DEBUG]");\
		for (i = 0; i< (len); i++) {\
			fprintf(stdout, "%02x ", (a)[i]);\
			if ((i + 1) % 16 == 0)\
				fprintf(stdout,"\n[GDIX_DEBUG]");\
		}\
		fprintf(stdout,"\n");\
	} while(0)
#else
#define gdix_dbg_array(fmt, arg...) do {} while(0)
#endif
#endif
