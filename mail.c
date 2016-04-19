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
#include "php_open_temporary_file.h"
#include "ext/standard/php_smart_str.h"
#include "ext/standard/base64.h"
#include "ext/standard/flock_compat.h"
#include "ext/standard/php_var.h"
#include "php_xlog.h"
#include "common.h"
#include "mail.h"
/* {{{ int build_mail_commands(zval **ret,char *username,char *password,char *from,char *fromName,char *to ,char *subject,char *body TSRMLS_DC)
*/
int build_mail_commands(
	zval **ret, 
	char *username, 
	char *password, 
	char *from, 
	char *fromName, 
	char *to ,
	char *subject,
	char *body
	TSRMLS_DC)
{
	if (
		ret == NULL 
		|| username == NULL 
		|| password == NULL 
		|| from == NULL 
		|| to == NULL 
		|| subject == NULL
		|| body == NULL){
		return FAILURE;
	}
	char **receviers;
	int receviers_count = 0;
	zval *commands = NULL;
	char separator[32] = { 0 };
	char buffer[1024];
	int  tmp_len = 0;
	smart_str head = { 0 };
	int i;
	if (split_string(to, ',', &receviers, &receviers_count) == FAILURE){
		return FAILURE;
	}
	MAKE_STD_ZVAL(commands);
	array_init(commands);
	php_sprintf(separator, "%s%d", "----=_Part_", time(NULL));
	ADD_MAIL_COMMAND(commands, "HELO sendmail\r\n", sizeof("HELO sendmail\r\n") - 1, 1, 250);
	if (*username != '\0' && *password !='\0'){
		ADD_MAIL_COMMAND(commands, "AUTH LOGIN\r\n", sizeof("AUTH LOGIN\r\n") - 1, 1, 334);
		
		char *base64_username = php_base64_encode(username, strlen(username), &tmp_len);
		memcpy(buffer, base64_username, tmp_len);
		buffer[tmp_len] = '\r';
		buffer[tmp_len + 1] = '\n';
		buffer[tmp_len + 2] = '\0';
		ADD_MAIL_COMMAND(commands, buffer, tmp_len + 2, 1, 334);
		efree(base64_username);

		tmp_len = 0;
		char *base64_password = php_base64_encode(password, strlen(password), &tmp_len);
		memcpy(buffer, base64_password, tmp_len);
		buffer[tmp_len] = '\r';
		buffer[tmp_len + 1] = '\n';
		buffer[tmp_len + 2] = '\0';
		ADD_MAIL_COMMAND(commands, buffer, tmp_len + 2, 1, 235);
		efree(base64_password);
	}
	
	php_sprintf(buffer, "MAIL FROM: <%s>\r\n", from);
	ADD_MAIL_COMMAND(commands, buffer, strlen(buffer), 1, 250);
	php_sprintf(buffer, "FROM: %s<%s>\r\n",fromName==NULL?from:fromName,from);
	smart_str_appendl(&head, buffer, strlen(buffer));

	if (receviers_count > 1){
		for (i = 0; i < receviers_count; i++){
			php_sprintf(buffer, "RCPT TO: <%s>\r\n", receviers[i]);
			ADD_MAIL_COMMAND(commands, buffer, strlen(buffer), 1, 250);
			if (i == 0){
				php_sprintf(buffer, "TO: <%s>", receviers[i]);
			}
			else if (i + 1 == receviers_count){
				php_sprintf(buffer, ",<%s>\r\n", receviers[i]);
			}
			else{
				php_sprintf(buffer, ",<%s>", receviers[i]);
			}
			smart_str_appends(&head, buffer);
		}
	}
	else{
		php_sprintf(buffer, "RCPT TO: <%s>\r\n", receviers[0]);
		ADD_MAIL_COMMAND(commands, buffer, strlen(buffer), 1, 250);
		php_sprintf(buffer, "TO: <%s>\r\n", receviers[0]);
		smart_str_appends(&head, buffer);
	}
	
	php_sprintf(buffer, "Subject: %s-%d\r\n", subject, XLOG_G(error_count));
	smart_str_appends(&head, buffer);
	smart_str_appends(&head, "Content-Type: multipart/alternative;\r\n");

	php_sprintf(buffer, "\tboundary=\"%s\"", separator);
	smart_str_appends(&head, buffer);
	smart_str_appends(&head, "\r\nMIME-Version: 1.0\r\n");

	php_sprintf(buffer, "\r\n--%s\r\n", separator);
	smart_str_appends(&head, buffer);
	smart_str_appends(&head, "Content-Type:text/html; charset=utf-8\r\n");
	smart_str_appends(&head, "Content-Transfer-Encoding: base64\r\n\r\n");
	
	tmp_len = 0;
	char *base64_body = php_base64_encode(body, strlen(body), &tmp_len);
	smart_str_appendl(&head, base64_body,tmp_len);
	efree(base64_body);

	smart_str_appendl(&head, "\r\n", 2);
	php_sprintf(buffer, "--%s\r\n", separator);
	smart_str_appends(&head, buffer);
	smart_str_appends(&head, "\r\n.\r\n");
	smart_str_0(&head);

	ADD_MAIL_COMMAND(commands, "DATA\r\n", sizeof("DATA\r\n") - 1, 1, 354);
	ADD_MAIL_COMMAND(commands, head.c, head.len, 0,250);
	ADD_MAIL_COMMAND(commands, "QUIT\r\n", sizeof("QUIT\r\n") - 1, 1, 221);
	split_string_free(&receviers, receviers_count);
	*ret = commands;
	return SUCCESS;
}
/* }}}*/

/* {{{ int mail_send(char *smtp,int port,zval *commands,int ssl TSRMLS_DC)
*/
int mail_send(char *smtp,int port,zval *commands,int ssl TSRMLS_DC)
{
	int ret = SUCCESS;
	php_stream *stream = NULL;
	char buffer[1024];
	php_sprintf(buffer, "%s:%d", smtp, port);
	struct timeval tv = { 2, 0 };
	char *errorstr = NULL;
	int errorno;
	time_t now = time(NULL);
	if (XLOG_G(mail_fail_time) > 0
		&& (now - XLOG_G(mail_fail_time) < XLOG_G(mail_retry_interval))
		){
		ret = FAILURE;
		goto END;
	}
	stream = php_stream_xport_create(
		buffer,
		strlen(buffer),
		ENFORCE_SAFE_MODE | REPORT_ERRORS,
		STREAM_XPORT_CLIENT | STREAM_XPORT_CONNECT,
		0,
		&tv,
		NULL,
		&errorstr,
		&errorno
		);
	if (stream == NULL){
		XLOG_G(mail_fail_time) = now;
		ret = FAILURE;
		goto END;
	}
	XLOG_G(mail_fail_time) = 0;
	if (ssl){
		php_stream_xport_crypto_setup(stream, STREAM_CRYPTO_METHOD_SSLv23_CLIENT, NULL TSRMLS_CC);
		php_stream_xport_crypto_enable(stream, 1 TSRMLS_CC);
	}
	php_stream_set_option(stream, PHP_STREAM_OPTION_BLOCKING, 1, NULL);
	php_stream_set_option(stream, PHP_STREAM_OPTION_READ_TIMEOUT, 0, &tv);
	buffer[0] = '\0';
	php_stream_read(stream, buffer, 1024);
	if (buffer[0] == '\0' || strstr(buffer, "220") == NULL){
		goto END;
	}
	zval **tmp;
	zend_hash_internal_pointer_reset(Z_ARRVAL_P(commands));
	while (zend_hash_has_more_elements(Z_ARRVAL_P(commands)) == SUCCESS){
		if (zend_hash_get_current_data(Z_ARRVAL_P(commands), (void **)&tmp) == SUCCESS){
			zval **command = NULL;
			zval **code = NULL;
			zend_hash_index_find(Z_ARRVAL_PP(tmp), 0, (void **)&command);
			zend_hash_index_find(Z_ARRVAL_PP(tmp), 1, (void **)&code);
			if (*command !=NULL && *code != NULL){
				buffer[0] = '\0';
				php_stream_write(stream, Z_STRVAL_PP(command), Z_STRLEN_PP(command));
				php_stream_read(stream, buffer, 1024);
				buffer[10] = '\0';
				if (atoi(buffer) != Z_LVAL_PP(code)){
					ret = FAILURE;
					goto END;
				}
			}
		}
		zend_hash_move_forward(Z_ARRVAL_P(commands));
	}

END:
	zval_dtor(commands);
	efree(commands);
	if (stream){
		php_stream_free(stream,PHP_STREAM_FREE_CLOSE);
	}
	if (errorstr != NULL){
		efree(errorstr);
	}
	return ret;
}
/* }}}*/

/* {{{ static void update_mail_strategy_file(char *file, int total_count, int pos, ErrorLine *line TSRMLS_DC)
*/
static void update_mail_strategy_file(char *file, int total_count, int pos, ErrorLine *line TSRMLS_DC)
{
	php_stream *stream = php_stream_open_wrapper(file, "rb+", IGNORE_URL_WIN | ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
	if (!stream){
		return;
	}
	if (php_stream_lock(stream, PHP_LOCK_EX - 1)){
		return;
	}
	php_stream_seek(stream, 0 + sizeof(int *), SEEK_SET);
	php_stream_write(stream, (char *)&total_count, sizeof(int *));
	if (pos && line != NULL){
		php_stream_seek(stream, 0 + pos, SEEK_SET);
		php_stream_write(stream, (char *)line, ERROR_LINE_SIZE);
	}
	php_stream_lock(stream, PHP_LOCK_UN - 1);
	php_stream_close(stream);
	php_stream_free(stream, PHP_STREAM_FREE_RELEASE_STREAM);
}
/* }}}*/

/*{{{ int mail_strategy_file(int level, const char *application, const char *module, const char *error_str, int error_no TSRMLS_DC)
*/
int mail_strategy_file(int level, const char *application, const char *module, const char *error_str, int error_no TSRMLS_DC)
{
	char *tmpdir;
	char buf[512] = { 0 };
	int len = -1, 
		day = 0, 
		count = 1,
		total_count=1, 
		error_count = 1,
		log_day = 0, 
		error_len = min(strlen(error_str), ERROR_MSG_MAX_LEN),
		i = 0,
		pos=0,
		flag = FAILURE;
	zend_bool found = 0;
	ErrorLine line = { 0 };

	if (XLOG_G(mail_strategy_log_path)[0] == '\0'){
		tmpdir = (char *)php_get_temporary_directory();
	}
	else {
		tmpdir = XLOG_G(mail_strategy_log_path);
		if (xlog_make_log_dir(tmpdir TSRMLS_CC) == FAILURE){
			return flag;
		}
	}

	uint hash_value = zend_hash_func(error_str, error_len);
	time_t now = time(NULL);
	strftime(buf, sizeof(buf), "%Y%m%d", localtime(&now));
	day = atoi(buf);
	CHECK_AND_SET_VALUE_IF_NULL(module, len, module, default_module);
	CHECK_AND_SET_VALUE_IF_NULL(application, len, application, default_application);
	php_sprintf(buf, "%s%cxlog_%s_%s_%s_%d.data", tmpdir, XLOG_DIRECTORY_SEPARATOR, XLOG_G(host), application, module, level);
	//check file exists
	zend_bool exists = VCWD_ACCESS(buf, 0) == 0 ? 1 : 0;
	php_stream *stream = php_stream_open_wrapper(buf, exists ? "rb+" : "wb", IGNORE_URL_WIN | ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
	
	if (!stream){
		return flag;
	}
	if (exists){
		php_stream_seek(stream, 0, SEEK_SET);
		php_stream_read(stream, (char*)&log_day, sizeof(int *));
		php_stream_read(stream, (char*)&count, sizeof(int *));
		total_count = count + 1;
		if (log_day == day){
			pos += sizeof(int *);
			pos += sizeof(int *);
			for (i = 0; i < count; i++){
				php_stream_read(stream, (char*)&line, ERROR_LINE_SIZE);
				if (line.error_no == error_no && line.hash == hash_value){
					php_stream_read(stream, buf, line.len);
					buf[line.len] = '\0';
					if (strncmp(error_str, buf, line.len) == 0){
						found = 1;
						error_count = line.count + 1;
						goto END;
					}
				}
				else{
					php_stream_seek(stream, line.len, SEEK_CUR);
				}
				pos += ERROR_LINE_SIZE;
				pos += line.len;
			}
			php_stream_seek(stream, 0, SEEK_END);
		}
		else{
			php_stream_close(stream);
			php_stream_free(stream, PHP_STREAM_FREE_RELEASE_STREAM);
			stream = php_stream_open_wrapper(buf, "wb", IGNORE_URL_WIN | ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
			if (!stream){
				goto END;
			}
			count = 1;
			php_stream_write(stream, (char *)&day, sizeof(int *));
			php_stream_write(stream, (char *)&count, sizeof(int *));
		}
	}
	else{
		php_stream_write(stream, (char *)&day, sizeof(int *));
		php_stream_write(stream, (char *)&count, sizeof(int *));
	}
	
	line.count = 1;
	line.hash = hash_value;
	line.len = error_len;
	line.time = now;
	line.error_no = error_no;
	php_stream_write(stream, (char *)&line, ERROR_LINE_SIZE);
	php_stream_write(stream, error_str, error_len);
END:
	if (stream){
		php_stream_close(stream);
		php_stream_free(stream, PHP_STREAM_FREE_RELEASE_STREAM);
	}

	if (error_count >= XLOG_G(mail_strategy_min) && error_count <=  XLOG_G(mail_strategy_max)){
		if (error_count == XLOG_G(mail_strategy_min) || error_count == XLOG_G(mail_strategy_max) || (error_count % XLOG_G(mail_strategy_avg) == 0)){
			flag = SUCCESS;
			XLOG_G(error_count) = error_count;
		}
	}

	if (total_count > 1){
		php_sprintf(buf, "%s%cxlog_%s_%s_%s_%d.data", tmpdir, XLOG_DIRECTORY_SEPARATOR, XLOG_G(host), application, module, level);
		if (found){
			line.time = now;
			line.count++;
			total_count--;
		}
		update_mail_strategy_file(buf, total_count, (found ? pos : 0), (found ? &line : NULL) TSRMLS_CC);
	}
	return flag;
}
/* }}}*/
