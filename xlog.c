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
#define _CRT_SECURE_NO_WARNINGS
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
#include "log.h"

ZEND_DECLARE_MODULE_GLOBALS(xlog)


/* True global resources - no need for thread safety here */
static int le_xlog;

zend_class_entry *xlog_ce;

/* {{{ xlog_functions[]
 *
 */
const zend_function_entry xlog_functions[] = {
	PHP_FE(confirm_xlog_compiled,	NULL)		/* For testing, remove later. */
	PHP_FE_END	/* Must be the last line in xlog_functions[] */
};
/* }}} */

ZEND_METHOD(XLog, debug)
{

}




#ifdef COMPILE_DL_XLOG
ZEND_GET_MODULE(xlog)
#endif

/* {{{ PHP_INI
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


zend_function_entry xlog_methods[] = {
	PHP_FE_END
};

/* {{{ php_xlog_init_globals
 */
/* Uncomment this function if you have INI entries
*/
static void php_xlog_init_globals(zend_xlog_globals *xlog_globals)
{
	xlog_globals->redis = NULL;
	xlog_globals->log = NULL;
	xlog_globals->log_index = 0;
}

/* }}} */

/** {{{ PHP_GINIT_FUNCTION
*/
PHP_GINIT_FUNCTION(xlog)
{
	XLOG_G(redis) = NULL;
	XLOG_G(log) = NULL;
	XLOG_G(log_index) = 0;
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(xlog)
{
	zend_class_entry ce;
	INIT_CLASS_ENTRY(ce, "XLog", xlog_methods);
	xlog_ce = zend_register_internal_class(&ce TSRMLS_CC);

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

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(xlog)
{
	XLOG_G(redis) = NULL;
	if (XLOG_G(log_buffer) > 0){
		init_log(&XLOG_G(log), XLOG_G(log_buffer)  TSRMLS_CC);
	}
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(xlog)
{
	if (XLOG_G(redis) != NULL){
		php_stream_free(XLOG_G(redis), PHP_STREAM_FREE_CLOSE);
	}
	if (XLOG_G(log) != NULL){
		destory_log(&XLOG_G(log), XLOG_G(log_buffer) TSRMLS_CC);
	}
	XLOG_G(log_index) = 0;
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
	/*
	char *msg;
	int   msg_len;
	zval *context = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a", &msg, &msg_len, &context) == FAILURE){
		RETURN_FALSE;
	}
	if (context != NULL){
		char *ret;
		int ret_len;
		if (strtr_array(msg, msg_len, context, &ret, &ret_len TSRMLS_CC) == SUCCESS){
			RETURN_STRINGL(ret, ret_len, 0);
		}
	}
	RETURN_STRINGL(msg, msg_len, 1);
	*/

	LogItem **log = NULL;
	int size = 10;
	if (init_log(&log, size TSRMLS_CC) == FAILURE){
		return;
	}
	add_log(log, 0, 1, "club", 4, "club has error", sizeof("club has error") - 1 TSRMLS_CC);
	add_log(log, 1, 2, "xywy", 4, "club has 12345", sizeof("club has 12345") - 1 TSRMLS_CC);
	add_log(log, 2, 3, "wenk", 4, "club has 00000", sizeof("club has 00000") - 1 TSRMLS_CC);
	add_log(log, 3, 4, "abcd", 4, "club has aaaaa", sizeof("club has aaaaa") - 1 TSRMLS_CC);
	for (int i = 0; i < 10; i++){
		if (log[i] == NULL)
			continue;
		php_printf("level:%d,time=%d,app:%s,msg:%s\n", log[i]->level,log[i]->time, log[i]->app_name, log[i]->msg);
	}
	destory_log(&log, 10 TSRMLS_CC);
	php_printf("%s\n", get_log_level_name(XLOG_LEVEL_CRITICAL));
	time_t now = time(NULL);
	char buf[32] = { 0 };
	strftime(buf, 32, "%Y%m%d%H", localtime(&now));
	php_printf("%s\n", buf);

}
/* }}} */

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
	PHP_RINIT(xlog),		
	PHP_RSHUTDOWN(xlog),
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
