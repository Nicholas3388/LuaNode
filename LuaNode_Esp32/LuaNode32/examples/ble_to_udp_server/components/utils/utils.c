#include "utils.h"

#include <stdlib.h>
#include <string.h>

int str2num(const char *str, int len)
{
    int res;
    unsigned char *temp = (unsigned char *)malloc(len + 1);
    memcpy(temp, str, len);
    temp[len] = 0;
    res = atoi((const char *)temp);
    free(temp);
    return res;
}