#ifndef COMMON_H
#define COMMON_H

#define XLOG_ALL                         "all"
#define XLOG_DEBUG                       "debug"
#define XLOG_INFO                        "info"
#define XLOG_NOTICE                      "notice"
#define XLOG_WARNING                     "warning"
#define XLOG_ERROR                       "error"
#define XLOG_CRITICAL                    "critical"
#define XLOG_ALERT                       "alert"
#define XLOG_EMERGENCY                   "emergency"

int split_string(const char *str, unsigned char split, char ***buf, int *count);
void split_string_free(char ***buf, int count);


void (*old_error_cb)(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args);
void xlog_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args);
void (*old_throw_exception_hook)(zval *exception TSRMLS_DC);
void xlog_throw_exception_hook(zval *exception TSRMLS_DC);
void init_error_hooks(TSRMLS_D);
void restore_error_hooks(TSRMLS_D);
void save_log(char *level,char *errmsg TSRMLS_DC);
int  get_serialize_debug_trace(char **ret, int *ret_len TSRMLS_DC);
int  get_print_data(char **ret, int *ret_len TSRMLS_DC);
#endif