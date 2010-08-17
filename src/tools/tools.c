/***************************************************************************
 *   Copyright (C) 2002~2005 by Yuking                                     *
 *   yuking_net@sohu.com                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
/**
 * @file   tools.c
 * @author Yuking yuking_net@sohu.com
 * @date   2008-1-16
 *
 * @brief  配置文件读写
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

#include "core/ime.h"
#include "tools/tools.h"
#include "ui/ui.h"
#include "version.h"
#include "ui/MainWindow.h"
#include "ui/InputWindow.h"
#include "ui/skin.h"
#include "im/pinyin/PYFA.h"
#include "im/pinyin/py.h"
#include "im/pinyin/sp.h"
#include "im/table/table.h"
#include "im/qw/qw.h"
#include "fcitx-config/uthash.h"

#include "utf8_in_gb18030.h"

pthread_mutex_t fcitxMutex;

typedef struct simple2trad_t
{
	int wc;
	char str[UTF8_MAX_LENGTH + 1];
	INT8 len;
	UT_hash_handle hh;
} simple2trad_t;

simple2trad_t* s2t_table = NULL;

/**
 * 该函数装载data/gbks2t.tab的简体转繁体的码表，
 * 然后按码表将GBK字符转换成GBK繁体字符。
 *
 * WARNING： 该函数返回新分配内存字符串，请调用者
 * 注意释放。
 */
char           *ConvertGBKSimple2Tradition (char *strHZ)
{
    FILE           *fp;
    char           *ret;
    char            strPath[PATH_MAX];
    int             i, len, ret_len;
    char           *strBuf = NULL;
    size_t          bufLen = 0;
	char           *ps;

    if (strHZ == NULL)
	return NULL;

	if (FcitxLock() != 0)
		return NULL;
    if (!s2t_table) {
	len = 0;

	strcpy (strPath, PKGDATADIR "/data/");
	strcat (strPath, TABLE_GBKS2T);

	/* zxd add begin */
        if( access( strPath, 0 ) && getenv( "FCITXDIR" ) ) {
            strcpy( strPath, getenv( "FCITXDIR" ) );
            strcat( strPath, "/share/fcitx/data/" );
            strcat( strPath, TABLE_GBKS2T );
        }
        /* zxd add end */
	
	fp = fopen (strPath, "rb");
	if (!fp) {
	    ret = (char *) malloc (sizeof (char) * (strlen (strHZ) + 1));
	    strcpy (ret, strHZ);
	    return ret;
	}
	while(getline(&strBuf, &bufLen, fp) != -1)
	{
        simple2trad_t *s2t;
		char *ps;
		int wc;

		ps = utf8_get_char(strBuf, &wc);
		s2t = (simple2trad_t*) malloc(sizeof(simple2trad_t));
		s2t->wc = wc;
		s2t->len = utf8_char_len(ps);
		strncpy(s2t->str, ps, s2t->len);
		s2t->str[s2t->len] = '\0';

		HASH_ADD_INT(s2t_table, wc, s2t);
	}
    if(strBuf)
        free(strBuf);
    }
	FcitxUnlock();

    i = 0;
    len = utf8_strlen (strHZ);
	ret_len = 0;
    ret = (char *) malloc (sizeof (char) * (UTF8_MAX_LENGTH * len + 1));
	ps = strHZ;
	ret[0] = '\0';
    for (; i < len; ++i) {
		int wc;
		simple2trad_t *s2t = NULL;
		int chr_len = utf8_char_len(ps);
		char *nps;
		nps = utf8_get_char(ps , &wc);
		HASH_FIND_INT(s2t_table, &wc, s2t);

		if (s2t)
		{
			strcat(ret, s2t->str);
			ret_len += s2t->len;
		}
		else
		{
			strncat(ret, ps, chr_len);
			ret_len += chr_len;
		}

		ps = nps;

    }
    ret[ret_len] = '\0';

    return ret;
}

static int cmpi(const void * a, const void *b)
{
	return (*((int*)a)) - (*((int*)b));
}

/*
 * 计算文件中有多少行
 * 注意:文件中的空行也做为一行处理
 */
int CalculateRecordNumber (FILE * fpDict)
{
    char           *strBuf = NULL;
    size_t          bufLen = 0;
    int             nNumber = 0;

        while(getline(&strBuf, &bufLen, fpDict) != -1){
        nNumber++;
    }
    rewind (fpDict);

    if (strBuf)
        free(strBuf);

    return nNumber;
}


int CalHZIndex (char *strHZ)
{
	unsigned int iutf = 0;
	int l = utf8_char_len(strHZ);
	unsigned char* utf = (unsigned char*) strHZ;
	unsigned int *res;
	int idx;
	
	if (l == 2)
	{
		iutf = *utf++ << 8;
		iutf |= *utf++;
	}
	else if (l == 3)
	{
		iutf = *utf++ << 16;
		iutf |= *utf++ << 8;
		iutf |= *utf++;
	}
	else if (l == 4)
	{
		iutf = *utf++ << 24;
		
		iutf |= *utf++ << 16;
		iutf |= *utf++ << 8;
		iutf |= *utf++;
	}

	res = bsearch(&iutf, utf8_in_gb18030, 63360, sizeof(int), cmpi);
	if (res)
		idx = res - utf8_in_gb18030;
	else
		idx = 63361;
    return idx;
}

/** 
 * @brief 去除字符串首末尾空白字符
 * 
 * @param s
 * 
 * @return malloc的字符串，需要free
 */
char *trim(char *s)
{
    register char *end;
    register char csave;
    
    while (isspace(*s))                 /* skip leading space */
        ++s;
    end = strchr(s,'\0') - 1;
    while (end >= s && isspace(*end))               /* skip trailing space */
        --end;
    
    csave = end[1];
    end[1] = '\0';
    s = strdup(s);
    end[1] = csave;
    return (s);
}

/** 
 * @brief 返回申请后的内存，并清零
 * 
 * @param 申请的内存长度
 * 
 * @return 申请的内存指针
 */
void *malloc0(size_t bytes)
{
    void *p = malloc(bytes);
    if (!p)
        return NULL;

    memset(p, 0, bytes);
    return p;
}

/** 
 * @brief 自定义的二分查找，和bsearch库函数相比支持不精确位置的查询
 * 
 * @param key
 * @param base
 * @param nmemb
 * @param size
 * @param accurate
 * @param compar
 * 
 * @return 
 */
void *custom_bsearch(const void *key, const void *base,
        size_t nmemb, size_t size, int accurate,
        int (*compar)(const void *, const void *))
{
    if (accurate)
        return bsearch(key, base, nmemb, size, compar);
    else
    {
        size_t l, u, idx;
        const void *p;
        int comparison;
        
        l = 0;
        u = nmemb;
        while (l < u)
        {
            idx = (l + u) / 2;
            p = (void *) (((const char *) base) + (idx * size));
            comparison = (*compar) (key, p);
            if (comparison <= 0)
                u = idx;
            else if (comparison > 0)
                l = idx + 1;
        }

        if (u >= nmemb)
            return NULL;
        else
            return (void *) (((const char *) base) + (l * size));
    }
}

void FcitxInitThread()
{
    pthread_mutexattr_t   mta;
    int rc;
    
    rc = pthread_mutexattr_init(&mta);
    if (rc != 0)
        FcitxLog(ERROR, _("pthread mutexattr init failed"));

    pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE);

    rc = pthread_mutex_init(&fcitxMutex, &mta);
    gs.bMutexInited = True;
    if (rc != 0)
        FcitxLog(ERROR, _("pthread mutex init failed"));
}

int FcitxLock()
{
    if (gs.bMutexInited)
        return pthread_mutex_lock(&fcitxMutex); 
    return 0;
}

int FcitxUnlock()
{
    if (gs.bMutexInited)        
        return pthread_mutex_unlock(&fcitxMutex);
    return 0;
}

/** 
 * @brief Fcitx记录Log的函数
 * 
 * @param ErrorLevel
 * @param fmt
 * @param ...
 */
void FcitxLogFunc(ErrorLevel e, const char* filename, const int line, const char* fmt, ...)
{
#ifndef _DEBUG
    if (e == DEBUG)
        return;
#endif
    switch (e)
    {
        case INFO:
            fprintf(stderr, _("Info:"));
            break;
        case ERROR:
            fprintf(stderr, _("Error:"));
            break;
        case DEBUG:
            fprintf(stderr, _("Debug:"));
            break;
        case WARNING:
            fprintf(stderr, _("Warning:"));
            break;
        case FATAL:
            fprintf(stderr, _("Fatal:"));
            break;
    }
    va_list ap;
    fprintf(stderr, "%s:%u-", filename, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}