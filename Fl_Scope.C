#ifndef Fl_Scope_Version
#define Fl_Scope_Version "V0.0.2"
#define DEBUG 0

#define MIDI_EOX 0xf7

/******************************************************************
*                      Fl_Scope.cxx
*
* A simple widget that simulates an oscilloscope type trace.
* Input is 8 bit.
* Data starts from left and moves right, then starts scrolling.
* The buffer is teh same as the width of the widget.
*
* Use ->add(unsigned char); to add next data point to the scope.
* 
* Author: Michael Pearce <mike@slavelighting.com>
*
* Started: 18 March 2003
*
* Copyright: Copyright 2003 Michael Pearce All Rights reserved.
*
* Licence:   GNU/GPL 
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program (GNU.txt); if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
* or visit http://www.gnu.org/licenses/licenses.html
*
************************************************************
*                  Version Information
************************************************************
* V0.1.0 - 14 February 2005
* Lots of changes:
*  Different Redraw modes added
*  Uses signed int for data
*  Has signed and unsigned display modes
************************************************************
* V0.0.2 - 6 August 2003
* Moved x(int) etc to  protected. 
************************************************************
* V0.0.1 - 4 August 2003
*  Work on the drawing functions.... seems to keep a blank!
************************************************************
* V0.0.0 - 1 August 2003
*  Modified for use with Makefile.
*  Added Version() function to return code version.
************************************************************/


#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include "Fl_Scope.H"
#include <stdlib.h>
#include <stdio.h>

void Fl_Scope::draw()
{
 draw(x(),y(),w(),h());
}

void Fl_Scope::draw(int xx, int yy, int ww, int hh)
{
 //unsigned char *Ptr,*Ptr2;
 int *Ptr,*Ptr2;
 int count,x;
 int Yval,Yval2;

 /* Push clip for drawing */
 fl_push_clip(xx,yy,ww,hh);

 /* Draw Main Box */
 fl_draw_box(FL_FLAT_BOX,xx,yy,ww,hh,_BackColour);

 fl_color(_TraceColour);
 
 /* Draw the scope Data */
 Ptr2=Ptr=ScopeData;
 Ptr2++;
 for(count=0;count<ScopeDataSize-1;count++)
 {
 
   
  switch(LineType)
  {
   default:
   case FL_SCOPE_LINE:  
    //fl_line(xx,(yy+hh) - (int)((float)*Ptr * ((float)hh/255.0)),xx+1,(yy+hh) - (int)((float)*Ptr2 * ((float)hh/255.0)) );
    if(DataType==FL_SCOPE_UNSIGNED)
    {
     fl_line(xx,(yy+hh) - (int)((float)*Ptr * ((float)hh/MIDI_EOX)),xx+1,(yy+hh) - (int)((float)*Ptr2 * ((float)hh/MIDI_EOX)) );
    }
    else
    {
     Yval=(int) (  (float)((int)*Ptr) * (float)hh/(MIDI_EOX/2.0));
     Yval2=(int) (  (float)((int)*Ptr2) * (float)hh/(MIDI_EOX/2.0));
     fl_line(xx,(yy+(hh/2)) - Yval,xx+1,(yy+(hh/2)) - Yval2 );
    }
    break;
   
   case FL_SCOPE_DOT:
    if(DataType==FL_SCOPE_UNSIGNED)
    {
     fl_point(xx,(yy+hh) - (int)((float)*Ptr * ((float)hh/MIDI_EOX)) );
    }
    else
    {
     Yval=(int) (  (float)((int)*Ptr) * (float)hh/(MIDI_EOX/2.0));
     fl_point(xx,(yy+(hh/2)) - Yval);
    }
    break;  
    
  }
  xx++;
  Ptr2++;
  Ptr++;
 }

 
 /* pop the clip */
 fl_pop_clip();
}



/*******************************************************
*               Fl_Scope::Add
*******************************************************/
//int Fl_Scope::Add(unsigned char data)
int Fl_Scope::Add(int data)
{
 //unsigned char *Ptr,*Ptr2;
 int *Ptr,*Ptr2;
 int count;
 
 
 if(ScopeDataPos > ScopeDataSize)ScopeDataPos=0;
 
 switch(TraceType)
 {
   default:
   case FL_SCOPE_TRACE_SCROLL:
    /* Move Data to left then add data at the end */
    Ptr=Ptr2=ScopeData;
    Ptr2++;
    for(count=0;count<ScopeDataSize;count++)
    {
     *Ptr=*Ptr2;
      Ptr++;Ptr2++;
    }
    *Ptr=data;
    break;
 
  case FL_SCOPE_TRACE_LOOP_CLEAR:
   if(ScopeDataPos==0)
   {
    Ptr=ScopeData;
    for(count=0;count<=ScopeDataSize;count++)
    {
     *Ptr=0;
     Ptr++;
    }
   }
     
  case FL_SCOPE_TRACE_LOOP:
   /* Insert data, and once at end loop back to the start */
   Ptr=ScopeData;
   Ptr+=ScopeDataPos;
   *Ptr=data;
   break;
  
 }
 
 ScopeDataPos++;
 
 switch(RedrawMode)
 {
  case FL_SCOPE_REDRAW_OFF:
   break;
  
  case FL_SCOPE_REDRAW_FULL:
   if(ScopeDataPos == ScopeDataSize) redraw();
   break;
 
  default:
  case FL_SCOPE_REDRAW_ALWAYS:
   redraw();
   break;
 }
 
 
  
 return(1); 
}



/********************************************************
*                   handle(int)
*
* Entry point for the event handling
********************************************************/
int Fl_Scope::handle(int event)
{
  if (event == FL_PUSH && Fl::visible_focus()) Fl::focus(this);

  return handle(event,
    x()+Fl::box_dx(box()),
    y()+Fl::box_dy(box()),
    w()-Fl::box_dw(box()),
    h()-Fl::box_dh(box()));

}

/********************************************************
*                   handle(int,int,int,int,int)
*
* Sort out what handles need to be done!!
********************************************************/
int Fl_Scope::handle(int event, int X, int Y, int W, int H)
{

 /* Check for Misc Things */
 switch (event)
 {
  case FL_PUSH:
   if (!Fl::event_inside(X, Y, W, H)) return 0;
   //handle_push();

  case FL_DRAG:
   return 0;

  case FL_RELEASE:
     //handle_release();
    return 0;

  case FL_KEYBOARD :
    switch (Fl::event_key())
    {
      case FL_Up:
       //return 1;
      case FL_Down:
       //return 1;
      case FL_Left:
       //return 1;
      case FL_Right:
        //return 1;
      default:
        return 0;
    }
    // break not required because of switch...
  case FL_FOCUS :
  case FL_UNFOCUS :
    if (Fl::visible_focus())
    {
      redraw();
      return 1;
    } else return 0;

  case FL_ENTER :
  case FL_LEAVE :
    //return 1;
  default:
    return 0;
  }
}



/*******************************************************
*               Fl_Scope::Fl_Scope
******************************************************/
Fl_Scope::Fl_Scope(int X, int Y, int W, int H, const char *l)
: Fl_Widget(X,Y,W,H,l)
{
 //unsigned char *Ptr;
 int *Ptr;
 int count;
 
 /* Size of it !! */
 x(X);y(Y);w(W);h(H);

 box(FL_UP_BOX);
  
 BackColour(FL_BLACK);
 TraceColour(FL_WHITE);
 
 /* Create Array for Scope Data */
 //Ptr=ScopeData=(unsigned char*)calloc(W,sizeof(char));
 Ptr=ScopeData=(int*)calloc(W,sizeof(int));
  
 ScopeDataSize=W-1;

 ScopeDataPos=0;
 
 /* Make Scope trace a scrolling type */
 tracetype(FL_SCOPE_TRACE_SCROLL);

 redrawmode(FL_SCOPE_REDRAW_ALWAYS);
 
 linetype(FL_SCOPE_LINE);
 
 datatype(FL_SCOPE_UNSIGNED);
 
 /* Clear Scope Data Array */
 for(count=0;count<W;count++)
 {
  *Ptr=0;Ptr++;
 }

}

/*******************************************************
*               Fl_Scope::~Fl_Scope
******************************************************/
Fl_Scope::~Fl_Scope()
{
 free(ScopeData); /* Free the scope data */
}


/*************** END OF FILE *************************/
#endif



