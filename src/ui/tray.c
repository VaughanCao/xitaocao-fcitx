#ifdef _ENABLE_TRAY

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xmd.h>

#include "ui/tray.h"
#include "ui/TrayWindow.h"
#include "tools/tools.h"

#define MAX_SUPPORTED_XEMBED_VERSION 1

#define XEMBED_MAPPED          (1 << 0)

/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY  0
#define XEMBED_WINDOW_ACTIVATE  1
#define XEMBED_WINDOW_DEACTIVATE  2
#define XEMBED_REQUEST_FOCUS 3
#define XEMBED_FOCUS_IN  4
#define XEMBED_FOCUS_OUT  5
#define XEMBED_FOCUS_NEXT 6
#define XEMBED_FOCUS_PREV 7
/* 8-9 were used for XEMBED_GRAB_KEY/XEMBED_UNGRAB_KEY */
#define XEMBED_MODALITY_ON 10
#define XEMBED_MODALITY_OFF 11
#define XEMBED_REGISTER_ACCELERATOR     12
#define XEMBED_UNREGISTER_ACCELERATOR   13
#define XEMBED_ACTIVATE_ACCELERATOR     14

static int iTrappedErrorCode = 0;
static int (*hOldErrorHandler) (Display *d, XErrorEvent *e);

extern int iScreen;

/* static void tray_map_window (Display* dpy, Window win); */

static int
ErrorHandler(Display     *display,
        XErrorEvent *error)
{
    iTrappedErrorCode = error->error_code;
    return 0;
}

static void
TrapErrors(void)
{
    iTrappedErrorCode = 0;
    hOldErrorHandler = XSetErrorHandler(ErrorHandler);
}

static int
UntrapErrors(void)
{
    XSetErrorHandler(hOldErrorHandler);
    return iTrappedErrorCode;
}

int
InitTray(Display* dpy, TrayWindow* tray)
{
    static char *atom_names[] = {
        NULL,
		"MANAGER", 
		"_NET_SYSTEM_TRAY_OPCODE",
		"_NET_SYSTEM_TRAY_ORIENTATION",
		"_NET_SYSTEM_TRAY_VISUAL"
    };
    memset(tray, 0, sizeof(TrayWindow));

	atom_names[0] = strdup("_NET_SYSTEM_TRAY_S0");
	atom_names[0][17] += iScreen;

    XInternAtoms (dpy, atom_names, 5, False, tray->atoms);
    tray->size = 22;

    return True;
}

int
TrayFindDock(Display *dpy, TrayWindow* tray)
{
    Window Dock;
    
    XGrabServer (dpy);

    Dock = XGetSelectionOwner(dpy, tray->atoms[ATOM_SELECTION]);

    if (!Dock)
        XSelectInput(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                StructureNotifyMask);

    XUngrabServer (dpy);
    XFlush (dpy);

    if (Dock != None) {
        TraySendOpcode(dpy, Dock, tray, SYSTEM_TRAY_REQUEST_DOCK, tray->window, 0, 0);
        return 1;
    } 

    return 0;
}

void TraySendOpcode(Display* dpy, Window dock, TrayWindow* tray,
        long message, long data1, long data2, long data3)
{
    XEvent ev;

    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = dock;
    ev.xclient.message_type = tray->atoms[ATOM_SYSTEM_TRAY_OPCODE];
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = CurrentTime;
    ev.xclient.data.l[1] = message;
    ev.xclient.data.l[2] = data1;
    ev.xclient.data.l[3] = data2;
    ev.xclient.data.l[4] = data3;

    TrapErrors();
    XSendEvent(dpy, dock, False, NoEventMask, &ev);
    XSync(dpy, False);
    if (UntrapErrors()) {
        FcitxLog(WARNING, _("X error %i on opcode send"),
                iTrappedErrorCode );
    }
}

XVisualInfo* TrayGetVisual(Display* dpy, TrayWindow* tray)
{
    Window Dock;
    Dock = XGetSelectionOwner(dpy, tray->atoms[ATOM_SELECTION]);

    if (Dock != None) {
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_remaining;
        unsigned char *data = 0;
        int result = XGetWindowProperty(dpy, Dock, tray->atoms[ATOM_VISUAL], 0, 1,
                                        False, XA_VISUALID, &actual_type,
                                        &actual_format, &nitems, &bytes_remaining, &data);
        VisualID vid = 0;
        if (result == Success && data && actual_type == XA_VISUALID && actual_format == 32 &&
                nitems == 1 && bytes_remaining == 0)
            vid = *(VisualID*)data;
        if (data)
            XFree(data);
        if (vid == 0)
            return 0;
        
        uint mask = VisualIDMask;
        XVisualInfo *vi, rvi;
        int count;
        rvi.visualid = vid;
        vi = XGetVisualInfo(dpy, mask, &rvi, &count);
        if (vi) {
            tray->visual = vi[0];
            XFree((char*)vi);
        }
        if (tray->visual.depth != 32)
            memset(&tray->visual, 0, sizeof(XVisualInfo));
    }

    return tray->visual.visual ? &tray->visual : 0;

}

Window TrayGetDock(Display* dpy, TrayWindow* tray)
{
    Window dock = XGetSelectionOwner(dpy, tray->atoms[ATOM_SELECTION]);
    return dock;
}

#endif