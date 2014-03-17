/*
 prodatum: E-MU Proteus family remote and preset editor
 Copyright 2011-2014 Jan Eidtmann

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#ifdef WIN32
#include <stdio.h>
#else
#include <unistd.h>
#endif

#include "ui.H"
#include <FL/filename.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Color_Chooser.H>

static void load_data();

const char* VERSION = "2.0rc22";
PD_UI* ui = 0;
MIDI* midi = 0;
PXK* pxk = 0;
extern Cfg* cfg;
extern PD_Arp_Step* arp_step[32];

static bool __auto_connect = true;
static int __device = -1;

/**
 * command line option parser
 */
int options(int argc, char **argv, int &i)
{
	if (argv[i][1] == 'd')
	{
		if (i + 1 >= argc)
			return 0;
		__device = atoi(argv[i + 1]);
		if (__device < 0 || __device > 126)
			__device = -1;
		i += 2;
		return 2;
	}
	if (argv[i][1] == 'a')
	{
		__auto_connect = false;
		i++;
		return 1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	Fl::scheme("gleam");
	// command line options
	int i = 1;
	if (!Fl::args(argc, argv, i, options))
	{
		printf("prodatum %s options:\n"
				" -d id\tConfig (Device ID) to load (default: last used ID)\n"
				" -a   \tdo not open device at startup\n", VERSION);
		return 1;
	}
	// loading... please wait.
	load_data();
	// create user interface
	ui = new PD_UI();
	if (!ui)
		return 1;
	cfg = new Cfg(__device);
	if (!cfg)
		return 2;
	cfg->apply();
#ifndef SYNCLOG
	ui->init_log_b->hide();
	ui->init_log_m->hide();
#endif
	char label[17];
	snprintf(label, 17, "prodatum %s", VERSION);
	ui->main_window->label(label);
	ui->main_window->free_position();
	ui->main_window->showup();
	Fl::check();
	pxk = new PXK();
	if (!pxk)
		return 3;
	midi = new MIDI();
	if (!midi)
		return 4;
	pxk->Boot(__auto_connect, cfg->get_cfg_option(CFG_DEVICE_ID));
	return Fl::run();
}

/**
 * "view"/layer selector of the main window.
 * hides/shows widgets depending on the selection
 * @param l the view/layer to show
 */
void PD_UI::select(int l)
{
	//pmesg("PD_UI::select(%d)\n", l);
	static int prev = 0;
	if (l == selected)
	{
		if (l == 5 && b_links->value())
		{
			b_links->value(0);
			b_links->do_callback();
			return;
		}
		if (l == 0)
		{
			if (b_master->value())
			{
				b_master->value(0);
				b_master->do_callback();
				return;
			}
		}
		l = prev;
	}
	((Fl_Button*) selector->child(l))->setonly();
	Fl::flush();
	prev = selected;
	if (l == 5)
	{
		preset_editor->show();
		g_pfx->show();
		if (prev > 0)
			layer_editor[prev - 1]->hide();
		else
		{
			if (b_master->value())
				g_multisetups->hide();
			main->hide();
		}
	}
	else if (l == 0)
	{
		main->show();
		if (b_master->value())
		{
			g_multisetups->show();
			g_pfx->hide();
		}
		else
		{
			g_pfx->show();
			g_multisetups->hide();
		}
		if (prev < 5 && prev > 0)
			layer_editor[prev - 1]->hide();
		else
			preset_editor->hide();
	}
	else
	{
		layer_editor[l - 1]->show();
		if (prev == 0)
		{
			main->hide();
			if (b_master->value())
				g_multisetups->hide();
			else
				g_pfx->hide();
		}
		else if (prev == 5)
		{
			preset_editor->hide();
			g_pfx->hide();
		}
		else
			layer_editor[prev - 1]->hide();
	}
	selected = l;
}

void PD_UI::set_color(Fl_Color t)
{
	unsigned char r, g, b;
	Fl::get_color(t, r, g, b);
	if (!fl_color_chooser("New color:", r, g, b))
		return;
	Fl::set_color(t, r, g, b);
	if (t == FL_FOREGROUND_COLOR)
		Fl_Tooltip::textcolor(t);
	else if (t == FL_BACKGROUND2_COLOR)
		Fl_Tooltip::color(FL_BACKGROUND2_COLOR);
	Fl::reload_scheme();
	switch ((int) t)
	{
		case (int) FL_BACKGROUND_COLOR:
			cfg->set_cfg_option(CFG_BGR, (int) r);
			cfg->set_cfg_option(CFG_BGG, (int) g);
			cfg->set_cfg_option(CFG_BGB, (int) b);
			break;
		case (int) FL_BACKGROUND2_COLOR:
			cfg->set_cfg_option(CFG_BG2R, (int) r);
			cfg->set_cfg_option(CFG_BG2G, (int) g);
			cfg->set_cfg_option(CFG_BG2B, (int) b);
			break;
		case (int) FL_FOREGROUND_COLOR:
			cfg->set_cfg_option(CFG_FGR, (int) r);
			cfg->set_cfg_option(CFG_FGG, (int) g);
			cfg->set_cfg_option(CFG_FGB, (int) b);
			break;
		case (int) FL_SELECTION_COLOR:
			cfg->set_cfg_option(CFG_SLR, (int) r);
			cfg->set_cfg_option(CFG_SLG, (int) g);
			cfg->set_cfg_option(CFG_SLB, (int) b);
			break;
		case (int) FL_INACTIVE_COLOR:
			cfg->set_cfg_option(CFG_INR, (int) r);
			cfg->set_cfg_option(CFG_ING, (int) g);
			cfg->set_cfg_option(CFG_INB, (int) b);
			break;
	}
}

void PD_UI::set_color(Fl_Color t, unsigned char r, unsigned char g, unsigned char b)
{
	Fl::set_color(t, r, g, b);
	if (t == FL_FOREGROUND_COLOR)
		Fl_Tooltip::textcolor(t);
	else if (t == FL_BACKGROUND2_COLOR)
		Fl_Tooltip::color(FL_BACKGROUND2_COLOR);
	Fl::reload_scheme();
}

void PD_UI::set_default_colors()
{
	for (unsigned char i = CFG_BGR; i <= CFG_KNOB_COLOR2; i++)
		cfg->getset_default(i);
	cfg->apply(true);
	Fl::reload_scheme();
}

extern char c_knob_1;
extern char c_knob_2;
void PD_UI::set_knobcolor(char type, char color)
{
	char* c = 0;
	Fl_Group* g = 0;
	if (type == 0)
	{
		g = ui->knob_color_a;
		c = &c_knob_1;
	}
	else
	{
		g = ui->knob_color_b;
		c = &c_knob_2;
	}
	switch ((int) color)
	{
		case 0:
			*c = FL_BACKGROUND_COLOR;
			((Fl_Button*) g->child(0))->setonly();
			break;
		case 1:
			*c = FL_INACTIVE_COLOR;
			((Fl_Button*) g->child(1))->setonly();
			break;
		default:
		case 2:
			*c = FL_BACKGROUND2_COLOR;
			((Fl_Button*) g->child(2))->setonly();
			break;
		case 3:
			*c = FL_FOREGROUND_COLOR;
			((Fl_Button*) g->child(3))->setonly();
			break;
		case 4:
			*c = FL_SELECTION_COLOR;
			((Fl_Button*) g->child(4))->setonly();
			break;
	}
	Fl::reload_scheme();
}

/**
 * gets selected view/layer
 * @return selected view/layer
 */
int PD_UI::get_selected()
{
	return selected;
}

static const char* tip_end =
		"End: This command signals the end of the pattern. Any steps programmed after the step containing the End command are ignored.";
static const char* tip_skip =
		"Skip: This command simply removes the step from the pattern. The Skip feature makes it easy to remove an unwanted step without rearranging the entire pattern. You'll be happy to know that the velocity, duration and repeat parameters are remembered if you decide to put the step back later.";
static const char* tip_rest =
		"Rest: Instead of playing a note, you can define the step as a Rest. The Duration parameter specifies the length of the rest. Rests can be tied together to form longer rests.";
static const char* tip_tie =
		"Tie: This function extends the duration of notes beyond the values given in the duration field by \"tying\" notes together. You can tie together any number of consecutive steps. IMPORTANT: The Gate function in the arpeggiator MUST be set to 100% when using the tie function, otherwise the tied note is retriggered instead of extended.";
static const char* tip_key = "Key: Play a note.";
static const char* tip_duration =
		"Duration: Sets the length of time for the current step, defined as a note value, based on the Master Tempo.";
static const char* tip_repeat = "Repeat: Each step can be played from 1 to 32 times.";
static const char* tip_velocity =
		"Velocity: Each note in the pattern plays using either a preset velocity value (from 1 through 127), or using the actual velocity of the played note (0).";
static const char* tip_offset = "Key Offset: Offset from the original note played (semitones).";
void PD_Arp_Step::init(int s)
{
	step = s;
	op->set_step(s);
	step_offset->set_step(s);
	step_velocity->set_id(785, s);
	step_duration->set_id(786, s);
	step_duration->value(4.);
	step_repeat->set_id(787, s);
	arp_step[s] = this;
	char buf[3];
	snprintf(buf, 3, "%d", s + 1);
	step_offset->copy_label(buf);
	// tooltips
	step_end->tooltip(tip_end);
	step_skip->tooltip(tip_skip);
	step_rest->tooltip(tip_rest);
	step_tie->tooltip(tip_tie);
	step_key->tooltip(tip_key);
	step_duration->tooltip(tip_duration);
	step_repeat->tooltip(tip_repeat);
	step_velocity->tooltip(tip_velocity);
	step_offset->tooltip(tip_offset);
}

void PD_Arp_Step::edit_value(int id, int value)
{
//	pmesg("PD_Arp_Step::edit_value(%d, %d)\n", id, value);
	if (!pxk->arp)
		return;
	if (ui->selected_step != step)
	{
		midi->edit_parameter_value(770, step);
		ui->selected_step = step;
	}
	midi->edit_parameter_value(id, value);
	if (id != 785)
		pxk->arp->update_sequence_length_information();
}

void PD_Arp_Step::set_values(int off, int vel, int dur, int rep)
{
//	pmesg("PD_Arp_Step::set_values(%d, %d, %d, %d)\n", off, vel, dur, rep);
	if (off > -49)
	{
		if (off < 49)
		{
			step_offset->value((double) off);
			((Fl_Button*) op->child(4))->setonly();
			op->c = 4;
			op->p = 0;
		}
	}
	else
	{
		if (off > -53)
		{
			off += 52;
			((Fl_Button*) op->child(off))->setonly();
			op->c = off;
			op->p = 4;
			step_offset->value((double) 0);
		}
	}
	step_velocity->value((double) vel);
	step_duration->value((double) dur);
	step_repeat->value((double) rep + 1);
}

void PD_UI::edit_arp_x(int x)
{
	pmesg("PD_UI::edit_arp_x(%d)\n", x);
	if (x == 0 || x == -1) // preset arp
	{
		if (pxk->arp && pxk->arp->get_number() == preset_editor->arp->value() - 1)
			ui->arp_editor_w->showup();
		else
			midi->request_arp_dump(preset_editor->arp->value() - 1, 0);
	}
	else if (x == 1) // master arp
	{
		if (pxk->arp && pxk->arp->get_number() == main->arp->value() - 1)
			ui->arp_editor_w->showup();
		else
			midi->request_arp_dump(main->arp->value() - 1, 0);
	}
}

/**
 * switches edit-all-alyers on and off
 * @param v if 0, switch edit all layer off, else on
 */
void PD_UI::set_eall(int v)
{
	//pmesg("PD_UI::set_eall(%d)\n", v);
	if (v && !eall)
	{
		pxk->widget_callback(269, 1); // enable edit all layers
		midi->edit_parameter_value(898, -1); // select all layers
		pxk->selected_layer = 0;
		// update UI
		m_voice2->hide();
		m_voice3->hide();
		m_voice4->hide();
		((Fl_Button*) selector->array()[2])->deactivate();
		((Fl_Button*) selector->array()[3])->deactivate();
		((Fl_Button*) selector->array()[4])->deactivate();
		for (int i = 1; i < 4; i++)
		{
			mute_b[i]->deactivate();
			solo_b[i]->deactivate();
		}
		main->layer_strip[0]->copy->deactivate();
		if (main->layer_strip[1]->active())
			main->layer_strip[1]->deactivate();
		if (main->layer_strip[2]->active())
			main->layer_strip[2]->deactivate();
		if (main->layer_strip[3]->active())
			main->layer_strip[3]->deactivate();
		layer_editor[0]->copy_env->deactivate();
		layer_editor[0]->copy_lfo->deactivate();
		layer_editor[0]->copy_pc->deactivate();
		layer_editor[1]->deactivate();
		layer_editor[2]->deactivate();
		layer_editor[3]->deactivate();
		// disconnect and reset value in-/output
		pwid_editing = 0;
		ui->value_input->minimum(0.);
		ui->value_input->maximum(0.);
		ui->value_input->value(0.);
		ui->forma_out->set_value(915, 0, 0); // just anything
		b_eall->set();
		if (!m_eall->value())
			m_eall->set();
		if (piano_w->shown())
			piano->redraw();
		eall = true;
		pxk->display_status("Edit all layers enabled.");
	}
	if (!v && eall)
	{
		pxk->widget_callback(269, 0);
		midi->edit_parameter_value(898, 0);
		m_voice2->show();
		m_voice3->show();
		m_voice4->show();
		((Fl_Button*) selector->array()[2])->activate();
		((Fl_Button*) selector->array()[3])->activate();
		((Fl_Button*) selector->array()[4])->activate();
		for (int i = 1; i < 4; i++)
		{
			mute_b[i]->activate();
			solo_b[i]->activate();
		}
		main->layer_strip[0]->copy->activate();
		layer_editor[0]->copy_env->activate();
		layer_editor[0]->copy_lfo->activate();
		layer_editor[0]->copy_pc->activate();
		layer_editor[1]->activate();
		layer_editor[2]->activate();
		layer_editor[3]->activate();
		b_eall->clear();
		m_eall->clear();
		eall = false;
		pxk->display_status("Edit all layers disabled.");
	}
}

/**
 * sets various details for the layer copy window and shows it
 * @param type what to copy (patchcords, layer, lfos...)
 * @param src_layer source layer
 * @see copy_layer
 */
void PD_UI::show_copy_layer(int type, int src_layer)
{
	pmesg("PD_UI::show_copy_layer(%d, %d)\n", type, src_layer);
	if (!pxk->preset)
		return;
	// reset buttons
	for (int i = 0; i < layer_dst->children(); i++)
	{
		((Fl_Button*) layer_dst->array()[i])->activate();
		((Fl_Button*) layer_dst->array()[i])->value(0);
	}
	// deactivate src layer
	((Fl_Button*) layer_dst->array()[src_layer])->deactivate();
	switch (type)
	{
		case C_LAYER:
			copy_layer->label("Copy Voice");
			break;
		case C_LAYER_COMMON:
			copy_layer->label("Copy Voice Common");
			break;
		case C_LAYER_FILTER:
			copy_layer->label("Copy Filter");
			break;
		case C_LAYER_LFO:
			copy_layer->label("Copy LFO's");
			break;
		case C_LAYER_ENVELOPE:
			copy_layer->label("Copy Envelopes");
			break;
		case C_LAYER_PATCHCORD:
			copy_layer->label("Copy Patchcords");
			break;
		default:
			return;
	}
	copy_type = type;
	copy_src = src_layer;
	copy_layer->showup();
}

/**
 * sets various details for the copy window and shows it
 * @param type what to copy (preset, arp, ..)
 * @see copy_preset
 */
void PD_UI::show_copy_preset(int type)
{
	pmesg("PD_UI::show_copy_preset(%d)\n", type);
	if (!pxk->preset)
	{
		pxk->display_status("*** Nothing to save or copy.");
		return;
	}
	switch (type)
	{
		case SAVE_PRESET:
			copy_preset->label("Save Program");
			g_copy_preset->label("TARGET");
			copy_arp_rom->set_value(0);
			copy_browser->set_id(type);
			g_copy_preset->show();
			g_copy_arp_pattern->hide();
			copy_arp_rom->deactivate();
			break;
		case C_PRESET:
			copy_preset->label("Copy Program");
			g_copy_preset->label("TARGET");
			copy_arp_rom->set_value(0);
			copy_browser->set_id(type);
			g_copy_preset->show();
			g_copy_arp_pattern->hide();
			copy_arp_rom->deactivate();
			break;
		case C_ARP:
			copy_preset->label("Copy Arp Settings");
			g_copy_preset->label("SOURCE");
			copy_browser->set_id(type);
			g_copy_preset->show();
			g_copy_arp_pattern->hide();
			copy_arp_rom->activate();
			break;
		case C_ARP_PATTERN:
			copy_preset->label("Copy Arp Pattern");
			copy_arp_pattern_browser->set_id(type);
			g_copy_arp_pattern->show();
			g_copy_preset->hide();
			break;
	}
	copy_preset->showup();
}

/**
 * initializes voice strips.
 * sets id and layer information for all widgets in the voice strips
 * @param l layer number of this strip (0-3)
 */
void PD_UI::create_about()
{
	//pmesg("PD_UI::create_about()\n");
	const char* OS;
#if defined(OSX)
	OS = "Mac OS X";
#elif defined(WIN32)
	OS = "Microsoft Windows";
#else
	OS = "GNU/Linux";
#endif
	char buf[512];
	snprintf(buf, 512, "prodatum %s\nfor %s", VERSION, OS);
	about_text->copy_label(buf);
}

/**
 * loads various data that needs to be initialized early
 */
FilterMap FM[51];
const char* rates[25];
Patchcord PatchS[78];
Patchcord PatchD[68];
Patchcord PresetPatchD[28];
const char* filter_tooltip;
static void load_data()
{
	//pmesg("load_data()\n");
	filter_tooltip = "* matches any sequence of 0 or more characters.\n"
			"? matches any single character.\n"
			"[set] matches any character in the set.\n"
			"[^set] or [!set] matches any character not in the set.\n"
			"{X|Y|Z} or {X,Y,Z} matches any one of the subexpressions literally.\n"
			"\\x quotes the character x so it has no special meaning.\n"
			"x all other characters must be matched exactly.\n"
			"NOTE: Your query is automatically expanded to '*query*'";
	rates[0] = "8/1   octal whole";
	rates[1] = "4/1d  dotted quad whole";
	rates[2] = "8/1t  octal whole triplet";
	rates[3] = "4/1   quad whole";
	rates[4] = "2/1d  dotted double whole";
	rates[5] = "4/1t  quad whole triplet";
	rates[6] = "2/1   double whole";
	rates[7] = "1/1d  dotted whole";
	rates[8] = "2/1t  double triplet";
	rates[9] = "1/1   whole note";
	rates[10] = "1/2d  dotted half";
	rates[11] = "1/1t  whole triplet";
	rates[12] = "1/2   half";
	rates[13] = "1/4d  dotted quarter";
	rates[14] = "1/2t  half triplet";
	rates[15] = "1/4   quarter";
	rates[16] = "1/8d  dotted 8th";
	rates[17] = "1/4t  quarter triplet";
	rates[18] = "1/8   8th";
	rates[19] = "1/16d dotted 16th";
	rates[20] = "1/8t  8th triplet";
	rates[21] = "1/16  16th";
	rates[22] = "1/32d dotted 32th";
	rates[23] = "1/16t 16th triplet";
	rates[24] = "1/32  32nd";
	// initialize filter map
	FM[0].id = 127;
	FM[0].name = "Off";
	FM[0].info = "Unfiltered sound";
	FM[1].id = 138;
	FM[1].name = "FuzziFace   12 DST";
	FM[1].info = "Nasty clipped distortion.\nQ functions as mid-frequency tone\ncontrol.";
	FM[2].id = 162;
	FM[2].name = "EarBender   12 WAH";
	FM[2].info = "Midway between wah & vowel.\nStrong mid-boost.\nNasty at high Q settings.";
	FM[3].id = 163;
	FM[3].name = "KlangKling  12 SFX";
	FM[3].info = "Ringing Flange filter.\nQ \"tunes\" the ring frequency.";
	FM[4].id = 1;
	FM[4].name = "Lowpass/Smooth       2 LPF";
	FM[4].info = "Typical OB type low-pass filter\nwith a shallow 12 dB/octave slope.";
	FM[5].id = 0;
	FM[5].name = "Lowpass/Classic      4 LPF";
	FM[5].info = "4-pole low-pass filter,\nthe standard filter on classic\nanalog synths.\n24 dB/octave rolloff.";
	FM[6].id = 2;
	FM[6].name = "Lowpass/Steeper      6 LPF";
	FM[6].info =
			"6-pole low-pass filter which has a\nsteeper slope than a 4-pole low-\npass filter.\n36 dB/octave rolloff!";
	FM[7].id = 132;
	FM[7].name = "Lowpass/MegaSweepz  12 LPF";
	FM[7].info = "\"Loud\" LPF with a hard Q.\nTweeters beware!";
	FM[8].id = 133;
	FM[8].name = "Lowpass/EarlyRizer  12 LPF";
	FM[8].info = "Classic analog sweeping\nwith hot Q and Lo-end.";
	FM[9].id = 136;
	FM[9].name = "Lowpass/KlubKlassi  12 LPF";
	FM[9].info = "Responsive low-pass filter sweep\nwith a wide spectrum of Q sounds.";
	FM[10].id = 137;
	FM[10].name = "Lowpass/BassBox-303 12 LPF";
	FM[10].info = "Pumped up lows with\nTB-like squelchy Q factor.";
	FM[11].id = 134;
	FM[11].name = "Lowpass/Millennium  12 LPF";
	FM[11].info = "Aggressive low-pass filter.\nQ gives you a variety of\nspiky tonal peaks.";
	FM[12].id = 8;
	FM[12].name = "Highpass/Shallow      2 HPF";
	FM[12].info = "2-pole high-pass filter.\n12 dB/octave slope.";
	FM[13].id = 9;
	FM[13].name = "Highpass/Deeper       4 HPF";
	FM[13].info = "Classic 4-pole high-pass filter.\nCutoff sweep progressively cuts\n4th Order High-pass.";
	FM[14].id = 16;
	FM[14].name = "Bandpass/Band-pass1   2 BPF";
	FM[14].info = "Band-pass filter with 6 dB/octave\nrolloff on either side of the\npassband and Q control.";
	FM[15].id = 17;
	FM[15].name = "Bandpass/Band-pass2   4 BPF";
	FM[15].info = "Band-pass filter with 12 dB/octave\nrolloff on either side of the\npassband and Q control.";
	FM[16].id = 18;
	FM[16].name = "Bandpass/ContraBand   6 BPF";
	FM[16].info = "A novel band-pass filter where the\nfrequency peaks and dips midway\nin the frequency range.";
	FM[17].id = 32;
	FM[17].name = "EQ/Swept1>oct   6 EQ+";
	FM[17].info = "Parametric filter with 24 dB of\nboost or cut and a one octave\nbandwidth.";
	FM[18].id = 33;
	FM[18].name = "EQ/Swept2>1oct  6 EQ+";
	FM[18].info =
			"Parametric filter with 24 dB of\nboost or cut. The bandwidth of the\nfilter is two octaves wide at the\nlow end of the audio spectrum,\ngradually changing to one octave\nwide at the upper end of the\nspectrum.";
	FM[19].id = 34;
	FM[19].name = "EQ/Swept3>1oct  6 EQ+";
	FM[19].info =
			"Parametric filter with 24 dB of\nboost or cut. The bandwidth of the\nfilter is three octaves wide at the\nlow end of the audio spectrum,\ngradually changing to one octave\nwide at the upper end of the\nspectrum.";
	FM[20].id = 140;
	FM[20].name = "EQ/TB-OrNot-TB 12 EQ+";
	FM[20].info = "Great Bassline \"Processor.\"";
	FM[21].id = 142;
	FM[21].name = "EQ/RolandBass  12 EQ+";
	FM[21].info = "Constant bass boost\nwith mid-tone Q control.";
	FM[22].id = 147;
	FM[22].name = "EQ/BassTracer  12 EQ+";
	FM[22].info = "Low Q boosts bass.\nTry sawtooth or square waveform\nwith Q set to 115.";
	FM[23].id = 148;
	FM[23].name = "EQ/RogueHertz  12 EQ+";
	FM[23].info = "Bass with mid-range boost and\nsmooth Q. Sweep cutoff with Q at\n127.";
	FM[24].id = 146;
	FM[24].name = "EQ/DJAlkaline  12 EQ+";
	FM[24].info = "Band accentuating filter,\nQ shifts \"ring\" frequency.";
	FM[25].id = 131;
	FM[25].name = "EQ/AceOfBass   12 EQ+";
	FM[25].info = "Bass-boost to bass-cut morph.";
	FM[26].id = 149;
	FM[26].name = "EQ/RazorBlades 12 EQ-";
	FM[26].info = "Cuts a series of frequency bands.\nQ selects different bands.";
	FM[27].id = 150;
	FM[27].name = "EQ/RadioCraze  12 EQ-";
	FM[27].info = "Band limited for a cheap\nradio-like EQ.";
	FM[28].id = 64;
	FM[28].name = "Phaser/PhazeShift1  6 PHA";
	FM[28].info =
			"Recreates a comb filter effect\ntypical of phase shifters. Freq.\n moves position of notches.\nQ varies the depth of the notches.";
	FM[29].id = 65;
	FM[29].name = "Phaser/PhazeShift2  6 PHA";
	FM[29].info =
			"Comb filter with slightly different\nnotch frequency moving the\nfrequency of notches. Q varies\nthe depth of the notches.";
	FM[30].id = 66;
	FM[30].name = "Phaser/BlissBlatz   6 PHA";
	FM[30].info = "Bat phaser from the Emulator 4.";
	FM[31].id = 154;
	FM[31].name = "Phaser/FreakShifta 12 PHA";
	FM[31].info = "Phasey movement.\nTry major 6 interval\nand maximum Q.";
	FM[32].id = 155;
	FM[32].name = "Phaser/CruzPusher  12 PHA";
	FM[32].info = "Accentuates harmonics at high Q.\nTry with a sawtooth LFO.";
	FM[33].id = 72;
	FM[33].name = "Flanger/FlangerLite  6 FLG";
	FM[33].info =
			"Contains three notches.\nFrequency moves frequency and\nspacing of notches.\nQ increases flanging depth.";
	FM[34].id = 156;
	FM[34].name = "Flanger/AngelzHairz 12 FLG";
	FM[34].info = "Smooth sweep flanger.\nGood with vox waves.\neg. I094, Q = 60.";
	FM[35].id = 157;
	FM[35].name = "Flanger/DreamWeava  12 FLG";
	FM[35].info = "Directional Flanger.\nPoles shift down at low Q\nand up at high Q.";
	FM[36].id = 80;
	FM[36].name = "Vowel/Aah-Ay-Eeh   6 VOW";
	FM[36].info =
			"Vowel formant filter which sweeps\nfrom \"Ah\" sound, through \"Ay\"\nsound to \"Ee\" sound at maximum\nfrequency setting. Q varies the\napparent size of the mouth\ncavity.";
	FM[37].id = 81;
	FM[37].name = "Vowel/Ooh-To-Aah   6 VOW";
	FM[37].info =
			"Vowel formant filter which sweeps\nfrom \"Oo\" sound, through \"Oh\"\nsound to \"Ah\" sound at maximum\nfrequency setting. Q varies the\napparent size of mouth\ncavity.";
	FM[38].id = 141;
	FM[38].name = "Vowel/Ooh-To-Eee  12 VOW";
	FM[38].info = "Oooh to Eeee formant morph.";
	FM[39].id = 143;
	FM[39].name = "Vowel/MultiQVox   12 VOW";
	FM[39].info = "Multi-Formant,\nMap Q To velocity.";
	FM[40].id = 144;
	FM[40].name = "Vowel/TalkingHedz 12 VOW";
	FM[40].info = "\"Oui\" morphing filter.\nQ adds peaks.";
	FM[41].id = 151;
	FM[41].name = "Vowel/Eeh-To-Aah  12 VOW";
	FM[41].info = "\"E\" to \"Ah\" formant movement.\nQ accentuates \"peakiness.\"";
	FM[42].id = 152;
	FM[42].name = "Vowel/UbuOrator   12 VOW";
	FM[42].info = "Aah-Uuh vowel with no Q.\nRaise Q for throaty vocals.";
	FM[43].id = 153;
	FM[43].name = "Vowel/DeepBouche  12 VOW";
	FM[43].info = "French vowels!\n\"Ou-Est\" vowel at low Q.";
	FM[44].id = 135;
	FM[44].name = "Resonance/MeatyGizmo  12 REZ";
	FM[44].info = "Filter inverts at mid-Q.";
	FM[45].id = 139;
	FM[45].name = "Resonance/DeadRinger  12 REZ";
	FM[45].info = "Permanent \"Ringy\" Q response.\nMany Q variations.";
	FM[46].id = 145;
	FM[46].name = "Resonance/ZommPeaks   12 REZ";
	FM[46].info = "High resonance nasal filter.";
	FM[47].id = 158;
	FM[47].name = "Resonance/AcidRavage  12 REZ";
	FM[47].info = "Great analog Q response.\nWide tonal range.\nTry with a sawtooth LFO.";
	FM[48].id = 159;
	FM[48].name = "Resonance/BassOMatic  12 REZ";
	FM[48].info = "Low boost for basslines.\nQ goes to distortion at\nthe maximum level.";
	FM[49].id = 160;
	FM[49].name = "Resonance/LucifersQ   12 REZ";
	FM[49].info = "Violent mid Q filter!\nTake care with Q values 40-90.";
	FM[50].id = 161;
	FM[50].name = "Resonance/ToothComb   12 REZ";
	FM[50].info = "Highly resonant harmonic peaks\nshift in unison.\nTry mid Q.";

	// Patchcord sources
	PatchS[0].id = 0;
	PatchS[0].name = "Off";

	PatchS[1].id = 8;
	PatchS[1].name = "Key/Key+";
	PatchS[2].id = 9;
	PatchS[2].name = "Key/Key~";
	PatchS[3].id = 10;
	PatchS[3].name = "Key/Vel+";
	PatchS[4].id = 11;
	PatchS[4].name = "Key/Vel~";
	PatchS[5].id = 12;
	PatchS[5].name = "Key/Vel<";
	PatchS[6].id = 13;
	PatchS[6].name = "Key/RlsVel";
	PatchS[7].id = 14;
	PatchS[7].name = "Key/Gate";
	PatchS[8].id = 48;
	PatchS[8].name = "Key/Glide";
	PatchS[9].id = 4;
	PatchS[9].name = "Key/XfdRand";
	PatchS[10].id = 100;
	PatchS[10].name = "Key/KeyRand 1";
	PatchS[11].id = 101;
	PatchS[11].name = "Key/KeyRand 2";

	PatchS[12].id = 16;
	PatchS[12].name = "Controller/PitchWhl";
	PatchS[13].id = 17;
	PatchS[13].name = "Controller/ModWhl";
	PatchS[14].id = 18;
	PatchS[14].name = "Controller/Pressure";
	PatchS[15].id = 19;
	PatchS[15].name = "Controller/Pedal";
	PatchS[16].id = 22;
	PatchS[16].name = "Controller/FootSw 1";
	PatchS[17].id = 23;
	PatchS[17].name = "Controller/FootSw 2";
	PatchS[18].id = 38;
	PatchS[18].name = "Controller/FootSw 3";
	PatchS[19].id = 24;
	PatchS[19].name = "Controller/FootSw 1FF";
	PatchS[20].id = 25;
	PatchS[20].name = "Controller/FootSw 2FF";
	PatchS[21].id = 39;
	PatchS[21].name = "Controller/FootSw 3FF";

	PatchS[22].id = 72;
	PatchS[22].name = "Envelope/VolEnv+";
	PatchS[23].id = 73;
	PatchS[23].name = "Envelope/VolEnv~";
	PatchS[24].id = 74;
	PatchS[24].name = "Envelope/VolEnv<";
	PatchS[25].id = 80;
	PatchS[25].name = "Envelope/FilEnv+";
	PatchS[26].id = 81;
	PatchS[26].name = "Envelope/FilEnv~";
	PatchS[27].id = 82;
	PatchS[27].name = "Envelope/FilEnv<";
	PatchS[28].id = 88;
	PatchS[28].name = "Envelope/AuxEnv+";
	PatchS[29].id = 89;
	PatchS[29].name = "Envelope/AuxEnv~";
	PatchS[30].id = 90;
	PatchS[30].name = "Envelope/AuxEnv<";

	PatchS[31].id = 96;
	PatchS[31].name = "LFO/LFO 1~";
	PatchS[32].id = 97;
	PatchS[32].name = "LFO/LFO 1+";
	PatchS[33].id = 104;
	PatchS[33].name = "LFO/LFO 2~";
	PatchS[34].id = 105;
	PatchS[34].name = "LFO/LFO 2+";

	PatchS[35].id = 98;
	PatchS[35].name = "Noise/White";
	PatchS[36].id = 99;
	PatchS[36].name = "Noise/Pink";

	PatchS[37].id = 106;
	PatchS[37].name = "Processor/Log0sum";
	PatchS[38].id = 107;
	PatchS[38].name = "Processor/Lag0";
	PatchS[39].id = 108;
	PatchS[39].name = "Processor/Log1sum";
	PatchS[40].id = 109;
	PatchS[40].name = "Processor/Lag1";
	PatchS[41].id = 128;
	PatchS[41].name = "Processor/PLagOut";
	PatchS[42].id = 129;
	PatchS[42].name = "Processor/PRampOut";
	PatchS[43].id = 160;
	PatchS[43].name = "Processor/DC";
	PatchS[44].id = 161;
	PatchS[44].name = "Processor/Sum";
	PatchS[45].id = 162;
	PatchS[45].name = "Processor/Switch";
	PatchS[46].id = 163;
	PatchS[46].name = "Processor/Abs";
	PatchS[47].id = 164;
	PatchS[47].name = "Processor/Diode";
	PatchS[48].id = 165;
	PatchS[48].name = "Processor/FlipFlop";
	PatchS[49].id = 166;
	PatchS[49].name = "Processor/Quantizer";
	PatchS[50].id = 167;
	PatchS[50].name = "Processor/Gain x 4";

	PatchS[51].id = 144;
	PatchS[51].name = "Clock Divisor/Double Whole";
	PatchS[52].id = 145;
	PatchS[52].name = "Clock Divisor/Whole";
	PatchS[53].id = 146;
	PatchS[53].name = "Clock Divisor/Half";
	PatchS[54].id = 147;
	PatchS[54].name = "Clock Divisor/Quarter";
	PatchS[55].id = 148;
	PatchS[55].name = "Clock Divisor/8th";
	PatchS[56].id = 149;
	PatchS[56].name = "Clock Divisor/16th";
	PatchS[57].id = 150;
	PatchS[57].name = "Clock Divisor/Octal";
	PatchS[58].id = 151;
	PatchS[58].name = "Clock Divisor/Quad";

	PatchS[59].id = 26;
	PatchS[59].name = "MIDI/MIDI Vol";
	PatchS[60].id = 27;
	PatchS[60].name = "MIDI/MIDI Pan";
	PatchS[61].id = 28;
	PatchS[61].name = "MIDI/MIDI Expr";
	PatchS[62].id = 20;
	PatchS[62].name = "MIDI/MIDI A";
	PatchS[63].id = 21;
	PatchS[63].name = "MIDI/MIDI B";
	PatchS[64].id = 32;
	PatchS[64].name = "MIDI/MIDI C";
	PatchS[65].id = 33;
	PatchS[65].name = "MIDI/MIDI D";
	PatchS[66].id = 34;
	PatchS[66].name = "MIDI/MIDI E";
	PatchS[67].id = 35;
	PatchS[67].name = "MIDI/MIDI F";
	PatchS[68].id = 36;
	PatchS[68].name = "MIDI/MIDI G";
	PatchS[69].id = 37;
	PatchS[69].name = "MIDI/MIDI H";
	PatchS[70].id = 40;
	PatchS[70].name = "MIDI/MIDI I";
	PatchS[71].id = 41;
	PatchS[71].name = "MIDI/MIDI J";
	PatchS[72].id = 42;
	PatchS[72].name = "MIDI/MIDI K";
	PatchS[73].id = 43;
	PatchS[73].name = "MIDI/MIDI L";
	// xtra controllers
	PatchS[74].id = 44;
	PatchS[74].name = "MIDI/MIDI M";
	PatchS[75].id = 45;
	PatchS[75].name = "MIDI/MIDI N";
	PatchS[76].id = 46;
	PatchS[76].name = "MIDI/MIDI O";
	PatchS[77].id = 47;
	PatchS[77].name = "MIDI/MIDI P";

	// Patchcord destinations
	PatchD[0].id = 0;
	PatchD[0].name = "Off";
	PatchD[1].id = 8;
	PatchD[1].name = "Voice/KeySust";
	PatchD[2].id = 47;
	PatchD[2].name = "Voice/FinePitch";
	PatchD[3].id = 48;
	PatchD[3].name = "Voice/Pitch";
	PatchD[4].id = 49;
	PatchD[4].name = "Voice/Glide";
	PatchD[5].id = 50;
	PatchD[5].name = "Voice/ChorusAmt";
	PatchD[6].id = 52;
	PatchD[6].name = "Voice/'SStart";
	PatchD[7].id = 53;
	PatchD[7].name = "Voice/SLoop";
	PatchD[8].id = 54;
	PatchD[8].name = "Voice/SRetrig";
	PatchD[9].id = 66;
	PatchD[9].name = "Voice/RT X-fade";
	PatchD[10].id = 56;
	PatchD[10].name = "Voice/FFrequency";
	PatchD[11].id = 57;
	PatchD[11].name = "Voice/F'Resonance";
	PatchD[12].id = 64;
	PatchD[12].name = "Voice/Amp Vol";
	PatchD[13].id = 65;
	PatchD[13].name = "Voice/Amp Pan";

	PatchD[14].id = 72;
	PatchD[14].name = "Envelope/VolEnvRts";
	PatchD[15].id = 73;
	PatchD[15].name = "Envelope/VolEnvAtk";
	PatchD[16].id = 74;
	PatchD[16].name = "Envelope/VolEnvDcy";
	PatchD[17].id = 75;
	PatchD[17].name = "Envelope/VolEnvRls";
	PatchD[18].id = 76;
	PatchD[18].name = "Envelope/VolEnvSus";
	PatchD[19].id = 80;
	PatchD[19].name = "Envelope/FilEnvRts";
	PatchD[20].id = 81;
	PatchD[20].name = "Envelope/FilEnvAtk";
	PatchD[21].id = 82;
	PatchD[21].name = "Envelope/FilEnvDcy";
	PatchD[22].id = 83;
	PatchD[22].name = "Envelope/FilEnvRls";
	PatchD[23].id = 84;
	PatchD[23].name = "Envelope/FilEnvSus";
	PatchD[24].id = 86;
	PatchD[24].name = "Envelope/FilEnvTrig";
	PatchD[25].id = 88;
	PatchD[25].name = "Envelope/AuxEnvRts";
	PatchD[26].id = 89;
	PatchD[26].name = "Envelope/AuxEnvAtk";
	PatchD[27].id = 90;
	PatchD[27].name = "Envelope/AuxEnvDcy";
	PatchD[28].id = 91;
	PatchD[28].name = "Envelope/AuxEnvRls";
	PatchD[29].id = 92;
	PatchD[29].name = "Envelope/AuxEnvSus";
	PatchD[30].id = 94;
	PatchD[30].name = "Envelope/AuxEnvTrig";

	PatchD[31].id = 96;
	PatchD[31].name = "LFO/LFO 1 Rate";
	PatchD[32].id = 97;
	PatchD[32].name = "LFO/LFO 1 Trig";
	PatchD[33].id = 104;
	PatchD[33].name = "LFO/LFO 2 Rate";
	PatchD[34].id = 105;
	PatchD[34].name = "LFO/LFO 2 Trig";

	PatchD[35].id = 106;
	PatchD[35].name = "Processor/Lag 0 in";
	PatchD[36].id = 108;
	PatchD[36].name = "Processor/Lag 1 in";
	PatchD[37].id = 161;
	PatchD[37].name = "Processor/Sum";
	PatchD[38].id = 162;
	PatchD[38].name = "Processor/Switch";
	PatchD[39].id = 163;
	PatchD[39].name = "Processor/Abs";
	PatchD[40].id = 164;
	PatchD[40].name = "Processor/Diode";
	PatchD[41].id = 165;
	PatchD[41].name = "Processor/FlipFlop";
	PatchD[42].id = 166;
	PatchD[42].name = "Processor/Quantize";
	PatchD[43].id = 167;
	PatchD[43].name = "Processor/Gain x 4";

	PatchD[44].id = 168;
	PatchD[44].name = "Attenuator/C 01 Amt";
	PatchD[45].id = 169;
	PatchD[45].name = "Attenuator/C 02 Amt";
	PatchD[46].id = 170;
	PatchD[46].name = "Attenuator/C 03 Amt";
	PatchD[47].id = 171;
	PatchD[47].name = "Attenuator/C 04 Amt";
	PatchD[48].id = 172;
	PatchD[48].name = "Attenuator/C 05 Amt";
	PatchD[49].id = 173;
	PatchD[49].name = "Attenuator/C 06 Amt";
	PatchD[50].id = 174;
	PatchD[50].name = "Attenuator/C 07 Amt";
	PatchD[51].id = 175;
	PatchD[51].name = "Attenuator/C 08 Amt";
	PatchD[52].id = 176;
	PatchD[52].name = "Attenuator/C 09 Amt";
	PatchD[53].id = 177;
	PatchD[53].name = "Attenuator/C 10 Amt";
	PatchD[54].id = 178;
	PatchD[54].name = "Attenuator/C 11 Amt";
	PatchD[55].id = 179;
	PatchD[55].name = "Attenuator/C 12 Amt";
	PatchD[56].id = 180;
	PatchD[56].name = "Attenuator/C 13 Amt";
	PatchD[57].id = 181;
	PatchD[57].name = "Attenuator/C 14 Amt";
	PatchD[58].id = 182;
	PatchD[58].name = "Attenuator/C 15 Amt";
	PatchD[59].id = 183;
	PatchD[59].name = "Attenuator/C 16 Amt";
	PatchD[60].id = 184;
	PatchD[60].name = "Attenuator/C 17 Amt";
	PatchD[61].id = 185;
	PatchD[61].name = "Attenuator/C 18 Amt";
	PatchD[62].id = 186;
	PatchD[62].name = "Attenuator/C 19 Amt";
	PatchD[63].id = 187;
	PatchD[63].name = "Attenuator/C 20 Amt";
	PatchD[64].id = 188;
	PatchD[64].name = "Attenuator/C 21 Amt";
	PatchD[65].id = 189;
	PatchD[65].name = "Attenuator/C 22 Amt";
	PatchD[66].id = 190;
	PatchD[66].name = "Attenuator/C 23 Amt";
	PatchD[67].id = 191;
	PatchD[67].name = "Attenuator/C 24 Amt";

	// preset patchcord destinations
	PresetPatchD[0].id = 0;
	PresetPatchD[0].name = "Off";

	PresetPatchD[1].id = 1;
	PresetPatchD[1].name = "Effects/FX A Send 1";
	PresetPatchD[2].id = 2;
	PresetPatchD[2].name = "Effects/FX A Send 2";
	PresetPatchD[3].id = 3;
	PresetPatchD[3].name = "Effects/FX A Send 3";
	PresetPatchD[4].id = 4;
	PresetPatchD[4].name = "Effects/FX A Send 4";
	PresetPatchD[5].id = 5;
	PresetPatchD[5].name = "Effects/FX B Send 1";
	PresetPatchD[6].id = 6;
	PresetPatchD[6].name = "Effects/FX B Send 2";
	PresetPatchD[7].id = 7;
	PresetPatchD[7].name = "Effects/FX B Send 3";
	PresetPatchD[8].id = 8;
	PresetPatchD[8].name = "Effects/FX B Send 4";

	PresetPatchD[9].id = 96;
	PresetPatchD[9].name = "Arpeggiator/Arp Rate";
	PresetPatchD[10].id = 97;
	PresetPatchD[10].name = "Arpeggiator/Arp Ext.";
	PresetPatchD[11].id = 98;
	PresetPatchD[11].name = "Arpeggiator/Arp Vel";
	PresetPatchD[12].id = 99;
	PresetPatchD[12].name = "Arpeggiator/Arp Gate";
	PresetPatchD[13].id = 100;
	PresetPatchD[13].name = "Arpeggiator/Arp Intvl";

	PresetPatchD[14].id = 112;
	PresetPatchD[14].name = "Beats/BeatsVelG1";
	PresetPatchD[15].id = 113;
	PresetPatchD[15].name = "Beats/BeatsVelG2";
	PresetPatchD[16].id = 114;
	PresetPatchD[16].name = "Beats/BeatsVelG3";
	PresetPatchD[17].id = 115;
	PresetPatchD[17].name = "Beats/BeatsVelG4";
	PresetPatchD[18].id = 116;
	PresetPatchD[18].name = "Beats/BeatsXpsG1";
	PresetPatchD[19].id = 117;
	PresetPatchD[19].name = "Beats/BeatsXpsG2";
	PresetPatchD[20].id = 118;
	PresetPatchD[20].name = "Beats/BeatsXpsG3";
	PresetPatchD[21].id = 119;
	PresetPatchD[21].name = "Beats/BeatsXpsG4";
	PresetPatchD[22].id = 120;
	PresetPatchD[22].name = "Beats/BeatsBusy";
	PresetPatchD[23].id = 121;
	PresetPatchD[23].name = "Beats/BeatsVari";

	PresetPatchD[24].id = 128;
	PresetPatchD[24].name = "Preset/PLag In";
	PresetPatchD[25].id = 129;
	PresetPatchD[25].name = "Preset/PLag Amt";
	PresetPatchD[26].id = 130;
	PresetPatchD[26].name = "Preset/PRamp In";
	PresetPatchD[27].id = 131;
	PresetPatchD[27].name = "Preset/PRamp Rt";
}

void PD_Layer_Strip::init(int l)
{
	//pmesg("PD_Layer_Strip::init(%d)\n", l);
	// initialize layer strips
	layer = l;
	layer_solo->set_id(1437, layer);
	layer_group->set_id(1438, layer);
	cutoff->set_id(1538, layer);
	emphasis->set_id(1539, layer);
	filter->set_id(1537, layer);
	for (int i = 0; i <= 50; i++)
		FM[i]._index = filter->add(FM[i].name);
	coarse->set_id(1425, layer);
	fine->set_id(1426, layer);
	chorus->set_id(1427, layer);
	width->set_id(1428, layer);
	offset->set_id(1436, layer);
	delay->set_id(1435, layer);
	glide->set_id(1432, layer);
	glide_curve->set_id(1433, layer);
	pan->set_id(1411, layer);
	bend->set_id(1431, layer);
	volume->set_id(1410, layer);
	mix_out->set_id(1412, layer);
	non_t->set_id(1430, layer);
	//	loop->set_id(1434, layer);
}

/**
 * initializes the layer editors
 * sets id and layer informations for all widgets contained in the layer editor
 * @param l layer of this editor (0-3)
 */
void PD_Layer_Editor::init(int l)
{
	//pmesg("PD_Layer_Editor::init(%d)\n", l);
	layer = l;
	instrument_rom->set_id(1439, l);
	instrument->set_id(1409, l);
	envelope_editor->set_layer(l);
	lfo1_waveform->set_id(1666, l);
	lfo1_rate->set_id(1665, l);
	lfo1_delay->set_id(1667, l);
	lfo1_variation->set_id(1668, l);
	lfo1_sync->set_id(1669, l);
	lfo2_waveform->set_id(1671, l);
	lfo2_rate->set_id(1670, l);
	lfo2_delay->set_id(1672, l);
	lfo2_variation->set_id(1673, l);
	lfo2_sync->set_id(1674, l);
	patchcords->init(l);
}

//// delete name files
void PD_UI::Reset(char user_data, char rom_data)
{
	if (!cfg || (user_data == -1 && rom_data == -1))
	{
		reset_w->hide();
		b_reset->activate();
		return;
	}
	pmesg("reset(%d, %d)\n", user_data, rom_data);
	char config_dir[PATH_MAX];
	snprintf(config_dir, PATH_MAX, "%s", cfg->get_config_dir());
	delete pxk;
	pxk = 0;
	// delete files
	dirent **files;
	int num_files = fl_filename_list(config_dir, &files);
	char buf[PATH_MAX];
	char f[20];
	int deleted = 0;
	if (user_data >= 0)
	{
		if (user_data == 127) // delete all user data
			snprintf(f, 20, "n_???_0_*");
		else
			snprintf(f, 20, "n_???_0_%d", user_data);
		for (int i = 0; i < num_files; i++)
		{
			if (fl_filename_match(files[i]->d_name, f))
			{
				snprintf(buf, PATH_MAX, "%s/%s", config_dir, files[i]->d_name);
				pmesg(" - - deleting %s ... ", buf);
#ifdef WIN32
				if (_unlink(buf))
#else
				if (unlink(buf))
#endif
				{
					fl_message("Could not delete\n%s", buf);
					pmesg(" failed!\n", buf);
				}
				else
				{
					pmesg(" success!\n", buf);
					++deleted;
				}
			}
		}
	}
	if (rom_data >= 1)
	{
		if (rom_data == 1) // delete all rom data
			snprintf(f, 20, "n_???_[123456789]*");
		else
			snprintf(f, 20, "n_???_%d", rom_data);
		for (int i = 0; i < num_files; i++)
		{
			if (fl_filename_match(files[i]->d_name, f))
			{
				snprintf(buf, PATH_MAX, "%s/%s", config_dir, files[i]->d_name);
				pmesg(" - - deleting %s! ... ", buf);
#ifdef WIN32
				if (_unlink(buf))
#else
				if (unlink(buf))
#endif
				{
					fl_message("Could not delete\n%s", buf);
					pmesg(" failed!\n", buf);
				}
				else
				{
					pmesg(" success!\n", buf);
					++deleted;
				}
			}
		}
	}
	// clean up
	for (int i = num_files; i > 0;)
		free((void*) (files[--i]));
	free((void*) files);
	// reload
	reset_w->hide();
	b_reset->activate();
	fl_message("OK. Deleted %d files from\n%s.\nPress OK to synchronize missing data.", deleted, config_dir);
	pxk = new PXK();
	pxk->Boot(true);
}
