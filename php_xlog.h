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
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_XLOG_H
#define PHP_XLOG_H

extern zend_module_entry xlog_module_entry;
#define phpext_xlog_ptr &xlog_module_entry

#define PHP_XLOG_VERSION "0.1.0" /* Replace with version number for your extension */


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

PHP_FUNCTION(confirm_xlog_compiled);	/* For testing, remove later. */

/* 
  	Declare any global variables you may need between the BEGIN
	and END macros here:     
	*/
ZEND_BEGIN_MODULE_GLOBALS(xlog)
	php_stream *redis;
	char *mail_smtp;
	int   mail_port;
	char *mail_username;
	char *mail_password;
	char *mail_from;
	char *mail_from_name;
	char *mail_to;
	zend_bool	send_mail;
	char *send_mail_level;
	zend_bool   mail_ssl;
	zend_bool   trace_error;
	zend_bool   trace_exception;
	zend_bool   file_enable;
	zend_bool   redis_enable;
	char  *redis_host;
	int    redis_port;
	char  *redis_auth;
	int	   redis_db;
ZEND_END_MODULE_GLOBALS(xlog)


/* In every utility function you add that needs to use variables
in php_xlog_globals, call TSRMLS_FETCH(); after declaring other
variables used by that function, or better yet, pass in TSRMLS_CC
after the last function argument and declare your utility function
with TSRMLS_DC after the last declared argument.  Always refer to
the globals in your function as XLOG_G(variable).  You are
encouraged to rename these macros something shorter, see
examples in any other php module directory.
*/

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
