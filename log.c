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
	*log = (LogItem **)ecalloc(sizeof(LogItem *), size);
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
	if ( log == NULL  || index < 0 || app_name == NULL || app_name_len < 1 || msg == NULL || msg_len < 1){
		return FAILURE;
	}
	check_if_need_reset(log, &index TSRMLS_CC);
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
	tmp->level = level;
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
	check_if_need_reset(log, &index TSRMLS_CC);
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

int	check_if_need_reset(LogItem **log,int *index TSRMLS_DC)
{
	if (*index < XLOG_G(log_buffer)){
		return FAILURE;
	}
	save_log_with_buffer(log TSRMLS_CC);
	*index = 0;
	XLOG_G(log_index) = 0;
	return SUCCESS;
}


/**{{{ int destory_log(LogItem ***log, int size TSRMLS_DC)
*/
int destory_log(LogItem ***log, int size TSRMLS_DC)
{
	if (log == NULL || *log == NULL || size < 0){
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

/**{{{ save_to_redis(int level, char *errmsg TSRMLS_DC)
*/
int save_to_redis(int level, char *errmsg TSRMLS_DC)
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
	char *key = get_log_level_name(level);
	command_len = build_redis_command(&command, "LPUSH", 5, "ss", key, strlen(key), errmsg, strlen(errmsg));
	execute_redis_command(stream, NULL, command, command_len TSRMLS_CC);
	return SUCCESS;
}
/**}}}*/

/**{{{ save_to_mail(int level, char *errmsg TSRMLS_DC)
*/
void save_to_mail(int level, char *errmsg TSRMLS_DC)
{
	zval *commands;
	char *subject = get_log_level_name(level);
	zend_bool result = build_mail_commands(
		&commands,
		XLOG_G(mail_username),
		XLOG_G(mail_password),
		XLOG_G(mail_from),
		XLOG_G(mail_from_name),
		XLOG_G(mail_to),
		subject,
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

/**{{{ void save_log_no_buffer(int level,char* app_name, char *errmsg TSRMLS_DC)
*/
void save_log_no_buffer(int level, char* app_name,char *errmsg TSRMLS_DC)
{
	if (level < 0 || level >8 || errmsg == NULL){
		return;
	}
	char buf[32] = { 0 };
	time_t now = time(NULL);
	char *tmp;
	if (app_name == NULL){
		app_name = "club";
	}
	strftime(buf, 32, "%Y-%m-%d %H:%M:%S", localtime(&now));
	spprintf(&tmp, 0, "level:%s,time:%s,app:%s,msg:%s\n", get_log_level_name(level), buf, app_name, errmsg);
	if (XLOG_G(redis_enable)){
		save_to_redis(level, tmp TSRMLS_CC);
	}
	/*
	if (XLOG_G(send_mail) && level >= XLOG_G(send_mail_level)){
		save_to_mail(level, tmp TSRMLS_CC);
	}
	*/
	efree(tmp);
}
/**}}}*/


void save_log_with_buffer(LogItem **log TSRMLS_DC)
{
	char buf[32] = { 0 };
	char *tmp;
	for (int i = 0; i < XLOG_G(log_buffer); i++){
		if (log[i] == NULL)
			continue;
		strftime(buf, 32, "%Y-%m-%d %H:%M:%S", localtime(&(log[i]->time)));
		spprintf(&tmp, 0, "level:%s,time:%s,app:%s,msg:%s\n", get_log_level_name(log[i]->level), buf, log[i]->app_name, log[i]->msg);
		if (XLOG_G(redis_enable)){
			save_to_redis(log[i]->level, tmp TSRMLS_CC);
		}
		/*
		if (XLOG_G(send_mail) && log[i]->level >= XLOG_G(send_mail_level)){
			save_to_mail(log[i]->level,tmp TSRMLS_CC);
		}
		*/
		efree(tmp);
		if (log[i]->app_name != NULL){
			efree(log[i]->app_name);
		}
		if (log[i]->msg != NULL){
			efree(log[i]->msg);
		}
		efree(log[i]);
		log[i] = NULL;
	}
}


/**{{{ char* get_log_level_name(int level)
*/
char* get_log_level_name(int level)
{
	char *name = NULL;
	switch (level){
	case XLOG_LEVEL_ALL		:	name = XLOG_ALL;break;
	case XLOG_LEVEL_DEBUG	:	name = XLOG_DEBUG; break;
	case XLOG_LEVEL_INFO	:	name = XLOG_INFO; break;
	case XLOG_LEVEL_NOTICE	:	name = XLOG_NOTICE; break;
	case XLOG_LEVEL_WARNING :	name = XLOG_WARNING; break;
	case XLOG_LEVEL_ERROR	:	name = XLOG_ERROR; break;
	case XLOG_LEVEL_CRITICAL:	name = XLOG_CRITICAL; break;
	case XLOG_LEVEL_ALERT	:	name = XLOG_ALERT; break;
	case XLOG_LEVEL_EMERGENCY:	name = XLOG_EMERGENCY; break;
	default					:	break;
	}
	return name;
}
/**}}}*/