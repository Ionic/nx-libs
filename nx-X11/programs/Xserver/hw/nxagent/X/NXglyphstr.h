/**************************************************************************/
/*                                                                        */
/* Copyright (c) 2001, 2010 NoMachine, http://www.nomachine.com/.         */
/*                                                                        */
/* NXAGENT, NX protocol compression and NX extensions to this software    */
/* are copyright of NoMachine. Redistribution and use of the present      */
/* software is allowed according to terms specified in the file LICENSE   */
/* which comes in the source distribution.                                */
/*                                                                        */
/* Check http://www.nomachine.com/licensing.html for applicability.       */
/*                                                                        */
/* NX and NoMachine are trademarks of Medialogic S.p.A.                   */
/*                                                                        */
/* All rights reserved.                                                   */
/*                                                                        */
/**************************************************************************/

/*
 * $XFree86: xc/programs/Xserver/render/glyphstr.h,v 1.3 2000/11/20 07:13:13 keithp Exp $
 *
 * Copyright © 2000 SuSE, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of SuSE not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  SuSE makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * SuSE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL SuSE
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, SuSE, Inc.
 */

/*
 * This must keep the same symbol as the original glyphstr.h
 * or symbols  will be redefined. The code here adds a field
 * to _GlyphSet. This should be done by defining a new type
 * and casting when appropriate.
 */

#ifndef _GLYPHSTR_H_
#define _GLYPHSTR_H_

#include <X11/extensions/renderproto.h>
#include "../../render/picture.h"
#include "screenint.h"

#define GlyphFormat1	0
#define GlyphFormat4	1
#define GlyphFormat8	2
#define GlyphFormat16	3
#define GlyphFormat32	4
#define GlyphFormatNum	5

typedef struct _Glyph {
    CARD32	refcnt;
    CARD32	size;	/* info + bitmap */
    xGlyphInfo	info;
    /* bits follow */
} GlyphRec, *GlyphPtr;

typedef struct _GlyphRef {
    CARD32	signature;
    GlyphPtr	glyph;
    CARD16      corruptedGlyph;
} GlyphRefRec, *GlyphRefPtr;

#define DeletedGlyph	((GlyphPtr) 1)

typedef struct _GlyphHashSet {
    CARD32	entries;
    CARD32	size;
    CARD32	rehash;
} GlyphHashSetRec, *GlyphHashSetPtr;

typedef struct _GlyphHash {
    GlyphRefPtr	    table;
    GlyphHashSetPtr hashSet;
    CARD32	    tableEntries;
} GlyphHashRec, *GlyphHashPtr;

typedef struct _GlyphSet {
    CARD32	    refcnt;
    PictFormatPtr   format;
    int		    fdepth;
    GlyphHashRec    hash;
    int             maxPrivate;
    pointer         *devPrivates;
    CARD32          remoteID;
} GlyphSetRec, *GlyphSetPtr;

#define GlyphSetGetPrivate(pGlyphSet,n)					\
	((n) > (pGlyphSet)->maxPrivate ?				\
	 (pointer) 0 :							\
	 (pGlyphSet)->devPrivates[n])

#define GlyphSetSetPrivate(pGlyphSet,n,ptr)				\
	((n) > (pGlyphSet)->maxPrivate ?				\
	 _GlyphSetSetNewPrivate(pGlyphSet, n, ptr) :			\
	 ((((pGlyphSet)->devPrivates[n] = (ptr)) != 0) || TRUE))

typedef struct _GlyphList {
    INT16	    xOff;
    INT16	    yOff;
    CARD8	    len;
    PictFormatPtr   format;
} GlyphListRec, *GlyphListPtr;

extern GlyphHashRec	globalGlyphs[GlyphFormatNum];

GlyphHashSetPtr
FindGlyphHashSet (CARD32 filled);

int
AllocateGlyphSetPrivateIndex (void);

void
ResetGlyphSetPrivateIndex (void);

Bool
_GlyphSetSetNewPrivate (GlyphSetPtr glyphSet, int n, pointer ptr);

Bool
GlyphInit (ScreenPtr pScreen);

GlyphRefPtr
FindGlyphRef (GlyphHashPtr hash, CARD32 signature, Bool match, GlyphPtr compare);

CARD32
HashGlyph (GlyphPtr glyph);

void
FreeGlyph (GlyphPtr glyph, int format);

void
AddGlyph (GlyphSetPtr glyphSet, GlyphPtr glyph, Glyph id);

Bool
DeleteGlyph (GlyphSetPtr glyphSet, Glyph id);

GlyphPtr
FindGlyph (GlyphSetPtr glyphSet, Glyph id);

GlyphPtr
AllocateGlyph (xGlyphInfo *gi, int format);

Bool
AllocateGlyphHash (GlyphHashPtr hash, GlyphHashSetPtr hashSet);

Bool
ResizeGlyphHash (GlyphHashPtr hash, CARD32 change, Bool global);

Bool
ResizeGlyphSet (GlyphSetPtr glyphSet, CARD32 change);

GlyphSetPtr
AllocateGlyphSet (int fdepth, PictFormatPtr format);

int
FreeGlyphSet (pointer   value,
	      XID       gid);



#endif /* _GLYPHSTR_H_ */