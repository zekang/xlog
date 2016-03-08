#define _CRT_SECURE_NO_WARNINGS
#include "php.h"
#include "common.h"

int split_string(const char *str, unsigned char split, char ***buf, int *count)
{
	int i = 0;
	const char *p = str;
	const char *last = str;
	char **tmp = NULL;
	int ret = SUCCESS;
	int lines = 0;
	while ((p = strchr(p, split)) != NULL){
		lines++;
		p++;//Ìø¹ýsplit×Ö·û
		last = p;
	}
	if (*last != '\0'){
		lines++;
	}
	tmp = (char **)ecalloc(lines , sizeof(char *));
	if (tmp == NULL){
		ret = FAILURE;
		goto END;
	}
	p = str;
	last = str;
	while ((p = strchr(p, split)) != NULL){
		tmp[i] = ecalloc((p - last) + 1, sizeof(char));
		if (tmp[i] == NULL){
			ret = FAILURE;
			goto END;
		}
		strncpy(tmp[i], last, p - last);
		i++;
		p++;//Ìø¹ýsplit×Ö·û
		last = p;
	}
	if (*last != '\0'){
		tmp[i] = ecalloc(strlen(last) + 1, sizeof(char));
		if (tmp[i] == NULL){
			ret = FAILURE;
			goto END;
		}
		strcpy(tmp[i], last);
		i++;
	}
	*count = i;
	*buf = tmp;
END:
	if (ret != SUCCESS && tmp != NULL){
		for (int j = 0; j < lines; j++){
			if (tmp[j] != NULL){
				efree(tmp[j]);
			}
		}
		efree(tmp);
	}
	return ret;
}

void split_string_free(char ***buf, int count)
{
	if (buf == NULL || count < 1){
		return;
	}
	char **tmp = *buf;
	for (int i = 0; i < count; i++){
		efree(tmp[i]);
		tmp[i] = NULL;
	}
	efree(tmp);
	*buf = NULL;
}