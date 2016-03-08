#include "php.h"
#include "mail.h"
#include "ext/standard/php_smart_str.h"
#include "ext/standard/base64.h"
#include "common.h"

int mail_build_commands(
	zval **result, 
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
		result == NULL 
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
	
	php_sprintf(buffer, "Subject: %s\r\n", subject);
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
	*result = commands;
	return SUCCESS;
}

int mail_send(char *smtp,int port,zval *commands,int ssl TSRMLS_DC)
{
	int ret = SUCCESS;
	php_stream *stream = NULL;
	char buffer[1024];
	php_sprintf(buffer, "%s:%d", smtp, port);
	struct timeval tv = { 3, 0 };
	char *errorstr = NULL;
	int errono = 0;
	stream = php_stream_xport_create(
		buffer,
		strlen(buffer),
		ENFORCE_SAFE_MODE | REPORT_ERRORS,
		STREAM_XPORT_CLIENT | STREAM_XPORT_CONNECT,
		0,
		&tv,
		NULL,
		&errorstr,
		&errono
		);
	if (stream == NULL){
		ret = FAILURE;
		goto END;
	}
	if (ssl){
		php_stream_xport_crypto_setup(stream, STREAM_CRYPTO_METHOD_SSLv23_CLIENT, NULL TSRMLS_CC);
		php_stream_xport_crypto_enable(stream, TRUE TSRMLS_CC);
	}
	php_stream_set_option(stream, PHP_STREAM_OPTION_BLOCKING, TRUE, NULL);
	php_stream_read(stream, buffer, 1024);
	if (strstr(buffer, "220")==NULL){
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
	if (errorstr){
		efree(errorstr);
	}
	if (stream){
		php_stream_free(stream,PHP_STREAM_FREE_CLOSE);
	}
	return ret;
}