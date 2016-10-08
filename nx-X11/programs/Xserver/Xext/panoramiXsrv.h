
#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#ifndef _PANORAMIXSRV_H_
#define _PANORAMIXSRV_H_

#include "panoramiX.h"

#ifndef _XTYPEDEF_POINTER
/* Don't let Xdefs.h define 'pointer' */
#define _XTYPEDEF_POINTER       1
#endif /* _XTYPEDEF_POINTER */

#include "dixstruct.h" /* for ClientPtr */

extern int PanoramiXNumScreens;
extern PanoramiXData *panoramiXdataPtr;
extern int PanoramiXPixWidth;
extern int PanoramiXPixHeight;
extern RegionRec PanoramiXScreenRegion;
extern XID *PanoramiXVisualTable;

extern void PanoramiXConsolidate(void);
extern Bool PanoramiXCreateConnectionBlock(void);
extern PanoramiXRes * PanoramiXFindIDByScrnum(RESTYPE, XID, int);
extern PanoramiXRes * PanoramiXFindIDOnAnyScreen(RESTYPE, XID);
extern WindowPtr PanoramiXChangeWindow(int, WindowPtr);
extern Bool XineramaRegisterConnectionBlockCallback(void (*func)(void));
extern int XineramaDeleteResource(void *, XID);

extern void XineramaReinitData(ScreenPtr);

extern RegionRec XineramaScreenRegions[MAXSCREENS];

extern unsigned long XRC_DRAWABLE;
extern unsigned long XRT_WINDOW;
extern unsigned long XRT_PIXMAP;
extern unsigned long XRT_GC;
extern unsigned long XRT_COLORMAP;
extern _X_EXPORT RESTYPE XRT_PICTURE;

extern void XineramaGetImageData(
    DrawablePtr *pDrawables,
    int left,
    int top,
    int width, 
    int height,
    unsigned int format,
    unsigned long planemask,
    char *data,
    int pitch,
    Bool isRoot
);


static inline void
panoramix_setup_ids(PanoramiXRes * resource, ClientPtr client, XID base_id)
{
    int j;

    resource->info[0].id = base_id;
    FOR_NSCREENS_FORWARD_SKIP(j) {
        resource->info[j].id = FakeClientID(client->index);
    }
}

#endif /* _PANORAMIXSRV_H_ */
