/***************************************************************************
 *   Copyright (C) 2005 by Yunfan                                          *
 *   yunfan_zg@163.com                                                     *
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

/**
 * @file AboutWindow.h
 * A very simple About Window for FCITX
 * @author Yunfan yunfan_zg@163.com
 */

#ifndef _ABOUT_WINDOW_H
#define _ABOUT_WINDOW_H

#include <X11/Xlib.h>
#include "skin.h"

#define ABOUT_WINDOW_HEIGHT 150
struct _FcitxClassicUI;
typedef struct _AboutWindow {
    Window window;
    cairo_surface_t* surface;
    FcitxConfigColor color;
    FcitxConfigColor fontColor;
    int width;
    int fontSize;
    Atom     about_kill_atom;
    struct _FcitxClassicUI* owner;
} AboutWindow;


AboutWindow* CreateAboutWindow(struct _FcitxClassicUI *classicui);
void            DisplayAboutWindow(AboutWindow* aboutWindow);
void            DrawAboutWindow(AboutWindow* aboutWindow);

#endif

// kate: indent-mode cstyle; space-indent on; indent-width 0;
