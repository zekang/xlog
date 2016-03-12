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
#include "common.h"

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

/**{{{ int add_log(LogItem **log, int index,short level, char *application, int application_len, char *msg, int msg_len ,short flag TSRMLS_DC)
*/
int add_log(LogItem **log, int index, short level, char *application, int application_len, char *msg, int msg_len ,short flag TSRMLS_DC)
{
	if ( log == NULL  || index < 0 || msg == NULL || msg_len < 1){
		return FAILURE;
	}
	check_if_need_reset(log, &index TSRMLS_CC);
	LogItem *tmp = ecalloc(sizeof(LogItem), 1);
	int ret_flag = FAILURE;
	if (tmp == NULL){
		return FAILURE;
	}
	
	CHECK_AND_SET_VALUE_IF_NULL(application, application_len, application, default_application);

	tmp->application = (char *)ecalloc(sizeof(char), application_len + 1);
	if (tmp->application == NULL){
		goto END;
	}
	memcpy(tmp->application, application, application_len);
	
	tmp->msg = (char *)ecalloc(sizeof(char), msg_len + 1);
	if (tmp->msg == NULL){
		goto END;
	}
	memcpy(tmp->msg, msg, msg_len);
	tmp->time = time(NULL);
	tmp->level = level;
	tmp->flag = flag;
	log[index] = tmp;
	ret_flag = SUCCESS;
END:
	if (flag == FAILURE){
		if (tmp->application != NULL){
			efree(tmp->application);
		}
		if ( tmp->msg !=     NULL){
			efree(tmp->msg);
		}
		efree(tmp);
	}
	return ret_flag;
}
/**}}}*/

/**{{{ int  add_log_no_malloc_msg(LogItem **log, int index, short level, char *application, int application_len, char *msg,short flag TSRMLS_DC)
*/
int  add_log_no_malloc_msg(LogItem **log, int index, short level, char *application, int application_len, char *msg, short flag TSRMLS_DC)
{
	if (log == NULL || index < 0  || msg == NULL){
		return FAILURE;
	}
	check_if_need_reset(log, &index TSRMLS_CC);
	LogItem *tmp = ecalloc(sizeof(LogItem), 1);
	int ret_flag = FAILURE;
	if (tmp == NULL){
		return FAILURE;
	}
	CHECK_AND_SET_VALUE_IF_NULL(application, application_len, application, default_application);

	tmp->application = (char *)ecalloc(sizeof(char), application_len + 1);
	if (tmp->application == NULL){
		goto END;
	}
	memcpy(tmp->application, application, application_len);
	tmp->msg = msg;
	tmp->time = time(NULL);
	tmp->level = level;
	tmp->flag = flag;
	log[index] = tmp;
	ret_flag = SUCCESS;
END:
	if (flag == FAILURE){
		if (tmp->application != NULL){
			efree(tmp->application);
		}
		efree(tmp);
	}
	return ret_flag;
}
/**}}}*/

int	check_if_need_reset(LogItem **log,int *index TSRMLS_DC)
{
	if (*index < XLOG_G(buffer)){
		return FAILURE;
	}
	save_log_with_buffer(log TSRMLS_CC);
	*index = 0;
	XLOG_G(index) = 0;
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
			if (tmp[i]->application != NULL){
				efree(tmp[i]->application);
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

/**{{{ save_to_redis(int level,  char *application, char *content TSRMLS_DC)
*/
int save_to_redis(int level, char *application, char *content TSRMLS_DC)
{
	php_stream *stream;
	int command_len = 0;
	char *command = NULL;
	stream = XLOG_G(redis);
	if (stream == NULL){
		time_t now = time(NULL);
		if (XLOG_G(redis_fail_time) > 0 
			&& (now - XLOG_G(redis_fail_time) < XLOG_G(redis_retry_interval))
		 ){
			return FAILURE;
		}
		struct timeval tv = { 1, 0 };
		char host[64];
		char *errorstr=NULL;
		int errorno;
		php_sprintf(host, "%s:%d", XLOG_G(redis_host), XLOG_G(redis_port));
		stream = php_stream_xport_create(
			host,
			strlen(host),
			ENFORCE_SAFE_MODE,
			STREAM_XPORT_CLIENT | STREAM_XPORT_CONNECT,
			0,
			&tv,
			NULL,
			&errorstr,
			&errorno
			);
		XLOG_G(redis) = stream;
		if (stream != NULL){
			command_len = build_redis_command(&command, "SELECT", 6, "d", XLOG_G(redis_db));
			execute_redis_command(stream, NULL, command, command_len TSRMLS_CC);
			XLOG_G(redis_fail_time) = 0;
		}
		else{
			XLOG_G(redis_fail_time) = now;
			if (errorstr){
				efree(errorstr);
			}
		}
	}
	if (stream == NULL){
		return FAILURE;
	}
	char key[256] = {0};
	char day[16] = { 0 };
	time_t now = time(NULL);
	strftime(day, 16, "%Y%m%d", localtime(&now));
	int app_len = -1;
	CHECK_AND_SET_VALUE_IF_NULL(application, app_len, application, default_application);

	php_sprintf(key, "%s_%s_%s_%s", XLOG_G(host), application, day, get_log_level_name(level));
		
	command_len = build_redis_command(&command, "LPUSH", 5, "ss", key, strlen(key), content, strlen(content));
	execute_redis_command(stream, NULL, command, command_len TSRMLS_CC);
	return SUCCESS;
}
/**}}}*/

/**{{{ void save_to_mail(int level,  char *application, char *content TSRMLS_DC)
*/
void save_to_mail(int level, char *application, char *content TSRMLS_DC)
{
	if (level <0 || level>8 || content == NULL){
		return;
	}
	zval *commands;
	int app_len = -1;
	CHECK_AND_SET_VALUE_IF_NULL(application, app_len, application, default_application);
	char subject[256] = { 0 };
	php_sprintf(subject, "%s-%s-%s", XLOG_G(host), application, get_log_level_name(level));
	zend_bool result = build_mail_commands(
		&commands,
		XLOG_G(mail_username),
		XLOG_G(mail_password),
		XLOG_G(mail_from),
		XLOG_G(mail_from_name),
		XLOG_G(mail_to),
		subject,
		content
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

/**{{{ void save_to_file(int level, char *application, char *content,int content_len TSRMLS_DC)
*/
void save_to_file(int level, char *application, char *content,int content_len TSRMLS_DC)
{
	if (level < 0 || level >8 || content == NULL){
		return;
	}
	int app_len = -1;
	CHECK_AND_SET_VALUE_IF_NULL(application, app_len, application, default_application);
	php_stream *stream = get_file_handle_from_cache(level, application TSRMLS_CC);
	if (stream != NULL){
		php_stream_write(stream,content,content_len);
	}

}
/**}}}*/


/**{{{ void save_log_no_buffer(int level,char* application, char *content,short flag  TSRMLS_DC)
*/
void save_log_no_buffer(int level, char* application, char *content ,short flag TSRMLS_DC)
{
	if (level < 0 || level >8 || content == NULL){
		return;
	}
	
	char buf[32] = { 0 };
	time_t now = time(NULL);
	char *msg;
	int len = -1;
	CHECK_AND_SET_VALUE_IF_NULL(application, len, application, default_application);

	strftime(buf, 32, "%Y-%m-%d %H:%M:%S", localtime(&now));
	len = spprintf(&msg, 0, "%s | %s | %s | %s\n", get_log_level_name(level), buf, application, content);
	if (XLOG_G(redis_enable)){
		save_to_redis(level, application, msg TSRMLS_CC);
	}
	if (XLOG_G(mail_enable) && flag == XLOG_FLAG_SEND_MAIL && (level >= XLOG_G(mail_level))){
		save_to_mail(level, application, msg TSRMLS_CC);
	}
	if (XLOG_G(file_enable)){
		save_to_file(level, application, msg,len TSRMLS_CC);
	}
	if (len > 0){
		efree(msg);
	}
}
/**}}}*/

/**{{{ void save_log_with_buffer(LogItem **log TSRMLS_DC)
*/
void save_log_with_buffer(LogItem **log TSRMLS_DC)
{
	char buf[32] = { 0 };
	char *msg;
	int len;
	for (int i = 0; i < XLOG_G(buffer); i++){
		if (log[i] == NULL)
			continue;
		strftime(buf, 32, "%Y-%m-%d %H:%M:%S", localtime(&(log[i]->time)));
		len = spprintf(&msg, 0, "%s | %s | %s | %s\n", get_log_level_name(log[i]->level), buf, log[i]->application, log[i]->msg);
		if (XLOG_G(redis_enable)){
			save_to_redis(log[i]->level, log[i]->application, msg TSRMLS_CC);
		}

		if (XLOG_G(mail_enable) && log[i]->flag == XLOG_FLAG_SEND_MAIL && (log[i]->level >= XLOG_G(mail_level))){
			save_to_mail(log[i]->level, log[i]->application, msg TSRMLS_CC);
		}

		if (XLOG_G(file_enable)){
			save_to_file(log[i]->level, log[i]->application, msg,len TSRMLS_CC);
		}
		efree(msg);
		if (log[i]->application != NULL){
			efree(log[i]->application);
		}
		if (log[i]->msg != NULL){
			efree(log[i]->msg);
		}
		efree(log[i]);
		log[i] = NULL;
	}
}
/**}}}*/

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

/**{{{ php_stream *get_file_handle_from_cache(int level, char *application TSRMLS_DC)
*/
php_stream *get_file_handle_from_cache(int level, char *application TSRMLS_DC)
{
	HashTable *file_handle = XLOG_G(file_handle);
	char key[256] = { 0 };
	int key_len;
	FileHandleCache *cache = NULL;
	php_stream *stream = NULL;
	FileHandleCache **tmp = NULL;
	if (file_handle == NULL){
		ALLOC_HASHTABLE(file_handle);
		zend_hash_init(file_handle, 8, NULL, file_handle_cache_ptr_dtor_wapper, 0);
		XLOG_G(file_handle) = file_handle;
	}
	key_len = php_sprintf(key, "%d_%s", level, application);
	if (zend_hash_find(file_handle, key, key_len + 1, (void **)&tmp) == FAILURE){
		char *dir = NULL, *file = NULL;
		char day[16] = { 0 };
		time_t now = time(NULL);
		strftime(day, 16, "%Y%m%d", localtime(&now));
		//logpath.host.application
		spprintf(&dir, 0, "%s%c%s%c%s", 
			XLOG_G(path) == NULL ? XLOG_G(default_path) : XLOG_G(path), 
			XLOG_DIRECTORY_SEPARATOR,
			XLOG_G(host),
			XLOG_DIRECTORY_SEPARATOR,
			application
			);
		//level.day
		spprintf(&file, 0, "%s%c%s.%s.log", 
			dir, 
			XLOG_DIRECTORY_SEPARATOR, 
			get_log_level_name(level), 
			day);
		
		cache = (FileHandleCache *)emalloc(sizeof(FileHandleCache)* 1);
		cache->filename = file;
		cache->application = estrdup(application);
		cache->level = level;
		cache->write_count = 0;
		cache->stream = NULL;
		zend_hash_add(file_handle, key, key_len + 1, &cache,sizeof(FileHandleCache **), NULL);
		if (xlog_make_log_dir(dir TSRMLS_CC) == SUCCESS){
			cache->stream = stream = php_stream_open_wrapper(file, "a", IGNORE_URL_WIN | ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
		}
		efree(dir);
	}
	else{
		cache = *tmp;
	}
	if (cache != NULL){
		cache->write_count++;
		return cache->stream;
	}
	return NULL;
}
/**}}}*/

/**{{{ void file_handle_cache_ptr_dtor_wapper(FileHandleCache **pCache)
*/
void file_handle_cache_ptr_dtor_wapper(FileHandleCache **pCache)
{
	if (pCache == NULL || *pCache == NULL){
		return;
	}
	TSRMLS_FETCH();
	FileHandleCache *cache = *pCache;
	if (cache->application != NULL){
		efree(cache->application);
	}
	if (cache->filename != NULL){
		efree(cache->filename);
	}
	if (cache->stream != NULL){
		php_stream_close(cache->stream);
		php_stream_free(cache->stream, PHP_STREAM_FREE_RELEASE_STREAM);
	}
	efree(cache);
}
/**}}}*/