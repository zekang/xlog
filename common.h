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
#ifndef COMMON_H
#define COMMON_H


#define XLOG_CONTEXT_KEY_CONTROL         "abcdefghijklmnopqrstuvwzxyABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_."
#define XLOG_CONTEXT_KEY_MAX_LEN          256
#define XLOG_CONTEXT_KEY_LEFT_DEILM		  '{'
#define XLOG_CONTEXT_KEY_RIGHT_DEILM	  '}'

int  split_string(const char *str, unsigned char split, char ***buf, int *count);
void split_string_free(char ***buf, int count);
void (*old_error_cb)(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args);
void xlog_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args);
void (*old_throw_exception_hook)(zval *exception TSRMLS_DC);
void xlog_throw_exception_hook(zval *exception TSRMLS_DC);
void init_error_hooks(TSRMLS_D);
void restore_error_hooks(TSRMLS_D);

int  get_debug_backtrace(zval *debug, TSRMLS_D);
int  get_serialize_debug_trace(char **ret, int *ret_len TSRMLS_DC);
int  get_print_data(char **ret, int *ret_len TSRMLS_DC);
int  get_var_export_data(char **ret, int *ret_len TSRMLS_DC);
int  strtr_array(const char *template, int template_len, zval *context, char **ret, int *ret_len TSRMLS_DC);
int  xlog_make_log_dir(char *dir TSRMLS_DC);
#endif