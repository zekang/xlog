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
/* $Id$ */

#ifndef PHP_XLOG_H
#define PHP_XLOG_H
#include "log.h"
extern zend_module_entry xlog_module_entry;
#define phpext_xlog_ptr &xlog_module_entry

#define PHP_XLOG_VERSION "0.1.0" 
#ifdef WINDOWS
#define XLOG_PATH_PATTERN "abcdefghijklmnopqrstuvwzxyABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.\\/:"
#else
#define XLOG_PATH_PATTERN "abcdefghijklmnopqrstuvwzxyABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.\\/"
#endif
#define XLOG_FILE_PATTERN "abcdefghijklmnopqrstuvwzxyABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-."

#define XLOG_INI_DEFAULT_PATH "/tmp"
#define XLOG_INI_DEFAULT_PATH_LEN 4

#define XLOG_INI_DEFAULT_APPLICATION "front"
#define XLOG_INI_DEFAULT_APPLICATION_LEN 5

#define XLOG_INI_DEFAULT_MODULE "default"
#define XLOG_INI_DEFAULT_MODULE_LEN 7

#define XLOG_INI_DEFAULT_HOST  "localhost"
#define XLOG_INI_DEFAULT_HOST_LEN 9

#define XLOG_INI_DEFALUT_EMPTY ""
#define XLOG_INI_DEFALUT_EMPTY_LEN 0



#ifdef PHP_WIN32
#	define PHP_XLOG_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_XLOG_API __attribute__ ((visibility("default")))
#else
#	define PHP_XLOG_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(xlog);
PHP_MSHUTDOWN_FUNCTION(xlog);
PHP_RINIT_FUNCTION(xlog);
PHP_RSHUTDOWN_FUNCTION(xlog);
PHP_MINFO_FUNCTION(xlog);


/*Declare  global variables*/

ZEND_BEGIN_MODULE_GLOBALS(xlog)
	time_t redis_fail_time;
	time_t mail_fail_time;

	php_stream	 *redis;
	LogItem **log;
	HashTable *file_handle;
	char  *application;
	char  *module;
	char  *path;
	int	  index;

	zend_bool   trace_error;
	zend_bool   trace_exception;

	zend_bool	mail_enable;
	int			mail_level;
	zend_bool   mail_ssl;
	char *mail_smtp;
	int   mail_port;
	char *mail_username;
	char *mail_password;
	char *mail_from;
	char *mail_from_name;
	char *mail_to;
	zend_bool mail_backtrace_args;
	int   mail_retry_interval;

	zend_bool   redis_enable;
	char  *redis_host;
	int    redis_port;
	char  *redis_auth;
	int	   redis_db;
	int    redis_retry_interval;

	char  *host;
	char  *default_application;
	char  *default_module;
	int	   buffer;
	zend_bool   buffer_enable;
	zend_bool   file_enable;
	char  *default_path;

	int   level;
ZEND_END_MODULE_GLOBALS(xlog)

#ifdef ZTS
#define XLOG_G(v) TSRMG(xlog_globals_id, zend_xlog_globals *, v)
#else
#define XLOG_G(v) (xlog_globals.v)
#endif
extern ZEND_DECLARE_MODULE_GLOBALS(xlog);
#endif	/* PHP_XLOG_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
