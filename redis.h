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
#ifndef REDIS_H
#define REDIS_H

#define REDIS_END_LINE "\r\n"
#define REDIS_END_LINE_LENGTH 2


int		build_redis_command(char **ret, char *keywokd, int keyword_len, char *format, ...);
int		execute_redis_command(php_stream *stream, zval **result, char *command, int command_len TSRMLS_DC);
zval*	parse_redis_response(php_stream *stream TSRMLS_DC);
int		parse_redis_response_discard_result(php_stream *stream TSRMLS_DC);

#endif

