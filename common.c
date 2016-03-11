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
#include "zend_builtin_functions.h"
#include "zend_exceptions.h"
#include "php_xlog.h"
#include "common.h"
#include "standard/php_var.h"
#include "standard/php_smart_str.h"
#include "php_output.h"
#include "log.h"

/**{{{ int split_string(const char *str, unsigned char split, char ***ret, int *count)
*/
int split_string(const char *str, unsigned char split, char ***ret, int *count)
{
	int i = 0;
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
		for (int j = 0; j < lines; j++){
			if (tmp[j] != NULL){
				efree(tmp[j]);
			}
		}
		efree(tmp);
	}
	return flag;
}
/**}}}*/


/**{{{ void split_string_free(char ***buf, int count)
*/
void split_string_free(char ***buf, int count)
{
	if (buf == NULL || count < 1){
		return;
	}
	char **tmp = *buf;
	for (int i = 0; i < count; i++){
		efree(tmp[i]);
		tmp[i] = NULL;
	}
	efree(tmp);
	*buf = NULL;
}
/**}}}*/

/**{{{ int get_debug_backtrace(zval *debug,TSRMLS_D)
*/
int get_debug_backtrace(zval *debug,TSRMLS_D)
{
	if (debug == NULL){
		return FAILURE;
	}
	zend_fetch_debug_backtrace(debug, 1, DEBUG_BACKTRACE_PROVIDE_OBJECT, 0 TSRMLS_CC);
	if (zend_hash_num_elements(Z_ARRVAL_P(debug)) > 0){
		return SUCCESS;
	}
	zval_dtor(debug);
	return FAILURE;
}
/**}}}*/

/**{{{ int  get_serialize_debug_trace(char **ret,int *ret_len TSRMLS_DC)
*/
int  get_serialize_debug_trace(char **ret,int *ret_len TSRMLS_DC)
{
	if (ret == NULL){
		return FAILURE;
	}
	zend_bool flag = SUCCESS;
	zval debug;
	php_serialize_data_t var_hash;
	smart_str buf = { 0 };
	if (get_debug_backtrace(&debug TSRMLS_CC) ==SUCCESS){
		PHP_VAR_SERIALIZE_INIT(var_hash);
		zval *tmp = &debug;
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
	}
	return flag;
}
/**}}}*/

/**{{{ int get_print_data(char **ret, int *ret_len TSRMLS_DC)
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
/**}}}*/

/**{{{ void xlog_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args)
*/
void xlog_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args)
{
	if (type == E_ERROR || type == E_PARSE || type == E_CORE_ERROR || type == E_COMPILE_ERROR || type == E_USER_ERROR || type == E_RECOVERABLE_ERROR) {
		
		char *msg,*error_msg,*format_msg;
		TSRMLS_FETCH();
		
		va_list args_copy;
		va_copy(args_copy, args);
		vspprintf(&msg, 0, format, args_copy);
		va_end(args_copy);
		spprintf(&error_msg, 0, "%s:%d:%s",error_filename,error_lineno,msg);
		if (XLOG_G(log_buffer) > 0){
			add_log_no_malloc_msg(XLOG_G(log), XLOG_G(log_index), XLOG_LEVEL_EMERGENCY, NULL, 0, error_msg TSRMLS_CC);
			XLOG_G(log_index)++;
		}
		else{
			save_log_no_buffer(XLOG_LEVEL_EMERGENCY,NULL, error_msg  TSRMLS_CC);
		}
		efree(msg);
		if (type == E_ERROR){
			if (get_serialize_debug_trace(&msg, NULL TSRMLS_CC) == SUCCESS){
				if (XLOG_G(log_buffer) > 0){
					add_log_no_malloc_msg(XLOG_G(log), XLOG_G(log_index), XLOG_LEVEL_EMERGENCY, NULL, 0, msg TSRMLS_CC);
					XLOG_G(log_index)++;
				}
				else{
					save_log_no_buffer(XLOG_LEVEL_EMERGENCY,NULL, msg TSRMLS_CC);
					efree(msg);
				}
			}
			if (get_print_data(&msg, NULL TSRMLS_CC) == SUCCESS){
				spprintf(&format_msg, 0, "%s\n<pre>%s</pre>", error_msg, msg);
				save_to_mail(XLOG_LEVEL_EMERGENCY,NULL, format_msg TSRMLS_CC);
				efree(msg);
				efree(format_msg);
			}
		}
		if (XLOG_G(log_buffer) < 1){
			efree(error_msg);
		}
	}
	old_error_cb(type, error_filename, error_lineno, format, args);
}
/**}}}*/

/**{{{ void xlog_throw_exception_hook(zval *exception TSRMLS_DC)
*/
void xlog_throw_exception_hook(zval *exception TSRMLS_DC)
{
	zval *message, *file, *line, *code;
	zend_class_entry *default_ce;
	if (!exception) {
		return;
	}
	default_ce = zend_exception_get_default(TSRMLS_C);
	message = zend_read_property(default_ce, exception, "message", sizeof("message") - 1, 0 TSRMLS_CC);
	file = zend_read_property(default_ce, exception, "file", sizeof("file") - 1, 0 TSRMLS_CC);
	line = zend_read_property(default_ce, exception, "line", sizeof("line") - 1, 0 TSRMLS_CC);
	code = zend_read_property(default_ce, exception, "code", sizeof("code") - 1, 0 TSRMLS_CC);

	char *errmsg;
	spprintf(&errmsg, 0, "%s:%d:%s", Z_STRVAL_P(file), Z_LVAL_P(line), Z_STRVAL_P(message));
	if (XLOG_G(log_buffer) > 0){
		add_log_no_malloc_msg(XLOG_G(log), XLOG_G(log_index), XLOG_LEVEL_EMERGENCY, NULL, 0, errmsg TSRMLS_CC);
		XLOG_G(log_index)++;
	}
	else{
		save_log_no_buffer(XLOG_LEVEL_EMERGENCY,NULL, errmsg TSRMLS_CC);
		efree(errmsg);
	}

	if (old_throw_exception_hook) {
		old_throw_exception_hook(exception TSRMLS_CC);
	}
}
/**}}}*/

/**{{{ void init_error_hooks(TSRMLS_D)
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
/**}}}*/

/**{{{ void restore_error_hooks(TSRMLS_D)
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
/**}}}*/


/**{{{ int strtr_array(const char *template,int template_len,zval *context,char **ret,int *ret_len TSRMLS_DC)
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
		if (key_len > 255 || key_len < 1){
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
/**}}}*/