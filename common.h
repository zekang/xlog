#ifndef COMMON_H
#define COMMON_H
int split_string(const char *str, unsigned char split, char ***buf, int *count);
void split_string_free(char ***buf, int count);
#endif