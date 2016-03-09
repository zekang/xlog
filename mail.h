#ifndef MAIL_H
#define MAIL_H

#define ADD_MAIL_COMMAND(array,command,command_len,duplicate, code) \
do{\
zval *__tmp;			\
MAKE_STD_ZVAL(__tmp);	\
array_init(__tmp);  	\
add_next_index_stringl(__tmp, (command), (command_len), (duplicate)); \
add_next_index_long(__tmp, (code)); \
add_next_index_zval((array), __tmp); \
} while (0);

int build_mail_commands(zval **result, char *username, char *password, char *from, char *fromName, char *to, char *subject, char *body TSRMLS_DC);
int mail_send(char *smtp, int port, zval *commands, int ssl TSRMLS_DC);
#endif // MAIL_H
