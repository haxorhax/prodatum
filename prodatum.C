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

const char* VERSION = "2.0rc9";
PD_UI* ui = 0;
MIDI* midi = 0;
PXK* pxk = 0;
extern Cfg* cfg;

extern FilterMap FM[51];
extern const char* rates[25];
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
	midi = new MIDI();
	if (!midi)
		return 2;
#ifdef NDEBUG
	ui->init_log_b->hide();
	ui->init_log_m->hide();
#endif
	ui->main_window->free_position();
	ui->main_window->show();
	pxk = new PXK(__auto_connect, __device);
	if (!pxk)
		return 3;
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
	static int prev;
	if (g_arp_edit->visible())
	{
		g_arp_edit->hide();
		g_main->show();
		if (l == selected)
			return;
	}
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
	for (unsigned char i = CFG_BGR; i <= CFG_INB; i++)
		cfg->getset_default(i);
	cfg->apply(true);
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

void PD_Arp_Step::init(int s)
{
	step = s;
	op->set_step(s);
	offset->set_step(s);
	velocity->set_id(785, s);
	duration->set_id(786, s);
	repeat->set_id(787, s);
	arp_step[s] = this;
	char buf[3];
	snprintf(buf, 3, "%d", s + 1);
	offset->copy_label(buf);
}

void PD_Arp_Step::edit_value(int id, int value)
{
	//pmesg("PD_Arp_Step::edit_value(%d, %d)\n", id, value);
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
	//pmesg("PD_Arp_Step::set_values(%d, %d, %d, %d)\n", off, vel, dur, rep);
	if (off > -49)
	{
		if (off < 49)
		{
			offset->value((double) off);
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
			offset->value((double) 0);
		}
	}
	velocity->value((double) vel);
	duration->value((double) dur);
	repeat->value((double) rep + 1);
}

void PD_UI::edit_arp_x(int x)
{
	pmesg("PD_UI::edit_arp_x(%d)\n", x);
	if (x == 0 || x == -1) // preset arp
	{
		if (pxk->arp && pxk->arp->get_number() == ui->preset_editor->arp->value() - 1)
		{
			ui->g_arp_edit->show();
			ui->g_main->hide();
			Fl::focus(ui->g_arp_edit);
		}
		else
			midi->request_arp_dump(ui->preset_editor->arp->value() - 1, 0);
	}
	else if (x == 1) // master arp
	{
		if (pxk->arp && pxk->arp->get_number() == ui->main->arp->value() - 1)
		{
			ui->g_arp_edit->show();
			ui->g_main->hide();
			Fl::focus(ui->g_arp_edit);
		}
		else
			midi->request_arp_dump(ui->main->arp->value() - 1, 0);
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
		//ui->select(0);
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
		main->layer_strip[1]->deactivate();
		main->layer_strip[2]->deactivate();
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
		pxk->display_status("Edit All Layers enabled.");
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
		main->layer_strip[1]->activate();
		main->layer_strip[2]->activate();
		main->layer_strip[3]->activate();
		layer_editor[0]->copy_env->activate();
		layer_editor[0]->copy_lfo->activate();
		layer_editor[0]->copy_pc->activate();
		layer_editor[1]->activate();
		layer_editor[2]->activate();
		layer_editor[3]->activate();
		b_eall->clear();
		m_eall->clear();
		eall = false;
		pxk->display_status("Edit All Layers disabled.");
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
			Fl::focus(copy_browser);
			g_copy_arp_pattern->hide();
			copy_arp_rom->deactivate();
			break;
		case C_PRESET:
			copy_preset->label("Copy Program");
			g_copy_preset->label("TARGET");
			copy_arp_rom->set_value(0);
			copy_browser->set_id(type);
			g_copy_preset->show();
			Fl::focus(copy_browser);
			g_copy_arp_pattern->hide();
			copy_arp_rom->deactivate();
			break;
		case C_ARP:
			copy_preset->label("Copy Arp Settings");
			g_copy_preset->label("SOURCE");
			copy_browser->set_id(type);
			g_copy_preset->show();
			Fl::focus(copy_browser);
			g_copy_arp_pattern->hide();
			copy_arp_rom->activate();
			break;
		case C_ARP_PATTERN:
			copy_preset->label("Copy Arp Pattern");
			copy_arp_pattern_browser->set_id(type);
			g_copy_arp_pattern->show();
			Fl::focus(copy_arp_pattern_browser);
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

/**
 * create about text.
 * creates the text that is shown in the about dialog window
 * @see about
 */
/**
 * loads various data that needs to be initialized early
 */
static void load_data()
{
	//pmesg("load_data()\n");
	// information
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
	FM[0].value = 127;
	FM[0].name = "Off";
	FM[0].info = "Unfiltered sound";
	FM[1].value = 138;
	FM[1].name = "FuzziFace   12 DST";
	FM[1].info = "Nasty clipped distortion.\nQ functions as mid-frequency tone\ncontrol.";
	FM[2].value = 162;
	FM[2].name = "EarBender   12 WAH";
	FM[2].info = "Midway between wah & vowel.\nStrong mid-boost.\nNasty at high Q settings.";
	FM[3].value = 163;
	FM[3].name = "KlangKling  12 SFX";
	FM[3].info = "Ringing Flange filter.\nQ \"tunes\" the ring frequency.";
	FM[4].value = 1;
	FM[4].name = "Lowpass/Smooth       2 LPF";
	FM[4].info = "Typical OB type low-pass filter\nwith a shallow 12 dB/octave slope.";
	FM[5].value = 0;
	FM[5].name = "Lowpass/Classic      4 LPF";
	FM[5].info = "4-pole low-pass filter,\nthe standard filter on classic\nanalog synths.\n24 dB/octave rolloff.";
	FM[6].value = 2;
	FM[6].name = "Lowpass/Steeper      6 LPF";
	FM[6].info =
			"6-pole low-pass filter which has a\nsteeper slope than a 4-pole low-\npass filter.\n36 dB/octave rolloff!";
	FM[7].value = 132;
	FM[7].name = "Lowpass/MegaSweepz  12 LPF";
	FM[7].info = "\"Loud\" LPF with a hard Q.\nTweeters beware!";
	FM[8].value = 133;
	FM[8].name = "Lowpass/EarlyRizer  12 LPF";
	FM[8].info = "Classic analog sweeping\nwith hot Q and Lo-end.";
	FM[9].value = 136;
	FM[9].name = "Lowpass/KlubKlassi  12 LPF";
	FM[9].info = "Responsive low-pass filter sweep\nwith a wide spectrum of Q sounds.";
	FM[10].value = 137;
	FM[10].name = "Lowpass/BassBox-303 12 LPF";
	FM[10].info = "Pumped up lows with\nTB-like squelchy Q factor.";
	FM[11].value = 134;
	FM[11].name = "Lowpass/Millennium  12 LPF";
	FM[11].info = "Aggressive low-pass filter.\nQ gives you a variety of\nspiky tonal peaks.";
	FM[12].value = 8;
	FM[12].name = "Highpass/Shallow      2 HPF";
	FM[12].info = "2-pole high-pass filter.\n12 dB/octave slope.";
	FM[13].value = 9;
	FM[13].name = "Highpass/Deeper       4 HPF";
	FM[13].info = "Classic 4-pole high-pass filter.\nCutoff sweep progressively cuts\n4th Order High-pass.";
	FM[14].value = 16;
	FM[14].name = "Bandpass/Band-pass1   2 BPF";
	FM[14].info = "Band-pass filter with 6 dB/octave\nrolloff on either side of the\npassband and Q control.";
	FM[15].value = 17;
	FM[15].name = "Bandpass/Band-pass2   4 BPF";
	FM[15].info = "Band-pass filter with 12 dB/octave\nrolloff on either side of the\npassband and Q control.";
	FM[16].value = 18;
	FM[16].name = "Bandpass/ContraBand   6 BPF";
	FM[16].info = "A novel band-pass filter where the\nfrequency peaks and dips midway\nin the frequency range.";
	FM[17].value = 32;
	FM[17].name = "EQ/Swept1>oct   6 EQ+";
	FM[17].info = "Parametric filter with 24 dB of\nboost or cut and a one octave\nbandwidth.";
	FM[18].value = 33;
	FM[18].name = "EQ/Swept2>1oct  6 EQ+";
	FM[18].info =
			"Parametric filter with 24 dB of\nboost or cut. The bandwidth of the\nfilter is two octaves wide at the\nlow end of the audio spectrum,\ngradually changing to one octave\nwide at the upper end of the\nspectrum.";
	FM[19].value = 34;
	FM[19].name = "EQ/Swept3>1oct  6 EQ+";
	FM[19].info =
			"Parametric filter with 24 dB of\nboost or cut. The bandwidth of the\nfilter is three octaves wide at the\nlow end of the audio spectrum,\ngradually changing to one octave\nwide at the upper end of the\nspectrum.";
	FM[20].value = 140;
	FM[20].name = "EQ/TB-OrNot-TB 12 EQ+";
	FM[20].info = "Great Bassline \"Processor.\"";
	FM[21].value = 142;
	FM[21].name = "EQ/RolandBass  12 EQ+";
	FM[21].info = "Constant bass boost\nwith mid-tone Q control.";
	FM[22].value = 147;
	FM[22].name = "EQ/BassTracer  12 EQ+";
	FM[22].info = "Low Q boosts bass.\nTry sawtooth or square waveform\nwith Q set to 115.";
	FM[23].value = 148;
	FM[23].name = "EQ/RogueHertz  12 EQ+";
	FM[23].info = "Bass with mid-range boost and\nsmooth Q. Sweep cutoff with Q at\n127.";
	FM[24].value = 146;
	FM[24].name = "EQ/DJAlkaline  12 EQ+";
	FM[24].info = "Band accentuating filter,\nQ shifts \"ring\" frequency.";
	FM[25].value = 131;
	FM[25].name = "EQ/AceOfBass   12 EQ+";
	FM[25].info = "Bass-boost to bass-cut morph.";
	FM[26].value = 149;
	FM[26].name = "EQ/RazorBlades 12 EQ-";
	FM[26].info = "Cuts a series of frequency bands.\nQ selects different bands.";
	FM[27].value = 150;
	FM[27].name = "EQ/RadioCraze  12 EQ-";
	FM[27].info = "Band limited for a cheap\nradio-like EQ.";
	FM[28].value = 64;
	FM[28].name = "Phaser/PhazeShift1  6 PHA";
	FM[28].info =
			"Recreates a comb filter effect\ntypical of phase shifters. Freq.\n moves position of notches.\nQ varies the depth of the notches.";
	FM[29].value = 65;
	FM[29].name = "Phaser/PhazeShift2  6 PHA";
	FM[29].info =
			"Comb filter with slightly different\nnotch frequency moving the\nfrequency of notches. Q varies\nthe depth of the notches.";
	FM[30].value = 66;
	FM[30].name = "Phaser/BlissBlatz   6 PHA";
	FM[30].info = "Bat phaser from the Emulator 4.";
	FM[31].value = 154;
	FM[31].name = "Phaser/FreakShifta 12 PHA";
	FM[31].info = "Phasey movement.\nTry major 6 interval\nand maximum Q.";
	FM[32].value = 155;
	FM[32].name = "Phaser/CruzPusher  12 PHA";
	FM[32].info = "Accentuates harmonics at high Q.\nTry with a sawtooth LFO.";
	FM[33].value = 72;
	FM[33].name = "Flanger/FlangerLite  6 FLG";
	FM[33].info =
			"Contains three notches.\nFrequency moves frequency and\nspacing of notches.\nQ increases flanging depth.";
	FM[34].value = 156;
	FM[34].name = "Flanger/AngelzHairz 12 FLG";
	FM[34].info = "Smooth sweep flanger.\nGood with vox waves.\neg. I094, Q = 60.";
	FM[35].value = 157;
	FM[35].name = "Flanger/DreamWeava  12 FLG";
	FM[35].info = "Directional Flanger.\nPoles shift down at low Q\nand up at high Q.";
	FM[36].value = 80;
	FM[36].name = "Vowel/Aah-Ay-Eeh   6 VOW";
	FM[36].info =
			"Vowel formant filter which sweeps\nfrom \"Ah\" sound, through \"Ay\"\nsound to \"Ee\" sound at maximum\nfrequency setting. Q varies the\napparent size of the mouth\ncavity.";
	FM[37].value = 81;
	FM[37].name = "Vowel/Ooh-To-Aah   6 VOW";
	FM[37].info =
			"Vowel formant filter which sweeps\nfrom \"Oo\" sound, through \"Oh\"\nsound to \"Ah\" sound at maximum\nfrequency setting. Q varies the\napparent size of mouth\ncavity.";
	FM[38].value = 141;
	FM[38].name = "Vowel/Ooh-To-Eee  12 VOW";
	FM[38].info = "Oooh to Eeee formant morph.";
	FM[39].value = 143;
	FM[39].name = "Vowel/MultiQVox   12 VOW";
	FM[39].info = "Multi-Formant,\nMap Q To velocity.";
	FM[40].value = 144;
	FM[40].name = "Vowel/TalkingHedz 12 VOW";
	FM[40].info = "\"Oui\" morphing filter.\nQ adds peaks.";
	FM[41].value = 151;
	FM[41].name = "Vowel/Eeh-To-Aah  12 VOW";
	FM[41].info = "\"E\" to \"Ah\" formant movement.\nQ accentuates \"peakiness.\"";
	FM[42].value = 152;
	FM[42].name = "Vowel/UbuOrator   12 VOW";
	FM[42].info = "Aah-Uuh vowel with no Q.\nRaise Q for throaty vocals.";
	FM[43].value = 153;
	FM[43].name = "Vowel/DeepBouche  12 VOW";
	FM[43].info = "French vowels!\n\"Ou-Est\" vowel at low Q.";
	FM[44].value = 135;
	FM[44].name = "Resonance/MeatyGizmo  12 REZ";
	FM[44].info = "Filter inverts at mid-Q.";
	FM[45].value = 139;
	FM[45].name = "Resonance/DeadRinger  12 REZ";
	FM[45].info = "Permanent \"Ringy\" Q response.\nMany Q variations.";
	FM[46].value = 145;
	FM[46].name = "Resonance/ZommPeaks   12 REZ";
	FM[46].info = "High resonance nasal filter.";
	FM[47].value = 158;
	FM[47].name = "Resonance/AcidRavage  12 REZ";
	FM[47].info = "Great analog Q response.\nWide tonal range.\nTry with a sawtooth LFO.";
	FM[48].value = 159;
	FM[48].name = "Resonance/BassOMatic  12 REZ";
	FM[48].info = "Low boost for basslines.\nQ goes to distortion at\nthe maximum level.";
	FM[49].value = 160;
	FM[49].name = "Resonance/LucifersQ   12 REZ";
	FM[49].info = "Violent mid Q filter!\nTake care with Q values 40-90.";
	FM[50].value = 161;
	FM[50].name = "Resonance/ToothComb   12 REZ";
	FM[50].info = "Highly resonant harmonic peaks\nshift in unison.\nTry mid Q.";
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
	char cfg_name = cfg->get_cfg_option(CFG_DEVICE_ID);
	delete pxk;
	pxk = 0;
	// delete files
	dirent **files;
	int num_files = fl_filename_list(config_dir, &files);
	char buf[PATH_MAX];
	int f_size = 20;
	char f[f_size];
	int deleted = 0;
	if (user_data >= 0)
	{
		if (user_data == 127) // delete all user data
			snprintf(f, f_size, "n_???_0_*");
		else
			snprintf(f, f_size, "n_???_0_%d", user_data);
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
			snprintf(f, f_size, "n_???_[123456789]*");
		else
			snprintf(f, f_size, "n_???_%d", rom_data);
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
	fl_message("OK. Deleted %d files from\n%s.\nWill reload missing data now.", deleted, config_dir);
	pxk = new PXK(true);
}

void PD_UI::Cancel()
{
	pmesg("PD_UI::Cancel() \n");
	pxk->Join();
	delete pxk;
	pxk = 0;
	init->hide();
	while (init->shown())
		Fl::wait(.1);
	pxk = new PXK(false);
}
