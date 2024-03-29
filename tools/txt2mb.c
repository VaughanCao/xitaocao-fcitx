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
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.              *
 ***************************************************************************/
#ifdef FCITX_HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "fcitx-utils/utf8.h"
#include "fcitx/fcitx.h"
#include "fcitx-config/fcitx-config.h"
#include "im/table/tabledict.h"

#define STR_KEYCODE 0
#define STR_CODELEN 1
#define STR_IGNORECHAR 2
#define STR_PINYIN 3
#define STR_PINYINLEN 4
#define STR_DATA 5
#define STR_RULE 6

#define CONST_STR_SIZE 7

#define MAX_CODE_LENGTH  30
#define PHRASE_MAX_LENGTH 10
#define FH_MAX_LENGTH  10
#define TABLE_AUTO_SAVE_AFTER 1024
#define AUTO_PHRASE_COUNT 10000
#define SINGLE_HZ_COUNT 66000

char* strConst[CONST_STR_SIZE] = { "键码=", "码长=", "规避字符=", "拼音=", "拼音长度=" , "[数据]", "[组词规则]"};
int strLength[CONST_STR_SIZE];

char            strInputCode[100] = "\0";
char            strIgnoreChars[100] = "\0";
char            cPinyinKey = '\0';

void InitStrLength()
{
    int i;

    for (i = 0; i < CONST_STR_SIZE; i++)
        strLength[i] = strlen(strConst[i]);
}

boolean IsValidCode(char cChar)
{
    char           *p;

    p = strInputCode;

    while (*p) {
        if (cChar == *p)
            return true;

        p++;
    }

    p = strIgnoreChars;

    while (*p) {
        if (cChar == *p)
            return true;

        p++;
    }

    if (cChar == cPinyinKey || cChar == '^' || cChar == '&')
        return true;

    return false;
}

int main(int argc, char *argv[])
{
    char            strCode[100];
    char            strHZ[100];
    char           *p;
    FILE           *fpDict, *fpNew;
    RECORD         *temp, *head, *newRec, *current;
    unsigned int    s = 0;
    int             i;
    unsigned int    iTemp;
    char           *pstr = 0;
    char            strTemp[10];
    unsigned char   bRule;
    RULE           *rule = NULL;
    unsigned int    l;

    unsigned char   iCodeLength = 0;
    unsigned char   iPYCodeLength = 0;

    int8_t          type;

    if (argc != 3) {
        printf("\nUsage: txt2mb <Source File> <IM File>\n\n");
        exit(1);
    }

    fpDict = fopen(argv[1], "r");

    if (!fpDict) {
        printf("\nCan not read source file!\n\n");
        exit(2);
    }

    InitStrLength();

    head = (RECORD *) malloc(sizeof(RECORD));
    head->next = head;
    head->prev = head;
    current = head;

    bRule = 0;
    l = 0;

    for (;;) {
        l++;

        if (!fgets(strCode, 100, fpDict))
            break;

        i = strlen(strCode) - 1;

        while ((i >= 0) && (strCode[i] == ' ' || strCode[i] == '\n' || strCode[i] == '\r'))
            strCode[i--] = '\0';

        pstr = strCode;

        if (*pstr == ' ')
            pstr++;

        if (pstr[0] == '#')
            continue;

        if (strstr(pstr, strConst[STR_KEYCODE])) {
            pstr += strLength[STR_KEYCODE];
            strcpy(strInputCode, pstr);
        } else if (strstr(pstr, strConst[STR_CODELEN])) {
            pstr += strLength[STR_CODELEN];
            iCodeLength = atoi(pstr);

            if (iCodeLength > MAX_CODE_LENGTH) {
                iCodeLength = MAX_CODE_LENGTH;
                printf("Max Code Length is %d\n", MAX_CODE_LENGTH);
            }
        } else if (strstr(pstr, strConst[STR_IGNORECHAR])) {
            pstr += strLength[STR_IGNORECHAR];
            strcpy(strIgnoreChars, pstr);
        } else if (strstr(pstr, strConst[STR_PINYIN])) {
            pstr += strLength[STR_PINYIN];

            while (*pstr == ' ')
                pstr++;

            cPinyinKey = *pstr;
        } else if (strstr(pstr, strConst[STR_PINYINLEN])) {
            pstr += strLength[STR_PINYINLEN];
            iPYCodeLength = atoi(pstr);
        }

        else if (strstr(pstr, strConst[STR_DATA]))
            break;
        else if (strstr(pstr, strConst[STR_RULE])) {
            bRule = 1;
            break;
        }
    }

    if (iCodeLength <= 0 || !strInputCode[0]) {
        printf("Source File Format Error!\n");
        exit(1);
    }

    if (bRule) {
        /*
         * 组词规则数应该比键码长度小1
         */
        rule = (RULE *) malloc(sizeof(RULE) * (iCodeLength - 1));

        for (iTemp = 0; iTemp < (iCodeLength - 1); iTemp++) {
            l++;

            if (!fgets(strCode, 100, fpDict))
                break;

            rule[iTemp].rule = (RULE_RULE *) malloc(sizeof(RULE_RULE) * iCodeLength);

            i = strlen(strCode) - 1;

            while ((i >= 0) && (strCode[i] == ' ' || strCode[i] == '\n' || strCode[i] == '\r'))
                strCode[i--] = '\0';

            pstr = strCode;

            if (*pstr == ' ')
                pstr++;

            if (pstr[0] == '#')
                continue;

            if (strstr(pstr, strConst[STR_DATA]))
                break;

            switch (*pstr) {

            case 'e':

            case 'E':
                rule[iTemp].iFlag = 0;
                break;

            case 'a':

            case 'A':
                rule[iTemp].iFlag = 1;
                break;

            default:
                printf("2   Phrase rules are not suitable!\n");
                printf("\t\t%s\n", strCode);
                exit(1);
            }

            pstr++;

            p = pstr;

            while (*p && *p != '=')
                p++;

            if (!(*p)) {
                printf("3   Phrase rules are not suitable!\n");
                printf("\t\t%s\n", strCode);
                exit(1);
            }

            strncpy(strTemp, pstr, p - pstr);

            strTemp[p - pstr] = '\0';
            rule[iTemp].iWords = atoi(strTemp);

            p++;

            for (i = 0; i < iCodeLength; i++) {
                while (*p == ' ')
                    p++;

                switch (*p) {

                case 'p':

                case 'P':
                    rule[iTemp].rule[i].iFlag = 1;
                    break;

                case 'n':

                case 'N':
                    rule[iTemp].rule[i].iFlag = 0;
                    break;

                default:
                    printf("4   Phrase rules are not suitable!\n");
                    printf("\t\t%s\n", strCode);
                    exit(1);
                }

                p++;

                rule[iTemp].rule[i].iWhich = *p++ - '0';
                rule[iTemp].rule[i].iIndex = *p++ - '0';

                while (*p == ' ')
                    p++;

                if (i != (iCodeLength - 1)) {
                    if (*p != '+') {
                        printf("5   Phrase rules are not suitable!\n");
                        printf("\t\t%s  %d\n", strCode, iCodeLength);
                        exit(1);
                    }

                    p++;
                }
            }
        }

        if (iTemp != iCodeLength - 1) {
            printf("6  Phrase rules are not suitable!\n");
            exit(1);
        }

        for (iTemp = 0; iTemp < (iCodeLength - 1); iTemp++) {
            l++;

            if (!fgets(strCode, 100, fpDict))
                break;

            i = strlen(strCode) - 1;

            while ((i >= 0) && (strCode[i] == ' ' || strCode[i] == '\n' || strCode[i] == '\r'))
                strCode[i--] = '\0';

            pstr = strCode;

            if (*pstr == ' ')
                pstr++;

            if (pstr[0] == '#')
                continue;

            if (strstr(pstr, strConst[STR_DATA]))
                break;
        }
    }

    if (iPYCodeLength < iCodeLength)
        iPYCodeLength = iCodeLength;

    if (!strstr(pstr, strConst[STR_DATA])) {
        printf("Source File Format Error!\n");
        exit(1);
    }

    for (;;) {
        l++;

        if (EOF == fscanf(fpDict, "%s %s\n", strCode, strHZ))
            break;

        if (!IsValidCode(strCode[0])) {
            printf("Invalid Format: Line-%d  %s %s\n", l, strCode, strHZ);

            exit(1);
        }

        if (((strCode[0] != cPinyinKey) && (strlen(strCode) > iCodeLength))
            || ((strCode[0] == cPinyinKey) && (strlen(strCode) > (iPYCodeLength + 1)))
            || ((strCode[0] == '^') && (strlen(strCode) > (iCodeLength + 1)))
            || ((strCode[0] == '&') && (strlen(strCode) > (iPYCodeLength + 1)))
        ) {
            printf("Delete:  %s %s, Too long\n", strCode, strHZ);
            continue;
        }

        size_t hzLen = fcitx_utf8_strlen(strHZ);
         // Utf-8 Longest Phrase Length is 10, longest construct code length is 1
        if (hzLen > PHRASE_MAX_LENGTH || (strCode[0] == '^' && hzLen != 1)) {
            printf("Delete:  %s %s, Too long\n", strCode, strHZ);
            continue;
        }

        type = RECORDTYPE_NORMAL;

        pstr = strCode;

        if (strCode[0] == cPinyinKey) {
            pstr ++;
            type = RECORDTYPE_PINYIN;
        } else if (strCode[0] == '^') {
            pstr ++;
            type = RECORDTYPE_CONSTRUCT;
        } else if (strCode[0] == '&') {
            pstr ++;
            type = RECORDTYPE_PROMPT;
        }

        //查找是否重复
        temp = current;

        if (temp != head) {
            if (strcmp(temp->strCode, pstr) >= 0) {
                while (temp != head && strcmp(temp->strCode, pstr) >= 0) {
                    if (!strcmp(temp->strHZ, strHZ) && !strcmp(temp->strCode, pstr) && temp->type == type) {
                        printf("Delete:  %s %s\n", pstr, strHZ);
                        goto _next;
                    }

                    temp = temp->prev;
                }

                if (temp == head)
                    temp = temp->next;

                while (temp != head && strcmp(temp->strCode, pstr) <= 0)
                    temp = temp->next;
            } else {
                while (temp != head && strcmp(temp->strCode, pstr) <= 0) {
                    if (!strcmp(temp->strHZ, strHZ) && !strcmp(temp->strCode, pstr) && temp->type == type) {
                        printf("Delete:  %s %s\n", pstr, strHZ);
                        goto _next;
                    }

                    temp = temp->next;
                }
            }
        }

        //插在temp的前面
        newRec = (RECORD *) malloc(sizeof(RECORD));

        newRec->strCode = (char *) malloc(sizeof(char) * (iPYCodeLength + 1));

        newRec->strHZ = (char *) malloc(sizeof(char) * strlen(strHZ) + 1);

        strcpy(newRec->strCode, pstr);

        strcpy(newRec->strHZ, strHZ);

        newRec->type = type;

        newRec->iHit = 0;

        newRec->iIndex = 0;

        temp->prev->next = newRec;

        newRec->next = temp;

        newRec->prev = temp->prev;

        temp->prev = newRec;

        current = newRec;

        s++;

    _next:
        continue;
    }

    fclose(fpDict);

    printf("\nReading %d records.\n\n", s);

    fpNew = fopen(argv[2], "w");

    if (!fpNew) {
        printf("\nCan not create target file!\n\n");
        exit(3);
    }

    int8_t iInternalVersion = INTERNAL_VERSION;

    //写入版本号--如果第一个字为0,表示后面那个字节为版本号
    iTemp = 0;
    fwrite(&iTemp, sizeof(unsigned int), 1, fpNew);
    fwrite(&iInternalVersion, sizeof(int8_t), 1, fpNew);

    iTemp = (unsigned int) strlen(strInputCode);
    fwrite(&iTemp, sizeof(unsigned int), 1, fpNew);
    fwrite(strInputCode, sizeof(char), iTemp + 1, fpNew);
    fwrite(&iCodeLength, sizeof(unsigned char), 1, fpNew);
    fwrite(&iPYCodeLength, sizeof(unsigned char), 1, fpNew);
    iTemp = (unsigned int) strlen(strIgnoreChars);
    fwrite(&iTemp, sizeof(unsigned int), 1, fpNew);
    fwrite(strIgnoreChars, sizeof(char), iTemp + 1, fpNew);

    fwrite(&bRule, sizeof(unsigned char), 1, fpNew);

    if (bRule) {
        for (i = 0; i < iCodeLength - 1; i++) {
            fwrite(&(rule[i].iFlag), sizeof(unsigned char), 1, fpNew);
            fwrite(&(rule[i].iWords), sizeof(unsigned char), 1, fpNew);

            for (iTemp = 0; iTemp < iCodeLength; iTemp++) {
                fwrite(&(rule[i].rule[iTemp].iFlag), sizeof(unsigned char), 1, fpNew);
                fwrite(&(rule[i].rule[iTemp].iWhich), sizeof(unsigned char), 1, fpNew);
                fwrite(&(rule[i].rule[iTemp].iIndex), sizeof(unsigned char), 1, fpNew);
            }
        }
    }

    fwrite(&s, sizeof(unsigned int), 1, fpNew);

    current = head->next;

    while (current != head) {
        fwrite(current->strCode, sizeof(char), iPYCodeLength + 1, fpNew);
        s = strlen(current->strHZ) + 1;
        fwrite(&s, sizeof(unsigned int), 1, fpNew);
        fwrite(current->strHZ, sizeof(char), s, fpNew);
        fwrite(&(current->type), sizeof(int8_t), 1, fpNew);
        fwrite(&(current->iHit), sizeof(unsigned int), 1, fpNew);
        fwrite(&(current->iIndex), sizeof(unsigned int), 1, fpNew);
        current = current->next;
    }

    fclose(fpNew);

    return 0;
}

// kate: indent-mode cstyle; space-indent on; indent-width 4;
