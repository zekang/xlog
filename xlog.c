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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_smart_str.h"
#include "ext/standard/php_var.h"
#include "php_xlog.h"
#include "common.h"
#include "redis.h"
#include "mail.h"


/* If you declare any globals in php_xlog.h uncomment this:
*/
ZEND_DECLARE_MODULE_GLOBALS(xlog)


/* True global resources - no need for thread safety here */
static int le_xlog;

/* {{{ xlog_functions[]
 *
 * Every user visible function must have an entry in xlog_functions[].
 */
const zend_function_entry xlog_functions[] = {
	PHP_FE(confirm_xlog_compiled,	NULL)		/* For testing, remove later. */
	PHP_FE_END	/* Must be the last line in xlog_functions[] */
};
/* }}} */


#ifdef COMPILE_DL_XLOG
ZEND_GET_MODULE(xlog)
#endif

/* {{{ PHP_INI
	*/
	/* Remove comments and fill if you need to have entries in php.ini
		*/
PHP_INI_BEGIN()
STD_PHP_INI_ENTRY("xlog.mail_smtp", "", PHP_INI_ALL, OnUpdateString, mail_smtp, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_port", "25", PHP_INI_ALL, OnUpdateLongGEZero, mail_port, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_username", "", PHP_INI_ALL, OnUpdateString, mail_username, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_password", "", PHP_INI_ALL, OnUpdateString, mail_password, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_ssl", "0", PHP_INI_ALL, OnUpdateBool, mail_ssl, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_from", "",PHP_INI_ALL,OnUpdateString,mail_from,zend_xlog_globals,xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_from_name", "", PHP_INI_ALL, OnUpdateString, mail_from_name, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_to", "", PHP_INI_ALL, OnUpdateString, mail_to, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.send_mail", "0", PHP_INI_ALL, OnUpdateBool, send_mail, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.send_mail_level", "",PHP_INI_ALL,OnUpdateString,send_mail_level,zend_xlog_globals,xlog_globals)
STD_PHP_INI_ENTRY("xlog.trace_error", "0", PHP_INI_ALL, OnUpdateBool, trace_error, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.trace_exception", "0", PHP_INI_ALL, OnUpdateBool, trace_exception, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.file_enable", "1", PHP_INI_ALL, OnUpdateBool, file_enable, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.redis_enable", "0", PHP_INI_ALL, OnUpdateBool, redis_enable, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.redis_host", "", PHP_INI_ALL, OnUpdateString, redis_host, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.redis_port", "6379", PHP_INI_ALL, OnUpdateLongGEZero, redis_port, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.redis_auth", "", PHP_INI_ALL, OnUpdateString, redis_auth, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.redis_db", "0", PHP_INI_ALL, OnUpdateLong, redis_db, zend_xlog_globals, xlog_globals)
PHP_INI_END()

/* }}} */

/* {{{ php_xlog_init_globals
 */
/* Uncomment this function if you have INI entries
*/
static void php_xlog_init_globals(zend_xlog_globals *xlog_globals)
{
	xlog_globals->redis = NULL;
}

/* }}} */

/** {{{ PHP_GINIT_FUNCTION
*/
PHP_GINIT_FUNCTION(xlog)
{
	XLOG_G(redis) = NULL;
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(xlog)
{
	/* If you have INI entries, uncomment these lines 
	*/
	REGISTER_INI_ENTRIES();
	init_error_hooks(TSRMLS_C);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(xlog)
{
	/* uncomment this line if you have INI entries
	*/
	UNREGISTER_INI_ENTRIES();
	restore_error_hooks(TSRMLS_C);
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(xlog)
{
	XLOG_G(redis) = NULL;
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(xlog)
{
	if (XLOG_G(redis) != NULL){
		php_stream_free(XLOG_G(redis), PHP_STREAM_FREE_CLOSE);
	}
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(xlog)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "xlog support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */



/* Remove the following function when you have successfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_xlog_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(confirm_xlog_compiled)
{
	/*
	char *command = NULL;
	int command_len = 0;
	php_stream *stream = XLOG_G(redis);
	if (stream == NULL){
		char *host = "127.0.0.1:6379";
		struct timeval tv = { 3, 0 };
		char *errorstr = NULL;
		int errono = 0;
		stream = php_stream_xport_create(
			host,
			strlen(host),
			ENFORCE_SAFE_MODE | REPORT_ERRORS,
			STREAM_XPORT_CLIENT | STREAM_XPORT_CONNECT,
			0,
			&tv,
			NULL,
			&errorstr,
			&errono
			);
		XLOG_G(redis) = stream;
		if (stream != NULL){
			command_len = build_redis_command(&command, "SELECT",6,"d",1);
			execute_redis_command(stream, NULL, command, command_len TSRMLS_CC);
		}
	}
	if (stream == NULL){
		php_printf("connect redis error\n");
		return;
	}
	zval *result;
	command_len = build_redis_command(&command, "GET",3, "s", "name",4);
	execute_redis_command(stream, &result, command, command_len TSRMLS_CC);
	RETURN_ZVAL(result, 0, 0);
	*/
	php_printf("%s\n", XLOG_G(mail_smtp));
}
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/
/* {{{ xlog_module_entry
*/
zend_module_entry xlog_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"xlog",
	xlog_functions,
	PHP_MINIT(xlog),
	PHP_MSHUTDOWN(xlog),
	PHP_RINIT(xlog),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(xlog),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(xlog),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_XLOG_VERSION,
#endif
	PHP_MODULE_GLOBALS(xlog),
	PHP_GINIT(xlog),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
