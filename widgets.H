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

#include "config.h"

/**
 * maps filter parameter values to index in selectors array.
 * also includes filter name and filter informations
 */
struct FilterMap
{
	int value;
	const char* name;
	const char* info;
	int _index;
};

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

/**
 * Double_Window class
 * just overwrites the default handler to support
 * key press+hold+release callbacks
 */
class Double_Window: public Fl_Double_Window
{
	int handle(int event);
	bool w__shown;
public:
	void showup();
	Double_Window(int w, int h, char* const label = 0) :
			Fl_Double_Window(w, h, label)
	{
		w__shown = false;
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
	static void dndcback(void *v)
	{
		DND_Box *dbox = (DND_Box*) v;
		dbox->dnd();
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
	bool selected;
	void draw();
	int handle(int event);
	void draw_scale(const int ox, const int oy, const int side);
	void draw_cursor(const int ox, const int oy, const int side);
	void dependency(int v) const;
public:
	enum Fl_Knobtype
	{
		DOTLIN = 0, DOTLOG_1, DOTLOG_2, DOTLOG_3, LINELIN, LINELOG_1, LINELOG_2, LINELOG_3
	};
	Fl_Knob(int xx, int yy, int ww, int hh, const char *l = 0);
	void type(int ty);
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
	struct ParaMap
	{
		int value;
		const char* name;
		int _index;
	} src[78];
	bool initialized;
	void init(int ec)
	{
		clear();
		for (int i = 0; i < 74 + ec; i++)
			src[i]._index = add(src[i].name);
		initialized = true;
	}
public:
	PCS_Choice(int x, int y, int w, int h, char* const label = 0) :
			Choice(x, y, w, h, label)
	{
		src[0].value = 0;
		src[0].name = "Off";

		src[1].value = 8;
		src[1].name = "Key/Key+";
		src[2].value = 9;
		src[2].name = "Key/Key~";
		src[3].value = 10;
		src[3].name = "Key/Vel+";
		src[4].value = 11;
		src[4].name = "Key/Vel~";
		src[5].value = 12;
		src[5].name = "Key/Vel<";
		src[6].value = 13;
		src[6].name = "Key/RlsVel";
		src[7].value = 14;
		src[7].name = "Key/Gate";
		src[8].value = 48;
		src[8].name = "Key/Glide";
		src[9].value = 4;
		src[9].name = "Key/XfdRand";
		src[10].value = 100;
		src[10].name = "Key/KeyRand 1";
		src[11].value = 101;
		src[11].name = "Key/KeyRand 2";

		src[12].value = 16;
		src[12].name = "Controller/PitchWhl";
		src[13].value = 17;
		src[13].name = "Controller/ModWhl";
		src[14].value = 18;
		src[14].name = "Controller/Pressure";
		src[15].value = 19;
		src[15].name = "Controller/Pedal";
		src[16].value = 22;
		src[16].name = "Controller/FootSw 1";
		src[17].value = 23;
		src[17].name = "Controller/FootSw 2";
		src[18].value = 38;
		src[18].name = "Controller/FootSw 3";
		src[19].value = 24;
		src[19].name = "Controller/FootSw 1FF";
		src[20].value = 25;
		src[20].name = "Controller/FootSw 2FF";
		src[21].value = 39;
		src[21].name = "Controller/FootSw 3FF";

		src[22].value = 72;
		src[22].name = "Envelope/VolEnv+";
		src[23].value = 73;
		src[23].name = "Envelope/VolEnv~";
		src[24].value = 74;
		src[24].name = "Envelope/VolEnv<";
		src[25].value = 80;
		src[25].name = "Envelope/FilEnv+";
		src[26].value = 81;
		src[26].name = "Envelope/FilEnv~";
		src[27].value = 82;
		src[27].name = "Envelope/FilEnv<";
		src[28].value = 88;
		src[28].name = "Envelope/AuxEnv+";
		src[29].value = 89;
		src[29].name = "Envelope/AuxEnv~";
		src[30].value = 90;
		src[30].name = "Envelope/AuxEnv<";

		src[31].value = 96;
		src[31].name = "LFO/LFO 1~";
		src[32].value = 97;
		src[32].name = "LFO/LFO 1+";
		src[33].value = 104;
		src[33].name = "LFO/LFO 2~";
		src[34].value = 105;
		src[34].name = "LFO/LFO 2+";

		src[35].value = 98;
		src[35].name = "Noise/White";
		src[36].value = 99;
		src[36].name = "Noise/Pink";

		src[37].value = 106;
		src[37].name = "Processor/Log0sum";
		src[38].value = 107;
		src[38].name = "Processor/Lag0";
		src[39].value = 108;
		src[39].name = "Processor/Log1sum";
		src[40].value = 109;
		src[40].name = "Processor/Lag1";
		src[41].value = 128;
		src[41].name = "Processor/PLagOut";
		src[42].value = 129;
		src[42].name = "Processor/PRampOut";
		src[43].value = 160;
		src[43].name = "Processor/DC";
		src[44].value = 161;
		src[44].name = "Processor/Sum";
		src[45].value = 162;
		src[45].name = "Processor/Switch";
		src[46].value = 163;
		src[46].name = "Processor/Abs";
		src[47].value = 164;
		src[47].name = "Processor/Diode";
		src[48].value = 165;
		src[48].name = "Processor/FlipFlop";
		src[49].value = 166;
		src[49].name = "Processor/Quantizer";
		src[50].value = 167;
		src[50].name = "Processor/Gain x 4";

		src[51].value = 144;
		src[51].name = "Clock Divisor/Double Whole";
		src[52].value = 145;
		src[52].name = "Clock Divisor/Whole";
		src[53].value = 146;
		src[53].name = "Clock Divisor/Half";
		src[54].value = 147;
		src[54].name = "Clock Divisor/Quarter";
		src[55].value = 148;
		src[55].name = "Clock Divisor/8th";
		src[56].value = 149;
		src[56].name = "Clock Divisor/16th";
		src[57].value = 150;
		src[57].name = "Clock Divisor/Octal";
		src[58].value = 151;
		src[58].name = "Clock Divisor/Quad";

		src[59].value = 26;
		src[59].name = "MIDI/MIDI Vol";
		src[60].value = 27;
		src[60].name = "MIDI/MIDI Pan";
		src[61].value = 28;
		src[61].name = "MIDI/MIDI Expr";
		src[62].value = 20;
		src[62].name = "MIDI/MIDI A";
		src[63].value = 21;
		src[63].name = "MIDI/MIDI B";
		src[64].value = 32;
		src[64].name = "MIDI/MIDI C";
		src[65].value = 33;
		src[65].name = "MIDI/MIDI D";
		src[66].value = 34;
		src[66].name = "MIDI/MIDI E";
		src[67].value = 35;
		src[67].name = "MIDI/MIDI F";
		src[68].value = 36;
		src[68].name = "MIDI/MIDI G";
		src[69].value = 37;
		src[69].name = "MIDI/MIDI H";
		src[70].value = 40;
		src[70].name = "MIDI/MIDI I";
		src[71].value = 41;
		src[71].name = "MIDI/MIDI J";
		src[72].value = 42;
		src[72].name = "MIDI/MIDI K";
		src[73].value = 43;
		src[73].name = "MIDI/MIDI L";
		// xtra controllers
		src[74].value = 44;
		src[74].name = "MIDI/MIDI M";
		src[75].value = 45;
		src[75].name = "MIDI/MIDI N";
		src[76].value = 46;
		src[76].name = "MIDI/MIDI O";
		src[77].value = 47;
		src[77].name = "MIDI/MIDI P";
		minimax[1] = 167;
		initialized = false;
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
	struct ParaMap
	{
		int value;
		const char* name;
		int _index;
	} dst[68];
public:
	PCD_Choice(int x, int y, int w, int h, char* const label = 0) :
			Choice(x, y, w, h, label)
	{
		dst[0].value = 0;
		dst[0].name = "Off";
		dst[1].value = 8;
		dst[1].name = "Voice/KeySust";
		dst[2].value = 47;
		dst[2].name = "Voice/FinePitch";
		dst[3].value = 48;
		dst[3].name = "Voice/Pitch";
		dst[4].value = 49;
		dst[4].name = "Voice/Glide";
		dst[5].value = 50;
		dst[5].name = "Voice/ChorusAmt";
		dst[6].value = 52;
		dst[6].name = "Voice/'SStart";
		dst[7].value = 53;
		dst[7].name = "Voice/SLoop";
		dst[8].value = 54;
		dst[8].name = "Voice/SRetrig";
		dst[9].value = 66;
		dst[9].name = "Voice/RT X-fade";
		dst[10].value = 56;
		dst[10].name = "Voice/FFrequency";
		dst[11].value = 57;
		dst[11].name = "Voice/F'Resonance";
		dst[12].value = 64;
		dst[12].name = "Voice/Amp Vol";
		dst[13].value = 65;
		dst[13].name = "Voice/Amp Pan";

		dst[14].value = 72;
		dst[14].name = "Envelope/VolEnvRts";
		dst[15].value = 73;
		dst[15].name = "Envelope/VolEnvAtk";
		dst[16].value = 74;
		dst[16].name = "Envelope/VolEnvDcy";
		dst[17].value = 75;
		dst[17].name = "Envelope/VolEnvRls";
		dst[18].value = 76;
		dst[18].name = "Envelope/VolEnvSus";
		dst[19].value = 80;
		dst[19].name = "Envelope/FilEnvRts";
		dst[20].value = 81;
		dst[20].name = "Envelope/FilEnvAtk";
		dst[21].value = 82;
		dst[21].name = "Envelope/FilEnvDcy";
		dst[22].value = 83;
		dst[22].name = "Envelope/FilEnvRls";
		dst[23].value = 84;
		dst[23].name = "Envelope/FilEnvSus";
		dst[24].value = 86;
		dst[24].name = "Envelope/FilEnvTrig";
		dst[25].value = 88;
		dst[25].name = "Envelope/AuxEnvRts";
		dst[26].value = 89;
		dst[26].name = "Envelope/AuxEnvAtk";
		dst[27].value = 90;
		dst[27].name = "Envelope/AuxEnvDcy";
		dst[28].value = 91;
		dst[28].name = "Envelope/AuxEnvRls";
		dst[29].value = 92;
		dst[29].name = "Envelope/AuxEnvSus";
		dst[30].value = 94;
		dst[30].name = "Envelope/AuxEnvTrig";

		dst[31].value = 96;
		dst[31].name = "LFO/LFO 1 Rate";
		dst[32].value = 97;
		dst[32].name = "LFO/LFO 1 Trig";
		dst[33].value = 104;
		dst[33].name = "LFO/LFO 2 Rate";
		dst[34].value = 105;
		dst[34].name = "LFO/LFO 2 Trig";

		dst[35].value = 106;
		dst[35].name = "Processor/Lag 0 in";
		dst[36].value = 108;
		dst[36].name = "Processor/Lag 1 in";
		dst[37].value = 161;
		dst[37].name = "Processor/Sum";
		dst[38].value = 162;
		dst[38].name = "Processor/Switch";
		dst[39].value = 163;
		dst[39].name = "Processor/Abs";
		dst[40].value = 164;
		dst[40].name = "Processor/Diode";
		dst[41].value = 165;
		dst[41].name = "Processor/FlipFlop";
		dst[42].value = 166;
		dst[42].name = "Processor/Quantize";
		dst[43].value = 167;
		dst[43].name = "Processor/Gain x 4";

		dst[44].value = 168;
		dst[44].name = "Attenuator/C 01 Amt";
		dst[45].value = 169;
		dst[45].name = "Attenuator/C 02 Amt";
		dst[46].value = 170;
		dst[46].name = "Attenuator/C 03 Amt";
		dst[47].value = 171;
		dst[47].name = "Attenuator/C 04 Amt";
		dst[48].value = 172;
		dst[48].name = "Attenuator/C 05 Amt";
		dst[49].value = 173;
		dst[49].name = "Attenuator/C 06 Amt";
		dst[50].value = 174;
		dst[50].name = "Attenuator/C 07 Amt";
		dst[51].value = 175;
		dst[51].name = "Attenuator/C 08 Amt";
		dst[52].value = 176;
		dst[52].name = "Attenuator/C 09 Amt";
		dst[53].value = 177;
		dst[53].name = "Attenuator/C 10 Amt";
		dst[54].value = 178;
		dst[54].name = "Attenuator/C 11 Amt";
		dst[55].value = 179;
		dst[55].name = "Attenuator/C 12 Amt";
		dst[56].value = 180;
		dst[56].name = "Attenuator/C 13 Amt";
		dst[57].value = 181;
		dst[57].name = "Attenuator/C 14 Amt";
		dst[58].value = 182;
		dst[58].name = "Attenuator/C 15 Amt";
		dst[59].value = 183;
		dst[59].name = "Attenuator/C 16 Amt";
		dst[60].value = 184;
		dst[60].name = "Attenuator/C 17 Amt";
		dst[61].value = 185;
		dst[61].name = "Attenuator/C 18 Amt";
		dst[62].value = 186;
		dst[62].name = "Attenuator/C 19 Amt";
		dst[63].value = 187;
		dst[63].name = "Attenuator/C 20 Amt";
		dst[64].value = 188;
		dst[64].name = "Attenuator/C 21 Amt";
		dst[65].value = 189;
		dst[65].name = "Attenuator/C 22 Amt";
		dst[66].value = 190;
		dst[66].name = "Attenuator/C 23 Amt";
		dst[67].value = 191;
		dst[67].name = "Attenuator/C 24 Amt";
		for (int i = 0; i < 68; i++)
			dst[i]._index = add(dst[i].name);
		minimax[1] = 191;
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
	struct ParaMap
	{
		int value;
		const char* name;
		int _index;
	} dst[28];
public:
	PPCD_Choice(int x, int y, int w, int h, char* const label = 0) :
			Choice(x, y, w, h, label)
	{
		dst[0].value = 0;
		dst[0].name = "Off";

		dst[1].value = 1;
		dst[1].name = "Effects/FX A Send 1";
		dst[2].value = 2;
		dst[2].name = "Effects/FX A Send 2";
		dst[3].value = 3;
		dst[3].name = "Effects/FX A Send 3";
		dst[4].value = 4;
		dst[4].name = "Effects/FX A Send 4";
		dst[5].value = 5;
		dst[5].name = "Effects/FX B Send 1";
		dst[6].value = 6;
		dst[6].name = "Effects/FX B Send 2";
		dst[7].value = 7;
		dst[7].name = "Effects/FX B Send 3";
		dst[8].value = 8;
		dst[8].name = "Effects/FX B Send 4";

		dst[9].value = 96;
		dst[9].name = "Arpeggiator/Arp Rate";
		dst[10].value = 97;
		dst[10].name = "Arpeggiator/Arp Ext.";
		dst[11].value = 98;
		dst[11].name = "Arpeggiator/Arp Vel";
		dst[12].value = 99;
		dst[12].name = "Arpeggiator/Arp Gate";
		dst[13].value = 100;
		dst[13].name = "Arpeggiator/Arp Intvl";

		dst[14].value = 112;
		dst[14].name = "Beats/BeatsVelG1";
		dst[15].value = 113;
		dst[15].name = "Beats/BeatsVelG2";
		dst[16].value = 114;
		dst[16].name = "Beats/BeatsVelG3";
		dst[17].value = 115;
		dst[17].name = "Beats/BeatsVelG4";
		dst[18].value = 116;
		dst[18].name = "Beats/BeatsXpsG1";
		dst[19].value = 117;
		dst[19].name = "Beats/BeatsXpsG2";
		dst[20].value = 118;
		dst[20].name = "Beats/BeatsXpsG3";
		dst[21].value = 119;
		dst[21].name = "Beats/BeatsXpsG4";
		dst[22].value = 120;
		dst[22].name = "Beats/BeatsBusy";
		dst[23].value = 121;
		dst[23].name = "Beats/BeatsVari";

		dst[24].value = 128;
		dst[24].name = "Preset/PLag In";
		dst[25].value = 129;
		dst[25].name = "Preset/PLag Amt";
		dst[26].value = 130;
		dst[26].name = "Preset/PRamp In";
		dst[27].value = 131;
		dst[27].name = "Preset/PRamp Rt";
		for (int i = 0; i < 28; i++)
			dst[i]._index = add(dst[i].name);
		minimax[1] = 131;
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
		int mode, repeat;
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
	void draw_envelope(int type, int x0, int y0);
	void copy_envelope(int src, int dst);
	void set_shape(int dst, int shape);
	int layer;
	int ee_x0;
	int ee_y0;
	int ee_w;
	int ee_h;
	int mode_button[5]; // x0 of the mode buttons (width = 75, h = 20)
	int copy_button[6];
	int shape_button[4];
	int button_hover;
	int zoomlevel;
	int dragbox[6][2];
	int hover;
	int hover_list; // 1, 2, 4, 8, 16, 32
	int push_x;
	int push_y;
	int mode;
	int modes;
	bool overlay;
	bool button_push;
	int bg;

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
	void set_data(int type, int* stages, int mode, int repeat);
	void set_layer(int l);
	void sync_view(int l, int m = 0, float z = .0, bool o = false);
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
	int mode; // 0 = keyrange, 1 = velocity, 2 = realtime
	int modes;
	int keyboard_x0, keyboard_y0, keyboard_w, keyboard_h;
	int h_white, w_white, h_black, w_black;
	int taste_x0[128][2];
	int dragbox[3][8][4][2]; // mode, layer, type, x/y
	int highlight_dragbox[8][4];
	int prev_key_value[3][8][4];
	int new_key_value[3][8][4];
	int pushed; // currently dragged part
	int pushed_range; // currently dragged range (low_key, low_fade...)
	int hovered_key, play_hovered_key;
	int active_keys[128];
	int previous_hovered_key;
	int selected_transpose_layer;
	int transpose[4];
	int push_x; // used for setting the key velocity
	int key_velocity;

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
		keyboard_y0 = this->y() + 10;
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
		for (int m = 0; m < 3; m++)
			for (int i = 0; i < 4; i++)
				for (int j = 0; j < 4; j++)
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
		for (int m = 0; m < 3; m++)
			for (int i = 0; i < 8; i++)
				set_range_values(m, i, 0, 0, 127, 0);
	}

	void set_range_values(int md, int layer, int low_k, int low_f, int high_k, int high_f);
	void set_transpose(int l1, int l2, int l3, int l4);
	void select_transpose_layer(int l);
	void set_mode(int m);
	void activate_key(int value, int key);
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
protected:
	void draw(int X, int Y, int W, int H);
public:
	Step_Offset(int x, int y, int w, int h, char* const label = 0) :
			Fl_Value_Slider(x, y, w, h, label)
	{
		;
	}
	void draw();
	void set_step(int step);
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
