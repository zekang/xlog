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
#include "php.h"
#include "log.h"
#include "php_xlog.h"
#include "redis.h"
#include "mail.h"

/**{{{ int init_log(LogItem ***log, int size TSRMLS_DC)
*/
int init_log(LogItem ***log, int size TSRMLS_DC)
{
	if (log == NULL || size < 0){
		return FAILURE;
	}
	*log = ecalloc(sizeof(LogItem *), size);
	if (*log == NULL){
		return FAILURE;
	}
	return SUCCESS;
}
/**}}}*/

/**{{{ int add_log(LogItem **log, int index,int level, char *app_name, int app_name_len, char *msg, int msg_len TSRMLS_DC)
*/
int add_log(LogItem **log, int index, int level, char *app_name, int app_name_len, char *msg, int msg_len TSRMLS_DC)
{
	if (log == NULL || index < 0 || app_name == NULL || app_name_len < 1 || msg == NULL || msg_len < 1){
		return FAILURE;
	}
	LogItem *tmp = ecalloc(sizeof(LogItem), 1);
	int flag = FAILURE;
	if (tmp == NULL){
		return FAILURE;
	}
	tmp->app_name = (char *)ecalloc(sizeof(char), app_name_len + 1);
	if (tmp->app_name == NULL){
		goto END;
	}
	memcpy(tmp->app_name, app_name, app_name_len);
	
	tmp->msg = (char *)ecalloc(sizeof(char), msg_len + 1);
	if (tmp->msg == NULL){
		goto END;
	}
	memcpy(tmp->msg, msg, msg_len);
	tmp->time = time(NULL);
	log[index] = tmp;
	flag = SUCCESS;
END:
	if (flag == FAILURE){
		if (tmp->app_name != NULL){
			efree(tmp->app_name);
		}
		if ( tmp->msg !=     NULL){
			efree(tmp->msg);
		}
		efree(tmp);
	}
	return flag;
}
/**}}}*/

/**{{{ int  add_log_no_malloc_msg(LogItem **log, int index, int level, char *app_name, int app_name_len, char *msg TSRMLS_DC)
*/
int  add_log_no_malloc_msg(LogItem **log, int index, int level, char *app_name, int app_name_len, char *msg TSRMLS_DC)
{
	if (log == NULL || index < 0 || app_name == NULL || app_name_len < 1 || msg == NULL){
		return FAILURE;
	}
	LogItem *tmp = ecalloc(sizeof(LogItem), 1);
	int flag = FAILURE;
	if (tmp == NULL){
		return FAILURE;
	}
	tmp->app_name = (char *)ecalloc(sizeof(char), app_name_len + 1);
	if (tmp->app_name == NULL){
		goto END;
	}
	memcpy(tmp->app_name, app_name, app_name_len);
	tmp->msg = msg;
	tmp->time = time(NULL);
	tmp->level = level;
	log[index] = tmp;
	flag = SUCCESS;
END:
	if (flag == FAILURE){
		if (tmp->app_name != NULL){
			efree(tmp->app_name);
		}
		efree(tmp);
	}
	return flag;
}
/**}}}*/

/**{{{ int destory_log(LogItem ***log, int size TSRMLS_DC)
*/
int destory_log(LogItem ***log, int size TSRMLS_DC)
{
	if (log == NULL || size < 0){
		return FAILURE;
	}
	LogItem **tmp = *log;
	for (int i = 0; i < size; i++){
		if (tmp[i] != NULL){
			if (tmp[i]->app_name != NULL){
				efree(tmp[i]->app_name);
			}
			if (tmp[i]->msg != NULL){
				efree(tmp[i]->msg);
			}
			efree(tmp[i]);
			tmp[i] = NULL;
		}
	}
	efree(tmp);
	*log = NULL;
	return SUCCESS;
}
/**}}}*/

/**{{{ save_to_redis(char *level, char *errmsg TSRMLS_DC)
*/
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
			ENFORCE_SAFE_MODE,
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
	command_len = build_redis_command(&command, "LPUSH", 5, "ss", level, strlen(level), errmsg, strlen(errmsg));
	execute_redis_command(stream, NULL, command, command_len TSRMLS_CC);
	return SUCCESS;
}
/**}}}*/

/**{{{ save_to_mail(char *level, char *errmsg TSRMLS_DC)
*/
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
/**}}}*/

/**{{{ void save_log(char *level, char *errmsg TSRMLS_DC)
*/
void save_log(char *level, char *errmsg TSRMLS_DC)
{
	if (level == NULL || errmsg == NULL){
		return;
	}
	if (XLOG_G(redis_enable)){
		save_to_redis(level, errmsg TSRMLS_CC);
	}
	if (XLOG_G(send_mail) && strstr(XLOG_G(send_mail_level), level) != NULL){
		save_to_mail(level, errmsg TSRMLS_CC);
	}
}
/**}}}*/
