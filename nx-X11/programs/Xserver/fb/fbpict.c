/*
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

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <string.h>

#include "fb.h"

#ifdef RENDER

#include "picturestr.h"
#include "mipict.h"
#include "fbpict.h"

typedef void	(*CompositeFunc) (CARD8      op,
				  PicturePtr pSrc,
				  PicturePtr pMask,
				  PicturePtr pDst,
				  INT16      xSrc,
				  INT16      ySrc,
				  INT16      xMask,
                                  INT16      yMask,
				  INT16      xDst,
				  INT16      yDst,
				  CARD16     width,
				  CARD16     height);

CARD32
fbOver (CARD32 x, CARD32 y)
{
    CARD16  a = ~x >> 24;
    CARD16  t;
    CARD32  m,n,o,p;

    m = FbOverU(x,y,0,a,t);
    n = FbOverU(x,y,8,a,t);
    o = FbOverU(x,y,16,a,t);
    p = FbOverU(x,y,24,a,t);
    return m|n|o|p;
}

CARD32
fbOver24 (CARD32 x, CARD32 y)
{
    CARD16  a = ~x >> 24;
    CARD16  t;
    CARD32  m,n,o;

    m = FbOverU(x,y,0,a,t);
    n = FbOverU(x,y,8,a,t);
    o = FbOverU(x,y,16,a,t);
    return m|n|o;
}

CARD32
fbIn (CARD32 x, CARD8 y)
{
    CARD16  a = y;
    CARD16  t;
    CARD32  m,n,o,p;

    m = FbInU(x,0,a,t);
    n = FbInU(x,8,a,t);
    o = FbInU(x,16,a,t);
    p = FbInU(x,24,a,t);
    return m|n|o|p;
}

/*
 * Naming convention:
 *
 *  opSRCxMASKxDST
 */

void
fbCompositeSolidMask_nx8x8888 (CARD8      op,
			       PicturePtr pSrc,
			       PicturePtr pMask,
			       PicturePtr pDst,
			       INT16      xSrc,
			       INT16      ySrc,
			       INT16      xMask,
			       INT16      yMask,
			       INT16      xDst,
			       INT16      yDst,
			       CARD16     width,
			       CARD16     height)
{
    CARD32	src, srca;
    CARD32	*dstLine, *dst, d, dstMask;
    CARD8	*maskLine, *mask, m;
    FbStride	dstStride, maskStride;
    CARD16	w;

    fbComposeGetSolid(pSrc, src, pDst->format);

    dstMask = FbFullMask (pDst->pDrawable->depth);
    srca = src >> 24;
    if (src == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, CARD8, maskStride, maskLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    *dst = src & dstMask;
		else
		    *dst = fbOver (src, *dst) & dstMask;
	    }
	    else if (m)
	    {
		d = fbIn (src, m);
		*dst = fbOver (d, *dst) & dstMask;
	    }
	    dst++;
	}
    }
}

void
fbCompositeSolidMask_nx8888x8888C (CARD8      op,
				   PicturePtr pSrc,
				   PicturePtr pMask,
				   PicturePtr pDst,
				   INT16      xSrc,
				   INT16      ySrc,
				   INT16      xMask,
				   INT16      yMask,
				   INT16      xDst,
				   INT16      yDst,
				   CARD16     width,
				   CARD16     height)
{
    CARD32	src, srca;
    CARD32	*dstLine, *dst, d, dstMask;
    CARD32	*maskLine, *mask, ma;
    FbStride	dstStride, maskStride;
    CARD16	w;
    CARD32	m, n, o, p;

    fbComposeGetSolid(pSrc, src, pDst->format);

    dstMask = FbFullMask (pDst->pDrawable->depth);
    srca = src >> 24;
    if (src == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, CARD32, maskStride, maskLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    ma = *mask++;
	    if (ma == 0xffffffff)
	    {
		if (srca == 0xff)
		    *dst = src & dstMask;
		else
		    *dst = fbOver (src, *dst) & dstMask;
	    }
	    else if (ma)
	    {
		d = *dst;
#define FbInOverC(src,srca,msk,dst,i,result) { \
    CARD16  __a = FbGet8(msk,i); \
    CARD32  __t, __ta; \
    CARD32  __i; \
    __t = FbIntMult (FbGet8(src,i), __a,__i); \
    __ta = (CARD8) ~FbIntMult (srca, __a,__i); \
    __t = __t + FbIntMult(FbGet8(dst,i),__ta,__i); \
    __t = (CARD32) (CARD8) (__t | (-(__t >> 8))); \
    result = __t << (i); \
}
		FbInOverC (src, srca, ma, d, 0, m);
		FbInOverC (src, srca, ma, d, 8, n);
		FbInOverC (src, srca, ma, d, 16, o);
		FbInOverC (src, srca, ma, d, 24, p);
		*dst = m|n|o|p;
	    }
	    dst++;
	}
    }
}

void
fbCompositeSolidMask_nx8x0888 (CARD8      op,
			       PicturePtr pSrc,
			       PicturePtr pMask,
			       PicturePtr pDst,
			       INT16      xSrc,
			       INT16      ySrc,
			       INT16      xMask,
			       INT16      yMask,
			       INT16      xDst,
			       INT16      yDst,
			       CARD16     width,
			       CARD16     height)
{
    CARD32	src, srca;
    CARD8	*dstLine, *dst;
    CARD32	d;
    CARD8	*maskLine, *mask, m;
    FbStride	dstStride, maskStride;
    CARD16	w;

    fbComposeGetSolid(pSrc, src, pDst->format);

    srca = src >> 24;
    if (src == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, CARD8, dstStride, dstLine, 3);
    fbComposeGetStart (pMask, xMask, yMask, CARD8, maskStride, maskLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    d = src;
		else
		{
		    d = Fetch24(dst);
		    d = fbOver24 (src, d);
		}
		Store24(dst,d);
	    }
	    else if (m)
	    {
		d = fbOver24 (fbIn(src,m), Fetch24(dst));
		Store24(dst,d);
	    }
	    dst += 3;
	}
    }
}

void
fbCompositeSolidMask_nx8x0565 (CARD8      op,
				  PicturePtr pSrc,
				  PicturePtr pMask,
				  PicturePtr pDst,
				  INT16      xSrc,
				  INT16      ySrc,
				  INT16      xMask,
				  INT16      yMask,
				  INT16      xDst,
				  INT16      yDst,
				  CARD16     width,
				  CARD16     height)
{
    CARD32	src, srca;
    CARD16	*dstLine, *dst;
    CARD32	d;
    CARD8	*maskLine, *mask, m;
    FbStride	dstStride, maskStride;
    CARD16	w;

    fbComposeGetSolid(pSrc, src, pDst->format);

    srca = src >> 24;
    if (src == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, CARD16, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, CARD8, maskStride, maskLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    d = src;
		else
		{
		    d = *dst;
		    d = fbOver24 (src, cvt0565to8888(d));
		}
		*dst = cvt8888to0565(d);
	    }
	    else if (m)
	    {
		d = *dst;
		d = fbOver24 (fbIn(src,m), cvt0565to8888(d));
		*dst = cvt8888to0565(d);
	    }
	    dst++;
	}
    }
}

void
fbCompositeSolidMask_nx8888x0565C (CARD8      op,
				   PicturePtr pSrc,
				   PicturePtr pMask,
				   PicturePtr pDst,
				   INT16      xSrc,
				   INT16      ySrc,
				   INT16      xMask,
				   INT16      yMask,
				   INT16      xDst,
				   INT16      yDst,
				   CARD16     width,
				   CARD16     height)
{
    CARD32	src, srca;
    CARD16	src16;
    CARD16	*dstLine, *dst;
    CARD32	d;
    CARD32	*maskLine, *mask, ma;
    FbStride	dstStride, maskStride;
    CARD16	w;
    CARD32	m, n, o;

    fbComposeGetSolid(pSrc, src, pDst->format);

    srca = src >> 24;
    if (src == 0)
	return;

    src16 = cvt8888to0565(src);

    fbComposeGetStart (pDst, xDst, yDst, CARD16, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, CARD32, maskStride, maskLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    ma = *mask++;
	    if (ma == 0xffffffff)
	    {
		if (srca == 0xff)
		{
		    *dst = src16;
		}
		else
		{
		    d = *dst;
		    d = fbOver24 (src, cvt0565to8888(d));
		    *dst = cvt8888to0565(d);
		}
	    }
	    else if (ma)
	    {
		d = *dst;
		d = cvt0565to8888(d);
		FbInOverC (src, srca, ma, d, 0, m);
		FbInOverC (src, srca, ma, d, 8, n);
		FbInOverC (src, srca, ma, d, 16, o);
		d = m|n|o;
		*dst = cvt8888to0565(d);
	    }
	    dst++;
	}
    }
}

void
fbCompositeSrc_8888x8888 (CARD8      op,
			 PicturePtr pSrc,
			 PicturePtr pMask,
			 PicturePtr pDst,
			 INT16      xSrc,
			 INT16      ySrc,
			 INT16      xMask,
			 INT16      yMask,
			 INT16      xDst,
			 INT16      yDst,
			 CARD16     width,
			 CARD16     height)
{
    CARD32	*dstLine, *dst, dstMask;
    CARD32	*srcLine, *src, s;
    FbStride	dstStride, srcStride;
    CARD8	a;
    CARD16	w;

    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);

    dstMask = FbFullMask (pDst->pDrawable->depth);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    a = s >> 24;
	    if (a == 0xff)
		*dst = s & dstMask;
	    else if (a)
		*dst = fbOver (s, *dst) & dstMask;
	    dst++;
	}
    }
}

void
fbCompositeSrc_8888x0888 (CARD8      op,
			 PicturePtr pSrc,
			 PicturePtr pMask,
			 PicturePtr pDst,
			 INT16      xSrc,
			 INT16      ySrc,
			 INT16      xMask,
			 INT16      yMask,
			 INT16      xDst,
			 INT16      yDst,
			 CARD16     width,
			 CARD16     height)
{
    CARD8	*dstLine, *dst;
    CARD32	d;
    CARD32	*srcLine, *src, s;
    CARD8	a;
    FbStride	dstStride, srcStride;
    CARD16	w;

    fbComposeGetStart (pDst, xDst, yDst, CARD8, dstStride, dstLine, 3);
    fbComposeGetStart (pSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    a = s >> 24;
	    if (a)
	    {
		if (a == 0xff)
		    d = s;
		else
		    d = fbOver24 (s, Fetch24(dst));
		Store24(dst,d);
	    }
	    dst += 3;
	}
    }
}

void
fbCompositeSrc_8888x0565 (CARD8      op,
			 PicturePtr pSrc,
			 PicturePtr pMask,
			 PicturePtr pDst,
			 INT16      xSrc,
			 INT16      ySrc,
			 INT16      xMask,
			 INT16      yMask,
			 INT16      xDst,
			 INT16      yDst,
			 CARD16     width,
			 CARD16     height)
{
    CARD16	*dstLine, *dst;
    CARD32	d;
    CARD32	*srcLine, *src, s;
    CARD8	a;
    FbStride	dstStride, srcStride;
    CARD16	w;

    fbComposeGetStart (pSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, CARD16, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    a = s >> 24;
	    if (a)
	    {
		if (a == 0xff)
		    d = s;
		else
		{
		    d = *dst;
		    d = fbOver24 (s, cvt0565to8888(d));
		}
		*dst = cvt8888to0565(d);
	    }
	    dst++;
	}
    }
}

void
fbCompositeSrc_0565x0565 (CARD8      op,
			  PicturePtr pSrc,
			  PicturePtr pMask,
			  PicturePtr pDst,
			  INT16      xSrc,
			  INT16      ySrc,
			  INT16      xMask,
			  INT16      yMask,
			  INT16      xDst,
			  INT16      yDst,
			  CARD16     width,
			  CARD16     height)
{
    CARD16	*dstLine, *dst;
    CARD16	*srcLine, *src;
    FbStride	dstStride, srcStride;
    CARD16	w;

    fbComposeGetStart (pSrc, xSrc, ySrc, CARD16, srcStride, srcLine, 1);

    fbComposeGetStart (pDst, xDst, yDst, CARD16, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	    *dst++ = *src++;
    }
}

void
fbCompositeSrcAdd_8000x8000 (CARD8	op,
			     PicturePtr pSrc,
			     PicturePtr pMask,
			     PicturePtr pDst,
			     INT16      xSrc,
			     INT16      ySrc,
			     INT16      xMask,
			     INT16      yMask,
			     INT16      xDst,
			     INT16      yDst,
			     CARD16     width,
			     CARD16     height)
{
    CARD8	*dstLine, *dst;
    CARD8	*srcLine, *src;
    FbStride	dstStride, srcStride;
    CARD16	w;
    CARD8	s, d;
    CARD16	t;

    fbComposeGetStart (pSrc, xSrc, ySrc, CARD8, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, CARD8, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    if (s)
	    {
		if (s != 0xff)
		{
		    d = *dst;
		    t = d + s;
		    s = t | (0 - (t >> 8));
		}
		*dst = s;
	    }
	    dst++;
	}
    }
}

void
fbCompositeSrcAdd_8888x8888 (CARD8	op,
			     PicturePtr pSrc,
			     PicturePtr pMask,
			     PicturePtr pDst,
			     INT16      xSrc,
			     INT16      ySrc,
			     INT16      xMask,
			     INT16      yMask,
			     INT16      xDst,
			     INT16      yDst,
			     CARD16     width,
			     CARD16     height)
{
    CARD32	*dstLine, *dst;
    CARD32	*srcLine, *src;
    FbStride	dstStride, srcStride;
    CARD16	w;
    CARD32	s, d;
    CARD16	t;
    CARD32	m,n,o,p;

    fbComposeGetStart (pSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    if (s)
	    {
		if (s != 0xffffffff)
		{
		    d = *dst;
		    if (d)
		    {
			m = FbAdd(s,d,0,t);
			n = FbAdd(s,d,8,t);
			o = FbAdd(s,d,16,t);
			p = FbAdd(s,d,24,t);
			s = m|n|o|p;
		    }
		}
		*dst = s;
	    }
	    dst++;
	}
    }
}

void
fbCompositeSrcAdd_1000x1000 (CARD8	op,
			     PicturePtr pSrc,
			     PicturePtr pMask,
			     PicturePtr pDst,
			     INT16      xSrc,
			     INT16      ySrc,
			     INT16      xMask,
			     INT16      yMask,
			     INT16      xDst,
			     INT16      yDst,
			     CARD16     width,
			     CARD16     height)
{
    FbBits	*dstBits, *srcBits;
    FbStride	dstStride, srcStride;
    int		dstBpp, srcBpp;
    int		dstXoff, dstYoff;
    int		srcXoff, srcYoff;

    fbGetDrawable(pSrc->pDrawable, srcBits, srcStride, srcBpp, srcXoff, srcYoff);

    fbGetDrawable(pDst->pDrawable, dstBits, dstStride, dstBpp, dstXoff, dstYoff);

    fbBlt (srcBits + srcStride * (ySrc + srcYoff),
	   srcStride,
	   xSrc + srcXoff,

	   dstBits + dstStride * (yDst + dstYoff),
	   dstStride,
	   xDst + dstXoff,

	   width,
	   height,

	   GXor,
	   FB_ALLONES,
	   srcBpp,

	   FALSE,
	   FALSE);
}

void
fbCompositeSolidMask_nx1xn (CARD8      op,
			    PicturePtr pSrc,
			    PicturePtr pMask,
			    PicturePtr pDst,
			    INT16      xSrc,
			    INT16      ySrc,
			    INT16      xMask,
			    INT16      yMask,
			    INT16      xDst,
			    INT16      yDst,
			    CARD16     width,
			    CARD16     height)
{
    FbBits	*dstBits;
    FbStip	*maskBits;
    FbStride	dstStride, maskStride;
    int		dstBpp, maskBpp;
    int		dstXoff, dstYoff;
    int		maskXoff, maskYoff;
    FbBits	src;

    fbComposeGetSolid(pSrc, src, pDst->format);

    if ((src & 0xff000000) != 0xff000000)
    {
	fbCompositeGeneral  (op, pSrc, pMask, pDst,
			     xSrc, ySrc, xMask, yMask, xDst, yDst,
			     width, height);
	return;
    }
    fbGetStipDrawable (pMask->pDrawable, maskBits, maskStride, maskBpp, maskXoff, maskYoff);
    fbGetDrawable (pDst->pDrawable, dstBits, dstStride, dstBpp, dstXoff, dstYoff);

    switch (dstBpp) {
    case 32:
	break;
    case 24:
	break;
    case 16:
	src = cvt8888to0565(src);
	break;
    }

    src = fbReplicatePixel (src, dstBpp);

    fbBltOne (maskBits + maskStride * (yMask + maskYoff),
	      maskStride,
	      xMask + maskXoff,

	      dstBits + dstStride * (yDst + dstYoff),
	      dstStride,
	      (xDst + dstXoff) * dstBpp,
	      dstBpp,

	      width * dstBpp,
	      height,

	      0x0,
	      src,
	      FB_ALLONES,
	      0x0);
}

# define mod(a,b)	((b) == 1 ? 0 : (a) >= 0 ? (a) % (b) : (b) - (-a) % (b))

void
fbComposite (CARD8      op,
	     PicturePtr pSrc,
	     PicturePtr pMask,
	     PicturePtr pDst,
	     INT16      xSrc,
	     INT16      ySrc,
	     INT16      xMask,
	     INT16      yMask,
	     INT16      xDst,
	     INT16      yDst,
	     CARD16     width,
	     CARD16     height)
{
    RegionRec	    region;
    int		    n;
    BoxPtr	    pbox;
    CompositeFunc   func = NULL;
    Bool	    srcRepeat = pSrc->pDrawable && pSrc->repeat == RepeatNormal;
    Bool	    maskRepeat = FALSE;
    Bool	    srcAlphaMap = pSrc->alphaMap != 0;
    Bool	    maskAlphaMap = FALSE;
    Bool	    dstAlphaMap = pDst->alphaMap != 0;
    int		    x_msk, y_msk, x_src, y_src, x_dst, y_dst;
    int		    w, h, w_this, h_this;

    xDst += pDst->pDrawable->x;
    yDst += pDst->pDrawable->y;
    if (pSrc->pDrawable) {
        xSrc += pSrc->pDrawable->x;
        ySrc += pSrc->pDrawable->y;
    }
    if (pMask && pMask->pDrawable)
    {
	xMask += pMask->pDrawable->x;
	yMask += pMask->pDrawable->y;
	maskRepeat = pMask->repeat == RepeatNormal;
	maskAlphaMap = pMask->alphaMap != 0;
    }

    if (pSrc->pDrawable && (!pMask || pMask->pDrawable)
        && !pSrc->transform && !(pMask && pMask->transform)
        && !maskAlphaMap && !srcAlphaMap && !dstAlphaMap
        && (pSrc->filter != PictFilterConvolution)
        && (!pMask || pMask->filter != PictFilterConvolution))
    switch (op) {
    case PictOpSrc:
	break;
    case PictOpOver:
	if (pMask)
	{
	    if (srcRepeat &&
		pSrc->pDrawable->width == 1 &&
		pSrc->pDrawable->height == 1)
	    {
		srcRepeat = FALSE;
		if (PICT_FORMAT_COLOR(pSrc->format)) {
		    switch (pMask->format) {
		    case PICT_a8:
			switch (pDst->format) {
			case PICT_r5g6b5:
			case PICT_b5g6r5:
			    func = fbCompositeSolidMask_nx8x0565;
			    break;
			case PICT_r8g8b8:
			case PICT_b8g8r8:
			    func = fbCompositeSolidMask_nx8x0888;
			    break;
			case PICT_a8r8g8b8:
			case PICT_x8r8g8b8:
			case PICT_a8b8g8r8:
			case PICT_x8b8g8r8:
			    func = fbCompositeSolidMask_nx8x8888;
			    break;
			}
			break;
		    case PICT_a8r8g8b8:
			if (pMask->componentAlpha) {
			    switch (pDst->format) {
			    case PICT_a8r8g8b8:
			    case PICT_x8r8g8b8:
				func = fbCompositeSolidMask_nx8888x8888C;
				break;
			    case PICT_r5g6b5:
				func = fbCompositeSolidMask_nx8888x0565C;
				break;
			    }
			}
			break;
		    case PICT_a8b8g8r8:
			if (pMask->componentAlpha) {
			    switch (pDst->format) {
			    case PICT_a8b8g8r8:
			    case PICT_x8b8g8r8:
				func = fbCompositeSolidMask_nx8888x8888C;
				break;
			    case PICT_b5g6r5:
				func = fbCompositeSolidMask_nx8888x0565C;
				break;
			    }
			}
			break;
		    case PICT_a1:
			switch (pDst->format) {
			case PICT_r5g6b5:
			case PICT_b5g6r5:
			case PICT_r8g8b8:
			case PICT_b8g8r8:
			case PICT_a8r8g8b8:
			case PICT_x8r8g8b8:
			case PICT_a8b8g8r8:
			case PICT_x8b8g8r8:
			    func = fbCompositeSolidMask_nx1xn;
			    break;
			}
			break;
		    }
		}
	    }
	    else /* has mask and non-repeating source */
	    {
		if (pSrc->pDrawable == pMask->pDrawable &&
		    xSrc == xMask && ySrc == yMask &&
		    !pMask->componentAlpha)
		{
		    /* source == mask: non-premultiplied data */
		    switch (pSrc->format) {
		    case PICT_x8b8g8r8:
			switch (pMask->format) {
			case PICT_a8r8g8b8:
			case PICT_a8b8g8r8:
			    switch (pDst->format) {
			    case PICT_a8r8g8b8:
			    case PICT_x8r8g8b8:
				break;
			    case PICT_r5g6b5:
				break;
			    }
			    break;
			}
			break;
		    case PICT_x8r8g8b8:
			switch (pMask->format) {
			case PICT_a8r8g8b8:
			case PICT_a8b8g8r8:
			    switch (pDst->format) {
			    case PICT_a8b8g8r8:
			    case PICT_x8b8g8r8:
				break;
			    case PICT_r5g6b5:
				break;
			    }
			    break;
			}
			break;
		    }
		    break;
		}
		else
		{
		    /* non-repeating source, repeating mask => translucent window */
		    if (maskRepeat &&
			pMask->pDrawable->width == 1 &&
			pMask->pDrawable->height == 1)
		    {
			if (pSrc->format == PICT_x8r8g8b8 &&
			    pDst->format == PICT_x8r8g8b8 &&
			    pMask->format == PICT_a8)
			{
			}
		    }
		}
	    }
	}
	else /* no mask */
	{
	    if (srcRepeat &&
		pSrc->pDrawable->width == 1 &&
		pSrc->pDrawable->height == 1)
	    {
		/* no mask and repeating source */
		switch (pSrc->format) {
		case PICT_a8r8g8b8:
		    switch (pDst->format) {
		    case PICT_a8r8g8b8:
		    case PICT_x8r8g8b8:
			break;
		    case PICT_r5g6b5:
			break;
		    }
		    break;
		}
	    }
	    else
	    {
		switch (pSrc->format) {
		case PICT_a8r8g8b8:
		    switch (pDst->format) {
		    case PICT_a8r8g8b8:
		    case PICT_x8r8g8b8:
			func = fbCompositeSrc_8888x8888;
			break;
		    case PICT_r8g8b8:
			func = fbCompositeSrc_8888x0888;
			break;
		    case PICT_r5g6b5:
			func = fbCompositeSrc_8888x0565;
			break;
		    }
		    break;
		case PICT_x8r8g8b8:
		    switch (pDst->format) {
		    case PICT_a8r8g8b8:
		    case PICT_x8r8g8b8:
			break;
		    }
		case PICT_x8b8g8r8:
		    switch (pDst->format) {
		    case PICT_a8b8g8r8:
		    case PICT_x8b8g8r8:
			break;
		    }
		    break;
		case PICT_a8b8g8r8:
		    switch (pDst->format) {
		    case PICT_a8b8g8r8:
		    case PICT_x8b8g8r8:
			func = fbCompositeSrc_8888x8888;
			break;
		    case PICT_b8g8r8:
			func = fbCompositeSrc_8888x0888;
			break;
		    case PICT_b5g6r5:
			func = fbCompositeSrc_8888x0565;
			break;
		    }
		    break;
		case PICT_r5g6b5:
		    switch (pDst->format) {
		    case PICT_r5g6b5:
			func = fbCompositeSrc_0565x0565;
			break;
		    }
		    break;
		case PICT_b5g6r5:
		    switch (pDst->format) {
		    case PICT_b5g6r5:
			func = fbCompositeSrc_0565x0565;
			break;
		    }
		    break;
		}
	    }
	}
	break;
    case PictOpAdd:
	if (pMask == 0)
	{
	    switch (pSrc->format) {
	    case PICT_a8r8g8b8:
		switch (pDst->format) {
		case PICT_a8r8g8b8:
		    func = fbCompositeSrcAdd_8888x8888;
		    break;
		}
		break;
	    case PICT_a8b8g8r8:
		switch (pDst->format) {
		case PICT_a8b8g8r8:
		    func = fbCompositeSrcAdd_8888x8888;
		    break;
		}
		break;
	    case PICT_a8:
		switch (pDst->format) {
		case PICT_a8:
		    func = fbCompositeSrcAdd_8000x8000;
		    break;
		}
		break;
	    case PICT_a1:
		switch (pDst->format) {
		case PICT_a1:
		    func = fbCompositeSrcAdd_1000x1000;
		    break;
		}
		break;
	    }
	}
	break;
    }

    if (!func) {
         /* no fast path, use the general code */
         fbCompositeGeneral(op, pSrc, pMask, pDst, xSrc, ySrc, xMask, yMask, xDst, yDst, width, height);
         return;
    }

    if (!miComputeCompositeRegion (&region,
 				   pSrc,
 				   pMask,
 				   pDst,
 				   xSrc,
 				   ySrc,
 				   xMask,
 				   yMask,
 				   xDst,
 				   yDst,
 				   width,
                                   height))
        return;

    n = RegionNumRects (&region);
    pbox = RegionRects (&region);
    while (n--)
    {
	h = pbox->y2 - pbox->y1;
	y_src = pbox->y1 - yDst + ySrc;
	y_msk = pbox->y1 - yDst + yMask;
	y_dst = pbox->y1;
	while (h)
	{
	    h_this = h;
	    w = pbox->x2 - pbox->x1;
	    x_src = pbox->x1 - xDst + xSrc;
	    x_msk = pbox->x1 - xDst + xMask;
	    x_dst = pbox->x1;
	    if (maskRepeat)
	    {
		y_msk = mod (y_msk - pMask->pDrawable->y, pMask->pDrawable->height);
		if (h_this > pMask->pDrawable->height - y_msk)
		    h_this = pMask->pDrawable->height - y_msk;
		y_msk += pMask->pDrawable->y;
	    }
	    if (srcRepeat)
	    {
		y_src = mod (y_src - pSrc->pDrawable->y, pSrc->pDrawable->height);
		if (h_this > pSrc->pDrawable->height - y_src)
		    h_this = pSrc->pDrawable->height - y_src;
		y_src += pSrc->pDrawable->y;
	    }
	    while (w)
	    {
		w_this = w;
		if (maskRepeat)
		{
		    x_msk = mod (x_msk - pMask->pDrawable->x, pMask->pDrawable->width);
		    if (w_this > pMask->pDrawable->width - x_msk)
			w_this = pMask->pDrawable->width - x_msk;
		    x_msk += pMask->pDrawable->x;
		}
		if (srcRepeat)
		{
		    x_src = mod (x_src - pSrc->pDrawable->x, pSrc->pDrawable->width);
		    if (w_this > pSrc->pDrawable->width - x_src)
			w_this = pSrc->pDrawable->width - x_src;
		    x_src += pSrc->pDrawable->x;
		}
		(*func) (op, pSrc, pMask, pDst,
			 x_src, y_src, x_msk, y_msk, x_dst, y_dst,
			 w_this, h_this);
		w -= w_this;
		x_src += w_this;
		x_msk += w_this;
		x_dst += w_this;
	    }
	    h -= h_this;
	    y_src += h_this;
	    y_msk += h_this;
	    y_dst += h_this;
	}
	pbox++;
    }
    RegionUninit(&region);
}

static pixman_image_t *
create_solid_fill_image(PicturePtr pict)
{
    PictSolidFill *solid = &pict->pSourcePict->solidFill;
    pixman_color_t color;
    CARD32 a, r, g, b;

    a = (solid->color & 0xff000000) >> 24;
    r = (solid->color & 0x00ff0000) >> 16;
    g = (solid->color & 0x0000ff00) >> 8;
    b = (solid->color & 0x000000ff) >> 0;

    color.alpha = (a << 8) | a;
    color.red = (r << 8) | r;
    color.green = (g << 8) | g;
    color.blue = (b << 8) | b;

    return pixman_image_create_solid_fill(&color);
}

static pixman_image_t *
create_linear_gradient_image(PictGradient * gradient)
{
    PictLinearGradient *linear = (PictLinearGradient *) gradient;
    pixman_point_fixed_t p1;
    pixman_point_fixed_t p2;

    p1.x = linear->p1.x;
    p1.y = linear->p1.y;
    p2.x = linear->p2.x;
    p2.y = linear->p2.y;

    return pixman_image_create_linear_gradient(&p1, &p2,
                                               (pixman_gradient_stop_t *)
                                               gradient->stops,
                                               gradient->nstops);
}

static pixman_image_t *
create_radial_gradient_image(PictGradient * gradient)
{
    PictRadialGradient *radial = (PictRadialGradient *) gradient;
    pixman_point_fixed_t c1;
    pixman_point_fixed_t c2;

    c1.x = radial->c1.x;
    c1.y = radial->c1.y;
    c2.x = radial->c2.x;
    c2.y = radial->c2.y;

    return pixman_image_create_radial_gradient(&c1, &c2, radial->c1.radius,
                                               radial->c2.radius,
                                               (pixman_gradient_stop_t *)
                                               gradient->stops,
                                               gradient->nstops);
}

static pixman_image_t *
create_conical_gradient_image(PictGradient * gradient)
{
    PictConicalGradient *conical = (PictConicalGradient *) gradient;
    pixman_point_fixed_t center;

    center.x = conical->center.x;
    center.y = conical->center.y;

    return pixman_image_create_conical_gradient(&center, conical->angle,
                                                (pixman_gradient_stop_t *)
                                                gradient->stops,
                                                gradient->nstops);
}

static pixman_image_t *
create_bits_picture(PicturePtr pict, Bool has_clip, int *xoff, int *yoff)
{
    PixmapPtr pixmap;
    FbBits *bits;
    FbStride stride;
    int bpp;
    pixman_image_t *image;

    fbGetDrawablePixmap(pict->pDrawable, pixmap, *xoff, *yoff);
    fbGetPixmapBitsData(pixmap, bits, stride, bpp);

    image = pixman_image_create_bits((pixman_format_code_t) pict->format,
                                     pixmap->drawable.width,
                                     pixmap->drawable.height, (uint32_t *) bits,
                                     stride * sizeof(FbStride));

    if (!image)
        return NULL;

#ifdef FB_ACCESS_WRAPPER
    pixman_image_set_accessors(image,
                               (pixman_read_memory_func_t) wfbReadMemory,
                               (pixman_write_memory_func_t) wfbWriteMemory);
#endif

    /* pCompositeClip is undefined for source pictures, so
     * only set the clip region for pictures with drawables
     */
    if (has_clip) {
        if (pict->clientClip)
            pixman_image_set_has_client_clip(image, TRUE);

        if (*xoff || *yoff)
            pixman_region_translate(pict->pCompositeClip, *xoff, *yoff);

        pixman_image_set_clip_region(image, pict->pCompositeClip);

        if (*xoff || *yoff)
            pixman_region_translate(pict->pCompositeClip, -*xoff, -*yoff);
    }

    /* Indexed table */
    if (pict->pFormat->index.devPrivate)
        pixman_image_set_indexed(image, pict->pFormat->index.devPrivate);

    /* Add in drawable origin to position within the image */
    *xoff += pict->pDrawable->x;
    *yoff += pict->pDrawable->y;

    return image;
}

static pixman_image_t *image_from_pict_internal(PicturePtr pict, Bool has_clip,
                                                int *xoff, int *yoff,
                                                Bool is_alpha_map);

static void image_destroy(pixman_image_t *image, void *data)
{
    fbFinishAccess((DrawablePtr)data);
}

static void
set_image_properties(pixman_image_t * image, PicturePtr pict, Bool has_clip,
                     int *xoff, int *yoff, Bool is_alpha_map)
{
    pixman_repeat_t repeat;
    pixman_filter_t filter;

    if (pict->transform) {
        /* For source images, adjust the transform to account
         * for the drawable offset within the pixman image,
         * then set the offset to 0 as it will be used
         * to compute positions within the transformed image.
         */
        if (!has_clip) {
            struct pixman_transform adjusted;

            adjusted = *pict->transform;
            pixman_transform_translate(&adjusted,
                                       NULL,
                                       pixman_int_to_fixed(*xoff),
                                       pixman_int_to_fixed(*yoff));
            pixman_image_set_transform(image, &adjusted);
            *xoff = 0;
            *yoff = 0;
        }
        else
            pixman_image_set_transform(image, pict->transform);
    }

    switch (pict->repeatType) {
    default:
    case RepeatNone:
        repeat = PIXMAN_REPEAT_NONE;
        break;

    case RepeatPad:
        repeat = PIXMAN_REPEAT_PAD;
        break;

    case RepeatNormal:
        repeat = PIXMAN_REPEAT_NORMAL;
        break;

    case RepeatReflect:
        repeat = PIXMAN_REPEAT_REFLECT;
        break;
    }

    pixman_image_set_repeat(image, repeat);

    /* Fetch alpha map unless 'pict' is being used
     * as the alpha map for this operation
     */
    if (pict->alphaMap && !is_alpha_map) {
        int alpha_xoff, alpha_yoff;
        pixman_image_t *alpha_map =
            image_from_pict_internal(pict->alphaMap, FALSE, &alpha_xoff,
                                     &alpha_yoff, TRUE);

        pixman_image_set_alpha_map(image, alpha_map, pict->alphaOrigin.x,
                                   pict->alphaOrigin.y);

        free_pixman_pict(pict->alphaMap, alpha_map);
    }

    pixman_image_set_component_alpha(image, pict->componentAlpha);

    switch (pict->filter) {
    default:
    case PictFilterNearest:
    case PictFilterFast:
        filter = PIXMAN_FILTER_NEAREST;
        break;

    case PictFilterBilinear:
    case PictFilterGood:
        filter = PIXMAN_FILTER_BILINEAR;
        break;

    case PictFilterConvolution:
        filter = PIXMAN_FILTER_CONVOLUTION;
        break;
    }

    if (pict->pDrawable)
        pixman_image_set_destroy_function(image, &image_destroy,
                                          pict->pDrawable);

    pixman_image_set_filter(image, filter,
                            (pixman_fixed_t *) pict->filter_params,
                            pict->filter_nparams);
    pixman_image_set_source_clipping(image, TRUE);
}

static pixman_image_t *
image_from_pict_internal(PicturePtr pict, Bool has_clip, int *xoff, int *yoff,
                         Bool is_alpha_map)
{
    pixman_image_t *image = NULL;

    if (!pict)
        return NULL;

    if (pict->pDrawable) {
        image = create_bits_picture(pict, has_clip, xoff, yoff);
    }
    else if (pict->pSourcePict) {
        SourcePict *sp = pict->pSourcePict;

        if (sp->type == SourcePictTypeSolidFill) {
            image = create_solid_fill_image(pict);
        }
        else {
            PictGradient *gradient = &pict->pSourcePict->gradient;

            if (sp->type == SourcePictTypeLinear)
                image = create_linear_gradient_image(gradient);
            else if (sp->type == SourcePictTypeRadial)
                image = create_radial_gradient_image(gradient);
            else if (sp->type == SourcePictTypeConical)
                image = create_conical_gradient_image(gradient);
        }
        *xoff = *yoff = 0;
    }

    if (image)
        set_image_properties(image, pict, has_clip, xoff, yoff, is_alpha_map);

    return image;
}

pixman_image_t *
image_from_pict(PicturePtr pict, Bool has_clip, int *xoff, int *yoff)
{
    return image_from_pict_internal(pict, has_clip, xoff, yoff, FALSE);
}

void
free_pixman_pict(PicturePtr pict, pixman_image_t * image)
{
    if (image)
        pixman_image_unref(image);
}

#endif /* RENDER */

Bool
fbPictureInit (ScreenPtr pScreen, PictFormatPtr formats, int nformats)
{

#ifdef RENDER

    PictureScreenPtr    ps;

    if (!miPictureInit (pScreen, formats, nformats))
	return FALSE;
    ps = GetPictureScreen(pScreen);
    ps->Composite = fbComposite;
    ps->Glyphs = miGlyphs;
    ps->CompositeRects = miCompositeRects;
    ps->RasterizeTrapezoid = fbRasterizeTrapezoid;
    ps->AddTraps = fbAddTraps;
    ps->AddTriangles = fbAddTriangles;

#endif /* RENDER */

    return TRUE;
}
