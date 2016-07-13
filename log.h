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


#ifdef WINDOWS
#define XLOG_DIRECTORY_SEPARATOR '\\'
#else
#define XLOG_DIRECTORY_SEPARATOR '/'
#endif

#define XLOG_ALL                         "all"
#define XLOG_DEBUG                       "debug"
#define XLOG_INFO                        "info"
#define XLOG_NOTICE                      "notice"
#define XLOG_WARNING                     "warning"
#define XLOG_ERROR                       "error"
#define XLOG_CRITICAL                    "critical"
#define XLOG_ALERT                       "alert"
#define XLOG_EMERGENCY                   "emergency"
#define XLOG_WITH_STACKINFO			     "stack_info"		
#define XLOG_ELAPSE_TIME			 "elapse_time"	


#define XLOG_LEVEL_ALL					 0
#define XLOG_LEVEL_DEBUG				 1
#define XLOG_LEVEL_INFO					 2
#define XLOG_LEVEL_NOTICE				 3
#define XLOG_LEVEL_WARNING				 4
#define XLOG_LEVEL_ERROR			     5
#define XLOG_LEVEL_CRITICAL				 6
#define XLOG_LEVEL_ALERT				 7
#define XLOG_LEVEL_EMERGENCY			 8
#define XLOG_LEVEL_WITH_STACKINFO        9
#define XLOG_LEVEL_ELAPSE_TIME			 10

#define XLOG_FLAG_NO_SEND_MAIL           0
#define XLOG_FLAG_SEND_MAIL				 1

#define XLOG_LEVEL_MIN  0
#define XLOG_LEVEL_USER_MAX  8
#define XLOG_LEVEL_MAX  10

#define CHECK_AND_SET_VALUE_IF_NULL(var,var_len,first,default)  \
if(var == NULL){ \
	var = XLOG_G(first) == NULL ? XLOG_G(default) : XLOG_G(first); \
	if (var_len == 0){ var_len = strlen(var); } \
}

struct _log_item
{
	time_t time ;
	char *module;
	char *content;	
	short level;
	short flag;
};
typedef struct _log_item LogItem;

struct _file_handle_cache
{
	int level;
	char *filename;
	int write_count;
	php_stream *stream;
};
typedef struct _file_handle_cache FileHandleCache;

int			init_log(LogItem ***log, int size TSRMLS_DC);
int			add_log(LogItem **log, int index, short level, char *module, int module_len, char *content, int content_len, short flag TSRMLS_DC);
int			add_log_no_malloc_msg(LogItem **log, int index, short level, char *module, int module_len, char *content, short flag  TSRMLS_DC);
int			check_if_need_reset(LogItem **log, int *index TSRMLS_DC);
int			log_free_item(LogItem **log);
int			destory_log(LogItem ***log, int size TSRMLS_DC);
int			save_to_redis(int level, char *module, char *content TSRMLS_DC);
int			save_to_redis_with_model(int level,  char *module, char *content TSRMLS_DC);
void		save_to_mail(int level,  char *module, char *content TSRMLS_DC);
void		save_to_file(int level,char *module, char *content, int content_len TSRMLS_DC);
void		save_log_no_buffer(int level, char* module, char *content, short flag TSRMLS_DC);
void		save_log_with_buffer(LogItem **log TSRMLS_DC);
char*		get_log_level_name(int level);
php_stream *get_file_handle_from_cache(int level, char *module TSRMLS_DC);
void		file_handle_cache_ptr_dtor_wapper(FileHandleCache **cache);
zend_bool	rotate_file(const char *filename, int filename_len, int max TSRMLS_DC);
#endif
