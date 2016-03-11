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

ZEND_BEGIN_ARG_INFO_EX(arg_info_log_common, 0, 0, 2)
ZEND_ARG_INFO(0, msg)
ZEND_ARG_ARRAY_INFO(0, context, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arg_info_log, 0, 0, 3)
ZEND_ARG_INFO(0, level)
ZEND_ARG_INFO(0, msg)
ZEND_ARG_ARRAY_INFO(0, context, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arg_info_void, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arg_info_set_base_path, 0, 0, 1)
ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO_EX(arg_info_set_logger, 0, 0, 1)
ZEND_ARG_INFO(0, logger)
ZEND_END_ARG_INFO();


/* {{{ xlog_functions[]
 *
 */
const zend_function_entry xlog_functions[] = {
	PHP_FE_END	/* Must be the last line in xlog_functions[] */
};
/* }}} */

static void process_log_no_context(int level,char *msg,int msg_len TSRMLS_DC)
{
	if (XLOG_G(log_buffer) > 0){
		if (add_log(XLOG_G(log), XLOG_G(log_index), level, NULL, 0, msg, msg_len TSRMLS_CC) == SUCCESS){
			XLOG_G(log_index)++;
		}
	}
	else{
		save_log_no_buffer(level, NULL, msg TSRMLS_CC);
	}
}

static void process_log_with_context(int level, char *msg, int msg_len, zval *context TSRMLS_DC)
{
	char *ret = NULL;
	if (strtr_array(msg, msg_len, context, &ret, NULL TSRMLS_CC) == SUCCESS){
		if (XLOG_G(log_buffer) > 0){
			if (add_log_no_malloc_msg(XLOG_G(log), XLOG_G(log_index), level, NULL, 0, ret TSRMLS_CC) == SUCCESS){
				XLOG_G(log_index)++;
			}
		}
		else{
			save_log_no_buffer(level, NULL, ret TSRMLS_CC);
			efree(ret);
		}
	}
}

static void process_log(INTERNAL_FUNCTION_PARAMETERS,int level)
{
	char *msg;
	int msg_len;
	zval *context = NULL;
	int argc = ZEND_NUM_ARGS();
	if (zend_parse_parameters(argc TSRMLS_CC, "s|a", &msg, &msg_len, &context) == FAILURE){
		RETURN_FALSE;
	}
	if (argc == 2 && context != NULL){
		process_log_with_context(level, msg, msg_len, context TSRMLS_CC);
	}
	else{
		process_log_no_context(level, msg, msg_len TSRMLS_CC);
	}
	RETURN_TRUE;
}

/**{{{ XLog::setBasePath($path)
*/
ZEND_METHOD(XLog, setBasePath)
{
	char *path;
	int path_len;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &path, &path_len) == FAILURE){
		RETURN_FALSE;
	}
	if (XLOG_G(current_log_path) == NULL){
		XLOG_G(current_log_path) = estrndup(path, path_len);
		RETURN_TRUE;
	}

	if (strcmp(XLOG_G(current_log_path), path) != 0){
		efree(XLOG_G(current_log_path));	
		XLOG_G(current_log_path) = estrndup(path, path_len);
	}	
	RETURN_TRUE;
}
/**}}}
*/

/**{{{ Xlog::getBasePath()
*/
ZEND_METHOD(XLog, getBasePath)
{
	if (XLOG_G(current_log_path) == NULL){
		RETURN_STRING(XLOG_G(log_path), 1);
	}
	else{
		RETURN_STRING(XLOG_G(current_log_path), 1);
	}
}
/**}}}*/

/**{{{ XLog::setLogger($logger)
*/
ZEND_METHOD(XLog, setLogger)
{
	char *logger;
	int logger_len;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &logger, &logger_len) == FAILURE){
		RETURN_FALSE;
	}
	if (XLOG_G(current_project_name) == NULL){
		XLOG_G(current_project_name) = estrndup(logger, logger_len);
		RETURN_TRUE;
	}

	if (strcmp(XLOG_G(current_project_name), logger) != 0){
		efree(XLOG_G(current_project_name));
		XLOG_G(current_project_name) = estrndup(logger, logger_len);
	}
	RETURN_TRUE;
}
/**}}}*/

/**{{{ XLog::getLastLogger()
*/
ZEND_METHOD(XLog, getLastLogger)
{
	if (XLOG_G(current_project_name) == NULL){
		RETURN_STRING(XLOG_G(project_name), 1);
	}
	else{
		RETURN_STRING(XLOG_G(current_project_name), 1);
	}
}
/**}}}*/

/**{{{ XLog::getBuffer()
*/
ZEND_METHOD(XLog, getBuffer)
{
	if (XLOG_G(log_buffer) < 1){
		RETURN_NULL();
	}
	array_init(return_value);
	char buf[32] = { 0 };
	char *tmp;
	int tmp_len;
	LogItem **log = XLOG_G(log);
	for (int i = 0; i < XLOG_G(log_buffer); i++){
		if (log[i] == NULL)
			continue;
		strftime(buf, 32, "%Y-%m-%d %H:%M:%S", localtime(&(log[i]->time)));
		tmp_len = spprintf(&tmp, 0, "level:%s,time:%s,app:%s,msg:%s", get_log_level_name(log[i]->level), buf, log[i]->app_name, log[i]->msg);
		add_next_index_stringl(return_value, tmp, tmp_len, 0);
	}
}
/**}}}*/

/**{{{ XLog::flush();
*/
ZEND_METHOD(XLog, flush)
{
	save_log_with_buffer(XLOG_G(log) TSRMLS_CC);
	RETURN_TRUE;
}
/**}}}*/

/**{{{	XLog::debug($msg,Array $context)
*/
ZEND_METHOD(XLog, debug)
{
	process_log(INTERNAL_FUNCTION_PARAM_PASSTHRU, XLOG_LEVEL_DEBUG);
}
/**}}}*/


/**{{{	XLog::info($msg,Array $context)
*/
ZEND_METHOD(XLog, info)
{
	process_log(INTERNAL_FUNCTION_PARAM_PASSTHRU, XLOG_LEVEL_INFO);
}
/**}}}*/


/**{{{	XLog::notice($msg,Array $context)
*/
ZEND_METHOD(XLog, notice)
{
	process_log(INTERNAL_FUNCTION_PARAM_PASSTHRU, XLOG_LEVEL_NOTICE);
}
/**}}}*/


/**{{{	XLog::warning($msg,Array $context)
*/
ZEND_METHOD(XLog, warning)
{
	process_log(INTERNAL_FUNCTION_PARAM_PASSTHRU, XLOG_LEVEL_WARNING);
}
/**}}}*/


/**{{{	XLog::error($msg,Array $context)
*/
ZEND_METHOD(XLog, error)
{
	process_log(INTERNAL_FUNCTION_PARAM_PASSTHRU, XLOG_LEVEL_ERROR);
}
/**}}}*/


/**{{{	XLog::critical($msg,Array $context)
*/
ZEND_METHOD(XLog, critical)
{
	process_log(INTERNAL_FUNCTION_PARAM_PASSTHRU, XLOG_LEVEL_CRITICAL);
}
/**}}}*/


/**{{{	XLog::alert($msg,Array $context)
*/
ZEND_METHOD(XLog, alert)
{
	process_log(INTERNAL_FUNCTION_PARAM_PASSTHRU, XLOG_LEVEL_ALERT);
}
/**}}}*/


/**{{{	XLog::emergency($msg,Array $context)
*/
ZEND_METHOD(XLog, emergency)
{
	process_log(INTERNAL_FUNCTION_PARAM_PASSTHRU, XLOG_LEVEL_EMERGENCY);
}
/**}}}*/


/**{{{	XLog::log($level,$msg,Array $context)
*/
ZEND_METHOD(XLog, log)
{
	char *msg;
	int msg_len;
	int level;
	zval *context = NULL;

	int argc = ZEND_NUM_ARGS();
	if (zend_parse_parameters(argc TSRMLS_CC, "ls|a", &level,&msg, &msg_len, &context) == FAILURE){
		RETURN_FALSE;
	}
	if (level<0 || level>8){
		RETURN_FALSE;
	}
	if (argc == 3 && context != NULL){
		process_log_with_context(level, msg, msg_len, context TSRMLS_CC);
	}
	else{
		process_log_no_context(level, msg, msg_len TSRMLS_CC);
	}
	RETURN_TRUE;
}
/**}}}*/



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
STD_PHP_INI_ENTRY("xlog.send_mail_level", "8", PHP_INI_ALL, OnUpdateLongGEZero, send_mail_level, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.trace_error", "0", PHP_INI_ALL, OnUpdateBool, trace_error, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.trace_exception", "0", PHP_INI_ALL, OnUpdateBool, trace_exception, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.file_enable", "1", PHP_INI_ALL, OnUpdateBool, file_enable, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.redis_enable", "0", PHP_INI_ALL, OnUpdateBool, redis_enable, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.redis_host", "", PHP_INI_ALL, OnUpdateString, redis_host, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.redis_port", "6379", PHP_INI_ALL, OnUpdateLongGEZero, redis_port, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.redis_auth", "", PHP_INI_ALL, OnUpdateString, redis_auth, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.redis_db", "0", PHP_INI_ALL, OnUpdateLongGEZero, redis_db, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.log_buffer", "100", PHP_INI_SYSTEM, OnUpdateLongGEZero, log_buffer, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.host_name", "", PHP_INI_SYSTEM, OnUpdateString, host_name, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.project_name", "", PHP_INI_SYSTEM, OnUpdateString, project_name, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.log_path", "/tmp", PHP_INI_SYSTEM, OnUpdateString, log_path, zend_xlog_globals, xlog_globals)
PHP_INI_END()

/* }}} */


zend_function_entry xlog_methods[] = {
	
	ZEND_ME(XLog, setBasePath, arg_info_set_base_path, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, getBasePath, arg_info_void, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, setLogger, arg_info_set_logger, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, getLastLogger, arg_info_void, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, getBuffer, arg_info_void, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, flush, arg_info_void, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)

	ZEND_ME(XLog, debug, arg_info_log_common, ZEND_ACC_PUBLIC |ZEND_ACC_STATIC)
	ZEND_ME(XLog, notice, arg_info_log_common, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, info, arg_info_log_common, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, warning, arg_info_log_common, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, error, arg_info_log_common, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, critical, arg_info_log_common, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, alert, arg_info_log_common, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, emergency, arg_info_log_common, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, log,    arg_info_log, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
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
	xlog_globals->current_log_path = NULL;
	xlog_globals->current_project_name = NULL;
}

/* }}} */

/** {{{ PHP_GINIT_FUNCTION
*/
PHP_GINIT_FUNCTION(xlog)
{
	XLOG_G(redis) = NULL;
	XLOG_G(log) = NULL;
	XLOG_G(log_index) = 0;
	XLOG_G(current_project_name) = NULL;
	XLOG_G(current_log_path) = NULL;
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(xlog)
{
	zend_class_entry ce;
	INIT_CLASS_ENTRY(ce, "XLog", xlog_methods);
	xlog_ce = zend_register_internal_class(&ce TSRMLS_CC);
	zend_declare_class_constant_long(xlog_ce, ZEND_STRL("ALL"),XLOG_LEVEL_ALL TSRMLS_CC);
	zend_declare_class_constant_long(xlog_ce, ZEND_STRL("DEBUG"), XLOG_LEVEL_DEBUG TSRMLS_CC);
	zend_declare_class_constant_long(xlog_ce, ZEND_STRL("INFO"), XLOG_LEVEL_INFO TSRMLS_CC);
	zend_declare_class_constant_long(xlog_ce, ZEND_STRL("NOTICE"), XLOG_LEVEL_NOTICE TSRMLS_CC);
	zend_declare_class_constant_long(xlog_ce, ZEND_STRL("WARNING"), XLOG_LEVEL_WARNING TSRMLS_CC);
	zend_declare_class_constant_long(xlog_ce, ZEND_STRL("ERROR"), XLOG_LEVEL_ERROR TSRMLS_CC);
	zend_declare_class_constant_long(xlog_ce, ZEND_STRL("CRITICAL"), XLOG_LEVEL_CRITICAL TSRMLS_CC);
	zend_declare_class_constant_long(xlog_ce, ZEND_STRL("ALERT"), XLOG_LEVEL_ALERT TSRMLS_CC);
	zend_declare_class_constant_long(xlog_ce, ZEND_STRL("EMERGENCY"), XLOG_LEVEL_EMERGENCY TSRMLS_CC);
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
	
	if (XLOG_G(log) != NULL){
		save_log_with_buffer(XLOG_G(log) TSRMLS_CC);
		destory_log(&XLOG_G(log), XLOG_G(log_buffer) TSRMLS_CC);
	}
	if (XLOG_G(redis) != NULL){
		php_stream_free(XLOG_G(redis), PHP_STREAM_FREE_CLOSE);
	}
	XLOG_G(log_index) = 0;
	if (XLOG_G(current_project_name) != NULL){
		efree(XLOG_G(current_project_name));
	}
	if (XLOG_G(current_log_path) != NULL){
		efree(XLOG_G(current_log_path));
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
