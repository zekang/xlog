#include "php.h"
#include "redis.h"
#include "ext/standard/php_smart_str.h"
zval * parse_redis_response(php_stream *stream TSRMLS_DC)
{
	char buffer[1024] = { 0 };
	php_stream_gets(stream, buffer, 1024);
	zval *retval = NULL;
	MAKE_STD_ZVAL(retval);
	switch (buffer[0]){
	case '+':
		ZVAL_TRUE(retval);
		break;
	case '-':
		ZVAL_FALSE(retval);
		break;
	case ':':
		ZVAL_STRING(retval, buffer, 1);
		break;
	case '$':
		if (buffer[1] == '-' && buffer[2] == '1'){
			ZVAL_NULL(retval);
		}
		else{
			int length = atoi(buffer + 1) + 2;
			int readLen = 0;
			smart_str data = { 0 };
			do{
				buffer[0] = '\0';
				php_stream_gets(stream, buffer, 1024);
				readLen = strlen(buffer);
				smart_str_appendl(&data, buffer, readLen);
				length -= readLen;
			} while (length > 0);
			if (data.len > 0){
				smart_str_0(&data);
				ZVAL_STRINGL(retval, data.c, data.len - 2, 0);
			}
		}
		break;
	case '*':
		array_init(retval);
		int lines = atoi(buffer + 1);
		for (int i = 0; i < lines; i++){
			add_next_index_zval(retval, parse_redis_response(stream TSRMLS_CC));
		}
		break;
	default:
		ZVAL_FALSE(retval);
		break;
	}
	return retval;
}

int  parse_redis_response_discard_result(php_stream *stream TSRMLS_DC)
{
	char buffer[1024] = { 0 };
	php_stream_gets(stream, buffer, 1024);
	int flag = FAILURE;
	switch (buffer[0]){
	case '+':
	case '-':
	case ':':
		flag = SUCCESS;
		break;
	case '$':
		if (buffer[1] == '-' && buffer[2] == '1'){
			flag = FAILURE;
		}
		else{
			int length = atoi(buffer + 1) + 2;
			int readLen = 0;
			do{
				buffer[0] = '\0';
				php_stream_gets(stream, buffer, 1024);
				readLen = strlen(buffer);
				length -= readLen;
			} while (length > 0);
			flag = SUCCESS;
		}
		break;

	case '*' :
		flag = SUCCESS;
		int lines = atoi(buffer + 1);
		for (int i = 0; i < lines; i++){
			parse_redis_response_discard_result(stream TSRMLS_CC);
		}
		break;
	default:
		flag = FAILURE;
		break;
	}
	return flag;
}

int build_redis_command(char **ret,char *keywokd,int keyword_len,char *format,...)
{
	smart_str command = { 0 };
	char *p = format;
	va_list ap;
	if (ret == NULL || keywokd == NULL || format == NULL){
		return 0;
	}

	va_start(ap, format);
	smart_str_appendc(&command, '*');
	smart_str_append_long(&command, (long)strlen(format) + 1);
	smart_str_appendl(&command, REDIS_END_LINE, REDIS_END_LINE_LENGTH);
	smart_str_appendc(&command, '$');
	smart_str_append_long(&command, keyword_len);
	smart_str_appendl(&command, REDIS_END_LINE, REDIS_END_LINE_LENGTH);
	smart_str_appends(&command, keywokd);
	smart_str_appendl(&command, REDIS_END_LINE, REDIS_END_LINE_LENGTH);
	while (*p){
		smart_str_appendc(&command, '$');
		switch (*p){
			case 's':{
						 char *str = va_arg(ap, char *);
						 int str_len = va_arg(ap, int);
						 smart_str_append_long(&command, str_len);
						 smart_str_appendl(&command, REDIS_END_LINE, REDIS_END_LINE_LENGTH);
						 smart_str_appendl(&command, str, str_len);
			}
				break;
			case 'i':
			case 'd': {
						  int i = va_arg(ap, int);
						  char tmp[32];
						  int tmp_len = snprintf(tmp, sizeof(tmp), "%d", i);
						  smart_str_append_long(&command, i);
						  smart_str_appendl(&command, REDIS_END_LINE, REDIS_END_LINE_LENGTH);
						  smart_str_appendl(&command, tmp, tmp_len);
			}
				break;
			case 'l':
			case 'L': {
						  long l = va_arg(ap, long);
						  char tmp[32];
						  int tmp_len = snprintf(tmp, sizeof(tmp), "%ld", l);
						  smart_str_append_long(&command, tmp_len);
						  smart_str_appendl(&command, REDIS_END_LINE, REDIS_END_LINE_LENGTH);
						  smart_str_appendl(&command, tmp, tmp_len);
			}
				break;
		}
		p++;
		smart_str_appendl(&command, REDIS_END_LINE, REDIS_END_LINE_LENGTH);

	}
	smart_str_0(&command);
	*ret = command.c;
	return command.len;
}

int execute_redis_command(php_stream *stream, zval **result, char *command, int command_len TSRMLS_DC)
{
	if (stream == NULL || command == NULL || command_len < 1){
		return FAILURE;
	}
	if (php_stream_write(stream, command, command_len)<0){
		efree(command);
		return FAILURE;
	}
	if (result != NULL){
		*result = parse_redis_response(stream TSRMLS_CC);
	}
	else{
		parse_redis_response_discard_result(stream TSRMLS_CC);
	}
	efree(command);
	return SUCCESS;
}