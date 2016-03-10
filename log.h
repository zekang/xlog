/*
+----------------------------------------------------------------------+
| PHP Version 5                                                        |
+----------------------------------------------------------------------+
| Copyright (c) 1997-2014 The PHP Group                                |
+----------------------------------------------------------------------+
| This source file is subject to version 3.01 of the PHP license,      |
| that is bundled with this package in the file LICENSE, and is        |
| available through the world-wide-web at the following url:           |
| http://www.php.net/license/3_01.txt                                  |
| If you did not receive a copy of the PHP license and are unable to   |
| obtain it through the world-wide-web, please send a note to          |
| license@php.net so we can mail you a copy immediately.               |
+----------------------------------------------------------------------+
| Author:  wanghouqian <whq654321@126.com>                             |
+----------------------------------------------------------------------+
*/
#ifndef LOG_H
#define LOG_H
#include "php.h"

#define XLOG_ALL                         "all"
#define XLOG_DEBUG                       "debug"
#define XLOG_INFO                        "info"
#define XLOG_NOTICE                      "notice"
#define XLOG_WARNING                     "warning"
#define XLOG_ERROR                       "error"
#define XLOG_CRITICAL                    "critical"
#define XLOG_ALERT                       "alert"
#define XLOG_EMERGENCY                   "emergency"


struct _log_item
{
	int level;
	time_t time ;
	char *app_name;
	char *msg;	
};
typedef struct _log_item LogItem;
int	 init_log(LogItem ***log, int size TSRMLS_DC);
int  add_log(LogItem **log, int index, int level, char *app_name, int app_name_len, char *msg, int msg_len TSRMLS_DC);
int  add_log_no_malloc_msg(LogItem **log, int index, int level, char *app_name, int app_name_len, char *msg TSRMLS_DC);
int  destory_log(LogItem ***log, int size TSRMLS_DC);
int  save_to_redis(char *level, char *errmsg TSRMLS_DC);
void save_to_mail(char *level, char *errmsg TSRMLS_DC);
void save_log(char *level, char *errmsg TSRMLS_DC);
#endif