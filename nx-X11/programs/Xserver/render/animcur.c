/*
 * Copyright © 2002 Keith Packard, member of The XFree86 Project, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Animated cursors for X.  Not specific to Render in any way, but
 * stuck there because Render has the other cool cursor extension.
 * Besides, everyone has Render.
 *
 * Implemented as a simple layer over the core cursor code; it
 * creates composite cursors out of a set of static cursors and
 * delta times between each image.
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <nx-X11/X.h>
#include <nx-X11/Xmd.h>
#include "servermd.h"
#include "scrnintstr.h"
#include "dixstruct.h"
#include "cursorstr.h"
#include "dixfontstr.h"
#include "opaque.h"
#include "picturestr.h"
#include "inputstr.h"

#ifdef NEED_NEWER_XORG_VERSION
#include "xace.h"
#endif

typedef struct _AnimCurElt {
    CursorPtr pCursor;          /* cursor to show */
    CARD32 delay;               /* in ms */
} AnimCurElt;

typedef struct _AnimCur {
    int nelt;                   /* number of elements in the elts array */
    AnimCurElt *elts;           /* actually allocated right after the structure */
} AnimCurRec, *AnimCurPtr;

typedef struct _AnimScrPriv {
#ifndef NEED_NEWER_XORG_VERSION
    CursorPtr pCursor;
    int elt;
    CARD32 time;
#endif

    CloseScreenProcPtr CloseScreen;

#ifndef NEED_NEWER_XORG_VERSION
    ScreenBlockHandlerProcPtr BlockHandler;
#endif

    CursorLimitsProcPtr CursorLimits;
    DisplayCursorProcPtr DisplayCursor;
    SetCursorPositionProcPtr SetCursorPosition;
    RealizeCursorProcPtr RealizeCursor;
    UnrealizeCursorProcPtr UnrealizeCursor;
    RecolorCursorProcPtr RecolorCursor;

#ifdef NEED_NEWER_XORG_VERSION
    OsTimerPtr timer;
    Bool timer_set;
#endif
} AnimCurScreenRec, *AnimCurScreenPtr;

#ifndef NEED_NEWER_XORG_VERSION
typedef struct _AnimCurState {
    CursorPtr			pCursor;
    ScreenPtr			pScreen;
    int				elt;
    CARD32			time;
} AnimCurStateRec, *AnimCurStatePtr;

/* What a waste. But we need an API change to alloc it per device only. */
static AnimCurStateRec animCurState[MAX_DEVICES];
#endif

static unsigned char empty[4];

static CursorBits animCursorBits = {
    empty, empty, 2, 1, 1, 0, 0, 1
};

#ifdef NEED_NEWER_XORG_VERSION
static DevPrivateKeyRec AnimCurScreenPrivateKeyRec;

#define AnimCurScreenPrivateKey (&AnimCurScreenPrivateKeyRec)
#else
int AnimCurGeneration;
int AnimCurScreenPrivateIndex = -1;
#endif

#define IsAnimCur(c)	    ((c) && ((c)->bits == &animCursorBits))

#ifdef NEED_NEWER_XORG_VERSION
#define GetAnimCur(c)	    ((AnimCurPtr) ((((char *)(c) + CURSOR_REC_SIZE))))
#define GetAnimCurScreen(s) ((AnimCurScreenPtr)dixLookupPrivate(&(s)->devPrivates, AnimCurScreenPrivateKey))
#define SetAnimCurScreen(s,p) dixSetPrivate(&(s)->devPrivates, AnimCurScreenPrivateKey, p)
#else
#define GetAnimCur(c)       ((AnimCurPtr) ((c) + 1))
#define GetAnimCurScreen(s) ((AnimCurScreenPtr) ((s)->devPrivates[AnimCurScreenPrivateIndex].ptr))
#define SetAnimCurScreen(s,p) ((s)->devPrivates[AnimCurScreenPrivateIndex].ptr = (void *) (p))
#endif

#define Wrap(as,s,elt,func) (((as)->elt = (s)->elt), (s)->elt = func)
#define Unwrap(as,s,elt)    ((s)->elt = (as)->elt)

static Bool
AnimCurCloseScreen(ScreenPtr pScreen)
{
    AnimCurScreenPtr as = GetAnimCurScreen(pScreen);
    Bool ret;

    Unwrap(as, pScreen, CloseScreen);

#ifndef NEED_NEWER_XORG_VERSION
    Unwrap(as, pScreen, BlockHandler);
#endif

    Unwrap(as, pScreen, CursorLimits);
    Unwrap(as, pScreen, DisplayCursor);
    Unwrap(as, pScreen, SetCursorPosition);
    Unwrap(as, pScreen, RealizeCursor);
    Unwrap(as, pScreen, UnrealizeCursor);
    Unwrap(as, pScreen, RecolorCursor);
    SetAnimCurScreen(pScreen, 0);
    ret = (*pScreen->CloseScreen) (pScreen);
    free(as);
    if (screenInfo.numScreens <= 1)
        AnimCurScreenPrivateIndex = -1;
    return ret;
}

static void
AnimCurCursorLimits(
#ifdef NEED_NEWER_XORG_VERSION
                    DeviceIntPtr pDev,
#endif
                    ScreenPtr pScreen,
                    CursorPtr pCursor, BoxPtr pHotBox, BoxPtr pTopLeftBox)
{
    AnimCurScreenPtr as = GetAnimCurScreen(pScreen);

    Unwrap(as, pScreen, CursorLimits);
    if (IsAnimCur(pCursor)) {
        AnimCurPtr ac = GetAnimCur(pCursor);

        (*pScreen->CursorLimits) (
#ifdef NEED_NEWER_XORG_VERSION
                                  pDev,
#endif
                                  pScreen, ac->elts[0].pCursor,
                                  pHotBox, pTopLeftBox);
    }
    else {
        (*pScreen->CursorLimits) (
#ifdef NEED_NEWER_XORG_VERSION
                                  pDev,
#endif
                                  pScreen, pCursor, pHotBox, pTopLeftBox);
    }
    Wrap(as, pScreen, CursorLimits, AnimCurCursorLimits);
}

#ifndef NEED_NEWER_XORG_VERSION
/*
 * This has to be a screen block handler instead of a generic
 * block handler so that it is well ordered with respect to the DRI
 * block handler responsible for releasing the hardware to DRI clients
 */

static void
AnimCurScreenBlockHandler (int screenNum,
			   pointer blockData,
			   pointer pTimeout,
			   pointer pReadmask)
#else
/*
 * The cursor animation timer has expired, go display any relevant cursor changes
 * and compute a new timeout value
 */

static CARD32
AnimCurTimerNotify(OsTimerPtr timer, CARD32 now, void *arg)
#endif
{
#ifdef NEED_NEWER_XORG_VERSION
    ScreenPtr pScreen = arg;
#else
    ScreenPtr		pScreen = screenInfo.screens[screenNum];
#endif
    AnimCurScreenPtr as = GetAnimCurScreen(pScreen);
    DeviceIntPtr dev;
#ifdef NEED_NEWER_XORG_VERSION
    Bool activeDevice = FALSE;
#else
    CARD32              now = 0;
#endif
    CARD32              soonest = ~0; /* earliest time to wakeup again */

    for (dev = inputInfo.devices; dev; dev = dev->next) {
#ifdef NEED_NEWER_XORG_VERSION
        if (IsPointerDevice(dev) && pScreen == dev->spriteInfo->anim.pScreen) {
#else
        if (IsPointerDevice(dev) && pScreen == animCurState[dev->id].pScreen) {
#endif
#ifdef NEED_NEWER_XORG_VERSION
            if (!activeDevice)
                activeDevice = TRUE;
#else
	    if (!now) now = GetTimeInMillis ();
#endif

#ifdef NEED_NEWER_XORG_VERSION
            if ((INT32) (now - dev->spriteInfo->anim.time) >= 0) {
                AnimCurPtr ac = GetAnimCur(dev->spriteInfo->anim.pCursor);
                int elt = (dev->spriteInfo->anim.elt + 1) % ac->nelt;
#else
            if ((INT32) (now - animCurState[dev->id].time) >= 0) {
                AnimCurPtr ac = GetAnimCur(animCurState[dev->id].pCursor);
                int elt = (animCurState[dev->id].elt + 1) % ac->nelt;
#endif
                DisplayCursorProcPtr DisplayCursor;

                /*
                 * Not a simple Unwrap/Wrap as this
                 * isn't called along the DisplayCursor
                 * wrapper chain.
                 */
                DisplayCursor = pScreen->DisplayCursor;
                pScreen->DisplayCursor = as->DisplayCursor;
                (void) (*pScreen->DisplayCursor) (
#ifdef NEED_NEWER_XORG_VERSION
                                                  dev,
#endif
                                                  pScreen,
                                                  ac->elts[elt].pCursor);
                as->DisplayCursor = pScreen->DisplayCursor;
                pScreen->DisplayCursor = DisplayCursor;

#ifdef NEED_NEWER_XORG_VERSION
                dev->spriteInfo->anim.elt = elt;
                dev->spriteInfo->anim.time = now + ac->elts[elt].delay;
#else
                animCurState[dev->id].elt = elt;
                animCurState[dev->id].time = now + ac->elts[elt].delay;
#endif
            }

#ifdef NEED_NEWER_XORG_VERSION
            if (soonest > dev->spriteInfo->anim.time)
                soonest = dev->spriteInfo->anim.time;
#else
            if (soonest > animCurState[dev->id].time)
                soonest = animCurState[dev->id].time;
#endif
        }
    }

#ifdef NEED_NEWER_XORG_VERSION
    if (activeDevice)
        TimerSet(as->timer, TimerAbsolute, soonest, AnimCurTimerNotify, pScreen);
    else
        as->timer_set = FALSE;

    return 0;
#else
    if (now)
        AdjustWaitForDelay (pTimeout, soonest - now);

    Unwrap (as, pScreen, BlockHandler);
    (*pScreen->BlockHandler) (screenNum, blockData, pTimeout, pReadmask);
    Wrap (as, pScreen, BlockHandler, AnimCurScreenBlockHandler);
#endif
}

static Bool
AnimCurDisplayCursor(
#ifdef NEED_NEWER_XORG_VERSION
                     DeviceIntPtr pDev,
#endif
                     ScreenPtr pScreen, CursorPtr pCursor)
{
    AnimCurScreenPtr as = GetAnimCurScreen(pScreen);
    Bool ret;

#ifdef NEED_NEWER_XORG_VERSION
    if (IsFloating(pDev))
        return FALSE;
#else
    DeviceIntPtr dev;
#endif


    Unwrap(as, pScreen, DisplayCursor);
#ifndef NEED_NEWER_XORG_VERSION
    ret = TRUE;

    for (dev = inputInfo.devices; dev; dev = dev->next) {
        if (IsPointerDevice(dev) && pScreen == animCurState[dev->id].pScreen) {
#endif
            if (IsAnimCur(pCursor)) {
#ifdef NEED_NEWER_XORG_VERSION
                if (pCursor != pDev->spriteInfo->anim.pCursor) {
#else
                if (pCursor != animCurState[dev->id].pCursor) {
#endif
                    AnimCurPtr ac = GetAnimCur(pCursor);

                    ret = (*pScreen->DisplayCursor)
                        (
#ifdef NEED_NEWER_XORG_VERSION
                         pDev,
#endif
                         pScreen, ac->elts[0].pCursor);
                    if (ret) {
#ifdef NEED_NEWER_XORG_VERSION
                        pDev->spriteInfo->anim.elt = 0;
                        pDev->spriteInfo->anim.time =
                            GetTimeInMillis() + ac->elts[0].delay;
                        pDev->spriteInfo->anim.pCursor = pCursor;
                        pDev->spriteInfo->anim.pScreen = pScreen;
#else
                        animCurState[dev->id].elt = 0;
                        animCurState[dev->id].time =
                            GetTimeInMillis() + ac->elts[0].delay;
                        animCurState[dev->id].pCursor = pCursor;
                        animCurState[dev->id].pScreen = pScreen;
#endif

#ifdef NEED_NEWER_XORG_VERSION
                        if (!as->timer_set) {
                            TimerSet(as->timer, TimerAbsolute, pDev->spriteInfo->anim.time,
                                     AnimCurTimerNotify, pScreen);
                            as->timer_set = TRUE;
                        }
#endif
                    }
                }
                else
                    ret = TRUE;
            }
            else {
#ifdef NEED_NEWER_XORG_VERSION
                pDev->spriteInfo->anim.pCursor = 0;
                pDev->spriteInfo->anim.pScreen = 0;
#else
                animCurState[dev->id].pCursor = 0;
                animCurState[dev->id].pScreen = 0;
#endif

                ret = (*pScreen->DisplayCursor) (
#ifdef NEED_NEWER_XORG_VERSION
                                                 pDev,
#endif
                                                 pScreen, pCursor);
            }
#ifndef NEED_NEWER_XORG_VERSION
        }
    }
#endif
    Wrap(as, pScreen, DisplayCursor, AnimCurDisplayCursor);
    return ret;
}

static Bool
AnimCurSetCursorPosition(
#ifdef NEED_NEWER_XORG_VERSION
                         DeviceIntPtr pDev,
#endif
                         ScreenPtr pScreen, int x, int y, Bool generateEvent)
{
    AnimCurScreenPtr as = GetAnimCurScreen(pScreen);
    Bool ret;

#ifndef NEED_NEWER_XORG_VERSION
    DeviceIntPtr dev;
#endif

    Unwrap(as, pScreen, SetCursorPosition);
#ifndef NEED_NEWER_XORG_VERSION
    for (dev = inputInfo.devices; dev; dev = dev->next) {
        if (IsPointerDevice(dev) && pScreen == animCurState[dev->id].pScreen) {
#endif
#ifdef NEED_NEWER_XORG_VERSION
            if (pDev->spriteInfo->anim.pCursor) {
                pDev->spriteInfo->anim.pScreen = pScreen;
#else
            if (animCurState[dev->id].pCursor) {
                animCurState[dev->id].pScreen = pScreen;
#endif

#ifdef NEED_NEWER_XORG_VERSION
                if (!as->timer_set) {
                    TimerSet(as->timer, TimerAbsolute, pDev->spriteInfo->anim.time,
                             AnimCurTimerNotify, pScreen);
                    as->timer_set = TRUE;
                }
#endif
            }
#ifndef NEED_NEWER_XORG_VERSION
        }
    }
#endif
    ret = (*pScreen->SetCursorPosition) (
#ifdef NEED_NEWER_XORG_VERSION
                                         pDev,
#endif
                                         pScreen, x, y, generateEvent);
    Wrap(as, pScreen, SetCursorPosition, AnimCurSetCursorPosition);
    return ret;
}

static Bool
AnimCurRealizeCursor(
#ifdef NEED_NEWER_XORG_VERSION
                     DeviceIntPtr pDev,
#endif
                     ScreenPtr pScreen, CursorPtr pCursor)
{
    AnimCurScreenPtr as = GetAnimCurScreen(pScreen);
    Bool ret;

    Unwrap(as, pScreen, RealizeCursor);
    if (IsAnimCur(pCursor))
        ret = TRUE;
    else
        ret = (*pScreen->RealizeCursor) (
#ifdef NEED_NEWER_XORG_VERSION
                                         pDev,
#endif
                                         pScreen, pCursor);
    Wrap(as, pScreen, RealizeCursor, AnimCurRealizeCursor);
    return ret;
}

static Bool
AnimCurUnrealizeCursor(
#ifdef NEED_NEWER_XORG_VERSION
                       DeviceIntPtr pDev,
#endif
                       ScreenPtr pScreen, CursorPtr pCursor)
{
    AnimCurScreenPtr as = GetAnimCurScreen(pScreen);
    Bool ret;

    Unwrap(as, pScreen, UnrealizeCursor);
    if (IsAnimCur(pCursor)) {
        AnimCurPtr ac = GetAnimCur(pCursor);
        int i;

        if (pScreen->myNum == 0)
            for (i = 0; i < ac->nelt; i++)
                FreeCursor(ac->elts[i].pCursor, 0);
        ret = TRUE;
    }
    else
        ret = (*pScreen->UnrealizeCursor) (
#ifdef NEED_NEWER_XORG_VERSION
                                           pDev,
#endif
                                           pScreen, pCursor);
    Wrap(as, pScreen, UnrealizeCursor, AnimCurUnrealizeCursor);
    return ret;
}

static void
AnimCurRecolorCursor(
#ifdef NEED_NEWER_XORG_VERSION
                     DeviceIntPtr pDev,
#endif
                     ScreenPtr pScreen, CursorPtr pCursor, Bool displayed)
{
    AnimCurScreenPtr as = GetAnimCurScreen(pScreen);

#ifndef NEED_NEWER_XORG_VERSION
    DeviceIntPtr dev;
#endif

    Unwrap(as, pScreen, RecolorCursor);
    if (IsAnimCur(pCursor)) {
#ifndef NEED_NEWER_XORG_VERSION
        for (dev = inputInfo.devices; dev; dev = dev->next) {
            if (IsPointerDevice(dev) && pScreen == animCurState[dev->id].pScreen) {
#endif
                AnimCurPtr ac = GetAnimCur(pCursor);
                int i;

                for (i = 0; i < ac->nelt; i++)
                    (*pScreen->RecolorCursor) (
#ifdef NEED_NEWER_XORG_VERSION
                                               pDev,
#endif
                                               pScreen, ac->elts[i].pCursor,
                                               displayed &&
#ifdef NEED_NEWER_XORG_VERSION
                                               pDev->spriteInfo->anim.elt == i
#else
                                               animCurState[dev->id].elt == i
#endif
                                              );
#ifndef NEED_NEWER_XORG_VERSION
            }
        }
#endif
    }
    else
        (*pScreen->RecolorCursor) (
#ifdef NEED_NEWER_XORG_VERSION
                                   pDev,
#endif
                                   pScreen, pCursor, displayed);
    Wrap(as, pScreen, RecolorCursor, AnimCurRecolorCursor);
}

Bool
AnimCurInit(ScreenPtr pScreen)
{
    AnimCurScreenPtr as;

#ifdef NEED_NEWER_XORG_VERSION
    if (!dixRegisterPrivateKey(&AnimCurScreenPrivateKeyRec, PRIVATE_SCREEN, 0))
        return FALSE;
#else
    if (AnimCurGeneration != serverGeneration) {
        int i = 0;

        AnimCurScreenPrivateIndex = AllocateScreenPrivateIndex();
        if (AnimCurScreenPrivateIndex < 0)
            return FALSE;
        AnimCurGeneration = serverGeneration;
        for (i = 0; i < MAX_DEVICES; i++) {
            animCurState[i].pCursor = 0;
            animCurState[i].pScreen = 0;
            animCurState[i].elt = 0;
            animCurState[i].time = 0;
        }
    }
#endif
    as = (AnimCurScreenPtr) malloc(sizeof(AnimCurScreenRec));
    if (!as)
        return FALSE;

#ifdef NEED_NEWER_XORG_VERSION
    as->timer = TimerSet(NULL, TimerAbsolute, 0, AnimCurTimerNotify, pScreen);
    if (!as->timer) {
        free(as);
        return FALSE;
    }
    as->timer_set = FALSE;
#endif

    Wrap(as, pScreen, CloseScreen, AnimCurCloseScreen);

#ifndef NEED_NEWER_XORG_VERSION
    Wrap(as, pScreen, BlockHandler, AnimCurScreenBlockHandler);
#endif

    Wrap(as, pScreen, CursorLimits, AnimCurCursorLimits);
    Wrap(as, pScreen, DisplayCursor, AnimCurDisplayCursor);
    Wrap(as, pScreen, SetCursorPosition, AnimCurSetCursorPosition);
    Wrap(as, pScreen, RealizeCursor, AnimCurRealizeCursor);
    Wrap(as, pScreen, UnrealizeCursor, AnimCurUnrealizeCursor);
    Wrap(as, pScreen, RecolorCursor, AnimCurRecolorCursor);
    SetAnimCurScreen(pScreen, as);
    return TRUE;
}

int
AnimCursorCreate(CursorPtr *cursors, CARD32 *deltas, int ncursor,
                 CursorPtr *ppCursor, ClientPtr client, XID cid)
{
    CursorPtr pCursor;
#ifdef NEED_NEWER_XORG_VERSION
    int rc;
#endif
    int i;
    AnimCurPtr ac;

    for (i = 0; i < screenInfo.numScreens; i++) {
#ifdef NEED_NEWER_XORG_VERSION
        if (!GetAnimCurScreen(screenInfo.screens[i]))
#else
        if (AnimCurScreenPrivateIndex == -1 || !GetAnimCurScreen(screenInfo.screens[i]))
#endif
            return BadImplementation;
    }

    for (i = 0; i < ncursor; i++)
        if (IsAnimCur(cursors[i]))
            return BadMatch;

    pCursor = (CursorPtr) calloc(CURSOR_REC_SIZE +
                                 sizeof(AnimCurRec) +
                                 ncursor * sizeof(AnimCurElt), 1);
    if (!pCursor)
        return BadAlloc;

#ifdef NEED_NEWER_XORG_VERSION
    dixInitPrivates(pCursor, pCursor + 1, PRIVATE_CURSOR);
#endif
    pCursor->bits = &animCursorBits;
    pCursor->refcnt = 1;

    pCursor->foreRed = cursors[0]->foreRed;
    pCursor->foreGreen = cursors[0]->foreGreen;
    pCursor->foreBlue = cursors[0]->foreBlue;

    pCursor->backRed = cursors[0]->backRed;
    pCursor->backGreen = cursors[0]->backGreen;
    pCursor->backBlue = cursors[0]->backBlue;

#ifdef NEED_NEWER_XORG_VERSION
    pCursor->id = cid;
    pCursor->devPrivates = NULL;
#endif

#ifdef NEED_NEWER_XORG_VERSION
    /* security creation/labeling check */
    rc = XaceHook(XACE_RESOURCE_ACCESS, client, cid, RT_CURSOR, pCursor,
                  RT_NONE, NULL, DixCreateAccess);
    if (rc != Success) {
        dixFiniPrivates(pCursor, PRIVATE_CURSOR);
        free(pCursor);
        return rc;
    }
#endif

    /*
     * Fill in the AnimCurRec
     */
    animCursorBits.refcnt++;
    ac = GetAnimCur(pCursor);
    ac->nelt = ncursor;
    ac->elts = (AnimCurElt *) (ac + 1);

    for (i = 0; i < ncursor; i++) {
        ac->elts[i].pCursor = RefCursor(cursors[i]);
        ac->elts[i].delay = deltas[i];
    }

    *ppCursor = pCursor;
    return Success;
}
