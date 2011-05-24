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
#ifndef _QUICKPHRASE_H
#define _QUICKPHRASE_H

#include "fcitx/ime.h"

#define QUICKPHRASE_CODE_LEN	20
#define QUICKPHRASE_PHRASE_LEN  40

typedef struct _QUICK_PHRASE {
    char strCode[QUICKPHRASE_CODE_LEN+1];
    char strPhrase[QUICKPHRASE_PHRASE_LEN * UTF8_MAX_LENGTH+1];
} QUICK_PHRASE;

void LoadQuickPhrase(void);
void FreeQuickPhrase(void);
INPUT_RETURN_VALUE QuickPhraseDoInput (KeySym sym, int state, int iCount);
INPUT_RETURN_VALUE QuickPhraseGetCandWords (SEARCH_MODE mode);

#endif
