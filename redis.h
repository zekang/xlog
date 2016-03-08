#ifndef REDIS_H
#define REDIS_H

#define REDIS_END_LINE "\r\n"
#define REDIS_END_LINE_LENGTH 2
int build_redis_command(char **ret, char *keywokd, int keyword_len, char *format, ...);
int execute_redis_command(php_stream *stream, zval **result, char *command, int command_len TSRMLS_DC);
zval * parse_redis_response(php_stream *stream TSRMLS_DC);
int  parse_redis_response_discard_result(php_stream *stream TSRMLS_DC);
#endif