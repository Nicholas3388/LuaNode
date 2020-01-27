/**
 * Config for easy_mem
 *
 * Nicholas3388
 */

#ifndef _EASY_CONF_H_
#define _EASY_CONF_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENABLE_DEBUG
//#define HASH_TABLE_CAN_EXTEND
#ifdef HASH_TABLE_CAN_EXTEND
#define EXTEND_THRESHOLD  3
#endif

#define DEBUG_PRINT     printf
#define ERROR_PRINT     printf

#define EASY_OS_VERSION   "1.0.1"

#define STR(x)                  VAL(x)
#define VAL(x)                  #x
#define easy_assert(x)          /*((x) ? (void)0 : ERROR_PRINT(__FILE__ ":" STR(__LINE__) " " #x"\n"))*/

#ifdef __cplusplus
}
#endif

#endif // _EASY_CONF_H_
