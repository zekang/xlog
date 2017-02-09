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
#include "profile.h"


#define XLOG_SET_METHOD(name,pattern,max_len)   \
	char *name; \
	int name##_len; \
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name, &name##_len) == FAILURE){ \
		RETURN_FALSE; \
	}\
	if (name##_len < 1){ \
		RETURN_FALSE; \
	} \
	if (strspn(name, pattern) != name##_len) { \
		php_error_docref(NULL TSRMLS_CC, E_WARNING,"path is invalid,Please check!"); \
		RETURN_FALSE; \
	}\
	if (XLOG_G(name) == NULL){ \
		XLOG_G(name) = estrndup(name, min(name##_len, max_len)); \
		RETURN_TRUE; \
	}\
	if (strcmp(XLOG_G(name), name) != 0){\
		efree(XLOG_G(name)); \
		XLOG_G(name) = estrndup(name, min(name##_len, max_len)); \
	} \
	RETURN_TRUE; 

#define XLOG_GET_METHOD(name)  \
	if (XLOG_G(name) == NULL){ \
		RETURN_STRING(XLOG_G(default_##name), 1);\
	}\
	else{\
		RETURN_STRING(XLOG_G(name), 1);\
	}

#define XLOG_CHECK_PATH_LENGTH_AND_VALIDATE(name,len,pattern) do {\
	size_t _len = strlen(name); \
	size_t spnlen = strspn(name, pattern); \
	if (spnlen != _len){ \
		name[spnlen] = '\0'; \
		break; \
	} \
	if (_len > len){ \
		name[len] = '\0'; \
	} \
}while (0);
ZEND_DECLARE_MODULE_GLOBALS(xlog)


/* True global resources - no need for thread safety here */
static int le_xlog;

zend_class_entry *xlog_ce;

/* {{{ arg_info
*
*/
ZEND_BEGIN_ARG_INFO_EX(arg_info_log_common, 0, 0, 3)
ZEND_ARG_INFO(0, message)
ZEND_ARG_ARRAY_INFO(0, context, 1)
ZEND_ARG_INFO(0,module)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arg_info_log, 0, 0, 4)
ZEND_ARG_INFO(0, level)
ZEND_ARG_INFO(0, message)
ZEND_ARG_ARRAY_INFO(0, context, 1)
ZEND_ARG_INFO(0, module)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arg_info_void, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arg_info_set_base_path, 0, 0, 1)
ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arg_info_set_logger, 0, 0, 1)
ZEND_ARG_INFO(0, logger)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arg_info_set_application,0,0,1)
ZEND_ARG_INFO(0,application)
ZEND_END_ARG_INFO()
/* }}} */


/**
* Start XHProf profiling in hierarchical mode.
*
* @param  long $flags  flags for hierarchical mode
* @return void
* @author kannan
*/
PHP_FUNCTION(profile_enable) {
	hp_begin( TSRMLS_CC);
}

/**
* Stops XHProf from profiling in hierarchical mode anymore and returns the
* profile info.
*
* @param  void
* @return array  hash-array of XHProf's profile info
* @author kannan, hzhao
*/
PHP_FUNCTION(profile_disable) {
	if (hp_globals.enabled) {
		hp_stop(TSRMLS_C);
		RETURN_ZVAL(hp_globals.stats_count, 1, 0);
	}
	/* else null is returned */
}


/* {{{ xlog_functions[]
 *
 */
const zend_function_entry xlog_functions[] = {
	PHP_FE(profile_enable,NULL)
	PHP_FE(profile_disable, NULL)
	PHP_FE_END	/* Must be the last line in xlog_functions[] */
};
/* }}} */

/*{{{ static void process_log_no_context(int level,char *msg,int msg_len,char *module ,int module_len TSRMLS_DC)
*/
static void process_log_no_context(int level,char *msg,int msg_len,char *module ,int module_len TSRMLS_DC)
{
	if (level < XLOG_G(level)){
		return;
	}
	if (module_len < 1){
		module_len = 0;
		module = NULL;
	}
	if (XLOG_G(buffer_enable)){
		if (add_log(XLOG_G(log), XLOG_G(index), level, module, module_len, msg, msg_len, XLOG_FLAG_SEND_MAIL TSRMLS_CC) == SUCCESS){
			XLOG_G(index)++;
		}
	}
	else{
		save_log_no_buffer(level, module, msg, XLOG_FLAG_SEND_MAIL TSRMLS_CC);
	}
}
/*}}} */

/*{{{ static void process_log_with_context(int level, char *msg, int msg_len, zval *context, char *module, int module_len TSRMLS_DC)
*/
static void process_log_with_context(int level, char *msg, int msg_len, zval *context, char *module, int module_len TSRMLS_DC)
{
	char *ret = NULL;
	if (level < XLOG_G(level)){
		return;
	}
	if (module_len < 1){
		module_len = 0;
		module = NULL;
	}
	if (strtr_array(msg, msg_len, context, &ret, NULL TSRMLS_CC) == SUCCESS){
		if (XLOG_G(buffer_enable) > 0){
			if (add_log_no_malloc_msg(XLOG_G(log), XLOG_G(index), level, module, module_len, ret, XLOG_FLAG_SEND_MAIL TSRMLS_CC) == SUCCESS){
				XLOG_G(index)++;
			}
		}
		else{
			save_log_no_buffer(level, module, ret, XLOG_FLAG_SEND_MAIL TSRMLS_CC);
			efree(ret);
		}
	}
}
/*}}} */

/*{{{ static void process_log(INTERNAL_FUNCTION_PARAMETERS,int level)
*/
static void process_log(INTERNAL_FUNCTION_PARAMETERS,int level)
{
	char *msg, *module = NULL;
	int msg_len, module_len = 0;
	zval *context = NULL;
	int argc = ZEND_NUM_ARGS();
	char real_module[64] = { 0 };
	if (level < XLOG_G(level) || level < XLOG_LEVEL_MIN  || level > XLOG_LEVEL_USER_MAX){
		return;
	}
	if (zend_parse_parameters(argc TSRMLS_CC, "s|as", &msg, &msg_len, &context,&module,&module_len) == FAILURE){
		RETURN_FALSE;
	}
	if (module_len > 0){
		memcpy(real_module, module, min(60,module_len));
		XLOG_CHECK_PATH_LENGTH_AND_VALIDATE(real_module, 60, XLOG_FILE_PATTERN);
	}
	if (argc >= 2 && context != NULL && Z_TYPE_P(context) == IS_ARRAY && zend_hash_num_elements(Z_ARRVAL_P(context))>0){
		process_log_with_context(level, msg, msg_len, context, real_module, module_len TSRMLS_CC);
	}
	else{
		process_log_no_context(level, msg, msg_len, real_module, module_len TSRMLS_CC);
	}
	RETURN_TRUE;
}
/*}}} */

/*{{{	proto public static XLog::setBasePath($path)
*/
ZEND_METHOD(XLog, setBasePath)
{
	XLOG_SET_METHOD(path, XLOG_PATH_PATTERN, 512);
}
/*}}}
*/

/*{{{	proto public static XLog::getBasePath()
*/
ZEND_METHOD(XLog, getBasePath)
{
	XLOG_GET_METHOD(path);
}
/*}}}*/




/*{{{	proto public static XLog::setLogger($logger)
*/
ZEND_METHOD(XLog, setLogger)
{
	XLOG_SET_METHOD(module,XLOG_FILE_PATTERN,60);
}
/*}}}*/

/*{{{	proto public static XLog::getLastLogger()
*/
ZEND_METHOD(XLog, getLastLogger)
{
	XLOG_GET_METHOD(module);
}
/*}}}*/



/*{{{	proto public static XLog::getBuffer()
*/
ZEND_METHOD(XLog, getBuffer)
{
	if (!XLOG_G(buffer_enable)){
		RETURN_NULL();
	}
	array_init(return_value);
	char timebuf[32] = { 0 };
	char *tmp;
	int tmp_len;
	LogItem **log = XLOG_G(log);
	int i;
	for (i = 0; i < XLOG_G(buffer); i++){
		if (log[i] == NULL)
			continue;
		strftime(timebuf, 32, "%Y-%m-%d %H:%M:%S", localtime(&(log[i]->time)));
		tmp_len = spprintf(&tmp, 0, "%s | %s | %s | %s", get_log_level_name(log[i]->level), timebuf, log[i]->module, log[i]->content);
		add_next_index_stringl(return_value, tmp, tmp_len, 0);
	}
}
/*}}}*/

/*{{{	proto public static XLog::flush()
*/
ZEND_METHOD(XLog, flush)
{
	save_log_with_buffer(XLOG_G(log) TSRMLS_CC);
	RETURN_TRUE;
}
/*}}}*/

/*{{{	proto public static XLog::debug($msg, Array $content=[],$logger='')
*/
ZEND_METHOD(XLog, debug)
{
	process_log(INTERNAL_FUNCTION_PARAM_PASSTHRU, XLOG_LEVEL_DEBUG);
}
/*}}}*/


/*{{{	proto public static XLog::info($msg, Array $content=[],$logger='')
*/
ZEND_METHOD(XLog, info)
{
	process_log(INTERNAL_FUNCTION_PARAM_PASSTHRU, XLOG_LEVEL_INFO);
}
/*}}}*/


/*{{{	proto public static XLog::notice($msg, Array $content=[],$logger='')
*/
ZEND_METHOD(XLog, notice)
{
	process_log(INTERNAL_FUNCTION_PARAM_PASSTHRU, XLOG_LEVEL_NOTICE);
}
/*}}}*/


/*{{{	proto public static XLog::warning($msg, Array $content=[],$logger='')
*/
ZEND_METHOD(XLog, warning)
{
	process_log(INTERNAL_FUNCTION_PARAM_PASSTHRU, XLOG_LEVEL_WARNING);
}
/*}}}*/


/*{{{	proto public static XLog::error($msg, Array $content=[],$logger='')
*/
ZEND_METHOD(XLog, error)
{
	process_log(INTERNAL_FUNCTION_PARAM_PASSTHRU, XLOG_LEVEL_ERROR);
}
/*}}}*/


/*{{{	proto public static XLog::critical($msg, Array $content=[],$logger='')
*/
ZEND_METHOD(XLog, critical)
{
	process_log(INTERNAL_FUNCTION_PARAM_PASSTHRU, XLOG_LEVEL_CRITICAL);
}
/*}}}*/


/*{{{	proto public static XLog::alert($msg, Array $content=[],$logger='')
*/
ZEND_METHOD(XLog, alert)
{
	process_log(INTERNAL_FUNCTION_PARAM_PASSTHRU, XLOG_LEVEL_ALERT);
}
/*}}}*/


/*{{{	proto public static XLog::emergency($msg, Array $content=[],$logger='')
*/
ZEND_METHOD(XLog, emergency)
{
	process_log(INTERNAL_FUNCTION_PARAM_PASSTHRU, XLOG_LEVEL_EMERGENCY);
}
/*}}}*/


/*{{{	proto public static XLog::log($level,$msg, Array $content=[],$logger='')
*/
ZEND_METHOD(XLog, log)
{
	char *msg,*module=NULL;
	int msg_len,level,module_len = 0;
	zval *context = NULL;

	int argc = ZEND_NUM_ARGS();
	if (zend_parse_parameters(argc TSRMLS_CC, "ls|as", &level,&msg, &msg_len, &context,&module,&module_len) == FAILURE){
		RETURN_FALSE;
	}
	if (level< XLOG_LEVEL_ALL || level > XLOG_LEVEL_EMERGENCY){
		RETURN_FALSE;
	}
	if (argc >= 3 && context != NULL){
		process_log_with_context(level, msg, msg_len, context,module,module_len TSRMLS_CC);
	}
	else{
		process_log_no_context(level, msg, msg_len,module,module_len TSRMLS_CC);
	}
	RETURN_TRUE;
}
/*}}}*/

/*{{{	proto public static XLog::status()
*/
ZEND_METHOD(XLog, status)
{
	array_init(return_value);
	add_assoc_long(return_value, "redis_fail_time", XLOG_G(redis_fail_time));
	add_assoc_long(return_value, "redis_retry_interval", XLOG_G(redis_retry_interval));
#ifdef MAIL_ENABLE
	add_assoc_long(return_value, "mail_tretry_interval", XLOG_G(mail_retry_interval));
	add_assoc_long(return_value, "mail_fail_time", XLOG_G(mail_fail_time));
#endif
}
/*}}}*/

/*{{{	proto public static XLog::reset()
*/
ZEND_METHOD(XLog, reset)
{
	XLOG_G(redis_fail_time) = 0;
#ifdef MAIL_ENABLE
	XLOG_G(mail_fail_time) = 0;
#endif
	RETURN_TRUE;
}
/*}}}*/

#ifdef COMPILE_DL_XLOG
ZEND_GET_MODULE(xlog)
#endif

/* {{{ PHP_INI
*/
PHP_INI_BEGIN()
#ifdef MAIL_ENABLE
STD_PHP_INI_ENTRY("xlog.mail_smtp", XLOG_INI_DEFALUT_EMPTY, PHP_INI_SYSTEM, OnUpdateString, mail_smtp, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_port", "25", PHP_INI_SYSTEM, OnUpdateLongGEZero, mail_port, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_username", "", PHP_INI_SYSTEM, OnUpdateString, mail_username, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_password", "", PHP_INI_SYSTEM, OnUpdateString, mail_password, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_ssl", "0", PHP_INI_SYSTEM, OnUpdateBool, mail_ssl, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_from", "", PHP_INI_SYSTEM, OnUpdateString, mail_from, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_from_name", "", PHP_INI_SYSTEM, OnUpdateString, mail_from_name, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_to", "", PHP_INI_SYSTEM, OnUpdateString, mail_to, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_enable", "0", PHP_INI_ALL, OnUpdateBool, mail_enable, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_level", "8", PHP_INI_ALL, OnUpdateLongGEZero, mail_level, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_retry_interval", "600", PHP_INI_SYSTEM, OnUpdateLongGEZero, mail_retry_interval, zend_xlog_globals, xlog_globals)

STD_PHP_INI_ENTRY("xlog.mail_strategy_enable", "1", PHP_INI_ALL, OnUpdateBool, mail_strategy_enable, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_strategy_log_path", XLOG_INI_DEFALUT_EMPTY, PHP_INI_SYSTEM, OnUpdateString, mail_strategy_log_path, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_strategy_min", "5", PHP_INI_ALL, OnUpdateLongGEZero, mail_strategy_min, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_strategy_max", "100", PHP_INI_ALL, OnUpdateLongGEZero, mail_strategy_max, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.mail_strategy_avg", "10", PHP_INI_ALL, OnUpdateLongGEZero, mail_strategy_avg, zend_xlog_globals, xlog_globals)
#endif
STD_PHP_INI_ENTRY("xlog.redis_enable", "0", PHP_INI_ALL, OnUpdateBool, redis_enable, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.redis_host", XLOG_INI_DEFALUT_EMPTY, PHP_INI_SYSTEM, OnUpdateString, redis_host, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.redis_port", "6379", PHP_INI_SYSTEM, OnUpdateLongGEZero, redis_port, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.redis_auth", "", PHP_INI_SYSTEM, OnUpdateString, redis_auth, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.redis_db", "0", PHP_INI_SYSTEM, OnUpdateLongGEZero, redis_db, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.redis_retry_interval", "600", PHP_INI_SYSTEM, OnUpdateLongGEZero, redis_retry_interval, zend_xlog_globals, xlog_globals)

STD_PHP_INI_ENTRY("xlog.file_enable", "1", PHP_INI_ALL, OnUpdateBool, file_enable, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.buffer_enable", "1", PHP_INI_ALL, OnUpdateBool, buffer_enable, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.buffer", "100", PHP_INI_SYSTEM, OnUpdateLongGEZero, buffer, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.rotate_enable", "0", PHP_INI_ALL, OnUpdateBool, rotate_enable, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.rotate_size", "2048", PHP_INI_ALL, OnUpdateLongGEZero, rotate_size, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.rotate_max", "100", PHP_INI_ALL, OnUpdateLongGEZero, rotate_max, zend_xlog_globals, xlog_globals)


STD_PHP_INI_ENTRY("xlog.default_module", XLOG_INI_DEFAULT_MODULE, PHP_INI_SYSTEM, OnUpdateString, default_module, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.default_path", XLOG_INI_DEFAULT_PATH, PHP_INI_SYSTEM, OnUpdateString, default_path, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.trace_error", "0", PHP_INI_ALL, OnUpdateBool, trace_error, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.trace_stack", "0", PHP_INI_ALL, OnUpdateBool, trace_stack, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.trace_exception", "0", PHP_INI_ALL, OnUpdateBool, trace_exception, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.level", "0", PHP_INI_ALL, OnUpdateLong, level, zend_xlog_globals, xlog_globals)
STD_PHP_INI_ENTRY("xlog.profiling_time", "-1", PHP_INI_SYSTEM, OnUpdateLong, profiling_time, zend_xlog_globals, xlog_globals)

PHP_INI_END()

/* }}} */

/* {{{ xlog_methods[]
*
*/
zend_function_entry xlog_methods[] = {
	ZEND_ME(XLog, setBasePath, arg_info_set_base_path, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, getBasePath, arg_info_void, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, setLogger, arg_info_set_logger, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, getLastLogger, arg_info_void, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, getBuffer, arg_info_void, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, flush, arg_info_void, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, debug, arg_info_log_common, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, notice, arg_info_log_common, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, info, arg_info_log_common, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, warning, arg_info_log_common, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, error, arg_info_log_common, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, critical, arg_info_log_common, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, alert, arg_info_log_common, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, emergency, arg_info_log_common, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, log, arg_info_log, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, status, arg_info_void, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	ZEND_ME(XLog, reset, arg_info_void, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_FE_END
};
/* }}} */


/* {{{ PHP_GINIT_FUNCTION
*/
PHP_GINIT_FUNCTION(xlog)
{
	xlog_globals->redis_fail_time = 0;
#ifdef MAIL_ENABLE
	xlog_globals->mail_fail_time = 0;
#endif
	xlog_globals->redis = NULL;
	xlog_globals->log = NULL;
	xlog_globals->index = 0;
	xlog_globals->module = NULL;
	xlog_globals->path = NULL;
	xlog_globals->file_handle = NULL;
	xlog_globals->error_count = 0;
	xlog_globals->profiling_time = -1;
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
	profile_minit(TSRMLS_C);
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
	profile_mend(TSRMLS_C);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(xlog)
{
	XLOG_G(redis) = NULL;
	XLOG_G(file_handle) = NULL;
	XLOG_G(module) = NULL;
	XLOG_G(path) = NULL;
	XLOG_G(index) = 0;
	XLOG_G(error_count) = 0;
	XLOG_G(redis_counter) = 0;
	XLOG_G(request_time) = time(NULL);
	if (XLOG_G(buffer_enable)){
		if (XLOG_G(buffer) < 1){
			XLOG_G(buffer) = 100;
		}
		init_log(&XLOG_G(log), XLOG_G(buffer)  TSRMLS_CC);
	}
	else{
		XLOG_G(log) = NULL;
	}
	if (XLOG_G(redis_retry_interval) < 10){
		XLOG_G(redis_retry_interval) = 10;
	}
#ifdef MAIL_ENABLE
	if (XLOG_G(mail_retry_interval) < 10){
		XLOG_G(mail_retry_interval) = 10;
	}
#endif
	XLOG_CHECK_PATH_LENGTH_AND_VALIDATE(XLOG_G(default_path), 512, XLOG_PATH_PATTERN);
	XLOG_CHECK_PATH_LENGTH_AND_VALIDATE(XLOG_G(default_module), 60, XLOG_FILE_PATTERN);
	XLOG_CHECK_PATH_LENGTH_AND_VALIDATE(XLOG_G(redis_host), 50, XLOG_FILE_PATTERN);
#ifdef MAIL_ENABLE
	XLOG_CHECK_PATH_LENGTH_AND_VALIDATE(XLOG_G(mail_smtp), 512, XLOG_FILE_PATTERN);
	XLOG_CHECK_PATH_LENGTH_AND_VALIDATE(XLOG_G(mail_strategy_log_path), 128, XLOG_PATH_PATTERN);
#endif	
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(xlog)
{
	xlog_elapse_time(TSRMLS_C);
	if (XLOG_G(log) != NULL){
		save_log_with_buffer(XLOG_G(log) TSRMLS_CC);
		destory_log(&XLOG_G(log), XLOG_G(buffer) TSRMLS_CC);
	}
	if (XLOG_G(redis) != NULL){
		php_stream_close(XLOG_G(redis));
	}
	if (XLOG_G(module) != NULL){
		efree(XLOG_G(module));
	}
	if (XLOG_G(path) != NULL){
		efree(XLOG_G(path));
	}
	if (XLOG_G(file_handle) != NULL){
		zend_hash_destroy(XLOG_G(file_handle));
		FREE_HASHTABLE(XLOG_G(file_handle));
	}
	profile_rend(TSRMLS_C);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(xlog)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "Xlog support", "enabled");
	php_info_print_table_row(2, "Xlog Version", PHP_XLOG_VERSION);
	php_info_print_table_row(2, "Xlog Supports", "https://github.com/zekang/xlog");
	php_info_print_table_end();
	DISPLAY_INI_ENTRIES();
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
