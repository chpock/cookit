/*
 * shape.c --
 *
 *	This module implements raw access to the X Shaped-Window extension.
 *
 * Copyright (c) 1997 by Donal K. Fellows
 *
 * The author hereby grants permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 *
 * IN NO EVENT SHALL THE AUTHOR OR DISTRIBUTORS BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
 * DERIVATIVES THEREOF, EVEN IF THE AUTHOR HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHOR AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
 * IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHOR AND DISTRIBUTORS HAVE
 * NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 * MODIFICATIONS.
 *
 * $Id: shape.c,v 1.2 2002/05/03 23:02:11 hobbs Exp $
 */ 

#include <tk.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
/*#include "panic.h"*/

#define min(x,y) ((x)<(y)?(x):(y))
#define max(x,y) ((x)<(y)?(y):(x))

typedef int (*shapeCommandHandler)
     _ANSI_ARGS_((Tk_Window tkwin, Tcl_Interp *interp, int opnum,
		  int objc, Tcl_Obj *CONST objv[]));

static int
shapeBoundClipOps _ANSI_ARGS_((Tk_Window tkwin, Tcl_Interp *interp, int opnum,
			       int objc, Tcl_Obj *CONST objv[]));
static int
shapeOffsetOp     _ANSI_ARGS_((Tk_Window tkwin, Tcl_Interp *interp, int opnum,
			       int objc, Tcl_Obj *CONST objv[]));
static int
shapeSetUpdateOps _ANSI_ARGS_((Tk_Window tkwin, Tcl_Interp *interp, int opnum,
			       int objc, Tcl_Obj *CONST objv[]));

enum {
  boundsCmd, getCmd, offsetCmd, setCmd, updateCmd, versionCmd
};
static char *subcommands[] = {
  "bounds", "get", "offset", "set", "update", "version", NULL
};
static shapeCommandHandler shapeCommandHandlers[] = {
  shapeBoundClipOps, shapeBoundClipOps,
  shapeOffsetOp, shapeSetUpdateOps, shapeSetUpdateOps,
  NULL
};

#ifdef DKF_SHAPE_DEBUGGING
static int
applyOperationToToplevelParent = 1;
#else
#define applyOperationToToplevelParent 1
#endif

static Window
getWindow(apptkwin, interp, pathName, isToplevel)
     Tk_Window apptkwin;
     Tcl_Interp *interp;
     Tcl_Obj *pathName;
     int *isToplevel;
{
  Tk_Window tkwin =
    Tk_NameToWindow(interp, Tcl_GetStringFromObj(pathName, NULL), apptkwin);

  if (!tkwin) {
    return None;
  }
  if (Tk_Display(tkwin) != Tk_Display(apptkwin)) {
    Tcl_AppendResult(interp, "can only apply shape operations to windows"
		     " on the same display as the main window of the"
		     " application", NULL);
    return None;
  }
  if (Tk_WindowId(tkwin) == None) {
    Tk_MakeWindowExist(tkwin);
    if (Tk_WindowId(tkwin) == None) {
      panic("bizarre failure to create window");
    }
  }
  *isToplevel = Tk_IsTopLevel(tkwin);
  return Tk_WindowId(tkwin);
}

static Window
getXParent(dpy, w)
     Display *dpy;
     Window w;
{
  Window root, parent, *kids;
  unsigned int nkids;

  if (XQueryTree(dpy, w, &root, &parent, &kids, &nkids)) {
    if (kids) {
      XFree(kids);
    }
    if (parent!=root) {
      return parent;
    }
  }
  return None;
}

static int
shapeBoundClipOps(tkwin, interp, opnum, objc, objv)
     Tk_Window tkwin;
     Tcl_Interp *interp;
     int opnum, objc;
     Tcl_Obj *CONST objv[];
{
  static char *options[] = {
    "-bounding", "-clip", NULL
  };
  int idx = 0,toplevel;
  Window w;

  if (objc<3||objc>4) {
    Tcl_WrongNumArgs(interp, 2, objv, "pathName ?-bounding/-clip?");
    return TCL_ERROR;
  } else if ((w=getWindow(tkwin, interp, objv[2], &toplevel)) == None) {
    return TCL_ERROR;
  } else if (objc==4 && Tcl_GetIndexFromObj(interp, objv[3],
	  (CONST84 char **) options, "option", 0, &idx) != TCL_OK) {
    return TCL_ERROR;
  } else {
    switch (opnum) {
    case boundsCmd: {
      int bShaped, xbs, ybs, cShaped, xcs, ycs;
      unsigned int wbs, hbs, wcs, hcs;
      Status s = XShapeQueryExtents(Tk_Display(tkwin), w,
				    &bShaped, &xbs, &ybs, &wbs, &hbs,
				    &cShaped, &xcs, &ycs, &wcs, &hcs);
      Tcl_Obj *result[4];

      if (!s) {
	Tcl_AppendResult(interp, "whoops - extents query failed", NULL);
	return TCL_ERROR;
      }
      if (idx==0 && bShaped) {
	result[0] = Tcl_NewIntObj(xbs);
	result[1] = Tcl_NewIntObj(ybs);
	result[2] = Tcl_NewIntObj(xbs+wbs-1);
	result[3] = Tcl_NewIntObj(ybs+hbs-1);
	Tcl_SetObjResult(interp, Tcl_NewListObj(4, result));
      } else if (idx==1 && cShaped) {
	result[0] = Tcl_NewIntObj(xcs);
	result[1] = Tcl_NewIntObj(ycs);
	result[2] = Tcl_NewIntObj(xcs+wcs-1);
	result[3] = Tcl_NewIntObj(ycs+hcs-1);
	Tcl_SetObjResult(interp, Tcl_NewListObj(4, result));
      }
      return TCL_OK;
    }

    case getCmd: {
      XRectangle *rects = NULL;
      int count,order,i;
      Tcl_Obj *rect[4], **retvals;

      switch (idx) {
      case 0:
	rects = XShapeGetRectangles(Tk_Display(tkwin), w, /* Which window? */
				    ShapeBounding,	  /* Which shape? */
				    &count,		  /* How many rects? */
				    &order);		  /* ignored */
	break;
      case 1:
	rects = XShapeGetRectangles(Tk_Display(tkwin), w, ShapeClip,
				    &count, &order);
	break;
      }

      if (count) {
	retvals = (Tcl_Obj **)Tcl_Alloc(sizeof(Tcl_Obj *)*count);
	for (i=0 ; i<count ; i++) {
	  rect[0] = Tcl_NewIntObj(rects[i].x);
	  rect[1] = Tcl_NewIntObj(rects[i].y);
	  rect[2] = Tcl_NewIntObj(rects[i].x + rects[i].width  - 1);
	  rect[3] = Tcl_NewIntObj(rects[i].y + rects[i].height - 1);
	  retvals[i] = Tcl_NewListObj(4, rect);
	}
	Tcl_SetObjResult(interp, Tcl_NewListObj(count, retvals));
	Tcl_Free((char *)retvals);
      }
      if (rects) {
	XFree(rects);
      }
      return TCL_OK;
    }

    default:
      panic("unexpected operation number %d", opnum);
    }
  }
  return TCL_ERROR;
}

static int
shapeOffsetOp(tkwin, interp, opnum, objc, objv)
     Tk_Window tkwin;
     Tcl_Interp *interp;
     int opnum, objc;
     Tcl_Obj *CONST objv[];
{
  static char *options[] = {
    "-bounding", "-clip", "-both", NULL
  };
  int idx = 2,x,y,toplevel;
  Window w, parent = None;

  /* Argument parsing */
  if (objc<5||objc>6) {
    Tcl_WrongNumArgs(interp, 2, objv, "pathName ?-bounding/-clip/-both? x y");
    return TCL_ERROR;
  } else if ((w=getWindow(tkwin, interp, objv[2], &toplevel)) == None) {
    return TCL_ERROR;
  } else if (objc==6 && Tcl_GetIndexFromObj(interp, objv[3],
	  (CONST84 char **) options, "option", 0, &idx) != TCL_OK) {
    return TCL_ERROR;
  } else if (Tcl_GetIntFromObj(interp, objv[objc-2], &x) != TCL_OK ||
	     Tcl_GetIntFromObj(interp, objv[objc-1], &y) != TCL_OK) {
    return TCL_ERROR;
  } else if (toplevel && applyOperationToToplevelParent) {
    parent = getXParent(Tk_Display(tkwin), w);
  }

  /* Now we call into the X extension */
  if (idx==0 || idx==2) {
    XShapeOffsetShape(Tk_Display(tkwin), w,	/* Which window */
		      ShapeBounding,		/* Which shape */
		      x, y);			/* How are we moving */
    if (parent!=None) {
      XShapeOffsetShape(Tk_Display(tkwin), parent, ShapeBounding, x, y);
    }
  }
  if (idx==1||idx==2) {
    XShapeOffsetShape(Tk_Display(tkwin), w, ShapeClip, x, y);
    if (parent!=None) {
      XShapeOffsetShape(Tk_Display(tkwin), parent, ShapeClip, x, y);
    }
  }

  return TCL_OK;
}

static int
shapeSetUpdateOps(tkwin, interp, opnum, objc, objv)
     Tk_Window tkwin;
     Tcl_Interp *interp;
     int opnum, objc;
     Tcl_Obj *CONST objv[];
{
  enum optkind {
    shapekind, offsetargs, sourceargs
  };
  static char *options[] = {
    "-bounding", "-clip", "-both",
    "-offset",
    "bitmap", "rectangles", "reset", "text", "window",
    NULL
  };
  static enum optkind optk[] = {
    shapekind, shapekind, shapekind,
    offsetargs,
    sourceargs, sourceargs, sourceargs, sourceargs, sourceargs
  };

  static char *operations[] = {
    "set", "union", "intersect", "subtract", "invert",
    ":=", "+=", "*=", "-=", "=", "||", "&&", NULL
  };
  static int opmap[] = {
    ShapeSet, ShapeUnion, ShapeIntersect, ShapeSubtract, ShapeInvert,
    ShapeSet, ShapeUnion, ShapeIntersect, ShapeSubtract, ShapeSet,
    ShapeUnion, ShapeIntersect
  };

  int operation = ShapeSet;

  int idx, kind=2, x=0, y=0, toplevel,objidx=0;
  Window w, parent = None;
  Display *dpy = Tk_Display(tkwin);

  switch (opnum) {
  case setCmd:
    if (objc<3) {
      Tcl_WrongNumArgs(interp, 1, objv, "set pathName ?options?");
      return TCL_ERROR;
    } else {
      objidx = 3;
      break;
    }
  case updateCmd:
    if (objc<4) {
      Tcl_WrongNumArgs(interp, 1, objv, "update pathName operation ?options?");
      return TCL_ERROR;
    } else {
      int opidx;

      objidx = 4;
      if (Tcl_GetIndexFromObj(interp, objv[3], (CONST84 char **) operations,
	      "operation", 0, &opidx) != TCL_OK) {
	return TCL_ERROR;
      }
      operation = opmap[opidx];
      break;
    }
  default:
    panic("bad operation: %d", opnum);
  }

  if ((w=getWindow(tkwin, interp, objv[2], &toplevel)) == None) {
    return TCL_ERROR;
  } else if (toplevel && applyOperationToToplevelParent) {
    parent = getXParent(Tk_Display(tkwin), w);
  }

  for (; objidx<objc ; objidx++) {
    if (Tcl_GetIndexFromObj(interp, objv[objidx], (CONST84 char **) options,
			    "option", 0, &idx) != TCL_OK) {
      return TCL_ERROR;
    }
    switch (optk[idx]) {
    case shapekind:
      kind = idx;
      break;
    case offsetargs:
      if (objidx+2>=objc) {
	Tcl_AppendResult(interp, "-offset reqires two args; x and y", NULL);
	return TCL_ERROR;
      } else if (Tcl_GetIntFromObj(interp, objv[objidx+1], &x) != TCL_OK ||
		 Tcl_GetIntFromObj(interp, objv[objidx+2], &y) != TCL_OK) {
	return TCL_ERROR;
      }
      objidx += 2;
      break;
    case sourceargs:
      switch (idx) {
      case 4: /* bitmap */
	if (objidx+2!=objc) {
	  Tcl_AppendResult(interp, "bitmap requires one argument; a "
			   "bitmap name", NULL);
	  return TCL_ERROR;
	} else {
	  char *bmap_name = Tcl_GetStringFromObj(objv[objidx+1],NULL);
	  Pixmap bmap = Tk_GetBitmap(interp, tkwin, Tk_GetUid(bmap_name));

	  if (bmap==None) {
	    return TCL_ERROR;
	  }
	  if (kind==0 || kind==2) {
	    XShapeCombineMask(dpy, w, ShapeBounding, x, y, bmap, operation);
	    if (parent!=None) {
	      XShapeCombineMask(dpy, parent, ShapeBounding, x, y, bmap,
				operation);
	    }
	  }
	  if (kind==0 || kind==2) {
	    XShapeCombineMask(dpy, w, ShapeClip, x, y, bmap, operation);
	    if (parent!=None) {
	      XShapeCombineMask(dpy, parent, ShapeClip, x, y, bmap, operation);
	    }
	  }
	  Tk_FreeBitmap(dpy, bmap);
	  return TCL_OK;
	}
      case 5: /* rectangles */
	if (objidx+2!=objc) {
	  Tcl_AppendResult(interp, "rectangles requires one argument; a "
			   "list of rectangles", NULL);
	  return TCL_ERROR;
	} else {
	  Tcl_Obj **ovec;
	  XRectangle *rects;
	  int count,i;

	  if (Tcl_ListObjGetElements(interp, objv[objidx+1],
				     &count, &ovec) != TCL_OK) {
	    return TCL_ERROR;
	  } else if (count<1) {
	    Tcl_AppendResult(interp, "need at least one rectangle", NULL);
	    return TCL_ERROR;
	  }

	  rects = (XRectangle *)Tcl_Alloc(sizeof(XRectangle)*count);
	  for (i=0 ; i<count ; i++) {
	    int x1,y1,x2,y2;
	    Tcl_Obj **rvec;
	    int rlen;

	    if (Tcl_ListObjGetElements(interp, ovec[i],
				       &rlen, &rvec) != TCL_OK) {
	      Tcl_Free((char *)rects);
	      return TCL_ERROR;
	    } else if (rlen!=4) {
	      Tcl_AppendResult(interp, "rectangles are described by four "
			       "numbers; x1, y1, x2, and y2", NULL);
	      Tcl_Free((char *)rects);
	      return TCL_ERROR;
	    } else if (Tcl_GetIntFromObj(interp, rvec[0], &x1) != TCL_OK ||
		       Tcl_GetIntFromObj(interp, rvec[1], &y1) != TCL_OK ||
		       Tcl_GetIntFromObj(interp, rvec[2], &x2) != TCL_OK ||
		       Tcl_GetIntFromObj(interp, rvec[3], &y2) != TCL_OK) {
	      Tcl_Free((char *)rects);
	      return TCL_ERROR;
	    }
	    rects[i].x = min(x1,x2);
	    rects[i].y = min(y1,y2);
	    rects[i].width  = max(x1-x2, x2-x1) + 1;
	    rects[i].height = max(y1-y2, y2-y1) + 1;
	  }

	  if (kind==0 || kind==2) {
	    XShapeCombineRectangles(dpy, w, ShapeBounding, 0, 0,
				    rects, count, operation, Unsorted);
	    if (parent!=None) {
	      XShapeCombineRectangles(dpy, parent, ShapeBounding, 0, 0,
				      rects, count, operation, Unsorted);
	    }
	  }
	  if (kind==1 || kind==2) {
	    XShapeCombineRectangles(dpy, w, ShapeClip, 0, 0,
				    rects, count, operation, Unsorted);
	    if (parent!=None) {
	      XShapeCombineRectangles(dpy, parent, ShapeClip, 0, 0,
				      rects, count, operation, Unsorted);
	    }
	  }

	  Tcl_Free((char *)rects);
	  return TCL_OK;	  
	}

      case 8: /* window */
	if (objidx+2!=objc) {
	  Tcl_AppendResult(interp, "window requires one argument; a "
			   "window pathName", NULL);
	  return TCL_ERROR;
	} else {
	  Window srcwin;

	  if ((srcwin=getWindow(tkwin, interp, objv[objidx+1],
				&toplevel)) == None) {
	    return TCL_ERROR;
	  }

	  if (kind==0 || kind==2) {
	    XShapeCombineShape(dpy, w, ShapeBounding, x, y,
			       srcwin, ShapeBounding, operation);
	    if (parent!=None) {
	      XShapeCombineShape(dpy, parent, ShapeBounding, x, y,
				 srcwin, ShapeBounding, operation);
	    }
	  }
	  if (kind==1 || kind==2) {
	    XShapeCombineShape(dpy, w, ShapeClip, x, y,
			       srcwin, ShapeClip, operation);
	    if (parent!=None) {
	      XShapeCombineShape(dpy, parent, ShapeClip, x, y,
				 srcwin, ShapeClip, operation);
	    }
	  }

	  return TCL_OK;
	}
      case 7: /* text */

	/* You are warned that the operation of this code is _highly_
	 * arcane, requiring the drawing of the text first on a
	 * pixmap, then the conversion of that pixmap to an image, and
	 * then that image to a (sorted) rectangle list which can then
	 * be combined with the shape region.  AAARGH!
	 *
	 * In other words, this code is awful and slow, but it works.
	 * Modify at your own risk...  */

	if (objidx+3!=objc) {
	  Tcl_AppendResult(interp, "text requires two arguments; the "
			   "string to display and the font to use to "
			   "display it", NULL);
	  return TCL_ERROR;
	} else {
	  Pixmap textPixmap;
	  int width, length, i,j,k;
	  Tk_Font tkfont;
	  Tk_FontMetrics metrics;
	  char *string;
	  GC gc;
	  XGCValues gcValues;
	  XImage *xi;
	  XRectangle *rects;
	  unsigned long black = BlackPixel(dpy, Tk_ScreenNumber(tkwin));
	  unsigned long white = WhitePixel(dpy, Tk_ScreenNumber(tkwin));

	  string = Tcl_GetStringFromObj(objv[objidx+1], &length);
	  tkfont = Tk_GetFont(interp, tkwin,
			      Tcl_GetStringFromObj(objv[objidx+2], NULL));

	  if (!tkfont) {
	    return TCL_ERROR;
	  }
	  Tk_GetFontMetrics(tkfont, &metrics);
	  width = Tk_TextWidth(tkfont, string, length);

	  /* Draw the text */
	  textPixmap = Tk_GetPixmap(dpy, w, width, metrics.linespace,
			       Tk_Depth(tkwin));
	  gcValues.foreground = white;
	  gcValues.graphics_exposures = False;
	  gc = Tk_GetGC(tkwin, GCForeground | GCGraphicsExposures, &gcValues);
	  XFillRectangle(dpy, textPixmap, gc, 0,0, width, metrics.linespace);
	  Tk_FreeGC(dpy, gc);
	  gcValues.font = Tk_FontId(tkfont);
	  gcValues.foreground = black;
	  gcValues.background = white;
	  gc = Tk_GetGC(tkwin, GCForeground | GCBackground | GCFont |
			GCGraphicsExposures, &gcValues);
	  Tk_DrawChars(dpy, textPixmap, gc, tkfont,
		       string, length, 0, metrics.ascent);
	  Tk_FreeGC(dpy, gc);
	  Tk_FreeFont(tkfont);

	  /* Convert to rectangles */
	  xi = XGetImage(dpy, textPixmap, 0,0, width,metrics.linespace,
			 AllPlanes, ZPixmap);
	  Tk_FreePixmap(dpy, textPixmap);
	  /* Note that this allocates far, far too much memory */
	  rects = (XRectangle *)
	    Tcl_Alloc(sizeof(XRectangle)*width*metrics.linespace);
	  for (j=1,k=0 ; j<metrics.linespace ; j++)
	    for(i=0 ; i<width ; i++) {
	      if (XGetPixel(xi, i, j)==black) {
		rects[k].x = i;
		rects[k].y = j;
		rects[k].width = 1;
		rects[k++].height = 1;
	      }
	    }
	  XDestroyImage(xi);

	  if (kind==0 || kind==2) {
	    XShapeCombineRectangles(dpy, w, ShapeBounding, 0, 0,
				    rects, k, operation, YXBanded);
	    if (parent!=None) {
	      XShapeCombineRectangles(dpy, parent, ShapeBounding, 0, 0,
				      rects, k, operation, YXBanded);
	    }
	  }
	  if (kind==1 || kind==2) {
	    XShapeCombineRectangles(dpy, w, ShapeClip, 0, 0,
				    rects, k, operation, YXBanded);
	    if (parent!=None) {
	      XShapeCombineRectangles(dpy, parent, ShapeClip, 0, 0,
				      rects, k, operation, YXBanded);
	    }
	  }
	  Tcl_Free((char *)rects);
	  return TCL_OK;
	}

      case 6: /* reset */
	if (opnum == updateCmd) {
	  Tcl_AppendResult(interp, "cannot reset shape while updating shape",
			   NULL);
	  return TCL_ERROR;
	} else if (objidx+1!=objc) {
	  Tcl_AppendResult(interp, "reset takes no arguments", NULL);
	  return TCL_ERROR;
	} else {
	  /* Reset to the window dimensions from the server */
	  Window root;
	  unsigned int width,height,bwidth,depth;
	  XRectangle rect;

	  if (XGetGeometry(dpy, w, &root, &x, &y,
			   &width, &height, &bwidth, &depth) == False) {
	    Tcl_AppendResult(interp, "window has no size to reset to", NULL);
	    return TCL_ERROR;
	  }
	  rect.x = 0;
	  rect.y = 0;
	  rect.width  = width;
	  rect.height = height;
	  if (kind==0 || kind==2) {
	    XShapeCombineRectangles(dpy, w, ShapeBounding, 0, 0,
				    &rect, 1, operation, Unsorted);
	    if (parent!=None) {
	      XShapeCombineRectangles(dpy, parent, ShapeBounding, 0, 0,
				      &rect, 1, operation, Unsorted);
	    }
	  }
	  if (kind==1 || kind==2) {
	    XShapeCombineRectangles(dpy, w, ShapeClip, 0, 0,
				    &rect, 1, operation, Unsorted);
	    if (parent!=None) {
	      XShapeCombineRectangles(dpy, parent, ShapeClip, 0, 0,
				      &rect, 1, operation, Unsorted);
	    }
	  }
	  return TCL_OK;
	}
      }
    default:
      Tcl_AppendResult(interp, "not supported", NULL);
      return TCL_ERROR;
    }
  }

  Tcl_AppendResult(interp, "no source to take shape from", NULL);
  return TCL_ERROR;
}

static int
shapeCmd(clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int subcmdidx;

  if (objc<2) {
    Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?window arg ...?");
    return TCL_ERROR;
  } else if (Tcl_GetIndexFromObj(interp, objv[1],
	  (CONST84 char **) subcommands, "subcommand", 0, &subcmdidx)
	  != TCL_OK) {
    return TCL_ERROR;
  }

  if (shapeCommandHandlers[subcmdidx]) {
    /* Farm out the more complex operations */
    return shapeCommandHandlers[subcmdidx](clientData, interp, subcmdidx,
					   objc, objv);
  } else switch (subcmdidx) {  
  case versionCmd:
    /* shape version */
    if (objc!=2) {
      Tcl_WrongNumArgs(interp, 1, objv, "version");
      return TCL_ERROR;
    } else {
      int major=-1, minor=-1;
      char buffer[64];
      Status s = XShapeQueryVersion(Tk_Display((Tk_Window)clientData),
				    &major, &minor);

      if (s==True) {
	sprintf(buffer, "%d.%d", major, minor);
	Tcl_AppendResult(interp, buffer, NULL);
      }
      return TCL_OK;
    }

  default:
    panic("switch fallthrough");
  }
  return TCL_ERROR;
}

int
Shape_Init(interp)
     Tcl_Interp *interp;
{
  Tk_Window tkwin = Tk_MainWindow(interp);
  int evBase, errBase;

  if (Tcl_PkgRequire(interp, "Tk", "8", 0) == NULL) {
    return TCL_ERROR;
  }
  if (XShapeQueryExtension(Tk_Display(tkwin), &evBase, &errBase) == False) {
    Tcl_AppendResult(interp, "shaped window extension not supported", NULL);
    return TCL_ERROR;
  }

  Tcl_CreateObjCommand(interp, "shape", shapeCmd, tkwin, NULL);
  Tcl_SetVar(interp, "shape_version", "0.3", TCL_GLOBAL_ONLY);
  Tcl_SetVar(interp, "shape_patchLevel", "0.3a1", TCL_GLOBAL_ONLY);
  return Tcl_PkgProvide(interp, "shape", "0.3");
}
