#define _CRT_SECURE_NO_WARNINGS
#include "php.h"
#include "zend.h"
#include "zend_builtin_functions.h"
#include "zend_exceptions.h"
#include "php_xlog.h"
#include "common.h"
#include "redis.h"
#include "mail.h"
#include "standard/php_var.h"
#include "standard/php_smart_str.h"
#include "php_output.h"
int split_string(const char *str, unsigned char split, char ***buf, int *count)
{
	int i = 0;
	const char *p = str;
	const char *last = str;
	char **tmp = NULL;
	int ret = SUCCESS;
	int lines = 0;
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
		ret = FAILURE;
		goto END;
	}
	p = str;
	last = str;
	while ((p = strchr(p, split)) != NULL){
		tmp[i] = ecalloc((p - last) + 1, sizeof(char));
		if (tmp[i] == NULL){
			ret = FAILURE;
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
			ret = FAILURE;
			goto END;
		}
		strcpy(tmp[i], last);
		i++;
	}
	*count = i;
	*buf = tmp;
END:
	if (ret != SUCCESS && tmp != NULL){
		for (int j = 0; j < lines; j++){
			if (tmp[j] != NULL){
				efree(tmp[j]);
			}
		}
		efree(tmp);
	}
	return ret;
}



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

int  get_serialize_debug_trace(char **ret,int *ret_len TSRMLS_DC)
{
	if (ret == NULL){
		return FAILURE;
	}
	zend_bool flag = SUCCESS;
	zval *debug;
	php_serialize_data_t var_hash;
	smart_str buf = { 0 };
	MAKE_STD_ZVAL(debug);
	zend_fetch_debug_backtrace(debug, 1, DEBUG_BACKTRACE_PROVIDE_OBJECT, 0 TSRMLS_CC);
	if (zend_hash_num_elements(Z_ARRVAL_P(debug)) > 0){
		PHP_VAR_SERIALIZE_INIT(var_hash);
		php_var_serialize(&buf, &debug, &var_hash TSRMLS_CC);
		PHP_VAR_SERIALIZE_DESTROY(var_hash);
		if (EG(exception)) {
			smart_str_free(&buf);
			flag = FAILURE;
			goto END;
		}
	}
END:
	zval_dtor(debug);
	efree(debug);
	if (flag == SUCCESS){
		*ret = buf.c;
		if (ret_len != NULL){
			*ret_len = buf.len;
		}
	}
	return flag;
}

int get_print_data(char **ret, int *ret_len TSRMLS_DC)
{
	if (ret == NULL){
		return FAILURE;
	}
	zend_bool flag = FAILURE;
	zval *debug;
	MAKE_STD_ZVAL(debug);
	zend_fetch_debug_backtrace(debug, 1, DEBUG_BACKTRACE_PROVIDE_OBJECT, 0 TSRMLS_CC);
	if (zend_hash_num_elements(Z_ARRVAL_P(debug)) > 0){
		php_output_start_default(TSRMLS_C);
		zend_print_zval_r(debug,0 TSRMLS_CC);
		zval tmp = { 0 };
		php_output_get_contents(&tmp TSRMLS_CC);
		*ret = Z_STRVAL_P(&tmp);
		if (ret_len != NULL){
			*ret_len = Z_STRLEN(tmp);
		}
		php_output_discard(TSRMLS_C);
		flag = SUCCESS;
	}
	zval_dtor(debug);
	efree(debug);
	return flag;
}

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
		//save_log(XLOG_ERROR, error_msg TSRMLS_CC);
		efree(msg);
		
		/*
		if (get_serialize_debug_trace(&msg, NULL TSRMLS_CC) == SUCCESS){
			save_log(XLOG_ERROR, msg TSRMLS_CC);
			efree(msg);
		}
		*/
		if (get_print_data(&msg, NULL TSRMLS_CC) == SUCCESS){
			spprintf(&format_msg, 0, "%s\n<pre>%s</pre>", error_msg,msg);
			save_log(XLOG_ERROR, format_msg TSRMLS_CC);
			efree(msg);
			efree(format_msg);
		}
		efree(error_msg);
	}
	old_error_cb(type, error_filename, error_lineno, format, args);
}

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
	save_log(XLOG_ERROR, errmsg TSRMLS_CC);
	efree(errmsg);

	if (old_throw_exception_hook) {
		old_throw_exception_hook(exception TSRMLS_CC);
	}
}

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

int save_to_redis(char *level, char *errmsg TSRMLS_DC)
{
	php_stream *stream;
	int command_len = 0;
	char *command = NULL;
	stream = XLOG_G(redis);
	if (stream == NULL){
		struct timeval tv = { 3, 0 };
		char host[32];
		php_sprintf(host, "%s:%d", XLOG_G(redis_host), XLOG_G(redis_port));
		stream = php_stream_xport_create(
			host,
			strlen(host),
			ENFORCE_SAFE_MODE ,
			STREAM_XPORT_CLIENT | STREAM_XPORT_CONNECT,
			0,
			&tv,
			NULL,
			NULL,
			NULL
			);
		XLOG_G(redis) = stream;
		if (stream != NULL){
			command_len = build_redis_command(&command, "SELECT", 6, "d", XLOG_G(redis_db));
			execute_redis_command(stream, NULL, command, command_len TSRMLS_CC);
		}
	}
	if (stream == NULL){
		return FAILURE;
	}
	command_len = build_redis_command(&command, "LPUSH", 5, "ss", level,strlen(level),errmsg,strlen(errmsg));
	execute_redis_command(stream, NULL, command, command_len TSRMLS_CC);
	return SUCCESS;
}

void save_to_mail(char *level, char *errmsg TSRMLS_DC)
{
	zval *commands;

	zend_bool result = build_mail_commands(
		&commands,
		XLOG_G(mail_username),
		XLOG_G(mail_password),
		XLOG_G(mail_from),
		XLOG_G(mail_from_name),
		XLOG_G(mail_to),
		level,
		errmsg
		TSRMLS_CC);
	if (result == SUCCESS){
		mail_send(
			XLOG_G(mail_smtp),
			XLOG_G(mail_port),
			commands,
			XLOG_G(mail_ssl)
			TSRMLS_CC
		);
	}

}

void save_log(char *level, char *errmsg TSRMLS_DC)
{
	if (level == NULL || errmsg == NULL){
		return;
	}
	if (XLOG_G(redis_enable)){
		save_to_redis(level, errmsg TSRMLS_CC);
	}
	if (XLOG_G(send_mail) && strstr(XLOG_G(send_mail_level),level)!=NULL){
		save_to_mail(level, errmsg TSRMLS_CC);
	}
}