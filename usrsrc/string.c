#include "string.h"
#include "stdio.h"
size_t strlen(const char *str)
{
	const char *tmp = str;

	while(*++tmp != '\0');
	//	tmp++;
	return tmp - str;
}

char * strdup(const char *str)
{
//	char *new = kcalloc(strlen(str) + 1,1);
//	if(new != NULL)
//		strcpy(new,str);
//	return new;	
	return (char *)str;
}

int strcmp ( const char * str1, const char * str2 )
{
	int i = 0;	

	for(; str1[i] == str2[i]; i++)
		if(str1[i] == '\0')
			return 0;	

	return str1[i] - str2[i]; 
}
int strncmp ( const char * str1, const char * str2, size_t num)
{
	uint32_t i = 0;	

	for(; str1[i] == str2[i]; i++)
	{
		if(str1[i] == '\0')
			return 0;	
		if(i == num)
			return 0;
	}
	return str1[i] - str2[i]; 
}

char * strtok ( char * str, const char * delimiters )
{
	str = str;
	delimiters = delimiters; 
	

	return NULL;
}
char * strchr(const char *str, int c)
{
	char *_str = (char *)str;

	while(1)
		if(*_str == (char)c)
			return _str;
		else if(*_str == '\0')
			return NULL;
		else
			_str++;

}
char * strtok_r ( char * str, const char * delimiters, char **save )
{
	char *out;

	if(str == NULL)
		str = *save;
	
	while(strchr(delimiters,*str) != NULL)
	{
		if(*str == '\0')
		{
			*save = str;
			return NULL;
		}	
		str++;
	}
	
	out = str;
	
	while(strchr(delimiters,*str) == NULL)
		str++;
	
	if(*str != '\0')
	{
		*str = '\0';
		*save = str + 1;
	} else {
		*save = str;
	}
	return out;
}
char *strcpy(char *dst, const char *src)
{
	const char *_src = src;
	char *_dst = dst;
	while(*_src != '\0')
		*_dst++ = *_src++;

	return dst;
}
char * strcat(char *dst, const char * src)
{
//	char *d = dst;
//	char *_src = (char *)src;
	while(*dst++ != '\0');
	dst--;
	strcpy(dst, (char *)src);



	return dst;
}


void *memcpy(void *dst, const void *src, size_t size)
{
	char *_dst = dst;
	char *_src = (char *)src;
	char *end = (char *)((char *)src + size);

	while(_src != end)
		*_dst++ = *_src++;

	return dst;
}
void *memset(void *dest, uint8_t val, size_t count)
{
	char *tmp = dest;
	char *end = (char *)(tmp + count);
	while(tmp != end)
		*tmp++ = val;

	return dest;
}
