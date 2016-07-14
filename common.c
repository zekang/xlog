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
#define _CRT_SECURE_NO_WARNINGS
#include "php.h"
#include "zend.h"
#include "main/SAPI.h"
#include "zend_builtin_functions.h"
#include "zend_exceptions.h"
#include "standard/file.h"
#include "php_xlog.h"
#include "common.h"
#include "standard/php_var.h"
#include "standard/php_smart_str.h"
#include "php_output.h"
#include "log.h"
#include "mail.h"

#ifdef PHP_WIN32
#include "win32/time.h"
#elif defined(NETWARE)
#include <sys/timeval.h>
#include <sys/time.h>
#else
#include <sys/time.h>
#endif

#define MICRO_IN_SEC 1000000.00

/* {{{ int split_string(const char *str, unsigned char split, char ***ret, int *count)
*/
int split_string(const char *str, unsigned char split, char ***ret, int *count)
{
	int i = 0,j;
	const char *p = str;
	const char *last = str;
	char **tmp = NULL;
	int flag = SUCCESS;
	int lines = 0;
	if (str == NULL || split == '\0' || ret == NULL || count == NULL){
		return FAILURE;
	}
	while ((p = strchr(p, split)) != NULL){
		lines++;
		p++;//skip split char
		last = p;
	}
	if (*last != '\0'){
		lines++;
	}
	tmp = (char **)ecalloc(lines , sizeof(char *));
	if (tmp == NULL){
		flag = FAILURE;
		goto END;
	}
	p = str;
	last = str;
	while ((p = strchr(p, split)) != NULL){
		tmp[i] = ecalloc((p - last) + 1, sizeof(char));
		if (tmp[i] == NULL){
			flag = FAILURE;
			goto END;
		}
		strncpy(tmp[i], last, p - last);
		i++;
		p++;//skip split char
		last = p;
	}
	if (*last != '\0'){
		tmp[i] = ecalloc(strlen(last) + 1, sizeof(char));
		if (tmp[i] == NULL){
			flag = FAILURE;
			goto END;
		}
		strcpy(tmp[i], last);
		i++;
	}
	*count = i;
	*ret = tmp;
END:
	if (flag != SUCCESS && tmp != NULL){
		for (j = 0; j < lines; j++){
			if (tmp[j] != NULL){
				efree(tmp[j]);
			}
		}
		efree(tmp);
	}
	return flag;
}
/* }}}*/


/* {{{ void split_string_free(char ***buf, int count)
*/
void split_string_free(char ***buf, int count)
{
	if (buf == NULL || count < 1){
		return;
	}
	char **tmp = *buf;
	int i;
	for (i = 0; i < count; i++){
		efree(tmp[i]);
		tmp[i] = NULL;
	}
	efree(tmp);
	*buf = NULL;
}
/* }}}*/

/* {{{ int get_debug_backtrace(zval *debug,TSRMLS_D)
*/
int get_debug_backtrace(zval *debug TSRMLS_DC)
{
	if (debug == NULL){
		return FAILURE;
	}
#if ZEND_MODULE_API_NO >= 20100525
	zend_fetch_debug_backtrace(debug, 1, DEBUG_BACKTRACE_IGNORE_ARGS, 0 TSRMLS_CC);
#else
	zend_fetch_debug_backtrace(debug, 1, DEBUG_BACKTRACE_IGNORE_ARGS TSRMLS_CC);
#endif
	if (zend_hash_num_elements(Z_ARRVAL_P(debug)) > 0){
		return SUCCESS;
	}
	zval_dtor(debug);
	return FAILURE;
}
/* }}}*/

/* {{{ int  get_serialize_debug_trace(char **ret,int *ret_len TSRMLS_DC)
*/
int  get_serialize_debug_trace(char **ret,int *ret_len TSRMLS_DC)
{
	if (ret == NULL){
		return FAILURE;
	}
	zend_bool flag = FAILURE;
	zval debug;
	
	size_t i;
	php_serialize_data_t var_hash;
	
	smart_str buf = { 0 };
	if (get_debug_backtrace(&debug TSRMLS_CC) ==SUCCESS){
		flag = SUCCESS;
		zval *tmp = &debug;
		
		PHP_VAR_SERIALIZE_INIT(var_hash);
		php_var_serialize(&buf, &tmp, &var_hash TSRMLS_CC);
		PHP_VAR_SERIALIZE_DESTROY(var_hash);
		
		zval_dtor(&debug);
		if (EG(exception)) {
			smart_str_free(&buf);
			flag = FAILURE;
			goto END;
		}
	}
END:
	if (flag == SUCCESS){
		*ret = buf.c;
		if (ret_len != NULL){
			*ret_len = buf.len;
		}
		if (strlen(buf.c) != buf.len){
			for (i = 0; i < buf.len; i++){
				if (buf.c[i] == '\0'){
					buf.c[i] = '*';
				}
			}
		}
	}
	return flag;
}
/* }}}*/

/* {{{ int  get_exception_trace(zval *exception ,char **ret,int *ret_len int mode TSRMLS_DC)
*/
int  get_exception_trace(zval *exception, char **ret, int *ret_len ,int mode TSRMLS_DC)
{
	if (ret == NULL || !exception){
		return FAILURE;
	}
	zend_bool flag = FAILURE;
	
	smart_str buf = { 0 };
	zval *trace=NULL;
	trace = zend_read_property(zend_exception_get_default(TSRMLS_C), exception, "trace", sizeof("trace") - 1, 0 TSRMLS_CC);
	if (trace && Z_TYPE_P(trace)==IS_ARRAY && zend_hash_num_elements(Z_ARRVAL_P(trace))>0){
		flag = SUCCESS;
		zval *old_exception = EG(exception);
		EG(exception) = NULL;
		zval tmp = *trace;
		zval_copy_ctor(&tmp);
		zval **pptmp;
		for (
			zend_hash_internal_pointer_reset(Z_ARRVAL(tmp));
			zend_hash_has_more_elements(Z_ARRVAL(tmp)) == SUCCESS;
		zend_hash_move_forward(Z_ARRVAL(tmp))
			){
			if (zend_hash_get_current_data(Z_ARRVAL(tmp), (void **)&pptmp) == SUCCESS){
				if (Z_TYPE_PP(pptmp) == IS_ARRAY){
					zend_hash_del(Z_ARRVAL_PP(pptmp), "args", sizeof("args"));
				}
			}
		}
		if (mode == XLOG_EXCEPTION_TRACE_SERIALIZE){
			php_serialize_data_t var_hash;
			PHP_VAR_SERIALIZE_INIT(var_hash);
			zval *ptmp = &tmp;
			php_var_serialize(&buf, &ptmp, &var_hash TSRMLS_CC);
			PHP_VAR_SERIALIZE_DESTROY(var_hash);
		}
		else if (mode == XLOG_EXCEPTION_TRACE_PRINT){
			php_output_start_default(TSRMLS_C);
			zend_print_zval_r(trace, 0 TSRMLS_CC);
			zval tmpreturl = { 0 };
			php_output_get_contents(&tmpreturl TSRMLS_CC);
			buf.c = Z_STRVAL_P(&tmpreturl);
			buf.len = Z_STRLEN(tmpreturl);
			php_output_discard(TSRMLS_C);
		}
		zval_dtor(&tmp);
		EG(exception) = old_exception;
		if (buf.len == 0 || EG(exception) != exception) {
			smart_str_free(&buf);
			flag = FAILURE;
			goto END;
		}
	}
END:
	if (flag == SUCCESS){
		*ret = buf.c;
		if (ret_len != NULL){
			*ret_len = buf.len;
		}
		if (mode == XLOG_EXCEPTION_TRACE_SERIALIZE && (strlen(buf.c) != buf.len)){
			size_t i;
			for (i = 0; i < buf.len; i++){
				if (buf.c[i] == '\0'){
					buf.c[i] = '*';
				}
			}
		}
	}
	return flag;
}
/* }}}*/


/* {{{ int get_print_data(char **ret, int *ret_len TSRMLS_DC)
*/
int get_print_data(char **ret, int *ret_len TSRMLS_DC)
{
	if (ret == NULL){
		return FAILURE;
	}
	zend_bool flag = FAILURE;
	zval debug;
	if (get_debug_backtrace(&debug TSRMLS_CC) ==SUCCESS){
		php_output_start_default(TSRMLS_C);
		zend_print_zval_r(&debug,0 TSRMLS_CC);
		zval tmp = { 0 };
		php_output_get_contents(&tmp TSRMLS_CC);
		*ret = Z_STRVAL_P(&tmp);
		if (ret_len != NULL){
			*ret_len = Z_STRLEN(tmp);
		}
		php_output_discard(TSRMLS_C);
		flag = SUCCESS;
		zval_dtor(&debug);
	}
	return flag;
}
/* }}}*/

/* {{{ int get_var_export_data(char **ret, int *ret_len TSRMLS_DC)
*/
int get_var_export_data(char **ret, int *ret_len TSRMLS_DC)
{
	if (ret == NULL){
		return FAILURE;
	}
	zend_bool flag = FAILURE;
	zval debug;
	smart_str buf = { 0 };
	if (get_debug_backtrace(&debug TSRMLS_CC) == SUCCESS){
		zval *tmp = &debug;
		php_var_export_ex(&tmp, 1, &buf TSRMLS_CC);
		*ret = buf.c;
		if (ret_len != NULL){
			*ret_len = buf.len;
		}
		flag = SUCCESS;
		zval_dtor(&debug);
	}
	return flag;
}
/* }}}*/

/* {{{ void xlog_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args)
*/
void xlog_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args)
{
	char *msg, *error_msg, *serialize_msg, *stack_msg;
#ifdef MAIL_ENABLE
	char *format_msg;
#endif
	if (type == E_ERROR || type == E_PARSE || type == E_CORE_ERROR || type == E_COMPILE_ERROR || type == E_USER_ERROR || type == E_RECOVERABLE_ERROR) {
		
		TSRMLS_FETCH();
		va_list args_copy;
		va_copy(args_copy, args);
		vspprintf(&msg, 0, format, args_copy);
		va_end(args_copy);
		spprintf(&error_msg, 0, "[fatal_error]%s:%d:%s",error_filename,error_lineno,msg);
		if (XLOG_G(buffer_enable) > 0){
			add_log_no_malloc_msg(XLOG_G(log), XLOG_G(index), XLOG_LEVEL_EMERGENCY, NULL, 0, error_msg, XLOG_FLAG_NO_SEND_MAIL TSRMLS_CC);
			XLOG_G(index)++;
		}
		else{
			save_log_no_buffer(XLOG_LEVEL_EMERGENCY, NULL, error_msg, XLOG_FLAG_NO_SEND_MAIL TSRMLS_CC);
		}
		
		if (XLOG_G(trace_stack) && (type == E_ERROR || type == E_PARSE)){
			if (get_serialize_debug_trace(&serialize_msg, NULL TSRMLS_CC) == SUCCESS){
				spprintf(&stack_msg, 0, "{error}=>%s:%d:%s\t{serialize}=>%s", error_filename, error_lineno,msg, serialize_msg);
				efree(serialize_msg);
				if (XLOG_G(buffer_enable) > 0){
					if (add_log_no_malloc_msg(XLOG_G(log), XLOG_G(index), XLOG_LEVEL_WITH_STACKINFO, NULL, 0, stack_msg, XLOG_FLAG_NO_SEND_MAIL TSRMLS_CC) == SUCCESS){
						XLOG_G(index)++;
					}
				}
				else{
					save_log_no_buffer(XLOG_LEVEL_WITH_STACKINFO, NULL, stack_msg, XLOG_FLAG_NO_SEND_MAIL TSRMLS_CC);
					efree(stack_msg);
				}
			}
		}
		if (msg != NULL){
			efree(msg);
		}
#ifdef MAIL_ENABLE
		if (XLOG_G(mail_enable)	&& mail_strategy(XLOG_LEVEL_EMERGENCY, NULL, NULL, error_filename, error_lineno) == SUCCESS){
			msg = NULL;
			if (get_print_data(&msg, NULL TSRMLS_CC) == SUCCESS){
				spprintf(&format_msg, 0, "<h3>%s</h3>\n<pre>%s</pre>", error_msg, msg);
			}
			else{
				spprintf(&format_msg, 0, "<h3>%s</h3>\n", error_msg, msg);
			}
			save_to_mail(XLOG_LEVEL_EMERGENCY, NULL, NULL, format_msg TSRMLS_CC);
			if (msg != NULL){
				efree(msg);
			}
			efree(format_msg);
		}
#endif
		if (!XLOG_G(buffer_enable)){
			efree(error_msg);
		}
	}
	old_error_cb(type, error_filename, error_lineno, format, args);
}
/* }}}*/

/* {{{ void xlog_throw_exception_hook(zval *exception TSRMLS_DC)
*/
void xlog_throw_exception_hook(zval *exception TSRMLS_DC)
{
	zval *message, *file, *line, *code;
	zend_class_entry *default_ce;
	char *serialize_msg, *stack_msg;
#ifdef MAIL_ENABLE
	char *format_msg, *msg;
#endif
	if (!exception) {
		return;
	}
	default_ce = zend_exception_get_default(TSRMLS_C);
	message = zend_read_property(default_ce, exception, "message", sizeof("message") - 1, 0 TSRMLS_CC);
	file = zend_read_property(default_ce, exception, "file", sizeof("file") - 1, 0 TSRMLS_CC);
	line = zend_read_property(default_ce, exception, "line", sizeof("line") - 1, 0 TSRMLS_CC);
	code = zend_read_property(default_ce, exception, "code", sizeof("code") - 1, 0 TSRMLS_CC);
	char *errmsg;
	int len = spprintf(&errmsg, 0, "[exception]:%s:%d:%s", Z_STRVAL_P(file), Z_LVAL_P(line), Z_STRVAL_P(message));
#ifdef MAIL_ENABLE
	if (XLOG_G(mail_enable) && mail_strategy(XLOG_LEVEL_EMERGENCY, NULL, NULL, Z_STRVAL_P(file), Z_LVAL_P(line)) == SUCCESS){
		msg = NULL;
		if (get_exception_trace(exception, &msg, NULL, XLOG_EXCEPTION_TRACE_PRINT TSRMLS_CC) == SUCCESS){
			spprintf(&format_msg, 0, "<h3>%s</h3>\n<pre>%s</pre>", errmsg, msg);
		}
		else{
			spprintf(&format_msg, 0, "<h3>%s</h3>\n", errmsg);
		}
		save_to_mail(XLOG_LEVEL_EMERGENCY, NULL, NULL, format_msg TSRMLS_CC);
		if (msg != NULL){
			efree(msg);
		}
		efree(format_msg);
	}
#endif
	if (XLOG_G(buffer_enable) > 0){
		add_log_no_malloc_msg(XLOG_G(log), XLOG_G(index), XLOG_LEVEL_EMERGENCY, NULL, 0, errmsg, XLOG_FLAG_NO_SEND_MAIL TSRMLS_CC);
		XLOG_G(index)++;
	}
	else{
		save_log_no_buffer(XLOG_LEVEL_EMERGENCY, NULL, errmsg, XLOG_FLAG_NO_SEND_MAIL TSRMLS_CC);
		efree(errmsg);
	}
	
	if (XLOG_G(trace_stack) && get_exception_trace(exception, &serialize_msg, NULL, XLOG_EXCEPTION_TRACE_SERIALIZE TSRMLS_CC) == SUCCESS){
		spprintf(&stack_msg, 0, "{error}=>%s:%d:%s\t{serialize}=>%s", Z_STRVAL_P(file), Z_LVAL_P(line), Z_STRVAL_P(message), serialize_msg);
		efree(serialize_msg);
		if (XLOG_G(buffer_enable) > 0){
			if (add_log_no_malloc_msg(XLOG_G(log), XLOG_G(index), XLOG_LEVEL_WITH_STACKINFO, NULL, 0, stack_msg, XLOG_FLAG_NO_SEND_MAIL TSRMLS_CC) == SUCCESS){
				XLOG_G(index)++;
			}
		}
		else{
			save_log_no_buffer(XLOG_LEVEL_WITH_STACKINFO, NULL, stack_msg, XLOG_FLAG_NO_SEND_MAIL TSRMLS_CC);
			efree(stack_msg);
		}
	}
	if (old_throw_exception_hook) {
		old_throw_exception_hook(exception TSRMLS_CC);
	}
}
/* }}}*/

/* {{{ void init_error_hooks(TSRMLS_D)
*/
void init_error_hooks(TSRMLS_D)
{
	if (XLOG_G(trace_error)) {
		old_error_cb = zend_error_cb;
		zend_error_cb = xlog_error_cb;
	}

	if (XLOG_G(trace_exception)) {
		if (zend_throw_exception_hook) {
			old_throw_exception_hook = zend_throw_exception_hook;
		}
		zend_throw_exception_hook = xlog_throw_exception_hook;
	}
}
/* }}}*/

/* {{{ void restore_error_hooks(TSRMLS_D)
*/
void restore_error_hooks(TSRMLS_D)
{
	if (XLOG_G(trace_error)) {
		if (old_error_cb) {
			zend_error_cb = old_error_cb;
		}
	}
	if (XLOG_G(trace_exception)) {
		if (old_throw_exception_hook) {
			zend_throw_exception_hook = old_throw_exception_hook;
		}
	}
}
/* }}}*/


/* {{{ int strtr_array(const char *template,int template_len,zval *context,char **ret,int *ret_len TSRMLS_DC)
*/
int strtr_array(const char *template,int template_len,zval *context,char **ret,int *ret_len TSRMLS_DC)
{
	int   key_len;
	char key[XLOG_CONTEXT_KEY_MAX_LEN];
	smart_str buf = { 0 };
	const char *cursor;
	char *first, *end;
	zval **replace;
	zval tmp;
	if (template == NULL || template_len < 1 || context == NULL || ret == NULL){
		return FAILURE;
	}
	cursor = template;
	do{
		first = strchr(cursor, XLOG_CONTEXT_KEY_LEFT_DEILM);
		if (first == NULL){
			break;
		}
		end = strchr(first + 1, XLOG_CONTEXT_KEY_RIGHT_DEILM);
		if (end == NULL){
			break;
		}
		key_len = end - first - 1;
		if (key_len >= XLOG_CONTEXT_KEY_MAX_LEN || key_len < 1){
			break;
		}
		memcpy(key, first + 1, key_len);
		key[key_len] = '\0';
		if (
			strspn(key, XLOG_CONTEXT_KEY_CONTROL) == key_len
			&& (zend_hash_find(Z_ARRVAL_P(context), key, key_len + 1, (void **)&replace) == SUCCESS))
		{
			smart_str_appendl(&buf, cursor, first - cursor);
			if (Z_TYPE_PP(replace) == IS_STRING){
				smart_str_appendl(&buf, Z_STRVAL_PP(replace), Z_STRLEN_PP(replace));
			}
			else{
				tmp = **replace;
				zval_copy_ctor(&tmp);
				convert_to_string(&tmp);
				smart_str_appendl(&buf, Z_STRVAL(tmp), Z_STRLEN(tmp));
				zval_dtor(&tmp);
			}
		}
		else{
			smart_str_appendl(&buf, cursor, end - cursor + 1);
		}
		cursor = end + 1;
	} while (1);
	smart_str_appends(&buf, cursor);
	smart_str_0(&buf);
	*ret = buf.c;
	if (ret_len!=NULL){
		*ret_len = buf.len;
	}
	return SUCCESS;
}
/* }}}*/

/* {{{ int  xlog_make_log_dir(char *dir TSRMLS_DC)
*/
int  xlog_make_log_dir(char *dir TSRMLS_DC)
{
	if (VCWD_ACCESS(dir, 0) == 0){ //Existence only
		/*Write permission*/
		return VCWD_ACCESS(dir, 2) == 0 ? SUCCESS : FAILURE;
	}
	zval *zcontext = NULL;
	long mode = 0777;
	zend_bool recursive = 1;
	php_stream_context *context;
	umask(1);
	context = php_stream_context_from_zval(zcontext, 0);
	if (!php_stream_mkdir(dir, mode, (recursive ? PHP_STREAM_MKDIR_RECURSIVE : 0) | REPORT_ERRORS, context)) {
		return FAILURE;
	}
	return SUCCESS;
}
/* }}}*/

/*  {{{ int  xlog_get_microtime(char *ret,int max,int extra)
*/
int  xlog_get_microtime(char *ret,int max,int extra)
{
	struct timeval tp = { 0 };
	if (ret == NULL || max < 1 || gettimeofday(&tp, NULL)){
		return FAILURE;
	}
	snprintf(ret, max, "%f%d", (double)tp.tv_sec + tp.tv_usec / MICRO_IN_SEC,extra);
	return SUCCESS;
}
/* }}}*/

/** {{{ zval * xlog_request_query(uint type, char * name, uint len TSRMLS_DC)
*/
zval * xlog_request_query(uint type, char * name, uint len TSRMLS_DC)
{
	zval 		**carrier = NULL, **ret;

#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 4)
	zend_bool 	jit_initialization = (PG(auto_globals_jit) && !PG(register_globals) && !PG(register_long_arrays));
#else
	zend_bool 	jit_initialization = PG(auto_globals_jit);
#endif

	switch (type) {
	case TRACK_VARS_POST:
	case TRACK_VARS_GET:
	case TRACK_VARS_FILES:
	case TRACK_VARS_COOKIE:
		carrier = &PG(http_globals)[type];
		break;
	case TRACK_VARS_ENV:
		if (jit_initialization) {
			zend_is_auto_global(ZEND_STRL("_ENV") TSRMLS_CC);
		}
		carrier = &PG(http_globals)[type];
		break;
	case TRACK_VARS_SERVER:
		if (jit_initialization) {
			zend_is_auto_global(ZEND_STRL("_SERVER") TSRMLS_CC);
		}
		carrier = &PG(http_globals)[type];
		break;
	case TRACK_VARS_REQUEST:
		if (jit_initialization) {
			zend_is_auto_global(ZEND_STRL("_REQUEST") TSRMLS_CC);
		}
		(void)zend_hash_find(&EG(symbol_table), ZEND_STRS("_REQUEST"), (void **)&carrier);
		break;
	default:
		break;
	}

	if (!carrier || !(*carrier)) {
		zval *empty;
		MAKE_STD_ZVAL(empty);
		ZVAL_NULL(empty);
		return empty;
	}

	if (!len) {
		Z_ADDREF_P(*carrier);
		return *carrier;
	}

	if (zend_hash_find(Z_ARRVAL_PP(carrier), name, len + 1, (void **)&ret) == FAILURE) {
		zval *empty;
		MAKE_STD_ZVAL(empty);
		ZVAL_NULL(empty);
		return empty;
	}

	Z_ADDREF_P(*ret);
	return *ret;
}
/* }}} */

/** {{{ int  xlog_elapse_time(TSRMLS_D)
*/
int  xlog_elapse_time(TSRMLS_D)
{
	char *buf;
	int buf_len = 0, elapse = 0;
	php_stream *stream = NULL;
	zval *uri = NULL;
	char *module = NULL;
	do{
		if (strncmp("cli", sapi_module.name, 3) == 0){
			break;
		}
		if (XLOG_G(profiling_time) < 0){
			break;
		}
		elapse = time(NULL) - XLOG_G(request_time);
		if (elapse < XLOG_G(profiling_time)){
			break;
		}
		uri = xlog_request_query(TRACK_VARS_SERVER, ZEND_STRL("REQUEST_URI") TSRMLS_CC);
		if(uri == NULL || Z_TYPE_P(uri) != IS_STRING){
			break;
		}
		module = XLOG_G(module) == NULL ? XLOG_G(default_module) : XLOG_G(module);
		buf_len = spprintf(&buf, 8192, "%d\t%s\n", elapse, Z_STRVAL_P(uri));
		if (XLOG_G(buffer_enable)){
			if (add_log(XLOG_G(log), XLOG_G(index), XLOG_LEVEL_ELAPSE_TIME, module, strlen(module), buf, buf_len, XLOG_FLAG_SEND_MAIL TSRMLS_CC) == SUCCESS){
				XLOG_G(index)++;
			}
		}
		else{
			save_log_no_buffer(XLOG_LEVEL_ELAPSE_TIME, module, buf, XLOG_FLAG_SEND_MAIL TSRMLS_CC);
		}
	} while (0);
	if (uri != NULL){
		zval_ptr_dtor(&uri);
	}
	return 0;
}
/* }}} */