// $Id$

/**
 \defgroup pd_widgets prodatum Widgets
 @{
 */

/*
 int ret = ::handle(ev);
 if (!foo)
 return ret;
 switch (ev)
 {
 // Mouse Events
 case FL_ENTER: // 1 = receive FL_LEAVE and FL_MOVE events (widget becomes Fl::belowmouse())
 case FL_LEAVE:
 case FL_MOVE: // sent to Fl::belowmouse()
 case FL_PUSH: // 1 = receive FL_DRAG and the matching (Fl::event_button()) FL_RELEASE event (becomes Fl::pushed())
 case FL_RELEASE:
 case FL_DRAG: // button state is in Fl::event_state() (FL_SHIFT FL_CAPS_LOCK FL_CTRL FL_ALT FL_NUM_LOCK FL_META FL_SCROLL_LOCK FL_BUTTON1 FL_BUTTON2 FL_BUTTON3)
 case FL_MOUSEWHEEL:
 // keyboard events
 case FL_FOCUS: // 1 = receive FL_KEYDOWN, FL_KEYUP, and FL_UNFOCUS events (widget becomes Fl::focus())
 case FL_UNFOCUS: // received when another widget gets the focus and we had the focus
 case FL_KEYDOWN: // key press (Fl::event_key())
 case FL_KEYUP: // key release (Fl::event_key())
 // DND events
 case FL_DND_ENTER: // 1 = receive FL_DND_DRAG, FL_DND_LEAVE and FL_DND_RELEASE events
 case FL_DND_DRAG: // to indicate if we want the data
 case FL_DND_RELEASE: // 1 = receive FL_PASTE
 }
 return ret;
 */

#ifndef WIDGETS_H_
#define WIDGETS_H_

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Value_Input.H>
#include <FL/Fl_Value_Output.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Spinner.H>
#include <FL/Fl_Counter.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Tooltip.H>

#include "config.h"

/**
 * maps filter parameter values to index in selectors array.
 * also includes filter name and filter informations
 */
struct FilterMap
{
	int id;
	const char* name;
	const char* info;
	int _index;
};

struct Patchcord
{
	int id;
	const char* name;
};

extern Patchcord PatchS[78];
extern Patchcord PatchD[68];
extern Patchcord PresetPatchD[28];

/**
 * shows confirmation dialog on various occasions
 * @param exit wether to act as an exit dialog
 * @return the chosen answer
 */
int dismiss(char exit);

/**
 * PWid abstract class.
 * all device parameter widgets derive from this. those can then all
 * put into the globa \c pwid[][] array, where they can be accessed
 * by a parameter ID and layer number
 */
class PWid
{
protected:
	/**
	 * callback.
	 * @param p parameter ID and Layer of the caller (id_layer[])
	 */
	static void cb(PWid*, void* p);
	/// holds parameter ID and layer number
	int id_layer[2];
	/// holds min and max values for this parameter
	int minimax[2];
public:
	virtual ~PWid()
	{
		;
	}
	/**
	 * assigns parameter ID and layer information.
	 * connects the callback, puts pointer to this into the global \c pwid[][]
	 * array
	 */
	virtual void set_id(int v, int l = 0) = 0;
	/**
	 * updates the value of the widget.
	 * @param v the new integer value
	 */
	virtual void set_value(int v) = 0;
	/**
	 * gets the current widget value.
	 * @return current integer value
	 */
	virtual int get_value() const = 0;
	/// returns a pointer to id_layer[]
	int* get_id_layer()
	{
		return id_layer;
	}
	/// returns a pointer to minimax[]
	virtual int* get_minimax()
	{
		return minimax;
	}
};

extern PWid* pwid[2000][4];
extern PWid* pwid_editing;
// some often used toltips
static const char* _ms = "Modulation Source";
static const char* _md = "Modulation Destination";
static const char* _ma = "Modulation Amount (double click to toggle between 0 and current value)";


/**
 * Double_Window class
 * just overwrites the default handler to support
 * key press+hold+release callbacks
 */
class Double_Window: public Fl_Double_Window
{
	int handle(int event);
	bool w__shown;
	bool supposed_to_be_shown;
public:
	void showup();
	virtual void hide();
	bool shown_called();
	void resize(int, int, int, int);
	bool __main;
	Double_Window(int w, int h, char* const label = 0) :
			Fl_Double_Window(w, h, label)
	{
		w__shown = false;
		__main = false;
		supposed_to_be_shown = false;
	}
};

class DND_Box: public Fl_Box
{
	int handle(int event);
public:
	DND_Box(int x, int y, int w, int h, char* const label = 0) :
			Fl_Box(x, y, w, h, label)
	{
		clear_visible_focus();
	}
	void dnd();
protected:
	char evt_txt[PATH_MAX];
};

/**
 * Browser class.
 * adds a filter and a name loader method (load_n) to the Fl_Browser class.
 * also adds "right-click to reset to initial" support
 */
class Browser: public Fl_Hold_Browser, public PWid
{
	/// current filter string
	char* filter;
	/// adds right-click support
	int handle(int event);
	/// local memory of the ROM ID that we have currently loaded
	int selected_rom;
public:
	Browser(int x, int y, int w, int h, char* const label = 0) :
			Fl_Hold_Browser(x, y, w, h, label)
	{
		filter = '\0';
		has_scrollbar(VERTICAL);
		selected_rom = -1;
	}
	void set_id(int v, int l = 0);
	void set_value(int v);
	int get_value() const;
	void set_filter(const char* filter_string);
	void apply_filter();
	void reset();
	void load_n(int type, int rom_id, int preset = -1);
	int* get_minimax()
	{
		minimax[1] = size() - 1;
		return minimax;
	}
};

/**
 * ROM_Choice class.
 * adds "right-click to reset to initial" and mousewheel support
 */
class ROM_Choice: public Fl_Choice, public PWid
{
	int no_user;
	int handle(int event);
	void dependency(int v, bool get) const;
public:
	ROM_Choice(int x, int y, int w, int h, char* const label = 0) :
			Fl_Choice(x, y, w, h, label)
	{
		no_user = 1;
	}
	void set_id(int v, int l = 0);
	void set_value(int v);
	int get_value() const;
};

/**
 * Input class.
 * used by name and filter inputs
 * changes to FL_Input: disabled the unfocus on enter key
 */
class Input: public Fl_Input
{
	int handle(int ev);
public:
	Input(int x, int y, int w, int h, char const* label = 0) :
			Fl_Input(x, y, w, h, label)
	{
		;
	}
};

/**
 * Value_Input class.
 * adds "right-click to reset to initial" and mousewheel support
 */
class Value_Input: public Fl_Value_Input, public PWid
{
	int handle(int event);
public:
	Value_Input(int x, int y, int w, int h, char const* label = 0) :
			Fl_Value_Input(x, y, w, h, label)
	{
		;
	}
	void set_id(int v, int l = 0);
	void set_value(int);
	int get_value() const;
};

/**
 * Formatted_Output class.
 * shows formatted parameter values
 */
class Formatted_Output: public Fl_Value_Output
{
	virtual int format(char* buf);
	int id;
	int layer;
public:
	Formatted_Output(int x, int y, int w, int h, char const* label = 0) :
			Fl_Value_Output(x, y, w, h, label)
	{
		;
	}
	void set_value(int p, int l, int v);
};

/**
 * Value_Output class.
 * adds "right-click to reset to initial" and mousewheel support
 */
class Value_Output: public Fl_Value_Output, public PWid
{
	int handle(int event);
	int double_click_value; // used for patchcord double click toggle
public:
	Value_Output(int x, int y, int w, int h, char const* label = 0) :
			Fl_Value_Output(x, y, w, h, label)
	{
		double_click_value = 0;
		tooltip(_ma);
	}
	void set_id(int v, int l = 0);
	void set_value(int v);
	int get_value() const;
};

/**
 * Slider class.
 * adds "right-click to reset to initial" and mousewheel support
 */
class Slider: public Fl_Slider, public PWid
{
	int handle(int event);
	mutable int prev_value;
protected:
	void draw(int X, int Y, int W, int H);
public:
	Slider(int x, int y, int w, int h, char const* label = 0) :
			Fl_Slider(x, y, w, h, label)
	{
		prev_value = -96;
		id_layer[0] = -1; // used by cc sliders
	}
	void set_id(int v, int l = 0);
	void set_value(int v);
	int get_value() const;
	void draw();
};

/**
 * Spinner class.
 * adds "right-click to reset to initial" and mousewheel support
 */
class Spinner: public Fl_Spinner, public PWid
{
	int handle(int event);
public:
	Spinner(int x, int y, int w, int h, char const* label = 0) :
			Fl_Spinner(x, y, w, h, label)
	{
		;
	}
	void set_id(int v, int l = 0);
	void set_value(int v);
	int get_value() const;
};

/**
 * Counter class.
 * adds "right-click to reset to initial" and mousewheel support
 */
class Counter: public Fl_Counter, public PWid
{
	int handle(int event);
public:
	Counter(int x, int y, int w, int h, char const* label = 0) :
			Fl_Counter(x, y, w, h, label)
	{
		;
	}
	void set_id(int v, int l = 0);
	void set_value(int v);
	int get_value() const;
};

/**
 * Group class.
 * adds "right-click to reset to initial" support
 */
class Group: public Fl_Group, public PWid
{
	int handle(int event);
	void dependency(int v) const;
public:
	Group(int x, int y, int w, int h, char const* label = 0) :
			Fl_Group(x, y, w, h, label)
	{
		;
	}
	void set_id(int v, int l = 0);
	virtual void set_value(int);
	virtual int get_value() const;
};

/**
 * Fl_Knob class.
 * adds "right-click to reset to initial" and mousewheel support
 */
class Fl_Knob: public Fl_Valuator, public PWid
{
	int _type;
	float _percent;
	int _scaleticks;
	short a1, a2;
	void draw();
	int handle(int event);
	void draw_scale(const int ox, const int oy, const int side);
	void draw_cursor(const int ox, const int oy, const int side);
	void shadow(const int offs, const uchar r, uchar g, uchar b);
	void dependency(int v) const;
public:
	Fl_Knob(int xx, int yy, int ww, int hh, const char *l = 0);
	void cursor(const int pc);
	void scaleticks(const int tck);
	void set_id(int v, int l = 0);
	void set_value(int);
	int get_value() const;
};

/**
 * Button class.
 * adds "right-click to reset to initial" support
 */
class Button: public Fl_Button, public PWid
{
	int handle(int event);
public:
	Button(int x, int y, int w, int h, char const* label = 0) :
			Fl_Button(x, y, w, h, label)
	{
		id_layer[0] = -1;
	}
	void set_id(int v, int l = 0);
	void set_value(int v);
	int get_value() const;
	//	void dependency(int v) const;
};

class Fixed_Button: public Fl_Button
{
	int handle(int event);
public:
	Fixed_Button(int x, int y, int w, int h, char const* label = 0) :
			Fl_Button(x, y, w, h, label)
	{
		;
	}
};

/**
 * Choice class.
 * adds "right-click to reset to initial" and mousewheel support
 */
class Choice: public Fl_Choice, public PWid
{
	int handle(int event);
	void dependency(int v) const;
public:
	Choice(int x, int y, int w, int h, char* const label = 0) :
			Fl_Choice(x, y, w, h, label)
	{
		;
	}
	void set_id(int v, int l = 0);
	void set_value(int v);
	int get_value() const;
};

/**
 * PCS_Choice class.
 * special class for patchcord sources
 */
class PCS_Choice: public Choice
{
	int index[78];
	bool initialized;
	void init(int ec)
	{
		clear();
		for (char i = 0; i < 74 + ec; i++)
			index[i] = add(PatchS[i].name);
		initialized = true;
	}
public:
	PCS_Choice(int x, int y, int w, int h, char* const label = 0) :
			Choice(x, y, w, h, label)
	{
		minimax[1] = 167;
		initialized = false;
		tooltip(_ms);
	}
	void set_value(int v);
	int get_value() const;
	void uninitialize()
	{
		initialized = false;
	}
};

/**
 * PCD_Choice class.
 * special class for patchcord destinations
 */
class PCD_Choice: public Choice
{
	int index[68];
public:
	PCD_Choice(int x, int y, int w, int h, char* const label = 0) :
			Choice(x, y, w, h, label)
	{
		for (char i = 0; i < 68; i++)
			index[i] = add(PatchD[i].name);
		minimax[1] = 191;
		tooltip(_md);
	}
	void set_value(int v);
	int get_value() const;
};

/**
 * PPCD_Choice class.
 * special class for preset patchcord destinations
 */
class PPCD_Choice: public Choice
{
	int index[28];
public:
	PPCD_Choice(int x, int y, int w, int h, char* const label = 0) :
			Choice(x, y, w, h, label)
	{
		for (char i = 0; i < 28; i++)
			index[i] = add(PresetPatchD[i].name);
		minimax[1] = 131;
		tooltip(_md);
	}
	void set_value(int v);
	int get_value() const;
};

/**
 * Envelope_Editor class.
 * features zooming and display of multiple envelopes. mousewheel switches
 * the currently selected envelope
 */
class Envelope_Editor: public Fl_Box
{
	struct envelope
	{
		int stage[6][2]; // x/y coordinates of the 6 envelope stages
		char mode, repeat;
	};
	envelope env[3];
	enum
	{
		ATK_1, DCY_1, RLS_1, ATK_2, DCY_2, RLS_2
	};
	enum
	{
		VOLUME, FILTER, AUXILIARY
	};
	enum
	{
		FACTORY,
		TIME_BASED,
		TEMPO_BASED,
		OVERLAY,
		SYNC_VOICE_VIEW,
		VOLUME_SELECTED,
		FILTER_SELECTED,
		AUXILIARY_SELECTED,
		CPY_VOLUME,
		CPY_FILTER,
		CPY_AUXILIARY,
		SHAPE_A,
		SHAPE_B,
		SHAPE_C,
		SHAPE_D
	};
	virtual int handle(int event);
	void draw();
	void draw_b_label(char, Fl_Color);
	void draw_envelope(unsigned char type, int x0, int y0, int luma);
	void copy_envelope(unsigned char src, unsigned char dst);
	void set_shape(unsigned char dst, char shape);
	char layer;
	int ee_x0;
	int ee_y0;
	int ee_w;
	int ee_h;
	int mode_button[5]; // x0 of the mode buttons (width = 75, h = 20)
	int copy_button[6];
	int shape_button[4];
	char button_hover;
	unsigned char zoomlevel;
	int dragbox[6][2];
	char hover;
	int hover_list; // 1, 2, 4, 8, 16, 32
	int push_x;
	int push_y;
	unsigned char mode;
	unsigned char modes;
	bool overlay;
	bool button_push;

public:
	Envelope_Editor(int x, int y, int w, int h, char* const label = 0) :
			Fl_Box(x, y, w, h, label)
	{
		zoomlevel = 4;
		modes = 3;
		mode = VOLUME;
		hover = -1;
		overlay = false;
		// initialize with some fake data
		set_shape(VOLUME, SHAPE_D);
		set_shape(FILTER, SHAPE_A);
		set_shape(AUXILIARY, SHAPE_C);
	}
	void set_data(unsigned char type, int* stages, char mode, char repeat);
	void set_layer(char l);
	void sync_view(char l, char m = 0, float z = .0, bool o = false);
};

/**
 * Piano class.
 * features a 127 key keyboard with velocity setting, pitch and modwheel,
 * 3 footswitches, layer transpose, layer, arp and link range/fade setup
 */
class Piano: public Fl_Box
{
	virtual int handle(int event);
	void draw();
	void draw_ranges();
	void draw_piano();
	void draw_highlights();
	void draw_case();
	void draw_curve(int type);
	void switch_mode();
	void commit_changes();
	void calc_hovered(int x, int y);
	enum
	{
		LOW_KEY, LOW_FADE, HIGH_KEY, HIGH_FADE
	};
	enum
	{
		NONE = -1, PRESET_ARP = 4, MASTER_ARP, LINK_ONE, LINK_TWO, PIANO
	};
	enum
	{
		KEYRANGE, VELOCITY, REALTIME
	};
	enum
	{
		D_RANGES = 1, D_KEYS, D_HIGHLIGHT = 4, D_CASE = 8
	};
	unsigned char mode; // 0 = keyrange, 1 = velocity, 2 = realtime
	unsigned char modes;
	int keyboard_x0, keyboard_y0, keyboard_w, keyboard_h;
	char h_white, w_white, h_black, w_black;
	int taste_x0[128][2];
	int dragbox[3][8][4][2]; // mode, layer, type, x/y
	char highlight_dragbox[8][4];
	char prev_key_value[3][8][4];
	char new_key_value[3][8][4];
	char pushed; // currently dragged part
	char pushed_range; // currently dragged range (low_key, low_fade...)
	char hovered_key, play_hovered_key;
	int active_keys[128];
	char previous_hovered_key;
	char selected_transpose_layer;
	char transpose[4];
	int push_x; // used for setting the key velocity
	char key_velocity;

public:
	Piano(int x, int y, int w, int h, char* const label = 0) :
			Fl_Box(x, y, w, h, label)
	{
		// tasten- h�hen/-breiten
		h_white = 32;
		w_white = 13; // 15
		h_black = 20;
		w_black = 7; // 9
		hovered_key = NONE;
		previous_hovered_key = NONE;
		play_hovered_key = 0;
		pushed = NONE;
		pushed_range = NONE;
		mode = KEYRANGE; // load piano at startup
		key_velocity = 100;
		modes = 3;
		selected_transpose_layer = 0;
		for (int i = 0; i < 128; i++)
			active_keys[i] = 0;
		// widget is 921 pixels wide
		// height is about 162
		// calculate our position and key koordinates
		keyboard_x0 = this->x() + 10;
		keyboard_y0 = this->y() + 11;
		keyboard_w = 75 * (w_white - 1);
		keyboard_h = h_white;
		int offset = 0;
		for (int i = 0; i < 11; i++)
		{
			// for each octave on the keyboard
			int octave = i * 12;
			taste_x0[0 + octave][0] = keyboard_x0 + offset;
			taste_x0[0 + octave][1] = 0;
			taste_x0[1 + octave][0] = keyboard_x0 + 9 + offset;
			taste_x0[1 + octave][1] = 1;
			taste_x0[2 + octave][0] = keyboard_x0 + w_white - 1 + offset;
			taste_x0[2 + octave][1] = 0;
			taste_x0[3 + octave][0] = keyboard_x0 + 9 + (w_white - 1) + offset;
			taste_x0[3 + octave][1] = 1;
			taste_x0[4 + octave][0] = keyboard_x0 + 2 * (w_white - 1) + offset;
			taste_x0[4 + octave][1] = 0;
			taste_x0[5 + octave][0] = keyboard_x0 + 3 * (w_white - 1) + offset;
			taste_x0[5 + octave][1] = 0;
			taste_x0[6 + octave][0] = keyboard_x0 + 9 + 3 * (w_white - 1) + offset;
			taste_x0[6 + octave][1] = 1;
			taste_x0[7 + octave][0] = keyboard_x0 + 4 * (w_white - 1) + offset;
			taste_x0[7 + octave][1] = 0;
			if (i == 10) // keyboard is smaller than full 10 full octaves
				break;
			taste_x0[8 + octave][0] = keyboard_x0 + 9 + 4 * (w_white - 1) + offset;
			taste_x0[8 + octave][1] = 1;
			taste_x0[9 + octave][0] = keyboard_x0 + 5 * (w_white - 1) + offset;
			taste_x0[9 + octave][1] = 0;
			taste_x0[10 + octave][0] = keyboard_x0 + 9 + 5 * (w_white - 1) + offset;
			taste_x0[10 + octave][1] = 1;
			taste_x0[11 + octave][0] = keyboard_x0 + 6 * (w_white - 1) + offset;
			taste_x0[11 + octave][1] = 0;
			offset += 7 * (w_white - 1);
		}
		// y-koordinaten der dragboxes
		for (unsigned char m = 0; m < 3; m++)
			for (unsigned char i = 0; i < 4; i++)
				for (unsigned char j = 0; j < 4; j++)
				{
					if (j == LOW_KEY || j == HIGH_KEY)
						dragbox[m][i][j][1] = keyboard_y0 + 5 + keyboard_h + i * 18;
					else
						dragbox[m][i][j][1] = keyboard_y0 + 5 + keyboard_h + i * 18 + 8;
				}
		// arps
		dragbox[0][4][LOW_KEY][1] = dragbox[0][3][LOW_KEY][1] + 20;
		dragbox[0][4][HIGH_KEY][1] = dragbox[0][4][LOW_KEY][1];
		dragbox[0][5][LOW_KEY][1] = dragbox[0][4][LOW_KEY][1] + 10;
		dragbox[0][5][HIGH_KEY][1] = dragbox[0][5][LOW_KEY][1];
		// links
		dragbox[0][6][LOW_KEY][1] = dragbox[0][5][LOW_KEY][1] + 10;
		dragbox[0][6][HIGH_KEY][1] = dragbox[0][6][LOW_KEY][1];
		dragbox[0][7][LOW_KEY][1] = dragbox[0][6][LOW_KEY][1] + 10;
		dragbox[0][7][HIGH_KEY][1] = dragbox[0][7][LOW_KEY][1];
		// x koordinaten
		for (unsigned char m = 0; m < 3; m++)
			for (unsigned char i = 0; i < 8; i++)
				set_range_values(m, i, 0, 0, 127, 0);
	}

	void set_range_values(unsigned char md, unsigned char layer, unsigned char low_k, unsigned char low_f,
			unsigned char high_k, unsigned char high_f);
	void set_transpose(char l1, char l2, char l3, char l4);
	void select_transpose_layer(char l);
	void set_mode(char m);
	void activate_key(char value, unsigned char key);
	void reset_active_keys();
};

/**
 * MiniPiano.
 * 2 octaves of key goodness
 */
class MiniPiano: public Fl_Box
{
	virtual int handle(int event);
	void draw();
	void draw_piano();
	void draw_highlights();
	void draw_case();
	void calc_hovered(int x, int y);
	void shift_octave(int);

	int keyboard_x0, keyboard_y0, keyboard_w, keyboard_h;
	float key_x, key_w, key_y;
	float h_white, w_white, h_black, w_black;
	float taste_x0[128][2];
	int octave;

	int hovered_key, play_hovered_key;
	int active_keys[128];
	int previous_hovered_key;
	int pushed;
	int push_x; // used for setting the key velocity
	int key_velocity;
	enum
	{
		NONE = -1, PIANO
	};
	enum
	{
		D_KEYS = 2, D_HIGHLIGHT = 4, D_CASE = 8
	};

public:
	MiniPiano(int x, int y, int w, int h, char* const label = 0) :
			Fl_Box(x, y, w, h, label)
	{
		pushed = NONE;
		hovered_key = NONE;
		previous_hovered_key = NONE;
		key_velocity = 100;
		for (int i = 0; i < 128; i++)
			active_keys[i] = 0;
		octave = 4;
	}
	void activate_key(int value, int key);
	void reset_active_keys();
};

// ###################
//
// ###################
class Pitch_Slider: public Fl_Slider
{
	int handle(int event);
	bool hold;
public:
	Pitch_Slider(int x, int y, int w, int h, char* const label = 0) :
			Fl_Slider(x, y, w, h, label)
	{
		hold = false;
	}
};

// ###################
//
// ###################
class Step_Type: public Fl_Group
{
	int handle(int event);
	int s;
public:
	Step_Type(int x, int y, int w, int h, char* const label = 0) :
			Fl_Group(x, y, w, h, label)
	{
		;
	}
	void set_step(int step);
	int p; // prev value
	int c; // current value
};

// ###################
//
// ###################
class Step_Value: public Fl_Value_Output
{
	virtual int format(char* buf);
	int handle(int event);
	int id;
	int s;
public:
	Step_Value(int x, int y, int w, int h, char const* label = 0) :
			Fl_Value_Output(x, y, w, h, label)
	{
		;
	}
	void set_id(int i, int step);
};

// ###################
//
// ###################
class Step_Offset: public Fl_Value_Slider
{
	int handle(int event);
	int s;
	char root;
protected:
	void draw(int X, int Y, int W, int H);
public:
	Step_Offset(int x, int y, int w, int h, char* const label = 0) :
			Fl_Value_Slider(x, y, w, h, label)
	{
		root = 0;
	}
	void draw();
	void set_step(int step);
	void set_root(char);
};

// ###################
//
// ###################
class Text_Display: public Fl_Text_Display
{
	virtual void resize(int X, int Y, int W, int H);
	int c_w;
public:
	Text_Display(int x, int y, int w, int h, char const* label = 0) :
			Fl_Text_Display(x, y, w, h, label)
	{
		textfont(FL_COURIER);
		textsize(12);
		fl_font(FL_COURIER, 12);
		c_w = fl_width("w");
		wrap_mode(1, w / c_w - 4);
	}
};
#endif /* WIDGETS_H_ */
/** @} */
