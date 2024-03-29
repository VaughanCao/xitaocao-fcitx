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
#include <stdio.h>
#include <limits.h>
#include <ctype.h>
#include <libintl.h>
#include <errno.h>

#include "fcitx/fcitx.h"

#include "QuickPhrase.h"
#include "fcitx-utils/log.h"
#include "fcitx-config/xdg.h"
#include "fcitx-utils/utarray.h"
#include "fcitx/instance.h"
#include "fcitx-utils/utils.h"
#include "fcitx/module.h"
#include "fcitx/hook.h"
#include "fcitx/keys.h"
#include "fcitx/frontend.h"
#include "fcitx/candidate.h"
#include "fcitx/context.h"
#include "module/punc/punc.h"


typedef enum _QuickPhraseTriggerKey
{
    QPTK_Semicolon,
    QPTK_Grave,
} QuickPhraseTriggerKey;

typedef struct _QuickPhraseState {
    FcitxGenericConfig gconfig;
    QuickPhraseTriggerKey triggerKey;
    unsigned int uQuickPhraseCount;
    UT_array *quickPhrases ;
    int iFirstQuickPhrase;
    int iLastQuickPhrase;
    boolean enabled;
    FcitxInstance* owner;
} QuickPhraseState;

typedef struct _QuickPhraseCand {
    QUICK_PHRASE* cand;
} QuickPhraseCand;

static void * QuickPhraseCreate(FcitxInstance *instance);
static void LoadQuickPhrase(QuickPhraseState* qpstate);
static void FreeQuickPhrase(void* arg);
static void ReloadQuickPhrase(void* arg);
static INPUT_RETURN_VALUE QuickPhraseDoInput(void* arg, FcitxKeySym sym, int state);
static INPUT_RETURN_VALUE QuickPhraseGetCandWords(QuickPhraseState* qpstate);
static UT_icd qp_icd = {sizeof(QUICK_PHRASE), NULL, NULL, NULL};
static void ShowQuickPhraseMessage(QuickPhraseState *qpstate);
static INPUT_RETURN_VALUE QuickPhraseGetCandWord(void* arg, FcitxCandidateWord* candWord);
static boolean LoadQuickPhraseConfig(QuickPhraseState* qpstate);
static void SaveQuickPhraseConfig(QuickPhraseState* qpstate);
static boolean QuickPhrasePostFilter(void* arg, FcitxKeySym sym,
                                     unsigned int state,
                                     INPUT_RETURN_VALUE *retval
                                    );
static boolean QuickPhrasePreFilter(void* arg, FcitxKeySym sym,
                                    unsigned int state,
                                    INPUT_RETURN_VALUE *retval
                                   );
static  void QuickPhraseReset(void* arg);

FCITX_EXPORT_API
FcitxModule module = {
    QuickPhraseCreate,
    NULL,
    NULL,
    FreeQuickPhrase,
    ReloadQuickPhrase
};

FCITX_EXPORT_API
int ABI_VERSION = FCITX_ABI_VERSION;

static const FcitxHotkey FCITX_QP_GRACE[2] = {
    {NULL, FcitxKey_grave, FcitxKeyState_None},
    {NULL, 0, 0},
};

static const FcitxHotkey* QuickPhraseTriggerKeys[] = {
    FCITX_SEMICOLON,
    FCITX_QP_GRACE
};

CONFIG_BINDING_BEGIN(QuickPhraseState)
CONFIG_BINDING_REGISTER("QuickPhrase", "QuickPhraseTriggerKey", triggerKey)
CONFIG_BINDING_END()

int PhraseCmp(const void* a, const void* b)
{
    return strcmp(((QUICK_PHRASE*)a)->strCode, ((QUICK_PHRASE*)b)->strCode);
}

int PhraseCmpA(const void* a, const void* b)
{
    int res, len;
    len = strlen(((QUICK_PHRASE*)a)->strCode);
    res = strncmp(((QUICK_PHRASE*)a)->strCode, ((QUICK_PHRASE*)b)->strCode, len);
    if (res)
        return res;
    else {
        return 1;
    }
}

void * QuickPhraseCreate(FcitxInstance *instance)
{
    QuickPhraseState *qpstate = fcitx_utils_malloc0(sizeof(QuickPhraseState));
    qpstate->iFirstQuickPhrase = -1;
    qpstate->owner = instance;
    qpstate->enabled = false;
    
    if (!LoadQuickPhraseConfig(qpstate)) {
        free(qpstate);
        return NULL;
    }
    
    LoadQuickPhrase(qpstate);

    FcitxKeyFilterHook hk;
    hk.arg = qpstate ;
    hk.func = QuickPhrasePostFilter;
    FcitxInstanceRegisterPostInputFilter(instance, hk);

    hk.func = QuickPhrasePreFilter;
    FcitxInstanceRegisterPreInputFilter(instance, hk);

    FcitxIMEventHook resethk;
    resethk.arg = qpstate;
    resethk.func = QuickPhraseReset;
    FcitxInstanceRegisterResetInputHook(instance, resethk);
    
    FcitxInstanceRegisterWatchableContext(instance, CONTEXT_DISABLE_QUICKPHRASE, FCT_Boolean, FCF_ResetOnInputMethodChange);

    return qpstate;
}

/**
 * 加载快速输入词典
 * @param void
 * @return void
 * @note 快速输入是在;的行为定义为2,并且输入;以后才可以使用的。
 * 加载快速输入词典.如：输入“zg”就直接出现“中华人民共和国”等等。
 * 文件中每一行数据的定义为：<字符组合> <短语>
 * 如：“zg 中华人民共和国”
 */
void LoadQuickPhrase(QuickPhraseState * qpstate)
{
    FILE *fp;
    char *buf = NULL;
    size_t len = 0;
    char *buf1 = NULL;

    QUICK_PHRASE tempQuickPhrase;

    qpstate->uQuickPhraseCount = 0;

    fp =  FcitxXDGGetFileWithPrefix("data", "QuickPhrase.mb", "r", NULL);
    if (!fp)
        return;

    // 这儿的处理比较简单。因为是单索引对单值。
    // 应该注意的是，它在内存中是以单链表保存的。
    utarray_new(qpstate->quickPhrases, &qp_icd);
    while (getline(&buf, &len, fp) != -1) {
        if (buf1)
            free(buf1);
        buf1 = fcitx_utils_trim(buf);
        char *p = buf1;

        while (*p && !isspace(*p))
            p ++;

        if (p == '\0')
            continue;

        while (isspace(*p)) {
            *p = '\0';
            p ++;
        }

        if (strlen(buf1) >= QUICKPHRASE_CODE_LEN)
            continue;

        if (strlen(p) >= QUICKPHRASE_PHRASE_LEN * UTF8_MAX_LENGTH + 1)
            continue;

        strcpy(tempQuickPhrase.strCode, buf1);
        strcpy(tempQuickPhrase.strPhrase, p);

        utarray_push_back(qpstate->quickPhrases, &tempQuickPhrase);
    }

    if (buf)
        free(buf);
    if (buf1)
        free(buf1);

    if (qpstate->quickPhrases) {
        utarray_sort(qpstate->quickPhrases, PhraseCmp);
    }

    fclose(fp);
}

void FreeQuickPhrase(void *arg)
{
    QuickPhraseState *qpstate = (QuickPhraseState*) arg;
    if (!qpstate->quickPhrases)
        return;

    utarray_free(qpstate->quickPhrases);
}

void ShowQuickPhraseMessage(QuickPhraseState *qpstate)
{
    FcitxInputState *input = FcitxInstanceGetInputState(qpstate->owner);
    FcitxInputStateSetCursorPos(input, strlen(FcitxInputStateGetRawInputBuffer(input)));
    FcitxInstanceCleanInputWindowUp(qpstate->owner);
    FcitxMessagesAddMessageAtLast(FcitxInputStateGetAuxUp(input), MSG_TIPS, "%s", _("Quick Phrase: "));
    FcitxMessagesAddMessageAtLast(FcitxInputStateGetPreedit(input), MSG_INPUT, "%s", FcitxInputStateGetRawInputBuffer(input));
    FcitxMessagesAddMessageAtLast(FcitxInputStateGetClientPreedit(input), MSG_INPUT, "%s", FcitxInputStateGetRawInputBuffer(input));
}

boolean QuickPhrasePreFilter(void* arg, FcitxKeySym sym,
                             unsigned int state,
                             INPUT_RETURN_VALUE *retval
                            )
{
    QuickPhraseState *qpstate = (QuickPhraseState*) arg;
    FcitxInputState *input = FcitxInstanceGetInputState(qpstate->owner);
    if (qpstate->enabled) {
        FcitxKeySym keymain = FcitxHotkeyPadToMain(sym);
        if (FcitxHotkeyIsHotKeySimple(keymain, state)) {
            *retval = QuickPhraseDoInput(qpstate, keymain, state);
            if (*retval == IRV_TO_PROCESS) {
                if (strlen(FcitxInputStateGetRawInputBuffer(input)) == 0
                    && (FcitxHotkeyIsHotKey(keymain, state, QuickPhraseTriggerKeys[qpstate->triggerKey]) || FcitxHotkeyIsHotKey(keymain, state, FCITX_SPACE))) {
                    char c[2] = { (char) (QuickPhraseTriggerKeys[qpstate->triggerKey][0].sym & 0xff), '\0'};
                    FcitxModuleFunctionArg farg;
                    FcitxKeySym s = QuickPhraseTriggerKeys[qpstate->triggerKey][0].sym;
                    farg.args[0] = &s;
                    char* strTemp = InvokeFunction(qpstate->owner, FCITX_PUNC, GETPUNC, farg);
                    strcpy(FcitxInputStateGetOutputString(input), strTemp ? strTemp : c);
                    *retval = IRV_COMMIT_STRING;
                } else {
                    char buf[2];
                    buf[0] = keymain;
                    buf[1] = '\0';
                    if (strlen(FcitxInputStateGetRawInputBuffer(input)) < MAX_USER_INPUT)
                        strcat(FcitxInputStateGetRawInputBuffer(input), buf);
                    ShowQuickPhraseMessage(qpstate);
                    *retval = QuickPhraseGetCandWords(qpstate);
                }
            } else
                return true;
        } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_BACKSPACE)) {
            size_t len = strlen(FcitxInputStateGetRawInputBuffer(input));
            if (len > 0)
                FcitxInputStateGetRawInputBuffer(input)[--len] = '\0';
            if (len == 0) {
                *retval = IRV_CLEAN;
            } else {
                ShowQuickPhraseMessage(qpstate);
                *retval = QuickPhraseGetCandWords(qpstate);
            }
        } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_ENTER)) {

            if (strlen(FcitxInputStateGetRawInputBuffer(input)) > 0) {
                strcpy(FcitxInputStateGetOutputString(input), FcitxInputStateGetRawInputBuffer(input));
                QuickPhraseReset(qpstate);
                *retval = IRV_COMMIT_STRING;
            } else {
                char c[2] = { (char) (QuickPhraseTriggerKeys[qpstate->triggerKey][0].sym & 0xff), '\0'};
                strcpy(FcitxInputStateGetOutputString(input), c);
                *retval = IRV_COMMIT_STRING;
            }
        } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_ESCAPE)) {
            *retval = IRV_CLEAN;
        } else
            *retval = IRV_DO_NOTHING;
        if (*retval == IRV_DISPLAY_MESSAGE) {
            FcitxMessagesSetMessageCount(FcitxInputStateGetAuxDown(input), 0);
            if (FcitxCandidateWordPageCount(FcitxInputStateGetCandidateList(input)) == 0)
                FcitxMessagesAddMessageAtLast(FcitxInputStateGetAuxDown(input), MSG_TIPS, "%s", _("Press Enter to input text"));
        }
        return true;
    }
    return false;
}
boolean QuickPhrasePostFilter(void* arg, FcitxKeySym sym,
                              unsigned int state,
                              INPUT_RETURN_VALUE *retval
                             )
{
    QuickPhraseState *qpstate = (QuickPhraseState*) arg;
    FcitxInputState *input = FcitxInstanceGetInputState(qpstate->owner);
    boolean disableQuickPhrase = FcitxInstanceGetContextBoolean(qpstate->owner, CONTEXT_DISABLE_QUICKPHRASE);

    if (*retval != IRV_TO_PROCESS)
        return false;

    if (!disableQuickPhrase 
        && !qpstate->enabled
        && FcitxInputStateGetRawInputBufferSize(input) == 0
        && FcitxHotkeyIsHotKey(sym, state, QuickPhraseTriggerKeys[qpstate->triggerKey])) {
        FcitxInstanceCleanInputWindow(qpstate->owner);
        FcitxInputStateSetShowCursor(input, true);
        FcitxMessagesAddMessageAtLast(FcitxInputStateGetAuxUp(input), MSG_TIPS, "%s", _("Quick Phrase: "));
        FcitxInputStateSetCursorPos(input, 0);
    
        char c[2] = { (char) (QuickPhraseTriggerKeys[qpstate->triggerKey][0].sym & 0xff), '\0'};
        FcitxModuleFunctionArg farg;
        FcitxKeySym s = QuickPhraseTriggerKeys[qpstate->triggerKey][0].sym;
        farg.args[0] = &s;
        char* strTemp = InvokeFunction(qpstate->owner, FCITX_PUNC, GETPUNC, farg);
        const char* full = strTemp ? strTemp : c;
        FcitxMessagesAddMessageAtLast(FcitxInputStateGetAuxDown(input), MSG_TIPS, _("Space for %s Enter for %s") , full, c);

        qpstate->enabled = true;
        *retval = IRV_DISPLAY_MESSAGE;

        return true;
    }
    return false;
}

void QuickPhraseReset(void* arg)
{
    QuickPhraseState *qpstate = (QuickPhraseState*) arg;
    qpstate->enabled = false;
}
INPUT_RETURN_VALUE QuickPhraseDoInput(void* arg, FcitxKeySym sym, int state)
{
    QuickPhraseState *qpstate = (QuickPhraseState*) arg;
    FcitxInputState *input = FcitxInstanceGetInputState(qpstate->owner);
    FcitxGlobalConfig* fc = FcitxInstanceGetGlobalConfig(qpstate->owner);
    int retVal = IRV_TO_PROCESS;
    
    const FcitxHotkey* hkPrevPage = FcitxInstanceGetContextHotkey(qpstate->owner, CONTEXT_ALTERNATIVE_PREVPAGE_KEY);
    if (hkPrevPage == NULL)
        hkPrevPage = fc->hkPrevPage;
    
    const FcitxHotkey* hkNextPage = FcitxInstanceGetContextHotkey(qpstate->owner, CONTEXT_ALTERNATIVE_NEXTPAGE_KEY);
    if (hkNextPage == NULL)
        hkNextPage = fc->hkNextPage;

    if (FcitxHotkeyIsHotKey(sym, state, hkPrevPage)) {
        if (FcitxCandidateWordGoPrevPage(FcitxInputStateGetCandidateList(input)))
            retVal = IRV_DISPLAY_MESSAGE;
    } else if (FcitxHotkeyIsHotKey(sym, state, hkNextPage)) {
        if (FcitxCandidateWordGoNextPage(FcitxInputStateGetCandidateList(input)))
            retVal = IRV_DISPLAY_MESSAGE;
    } else if (FcitxHotkeyIsHotKeyDigit(sym, state)) {
        int iKey = FcitxHotkeyCheckChooseKey(sym, state, DIGIT_STR_CHOOSE);
        if (iKey >= 0)
            retVal = FcitxCandidateWordChooseByIndex(FcitxInputStateGetCandidateList(input), iKey);
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_SPACE)) {
        if (FcitxCandidateWordPageCount(FcitxInputStateGetCandidateList(input)) != 0)
            retVal = FcitxCandidateWordChooseByIndex(FcitxInputStateGetCandidateList(input), 0);
    }

    return retVal;
}

INPUT_RETURN_VALUE QuickPhraseGetCandWords(QuickPhraseState* qpstate)
{
    int iInputLen;
    QUICK_PHRASE searchKey, *pKey, *currentQuickPhrase, *lastQuickPhrase;
    FcitxInputState *input = FcitxInstanceGetInputState(qpstate->owner);
    FcitxInstance *instance = qpstate->owner;
    FcitxGlobalConfig* config = FcitxInstanceGetGlobalConfig(instance);
    
    FcitxInstanceCleanInputWindowDown(qpstate->owner);

    FcitxCandidateWordSetPageSize(FcitxInputStateGetCandidateList(input), config->iMaxCandWord);
    FcitxCandidateWordSetChoose(FcitxInputStateGetCandidateList(input), DIGIT_STR_CHOOSE);

    pKey = &searchKey;

    if (!qpstate->quickPhrases)
        return IRV_DISPLAY_MESSAGE;

    iInputLen = strlen(FcitxInputStateGetRawInputBuffer(input));
    if (iInputLen > QUICKPHRASE_CODE_LEN)
        return IRV_DISPLAY_MESSAGE;

    strcpy(searchKey.strCode, FcitxInputStateGetRawInputBuffer(input));

    FcitxCandidateWordReset(FcitxInputStateGetCandidateList(input));

    currentQuickPhrase = utarray_custom_bsearch(pKey, qpstate->quickPhrases, false, PhraseCmp);
    qpstate->iFirstQuickPhrase = utarray_eltidx(qpstate->quickPhrases, currentQuickPhrase);
    lastQuickPhrase = utarray_custom_bsearch(pKey, qpstate->quickPhrases, false, PhraseCmpA);
    qpstate->iLastQuickPhrase = utarray_eltidx(qpstate->quickPhrases, lastQuickPhrase);
    if (qpstate->iLastQuickPhrase < 0)
        qpstate->iLastQuickPhrase = utarray_len(qpstate->quickPhrases);
    if (!currentQuickPhrase || strncmp(FcitxInputStateGetRawInputBuffer(input), currentQuickPhrase->strCode, iInputLen)) {
        FcitxInstanceCleanInputWindowDown(instance);
        currentQuickPhrase = NULL;
        return IRV_DISPLAY_MESSAGE;
    }

    for (currentQuickPhrase = (QUICK_PHRASE*) utarray_eltptr(qpstate->quickPhrases,
                              qpstate->iFirstQuickPhrase);
            currentQuickPhrase != NULL;
            currentQuickPhrase = (QUICK_PHRASE*) utarray_next(qpstate->quickPhrases, currentQuickPhrase)) {
        if (!strncmp(FcitxInputStateGetRawInputBuffer(input), currentQuickPhrase->strCode, iInputLen)) {
            QuickPhraseCand* qpcand = fcitx_utils_malloc0(sizeof(QuickPhraseCand));
            qpcand->cand = currentQuickPhrase;
            FcitxCandidateWord candWord;
            candWord.callback = QuickPhraseGetCandWord;
            candWord.owner = qpstate;
            candWord.priv = qpcand;
            candWord.strExtra = strdup(currentQuickPhrase->strCode + iInputLen);
            candWord.strWord = strdup(currentQuickPhrase->strPhrase);
            candWord.wordType = MSG_OTHER;
            candWord.extraType = MSG_CODE;
            FcitxCandidateWordAppend(FcitxInputStateGetCandidateList(input), &candWord);
        }
    }

    return IRV_DISPLAY_MESSAGE;
}

void ReloadQuickPhrase(void* arg)
{
    QuickPhraseState *qpstate = (QuickPhraseState*) arg;
    LoadQuickPhraseConfig(qpstate);
    FreeQuickPhrase(arg);
    LoadQuickPhrase(qpstate);
}

INPUT_RETURN_VALUE QuickPhraseGetCandWord(void* arg, FcitxCandidateWord* candWord)
{
    QuickPhraseState *qpstate = (QuickPhraseState*) arg;
    QuickPhraseCand* qpcand = candWord->priv;
    FcitxInputState *input = FcitxInstanceGetInputState(qpstate->owner);
    strcpy(FcitxInputStateGetOutputString(input), qpcand->cand->strPhrase);
    return IRV_COMMIT_STRING;
}

CONFIG_DESC_DEFINE(GetQuickPhraseConfigDesc, "fcitx-quickphrase.desc")

boolean LoadQuickPhraseConfig(QuickPhraseState* qpstate)
{
    FcitxConfigFileDesc* configDesc = GetQuickPhraseConfigDesc();
    if (configDesc == NULL)
        return false;

    FILE *fp;
    char *file;
    fp = FcitxXDGGetFileUserWithPrefix("conf", "fcitx-quickphrase.config", "r", &file);
    FcitxLog(DEBUG, "Load Config File %s", file);
    free(file);
    if (!fp) {
        if (errno == ENOENT)
            SaveQuickPhraseConfig(qpstate);
    }

    FcitxConfigFile *cfile = FcitxConfigParseConfigFileFp(fp, configDesc);

    QuickPhraseStateConfigBind(qpstate, cfile, configDesc);
    FcitxConfigBindSync((FcitxGenericConfig*)qpstate);

    if (fp)
        fclose(fp);

    return true;
}

void SaveQuickPhraseConfig(QuickPhraseState* qpstate)
{
    FcitxConfigFileDesc* configDesc = GetQuickPhraseConfigDesc();
    char *file;
    FILE *fp = FcitxXDGGetFileUserWithPrefix("conf", "fcitx-quickphrase.config", "w", &file);
    FcitxLog(DEBUG, "Save Config to %s", file);
    FcitxConfigSaveConfigFileFp(fp, &qpstate->gconfig, configDesc);
    free(file);
    if (fp)
        fclose(fp);
}


// kate: indent-mode cstyle; space-indent on; indent-width 0;
