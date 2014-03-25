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

// Heavily modified for prodatum (Jan Eidtmann)

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

	Fl_Color col = FL_BLACK;
	if (bc == FL_BACKGROUND_COLOR)
		col = FL_FOREGROUND_COLOR;
	gleam_color(fl_color_average(bc, col, 0.5));
	fl_rect(x, y, w, h);
}

static void shade_rect_up(int x, int y, int w, int h, Fl_Color bc)
{
	unsigned char r, g, b;
	Fl::get_color(bc, r, g, b);
	Fl_Color col = FL_WHITE;
	if ((r + r + b + g + g + g) / 6 > 128)
		col = FL_BLACK;
	// Draws the shiny
	float third = (float) h / 3;
	gleam_color(bc);
	fl_rectf(x, y, w, third + 1);
	float step_size = 0.10 / ((float) h - third);
	int j = 0;

	/**
	 * This loop generates the nice gradient at the bottom of the
	 * widget
	 **/
	for (float k = 1; k >= .90; k -= step_size)
	{
		j++;
		gleam_color(fl_color_average(bc, col, k));
		fl_line(x, y + j + third - 1, x + w, y + j + third - 1);
	}
}

static void shade_rect_down(int x, int y, int w, int h, Fl_Color bc)
{
	gleam_color(bc);
	fl_rectf(x, y, w, h);
}

static void up_box(int x, int y, int w, int h, Fl_Color c)
{
	shade_rect_up(x + 1, y, w - 2, h - 1, c);
	frame_rect(x, y, w, h, c);
}

static void down_box(int x, int y, int w, int h, Fl_Color c)
{
	shade_rect_down(x + 1, y, w - 2, h, c);
	frame_rect(x, y, w, h, c);
}

extern void fl_internal_boxtype(Fl_Boxtype, Fl_Box_Draw_F*);

Fl_Boxtype fl_define_FL_GLEAM_UP_BOX()
{
	fl_internal_boxtype(_FL_GLEAM_UP_BOX, up_box);
	fl_internal_boxtype(_FL_GLEAM_DOWN_BOX, down_box);
	fl_internal_boxtype(_FL_GLEAM_UP_FRAME, frame_rect);
	fl_internal_boxtype(_FL_GLEAM_DOWN_FRAME, frame_rect);
	fl_internal_boxtype(_FL_GLEAM_THIN_UP_BOX, up_box);
	fl_internal_boxtype(_FL_GLEAM_THIN_DOWN_BOX, down_box);
	fl_internal_boxtype(_FL_GLEAM_ROUND_UP_BOX, up_box);
	fl_internal_boxtype(_FL_GLEAM_ROUND_DOWN_BOX, down_box);

	return _FL_GLEAM_UP_BOX;
}
