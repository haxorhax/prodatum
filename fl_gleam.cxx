//
// "Gleam" drawing routines for the Fast Light Tool Kit (FLTK).
//
// These box types provide a sort of Clearlooks Glossy scheme
// for FLTK.
//
// Copyright 2001-2005 by Colin Jones.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA.
//
// Please report all bugs and problems on the following page:
//
//     http://www.fltk.org/str.php
//

// $Id: fl_gleam.cxx 386 2011-03-29 14:52:41Z vvd_ $

// Box drawing code for an obscure box type.
// These box types are in seperate files so they are not linked
// in if not used.

#include <FL/Fl.H>
#include <FL/fl_draw.H>

static void gleam_color(Fl_Color c)
{
	if (Fl::draw_box_active())
		fl_color(c);
	else
		fl_color(fl_inactive(c));
}
static void frame_rect(int x, int y, int w, int h, Fl_Color bc)
{

	// Draw the outline around the perimeter of the box
	fl_color(fl_color_average(FL_BLACK, FL_BACKGROUND_COLOR, .1));
	fl_line(x, y, x + w, y);
	fl_line(x + w, y, x + w, y + h);
	fl_line(x + w, y + h, x, y + h);
	fl_line(x, y + h, x, y);

}

static void shade_rect_up(int x, int y, int w, int h, Fl_Color bc)
{
	// Draws the shiny
	float third = (float) h / 3;
	gleam_color(bc);
	fl_rectf(x, y, w, third + 1);

	//gleam_color(fl_color_average(bc, FL_WHITE, .90f));
	//fl_rectf(x, y, w, half + 1);

	float step_size = 0.10 / ((float) h - third);
	int j = 0;
	//step_size = (.1 / (float) half);
	//printf("1 / %i = %f \n", half, (1.0/half));

	/**
	 * This loop generates the nice gradient at the bottom of the
	 * widget
	 **/
	for (float k = 1; k >= .90; k -= step_size)
	{
		j++;
		gleam_color(fl_color_average(bc, FL_WHITE, k));
		fl_line(x, y + j + third - 1, x + w - 1, y + j + third - 1);
	}

}

static void frame_rect_up(int x, int y, int w, int h, Fl_Color bc)
{

	// Draw the outline around the perimeter of the box
	gleam_color(bc);
	fl_line(x, y, x + w, y); //Go across.
	fl_line(x, y + (h / 2), x, y + 1); //Go to top
	fl_line(x + w, y + (h / 2), x + w, y + 1); //Go to top

	gleam_color(fl_darker(bc));
	fl_line(x, y + h, x + w, y + h); //Go across again!
	fl_line(x, y + (h / 2), x, y + h - 1); //Go to top
	fl_line(x + w, y + (h / 2), x + w, y + h - 1); //Go to top

}

static void frame_rect_down(int x, int y, int w, int h, Fl_Color bc)
{

	// Draw the outline around the perimeter of the box
	gleam_color(fl_darker(bc));
	fl_line(x, y, x + w, y); //Go across.
	fl_line(x, y + (h / 2), x, y + 1); //Go to top
	fl_line(x + w, y + (h / 2), x + w, y + 1); //Go to top

	//gleam_color(bc);
	fl_line(x, y + h, x + w, y + h); //Go across again!
	fl_line(x, y + (h / 2), x, y + h - 1); //Go to top
	fl_line(x + w, y + (h / 2), x + w, y + h - 1); //Go to top

}

static void shade_rect_down(int x, int y, int w, int h, Fl_Color bc)
{

	gleam_color(bc);
	Fl_Color color = fl_color();
	fl_rectf(x, y, w, h);
	gleam_color(fl_color_average(bc, fl_darker(color), 0.65));
	fl_line(x, y + 1, x + w, y + 1);
	fl_line(x, y + 1, x, y + h - 2);
	gleam_color(fl_color_average(bc, fl_darker(color), 0.85));
	fl_line(x + 1, y + 2, x + w, y + 2);
	fl_line(x + 1, y + 2, x + 1, y + h - 2);

}

static void up_frame(int x, int y, int w, int h, Fl_Color c)
{
	frame_rect_up(x, y, w - 1, h - 1, fl_darker(c));
}

static void thin_up_box(int x, int y, int w, int h, Fl_Color c)
{

	shade_rect_up(x + 1, y, w - 2, h - 1, c);
	frame_rect(x + 1, y + 1, w - 3, h - 3, fl_color_average(c, FL_WHITE, .25f));
	frame_rect_up(x, y, w - 1, h - 1, fl_darker(c));

}

static void up_box(int x, int y, int w, int h, Fl_Color c)
{
	shade_rect_up(x + 1, y, w - 2, h - 1, c);
	frame_rect_up(x, y, w - 1, h - 1, fl_darker(c));
	//draw the inner rect.
	frame_rect(x + 1, y + 1, w - 3, h - 3, fl_color_average(c, FL_WHITE, .25f));

}

static void down_frame(int x, int y, int w, int h, Fl_Color c)
{
	frame_rect_down(x, y, w - 1, h - 1, fl_darker(c));
}

static void down_box(int x, int y, int w, int h, Fl_Color c)
{
	shade_rect_down(x + 1, y, w - 2, h, c);
	down_frame(x, y, w, h, fl_darker(c));
	//draw the inner rect.
	//frame_rect(x + 1, y + 1, w - 3, h - 3, fl_color_average(c, FL_BLACK, .65));
}

static void thin_down_box(int x, int y, int w, int h, Fl_Color c)
{

	down_box(x, y, w, h, c);

}

extern void fl_internal_boxtype(Fl_Boxtype, Fl_Box_Draw_F*);

Fl_Boxtype fl_define_FL_GLEAM_UP_BOX()
{
	fl_internal_boxtype(_FL_GLEAM_UP_BOX, up_box);
	fl_internal_boxtype(_FL_GLEAM_DOWN_BOX, down_box);
	fl_internal_boxtype(_FL_GLEAM_UP_FRAME, up_frame);
	fl_internal_boxtype(_FL_GLEAM_DOWN_FRAME, down_frame);
	fl_internal_boxtype(_FL_GLEAM_THIN_UP_BOX, thin_up_box);
	fl_internal_boxtype(_FL_GLEAM_THIN_DOWN_BOX, thin_down_box);
	fl_internal_boxtype(_FL_GLEAM_ROUND_UP_BOX, up_box);
	fl_internal_boxtype(_FL_GLEAM_ROUND_DOWN_BOX, down_box);

	return _FL_GLEAM_UP_BOX;
}
