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

/* {{{ int init_log(LogItem ***log, int size TSRMLS_DC)
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
/* }}}*/

/* {{{ int add_log(LogItem **log, int index,short level, char *module, int module_len, char *content, int content_len ,short flag TSRMLS_DC)
*/
int add_log(LogItem **log, int index, short level, char *module, int module_len, char *content, int content_len, short flag TSRMLS_DC)
{
	if (level < XLOG_LEVEL_MIN || level > XLOG_LEVEL_MAX || log == NULL || index < 0 || content == NULL || content_len < 1){
		return FAILURE;
	}
	check_if_need_reset(log, &index TSRMLS_CC);
	LogItem *tmp = ecalloc(sizeof(LogItem), 1);
	int ret_flag = FAILURE;
	if (tmp == NULL){
		return FAILURE;
	}
	
	CHECK_AND_SET_VALUE_IF_NULL(module, module_len, module, default_module);
	tmp->module = (char *)ecalloc(sizeof(char), module_len + 1);
	if (tmp->module == NULL){
		goto END;
	}
	memcpy(tmp->module, module, module_len);
	tmp->content = (char *)ecalloc(sizeof(char), content_len + 1);
	if (tmp->content == NULL){
		goto END;
	}

	memcpy(tmp->content, content, content_len);
	tmp->time = time(NULL);
	tmp->level = level;
	tmp->flag = flag;
	log[index] = tmp;
	ret_flag = SUCCESS;
END:
	if (flag == FAILURE){
		log_free_item(&tmp);
	}
	return ret_flag;
}
/* }}}*/

/* {{{ int  add_log_no_malloc_msg(LogItem **log, int index, short level, char *module, int module_len, char *content,short flag TSRMLS_DC)
*/
int  add_log_no_malloc_msg(LogItem **log, int index, short level, char *module, int module_len, char *content, short flag TSRMLS_DC)
{
	if (log == NULL || index < 0 || content == NULL || level < XLOG_LEVEL_MIN || level > XLOG_LEVEL_MAX){
		return FAILURE;
	}
	check_if_need_reset(log, &index TSRMLS_CC);
	LogItem *tmp = ecalloc(sizeof(LogItem), 1);
	int ret_flag = FAILURE;
	if (tmp == NULL){
		return FAILURE;
	}
	tmp->content = content;

	CHECK_AND_SET_VALUE_IF_NULL(module, module_len, module, default_module);
	tmp->module = (char *)ecalloc(sizeof(char), module_len + 1);
	if (tmp->module == NULL){
		goto END;
	}
	memcpy(tmp->module, module, module_len);
	tmp->time = time(NULL);
	tmp->level = level;
	tmp->flag = flag;
	log[index] = tmp;
	ret_flag = SUCCESS;
END:
	if (flag == FAILURE){
		log_free_item(&tmp);
	}
	return ret_flag;
}
/* }}}*/

/* {{{ int	check_if_need_reset(LogItem **log,int *index TSRMLS_DC)
*/
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
/* }}}*/

/* {{{ int log_free_item(LogItem **log)
*/
int log_free_item(LogItem **log)
{
	if (log == NULL || *log == NULL){
		return FAILURE;
	}
	LogItem *tmp = *log;
	if (tmp->module != NULL){
		efree(tmp->module);
	}
	if (tmp->content != NULL){
		efree(tmp->content);
	}
	efree(tmp);
	*log = NULL;
	return SUCCESS;
}
/* }}}*/


/* {{{ int destory_log(LogItem ***log, int size TSRMLS_DC)
*/
int destory_log(LogItem ***log, int size TSRMLS_DC)
{
	if (log == NULL || *log == NULL || size < 0){
		return FAILURE;
	}
	LogItem **tmp = *log;
	int i;
	for (i = 0; i < size; i++){
		log_free_item(&tmp[i]);
	}
	efree(tmp);
	*log = NULL;
	return SUCCESS;
}
/* }}}*/

/* {{{ save_to_redis(int level,  char *module, char *content TSRMLS_DC)
*/
int save_to_redis(int level, char *module, char *content TSRMLS_DC)
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
			int auth_len = strlen(XLOG_G(redis_auth));
			if (auth_len > 0){
				zval *result =NULL;
				command_len = build_redis_command(&command, "AUTH", 4, "s", XLOG_G(redis_auth),auth_len);
				execute_redis_command(stream, &result, command, command_len TSRMLS_CC);
				if (result && !zend_is_true(result)){
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "the redis auth password is invalid");
					zval_dtor(result);
					efree(result);
					return FAILURE;
				}
			}
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
	int len = -1;
	len = php_sprintf(key, "%s_%s_%s", module, day, get_log_level_name(level));
		
	command_len = build_redis_command(&command, "LPUSH", 5, "ss", key, len, content, strlen(content));
	execute_redis_command(stream, NULL, command, command_len TSRMLS_CC);
	return SUCCESS;
}
/* }}}*/



/* {{{ save_to_redis_with_model(int level,   char *module, char *content TSRMLS_DC)
*/
int save_to_redis_with_model(int level, char *module, char *content TSRMLS_DC)
{
	php_stream *stream;
	int command_len = 0;
	char *command = NULL;
	char buf_microtime[64] = { 0 };
	stream = XLOG_G(redis);
	if (xlog_get_microtime(buf_microtime, sizeof(buf_microtime)-1,XLOG_G(redis_counter))==FAILURE){
		return FAILURE;
	}
	XLOG_G(redis_counter) += 1;
	if (stream == NULL){
		time_t now = time(NULL);
		if (XLOG_G(redis_fail_time) > 0
			&& (now - XLOG_G(redis_fail_time) < XLOG_G(redis_retry_interval))
			){
			return FAILURE;
		}
		struct timeval tv = { 1, 0 };
		char host[64];
		char *errorstr = NULL;
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
			int auth_len = strlen(XLOG_G(redis_auth));
			if (auth_len > 0){
				zval *result = NULL;
				command_len = build_redis_command(&command, "AUTH", 4, "s", XLOG_G(redis_auth), auth_len);
				execute_redis_command(stream, &result, command, command_len TSRMLS_CC);
				if (result && !zend_is_true(result)){
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "the redis auth password is invalid");
					zval_dtor(result);
					efree(result);
					return FAILURE;
				}
			}
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
	char key[256] = { 0 };
	int len = -1, buf_microtime_len = strlen(buf_microtime);
	
	CHECK_AND_SET_VALUE_IF_NULL(module, len, module, default_module);
	len = php_sprintf(key, "xlog_%s",  module);
	command_len = build_redis_command(&command, "RPUSH", 5, "ss", key, len, buf_microtime, buf_microtime_len);
	execute_redis_command(stream, NULL, command, command_len TSRMLS_CC);
	len = php_sprintf(key, "xlog_%s:a:%s", module, buf_microtime);
	command_len = build_redis_command(&command, "HMSET", 5, 
		"ssssdss", 
		key, len, 
		"time",4,
		buf_microtime, buf_microtime_len,
		"level",5,
		level,
		"content",7,
		content, strlen(content)
		);
	execute_redis_command(stream, NULL, command, command_len TSRMLS_CC);
	return SUCCESS;
}
/* }}}*/

#ifdef MAIL_ENABLE
/* {{{ void save_to_mail(int level, char *module, char *content TSRMLS_DC)
*/
void save_to_mail(int level,char *module, char *content TSRMLS_DC)
{
	if (content == NULL){
		return;
	}
	zval *commands;
	int len = -1;
	CHECK_AND_SET_VALUE_IF_NULL(module, len, module, default_module);
	char subject[256] = { 0 };
	php_sprintf(subject, "%s-%s", module, get_log_level_name(level));
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
/* }}}*/
#endif

/* {{{ void save_to_file(int level, char *module, char *content,int content_len TSRMLS_DC)
*/
void save_to_file(int level,char *module, char *content,int content_len TSRMLS_DC)
{
	if (content == NULL){
		return;
	}
	int len = -1;
	CHECK_AND_SET_VALUE_IF_NULL(module, len, module, default_module);
	php_stream *stream = get_file_handle_from_cache(level, module TSRMLS_CC);
	if (stream != NULL){
		php_stream_write(stream,content,content_len);
	}

}
/* }}}*/


/* {{{ void save_log_no_buffer(int level,char* module, char *content,short flag  TSRMLS_DC)
*/
void save_log_no_buffer(int level, char* module, char *content ,short flag TSRMLS_DC)
{
	if (level < XLOG_LEVEL_MIN || level > XLOG_LEVEL_MAX || content == NULL){
		return;
	}
	char buf[32] = { 0 };
	time_t now = time(NULL);
	char *msg;
	int len = -1;
	CHECK_AND_SET_VALUE_IF_NULL(module, len, module, default_module);
	strftime(buf, 32, "%Y-%m-%d %H:%M:%S", localtime(&now));
	len = spprintf(&msg, 0, "%s | %s | %s | %s\n", get_log_level_name(level), buf, module, content);
	if (XLOG_G(redis_enable)){
		save_to_redis_with_model(level, module, content TSRMLS_CC);
	}
#ifdef MAIL_ENABLE
	if (XLOG_G(mail_enable) 
		&& flag == XLOG_FLAG_SEND_MAIL 
		&& (level >= XLOG_G(mail_level))
		&& mail_strategy(level, module, content, 0) == SUCCESS
		){
		save_to_mail(level, module, msg TSRMLS_CC);
	}
#endif
	if (XLOG_G(file_enable)){
		save_to_file(level, module, msg, len TSRMLS_CC);
	}
	if (len > 0){
		efree(msg);
	}
}
/* }}}*/

/* {{{ void save_log_with_buffer(LogItem **log TSRMLS_DC)
*/
void save_log_with_buffer(LogItem **log TSRMLS_DC)
{
	char buf[32] = { 0 };
	char *msg;
	int len,i;
	for (i = 0; i < XLOG_G(buffer); i++){
		if (log[i] == NULL)
			continue;
		strftime(buf, 32, "%Y-%m-%d %H:%M:%S", localtime(&(log[i]->time)));
		len = spprintf(&msg, 0, "%s | %s | %s | %s\n", get_log_level_name(log[i]->level), buf,log[i]->module, log[i]->content);

		if (XLOG_G(redis_enable)){
			save_to_redis_with_model(log[i]->level, log[i]->module, log[i]->content TSRMLS_CC);
		}
#ifdef MAIL_ENABLE
		if (XLOG_G(mail_enable) 
			&& log[i]->flag == XLOG_FLAG_SEND_MAIL 
			&& (log[i]->level >= XLOG_G(mail_level))
			&& mail_strategy(log[i]->level, log[i]->module, log[i]->content, 0) == SUCCESS
			){
			save_to_mail(log[i]->level, log[i]->module, msg TSRMLS_CC);
		}
#endif
		if (XLOG_G(file_enable)){
			save_to_file(log[i]->level,log[i]->module, msg, len TSRMLS_CC);
		}
		efree(msg);
		log_free_item(&log[i]);
	}
}
/* }}}*/

/* {{{ char* get_log_level_name(int level)
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
	case XLOG_LEVEL_WITH_STACKINFO:	name = XLOG_WITH_STACKINFO; break;
	case XLOG_LEVEL_ELAPSE_TIME:	name = XLOG_ELAPSE_TIME; break;
	default					:	break;
	}
	return name;
}
/* }}}*/

/* {{{ php_stream *get_file_handle_from_cache(int level,char *module TSRMLS_DC)
*/
php_stream *get_file_handle_from_cache(int level,char *module TSRMLS_DC)
{
	HashTable *file_handle = XLOG_G(file_handle);
	char key[256] = { 0 };
	int key_len;
	FileHandleCache *cache = NULL;
	FileHandleCache **tmp = NULL;
	struct stat sb = { 0 };
	if (file_handle == NULL){
		ALLOC_HASHTABLE(file_handle);
		zend_hash_init(file_handle, 8, NULL, (void(*)(void *))file_handle_cache_ptr_dtor_wapper, 0);
		XLOG_G(file_handle) = file_handle;
	}
	key_len = php_sprintf(key, "%d_%s", level, module);
	if (zend_hash_find(file_handle, key, key_len + 1, (void **)&tmp) == FAILURE){
		char *dir = NULL, *file = NULL;
		char day[16] = { 0 };
		time_t now = time(NULL);
		strftime(day, 16, "%Y%m%d", localtime(&now));
		//logpath.module
		spprintf(&dir, 0, "%s%c%s", 
			XLOG_G(path) == NULL ? XLOG_G(default_path) : XLOG_G(path), 
			XLOG_DIRECTORY_SEPARATOR,
			module
			);
		//dir.level.day.log
		int len = spprintf(&file, 0, "%s%c%s.%s.log", 
			dir, 
			XLOG_DIRECTORY_SEPARATOR, 
			get_log_level_name(level), 
			day);
		if (XLOG_G(rotate_enable)){
			if (VCWD_STAT(file, &sb) == 0){
				if (sb.st_size > XLOG_G(rotate_size) * 1024){
					rotate_file(file, len, XLOG_G(rotate_max) TSRMLS_CC);
				}
			}
		}
		cache = (FileHandleCache *)emalloc(sizeof(FileHandleCache)* 1);
		cache->filename = file;
		cache->level = level;
		cache->write_count = 0;
		cache->stream = NULL;
		zend_hash_add(file_handle, key, key_len + 1, &cache,sizeof(FileHandleCache **), NULL);
		if (xlog_make_log_dir(dir TSRMLS_CC) == SUCCESS){
			cache->stream = php_stream_open_wrapper(file, "a", IGNORE_URL_WIN | ENFORCE_SAFE_MODE | REPORT_ERRORS, NULL);
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
/* }}}*/

/* {{{ void file_handle_cache_ptr_dtor_wapper(FileHandleCache **pCache)
*/
void file_handle_cache_ptr_dtor_wapper(FileHandleCache **pCache)
{
	if (pCache == NULL || *pCache == NULL){
		return;
	}
	TSRMLS_FETCH();
	FileHandleCache *cache = *pCache;
	if (cache->filename != NULL){
		efree(cache->filename);
	}
	if (cache->stream != NULL){
		php_stream_close(cache->stream);
	}
	efree(cache);
}
/* }}}*/

/* {{{ zend_bool rotate_file(const char *filename,int filename_len,int max TSRMLS_DC);
*/
zend_bool rotate_file(const char *filename, int filename_len, int max TSRMLS_DC)
{
	int i;
	char source[256];
	char dest[256];
	char *oldname;
	
	if (filename == NULL || filename_len > 240 || filename_len < 1 || max < 1) {
		return FAILURE;
	}
	for (i = max; i>=0; i--){
		if (i == 0){
			oldname = (char *)filename;
		}
		else{
			php_sprintf(source, "%s.%d", filename, i);
			oldname = source;
		}
		struct stat sb = { 0 };
		if (VCWD_STAT(oldname, &sb) == 0){
			if (S_ISREG(sb.st_mode)){
				if (i == max){
					VCWD_UNLINK(oldname);
				}
				else{
					php_sprintf(dest, "%s.%d", filename, i+1);
					VCWD_RENAME(oldname, dest);
				}
			}
		}
	}
	return  SUCCESS;
}
/* }}}*/