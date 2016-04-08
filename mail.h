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
#ifndef MAIL_H
#define MAIL_H
struct _error_line
{
	time_t time;
	uint hash;
	size_t len;
	int count;
	int error_no;
};
typedef struct _error_line ErrorLine;
#define ERROR_LINE_SIZE sizeof(ErrorLine)
#define ERROR_MSG_MAX_LEN 256
#define ADD_MAIL_COMMAND(array,command,command_len,duplicate, code) \
	do{ \
		zval *__tmp;			\
		MAKE_STD_ZVAL(__tmp);	\
		array_init(__tmp);  	\
		add_next_index_stringl(__tmp, (command), (command_len), (duplicate)); \
		add_next_index_long(__tmp, (code)); \
		add_next_index_zval((array), __tmp); \
	} while (0);
#define mail_strategy(level,application,module,error_str,error_no) \
	(XLOG_G(mail_strategy_enable) ? mail_strategy_file(level, application, module, error_str, error_no TSRMLS_CC) : SUCCESS)
int build_mail_commands(zval **result, char *username, char *password, char *from, char *fromName, char *to, char *subject, char *body TSRMLS_DC);
int mail_send(char *smtp, int port, zval *commands, int ssl TSRMLS_DC);
int mail_strategy_file(int level, const char *application, const char *module, const char *error_str, int error_no TSRMLS_DC);
#endif // MAIL_H
