#ifndef Fl_Scope_H
#define Fl_Scope_H
/***********************************************************
*                        Fl_Scope.h
*
* Author: Michael Pearce
*
* Started: 1 August 2003
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
*   See Fl_Scope.cxx for Version Information
***********************************************************/

#ifndef Fl_Widget_H
#include <FL/Fl_Widget.H>
#endif


#define FL_SCOPE_TRACE_SCROLL     0
#define FL_SCOPE_TRACE_LOOP       1
#define FL_SCOPE_TRACE_LOOP_CLEAR 2

#define FL_SCOPE_REDRAW_OFF 0
#define FL_SCOPE_REDRAW_FULL 1
#define FL_SCOPE_REDRAW_ALWAYS 2

#define FL_SCOPE_DOT  0
#define FL_SCOPE_LINE 1

#define FL_SCOPE_SIGNED    0
#define FL_SCOPE_UNSIGNED  1

class FL_EXPORT Fl_Scope : public Fl_Widget
{
  int       _x,_y,_w,_h;     /* The draw position */

  //unsigned char *ScopeData;  /* Pointer to dynamic array of track info */
  int *ScopeData;  /* Pointer to dynamic array of track info */
  
  int ScopeDataSize;
  int ScopeDataPos;
  
  Fl_Color _TraceColour;     /* Trace Colour */
  Fl_Color _BackColour;      /* Background Colour */

  int TraceType;
  int RedrawMode;
  int LineType;
  int DataType;
  
protected:

  void draw(int,int,int,int);

  int handle(int,int,int,int,int);
    
  void draw();

  
  /* These are protected because changing the size screws up the buffer */
  /* May Fix this problem later                                         */
  void x(int X){ _x=X;};
  void y(int Y){ _y=Y;};
  void w(int W){ _w=W;};
  void h(int H){ _h=H;};
  
  
  
public:


  int x(){return _x;};
  //void x(int X){ _x=X;};

  int y(){return _y;};
  //void y(int Y){ _y=Y;};

  int w(){return _w;};
  //void w(int W){ _w=W;};

  int h(){return _h;};
  //void h(int H){ _h=H;};

  
  int tracetype(){return TraceType;};
  void tracetype(int t){TraceType=t;};
  
  int redrawmode(){return RedrawMode;};
  void redrawmode(int t){RedrawMode=t;};
  
  int linetype(){return LineType;};
  void linetype(int t){LineType=t;};
  
  
  int datatype(){return DataType;};
  void datatype(int t){DataType=t;};
  
  
  //int Add(unsigned char);          /* Add Data to Scope */
  int Add(int);          /* Add Data to Scope */ 
   
  Fl_Color TraceColour(){return _TraceColour;};
  void     TraceColour(Fl_Color c){_TraceColour=c;};

  Fl_Color BackColour(){return _BackColour;};
  void     BackColour(Fl_Color c){_BackColour=c;};

  virtual int handle(int);
  Fl_Scope(int,int,int,int,const char * = 0);
  ~Fl_Scope();
};


/**************** END OF FILE ******************************/
#endif


