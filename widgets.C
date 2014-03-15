/*
 This file is part of prodatum.
 Copyright 2011-2014 Jan Eidtmann

 prodatum is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 prodatum is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with prodatum.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <FL/fl_ask.H>
#include <FL/filename.H>
#include <math.h>
#include <string.h>

#include "ui.H"
/**
 * global array that holds all device parameter widgets.
 * every parameter can be accessed by their ID and layer.
 */
PWid* pwid[2000][4];
/// pointer to widgets that is currently being edited
PWid* pwid_editing;
/// contains informations for all 51 filter types
extern FilterMap FM[51];
/// contains strings for some tempo related parameters
extern const char* rates[25];

extern PD_UI* ui;
extern PXK* pxk;
extern Cfg* cfg;
extern MIDI* midi;

/// show warning when we are about to erase an edited edit buffer
int dismiss(char exit)
{
	if (!pxk->preset || !pxk->preset->is_changed() || !ui->confirm_dismiss->value())
		return 1; // dismiss
	int answer;
	if (exit == 2)
		answer = fl_choice("Dismiss changes and open a different device?", "Cancel", "Dismiss", "Save");
	else if (exit == 1)
		answer = fl_choice("Dismiss changes and exit?", "Cancel", "Dismiss", "Save");
	else
		answer = fl_choice("Dismiss changes?", "Cancel", "Dismiss", "Save");
	if (answer == 2)
		ui->show_copy_preset(SAVE_PRESET);
	return answer;
}

// ###################
//
// ###################
void PWid::cb(PWid*, void* p)
{
	if (!pxk->Synchronized())
		return;
	//pmesg("cb: id: %d, layer: %d\n", ((int*) p)[0], ((int*) p)[1]);
	if (((int*) p)[0] == 897 && (ui->b_save_p->value() || ui->b_copy_p->value())) // saving in the preset browser
		goto SKIP_DISMISS;
	if ((((int*) p)[0] == 129 || ((int*) p)[0] == 138 || ((int*) p)[0] == 897) && dismiss(0) != 1)
	{
		// reset to previous value and return
		if (((int*) p)[0] == 129)
		{
			pwid[129][0]->set_value(pxk->selected_channel);
			ui->main->channel_select->activate();
			ui->value_input->activate();
		}
		else if (((int*) p)[0] == 138)
			pwid[138][0]->set_value(pxk->setup->get_value(138, pxk->selected_channel));
		else if (((int*) p)[0] == 897)
		{
			pwid[897][0]->set_value(pxk->setup->get_value(130, pxk->selected_channel));
			ui->g_preset->activate();
			ui->value_input->activate();
		}
		return;
	}
	SKIP_DISMISS: int value = 0;
	// editing with the value input widget
	if (((int*) p)[0] == 1)
	{
		if (!pwid_editing)
			return;
		int* layer_id = pwid_editing->get_id_layer();
		int* minimax = pwid_editing->get_minimax();
		value = pwid[1][0]->get_value();
		if (layer_id[0] == 1410 && value == -999) // layer volume
			return;
		// clamp value
		if (value < minimax[0])
		{
			value = minimax[0];
			pwid[1][0]->set_value(value);
		}
		else if (value > minimax[1])
		{
			value = minimax[1];
			pwid[1][0]->set_value(value);
		}
		pwid_editing->set_value(value);
		ui->forma_out->set_value(layer_id[0], layer_id[1], value);
		if (layer_id[0] == 897 && (ui->b_save_p->value() || ui->b_copy_p->value())) // saving in the preset browser
			return;
		pxk->widget_callback(layer_id[0], value, layer_id[1]);
	}
	else
	{
		// return for those id's we use internally (that have no equivalent on the device)
		if (((int*) p)[0] == 2 || (((int*) p)[0] >= 0x20 && ((int*) p)[0] <= 0x2d)) // copy_preset window widgets
		{
			if (((int*) p)[0] == 2)
				pwid[2][0]->get_value(); // update preset arp "copy from" browser
			return;
		}
		if (pwid_editing != pwid[((int*) p)[0]][((int*) p)[1]])
		{
			pwid_editing = pwid[((int*) p)[0]][((int*) p)[1]];
			int* minimax = pwid_editing->get_minimax();
			ui->value_input->minimum((double) minimax[0]);
			ui->value_input->maximum((double) minimax[1]);
		}
		value = pwid[((int*) p)[0]][((int*) p)[1]]->get_value();
		if (((int*) p)[0] == 1410 && value == -999) // layer volume
			return;
		ui->value_input->value((double) value);
		ui->forma_out->set_value(((int*) p)[0], ((int*) p)[1], value);
		if (((int*) p)[0] == 897 && (ui->b_save_p->value() || ui->b_copy_p->value())) // saving in the preset browser
			return;
		pxk->widget_callback(((int*) p)[0], value, ((int*) p)[1]);
	}
}

// ###################
//
// ###################
void Double_Window::resize(int x, int y, int w, int h)
{
	if (__main && (w != this->w() || h != this->h()) && cfg != 0)
	{
		if (cfg->get_default(CFG_WINDOW_WIDTH) != w || cfg->get_default(CFG_WINDOW_HEIGHT) != h)
		{
			ui->scope_i->hide();
			ui->scope_o->hide();
		}
		else
		{
			ui->scope_i->show();
			ui->scope_o->show();
		}
	}
	Fl_Double_Window::resize(x, y, w, h);
}

static char scut_b_key = 57; // last played note in minipianos
static char scut_b_velo = 100;

int Double_Window::handle(int ev)
{
	static char playing = -1;
	switch (ev)
	{
		case FL_KEYDOWN:
			if (playing == -1 && Fl::event_key() == 'b')
			{
				playing = scut_b_key;
				if (midi)
					midi->write_event(NOTE_ON, playing, scut_b_velo);
				ui->piano->activate_key(1, playing);
				ui->main->minipiano->activate_key(1, playing);
				ui->global_minipiano->activate_key(1, playing);
				return 1;
			}
			else if (Fl::event_key() == 'f')
			{
				if (ui->g_effects->visible())
				{
					ui->g_preset->show();
					ui->g_effects->hide();
				}
				else
				{
					ui->g_preset->hide();
					ui->g_effects->show();
				}
				return 1;
			}
			break;
		case FL_KEYUP:
			if (playing != -1 && Fl::event_key() == 'b')
			{
				if (midi)
					midi->write_event(NOTE_OFF, playing, 0);
				ui->piano->activate_key(-1, playing);
				ui->main->minipiano->activate_key(-1, playing);
				ui->global_minipiano->activate_key(-1, playing);
				playing = -1;
				return 1;
			}
			break;
		case FL_DND_ENTER:
			ui->dnd_box->show();
			break;
		case FL_DND_LEAVE:
			ui->dnd_box->hide();
			break;
	}
	return Fl_Double_Window::handle(ev);
}

void Double_Window::showup()
{
	supposed_to_be_shown = true;
	if (shown())
	{
		Fl_Window::show();
		return;
	}
	if (!w__shown && this != ui->main_window)
	{
		position(ui->main_window->x() + (ui->main_window->w() / 2) - (w() / 2), ui->main_window->y() + 80);
		w__shown = true;
	}
	Fl_Window::show();
}

void Double_Window::hide()
{
	supposed_to_be_shown = false;
	Fl_Window::hide();
}

bool Double_Window::shown_called()
{
	return supposed_to_be_shown;
}

static void dndcback(void *v)
{
	DND_Box *dbox = (DND_Box*) v;
	dbox->dnd();
}

int DND_Box::handle(int ev)
{
	switch (ev)
	{
		case FL_DND_ENTER:
		case FL_DND_RELEASE:
		case FL_DND_LEAVE:
		case FL_DND_DRAG:
			return 1;
		case FL_PASTE:
			snprintf(evt_txt, Fl::event_length() + 1, "%s", Fl::event_text());
			Fl::add_timeout(0.0, dndcback, (void*) this);
			hide();
			return 1;
	}
	return 0;
}

void DND_Box::dnd()
{
	pxk->load_export(evt_txt);
}

// ###################
//
// ###################
void Browser::set_id(int v, int l)
{
	id_layer[0] = v;
	id_layer[1] = l;
	if (v == 1281 || v == 1290) // preset links
		minimax[0] = -1;
	else
		minimax[0] = 0;
	minimax[1] = 0;
	callback((Fl_Callback*) cb, (void*) id_layer);
	pwid[v][l] = this;
}

int Browser::get_value() const
{
	int v = value();
	if (id_layer[0] == 897)
		ui->copy_browser->value(v);
	// update instrument names in channel strips
	if (id_layer[0] == 1409)
	{
		ui->main->layer_strip[id_layer[1]]->instrument->label(text(v) + 5);
		if (v != 1)
		{
			if (!ui->main->layer_strip[id_layer[1]]->active())
				ui->main->layer_strip[id_layer[1]]->activate();
		}
		else if (ui->main->layer_strip[id_layer[1]]->active())
			ui->main->layer_strip[id_layer[1]]->deactivate();
	}
	else if (id_layer[0] == 1281 || id_layer[0] == 1290) // preset links
	{
		if (v == 1)
		{
			if (id_layer[0] == 1281)
				ui->preset_editor->g_link1->deactivate();
			else
				ui->preset_editor->g_link2->deactivate();
		}
		else
		{
			if (id_layer[0] == 1281)
				ui->preset_editor->g_link1->activate();
			else
				ui->preset_editor->g_link2->activate();
		}
		return v - 2;
	}
	return v - 1;
}

void Browser::set_value(int v)
{
	//pmesg("Browser::set_value(%d) (id:%d layer:%d)\n", v, id_layer[0], id_layer[1]);
	if (size() > v)
	{
		if ((id_layer[0] == 1281 || id_layer[0] == 1290) && v >= -1) // preset links
		{
			if (v == -1)
			{
				if (id_layer[0] == 1281)
					ui->preset_editor->g_link1->deactivate();
				else
					ui->preset_editor->g_link2->deactivate();
			}
			else
			{
				if (id_layer[0] == 1281)
					ui->preset_editor->g_link1->activate();
				else
					ui->preset_editor->g_link2->activate();
			}
			select(v + 2);
			Fl::flush();
		}
		else if (v >= 0)
		{
			select(v + 1);
			Fl::flush();
		}
		else
			return;
		apply_filter();
		if (id_layer[0] == 897)
			ui->copy_browser->select(v + 1);
		// update instrument names in channel strips
		else if (id_layer[0] == 1409)
		{
			if (v >= 0 && v < size())
				ui->main->layer_strip[id_layer[1]]->instrument->label(text(v + 1) + 5);
			if (v != 0)
			{
				if (!ui->main->layer_strip[id_layer[1]]->active())
					ui->main->layer_strip[id_layer[1]]->activate();
			}
			else if (ui->main->layer_strip[id_layer[1]]->active())
				ui->main->layer_strip[id_layer[1]]->deactivate();
		}
	}
}

void Browser::reset()
{
	Fl_Browser::clear();
	selected_rom = -1;
}

void Browser::load_n(int type, int rom_id, int preset)
{
	//pmesg("Browser::load_n(%d, %d, %d) (id:%d layer:%d)\n", type, rom_id, preset, id_layer[0], id_layer[1]);
	unsigned char rom_ = pxk->get_rom_index(rom_id);
	if (rom_ == 5)
		return;
	if (!pxk->rom[rom_])
		return;
	int val = value();
	// only load a new list if its different from the loaded one
	if (selected_rom != rom_id || preset != -1)
	{
		selected_rom = rom_id;
		char name[22];
		// load a new list
		if (preset == -1)
		{
			int number = pxk->rom[rom_]->get_attribute(type);
			const unsigned char* names = pxk->rom[rom_]->get_name(type, 0);
			clear();
			if (id_layer[0] == 1409) // instruments
			{
				for (int i = 0; i < number; i++)
				{
					snprintf(name, 22, "%04d %s", i, names + i * 16);
					add(name);
				}
			}
			else
			{
				if (id_layer[0] == 1281 || id_layer[0] == 1290) // preset links
					add("Off");
				for (int i = 0; i < number; i++)
				{
					snprintf(name, 21, "%03d %s", i, names + i * 16);
					add(name);
				}
			}
			if (val <= size() && val > 0)
			{
				select(val);
				apply_filter();
			}
		}
		// replace single item
		else
		{
			snprintf(name, 21, "%03d %s", preset, pxk->rom[rom_]->get_name(type, preset));
			if (id_layer[0] == 1281 || id_layer[0] == 1290) // preset links
				text(preset + 2, name);
			else
				text(preset + 1, name);
		}
	}
	// update instrument names in channel strips
	if (id_layer[0] == 1409)
	{
		if (val > 0 && val <= size())
			ui->main->layer_strip[id_layer[1]]->instrument->label(text(val) + 5);
	}
}

void Browser::set_filter(const char* fs)
{
	pmesg("Browser::set_filter(char*) (id:%d layer:%d)\n", id_layer[0], id_layer[1]);
	free((void*) filter);
	filter = strdup(fs);
	apply_filter();
}

void Browser::apply_filter()
{
	//pmesg("Browser::apply_filter() (id:%d layer:%d)\n", id_layer[0], id_layer[1]);
	if (!size() || !filter)
		return;
	int val = value();
	static char f[19];
	int i;
	int l = snprintf(f, 19, "*%s*", filter);
	if (l > 2)
	{
		for (i = 1; i < l - 1; i++)
			if (f[i] < (0x20 & 0x7f) || f[i] > (0x7e & 0x7f))
				f[i] = 0x3f;
		for (i = 1; i <= size(); i++)
		{
			if (fl_filename_match(text(i), f))
				show(i);
			else
				hide(i);
		}
	}
	else
		for (i = 1; i <= size(); i++)
			show(i);
	// scroll the list
	if (val > 0)
	{
		show(val);
		topline(1); // fixes issues with the scrollbar not being updated correctly
		deselect();
		select(val);
	}
}

int Browser::handle(int ev)
{
	static int key;
	switch (ev)
	{
		case FL_PUSH: // 1 = receive FL_DRAG and the matching (Fl::event_button()) FL_RELEASE event (becomes Fl::pushed())
			if (ev == Fl::event_clicks() && pxk->preset) // double click
			{
				Fl::event_clicks(-1);
				if (id_layer[0] == 643 && ui->main->arp_rom->value() == 0) //master arp browser
				{
					ui->main->edit_arp->do_callback();
					return 1;
				}
				if (id_layer[0] == 1027 && ui->preset_editor->arp_rom->value() == 0) //preset arp browser
				{
					ui->preset_editor->edit_arp->do_callback();
					return 1;
				}
				// saving with the browsers
				if (id_layer[0] == 897) // preset browser
				{
					if (ui->b_save_p->value())
					{
						pxk->preset->copy(SAVE_PRESET, -1, value() - 1);
						if (Fl::event_state(FL_SHIFT))
						{
							ui->b_save_p->clear();
							ui->b_save_p->do_callback();
						}
						return 1;
					}
					else if (ui->b_copy_p->value())
					{
						pxk->preset->copy(C_PRESET, -1, value() - 1);
						if (Fl::event_state(FL_SHIFT))
						{
							ui->b_copy_p->clear();
							ui->b_copy_p->do_callback();
						}
						return 1;
					}
				}
				if (id_layer[0] >= 0x20 && id_layer[0] <= 0x2d) // copy browsers
				{
					pxk->preset->copy(id_layer[0], -1, value() - 1); // check enum in pxk.H for id meaning
					if (Fl::event_state(FL_SHIFT))
						ui->copy_preset->hide();
					return 1;
				}
			}
			if (FL_RIGHT_MOUSE == Fl::event_button())
				return 1;
			break;
		case FL_RELEASE:
			// right-click "undo"
			if (FL_RIGHT_MOUSE == Fl::event_button() && pxk->setup_copy && pxk->preset_copy)
			{
				if (id_layer[0] == 897) // preset
				{
					// select preset in browser
					select(pxk->setup_copy->get_value(130, pxk->selected_channel) + 1);
					// set rom (will also trigger the browser callback)
					pwid[138][0]->set_value(pxk->setup_copy->get_value(138, pxk->selected_channel));
					ui->preset_rom->do_callback();
				}
				else if (id_layer[0] == 928) // preset riff
				{
					pwid[929][0]->set_value(pxk->preset_copy->get_value(929));
					ui->preset_editor->riff_rom->do_callback();
					set_value(pxk->preset_copy->get_value(id_layer[0]));
					do_callback();
				}
				else if (id_layer[0] == 1409) // instrument
				{
					pwid[1439][id_layer[1]]->set_value(pxk->preset_copy->get_value(1439, id_layer[1]));
					ui->layer_editor[id_layer[1]]->instrument_rom->do_callback();
					set_value(pxk->preset_copy->get_value(id_layer[0], id_layer[1]));
					do_callback();
				}
				else if (id_layer[0] == 278) // master riff
				{
					pwid[277][0]->set_value(pxk->setup_copy->get_value(277));
					ui->main->riff_rom->do_callback();
					set_value(pxk->setup_copy->get_value(id_layer[0]));
					do_callback();
				}
				else if (id_layer[0] == 1027) // preset arp
				{
					pwid[1042][0]->set_value(pxk->preset_copy->get_value(1042));
					ui->preset_editor->arp_rom->do_callback();
					set_value(pxk->preset_copy->get_value(id_layer[0]));
					do_callback();
				}
				else if (id_layer[0] == 1281) // link1
				{
					pwid[1299][0]->set_value(pxk->preset_copy->get_value(1299));
					ui->preset_editor->l1_rom->do_callback();
					set_value(pxk->preset_copy->get_value(id_layer[0]));
					do_callback();
				}
				else if (id_layer[0] == 1290) // link2
				{
					pwid[1300][0]->set_value(pxk->preset_copy->get_value(1300));
					ui->preset_editor->l2_rom->do_callback();
					set_value(pxk->preset_copy->get_value(id_layer[0]));
					do_callback();
				}
				else if (id_layer[0] == 643) // master arp
				{
					pwid[660][0]->set_value(pxk->setup_copy->get_value(660));
					ui->main->arp_rom->do_callback();
					set_value(pxk->setup_copy->get_value(id_layer[0]));
					do_callback();
				}
				return 1;
			}
			break;
		case FL_DRAG: // button state is in Fl::event_state() (FL_SHIFT FL_CAPS_LOCK FL_CTRL FL_ALT FL_NUM_LOCK FL_META FL_SCROLL_LOCK FL_BUTTON1 FL_BUTTON2 FL_BUTTON3)
			return 0;
			// keyboard events
		case FL_KEYDOWN: // key press (Fl::event_key())
			key = Fl::event_key();
			if (key == FL_Down)
			{
				select(value() + 1);
				return 1;
			}
			if (key == FL_Up)
			{
				select(value() - 1);
				return 1;
			}
			if (key == FL_Enter || key == 32) // copy/save with enter or space key
			{
				if (id_layer[0] == 897) // preset browser
				{
					if (ui->b_save_p->value())
					{
						pxk->preset->copy(SAVE_PRESET, -1, value() - 1);
						if (Fl::event_state(FL_SHIFT))
						{
							ui->b_save_p->clear();
							ui->b_save_p->do_callback();
						}
						return 1;
					}
					else if (ui->b_copy_p->value())
					{
						pxk->preset->copy(C_PRESET, -1, value() - 1);
						if (Fl::event_state(FL_SHIFT))
						{
							ui->b_copy_p->clear();
							ui->b_copy_p->do_callback();
						}
						return 1;
					}
				}
				if (id_layer[0] >= 0x20 && id_layer[0] <= 0x2d) // copy browsers
				{
					pxk->preset->copy(id_layer[0], -1, value() - 1); // check enum in pxk.H for id meaning
					if (Fl::event_state(FL_SHIFT))
						ui->copy_preset->hide();
					return 1;
				}
			}
			else if (key == FL_BackSpace) // cancel copy/save
			{
				if (id_layer[0] == 897) // preset browser
				{
					if (ui->b_save_p->value())
					{
						ui->b_save_p->value(0);
						ui->b_save_p->do_callback();
						return 1;
					}
					else if (ui->b_copy_p->value())
					{
						ui->b_copy_p->value(0);
						ui->b_copy_p->do_callback();
						return 1;
					}
				}
				else if (id_layer[0] >= 0x20 && id_layer[0] <= 0x2d) // copy browsers
				{
					ui->copy_preset->hide(); // check enum in pxk.H for id meaning
					return 1;
				}
			}
			break;
		case FL_KEYUP: // key release (Fl::event_key())
			if (!Fl::event_key(key) && (key == FL_Down || key == FL_Up))
			{
				do_callback();
				return 1;
			}
			break;
		case FL_DND_ENTER: // 1 = receive FL_DND_DRAG, FL_DND_LEAVE and FL_DND_RELEASE events
			return 0;
		case FL_MOUSEWHEEL:
			if (this != Fl::belowmouse())
				return 0;
	}
	return Fl_Browser::handle(ev);
}

// ###################
//
// ###################
void ROM_Choice::set_id(int v, int l)
{
	id_layer[0] = v;
	id_layer[1] = l;
	minimax[0] = 0;
	minimax[1] = 0;
	callback((Fl_Callback*) cb, (void*) id_layer);
	pwid[v][l] = this;
	// do we also include user "rom"?
	if (v == 2 || v == 138 || v == 660 || v == 1042 || v == 1299 || v == 1300) // presets and arps and links and copy browsers
		no_user = 0;
}

void ROM_Choice::set_value(int v)
{
	if (v == 0 || pxk->get_rom_index(v) != 5)
	{
		value(pxk->get_rom_index(v) - no_user);
		dependency(v, false);
	}
}

int ROM_Choice::get_value() const
{
	if (!size())
		return 0;
	int v = pxk->rom[value() + no_user]->get_attribute(ID);
	dependency(v, true);
	return v;
}

void ROM_Choice::dependency(int v, bool get) const
{
	// load new name list
	switch (id_layer[0])
	{
		case 2: // copy arp
			ui->copy_browser->load_n(PRESET, v);
			break;
		case 138: // preset
			if (v == 0)
				ui->preset_editor->copy_arp_b->activate();
			else
				ui->preset_editor->copy_arp_b->deactivate();
			ui->preset->load_n(PRESET, v);
			break;
		case 277: // master riff
			if (get)
			{
				ui->main->riff->select(1);
				pxk->setup->set_value(278, 0);
			}
			ui->main->riff->load_n(RIFF, v);
			break;
		case 660: // master arp pattern
			if (get)
			{
				ui->main->arp->set_value(0);
				pxk->setup->set_value(643, 0);
			}
			ui->main->arp->load_n(ARP, v);
			if (v)
			{
				ui->main->edit_arp->deactivate();
				if (ui->main->g_main_arp->get_value() == 1)
					ui->main->main_edit_arp->deactivate();
			}
			else
			{
				ui->main->edit_arp->activate();
				if (ui->main->g_main_arp->get_value() == 1)
					ui->main->main_edit_arp->activate();
			}
			break;
		case 929: // preset riff
			if (get)
			{
				ui->preset_editor->riff->select(1);
				pxk->preset->set_value(928, 0);
			}
			ui->preset_editor->riff->load_n(RIFF, v);
			break;
		case 1042: // preset arp pattern
			if (get)
			{
				ui->preset_editor->arp->set_value(0);
				pxk->preset->set_value(1027, 0);
			}
			ui->preset_editor->arp->load_n(ARP, v);
			if (v)
			{
				ui->preset_editor->edit_arp->deactivate();
				if (ui->main->g_main_arp->get_value() == 0 || ui->main->g_main_arp->get_value() == -1)
					ui->main->main_edit_arp->deactivate();
			}
			else
			{
				ui->preset_editor->edit_arp->activate();
				if (ui->main->g_main_arp->get_value() == 0 || ui->main->g_main_arp->get_value() == -1)
					ui->main->main_edit_arp->activate();
			}
			break;
		case 1299: // link 1
			if (get)
			{
				ui->preset_editor->l1->select(2);
				pxk->preset->set_value(1281, 0);
				ui->preset_editor->g_link1->activate();
			}
			ui->preset_editor->l1->load_n(PRESET, v);
			break;
		case 1300: // link 2
			if (get)
			{
				ui->preset_editor->l2->select(2);
				pxk->preset->set_value(1290, 0);
				ui->preset_editor->g_link2->activate();
			}
			ui->preset_editor->l2->load_n(PRESET, v);
			break;
		case 1439: // instrument rom
			if (get)
			{
				ui->layer_editor[id_layer[1]]->instrument->select(1);
				pxk->preset->set_value(1409, 0, id_layer[1]);
			}
			ui->layer_editor[id_layer[1]]->instrument->load_n(INSTRUMENT, v);
			break;
	}
}

int ROM_Choice::handle(int ev)
{
	switch (ev)
	{
		case FL_PUSH: // 1 = receive FL_DRAG and the matching (Fl::event_button()) FL_RELEASE event (becomes Fl::pushed())
			if (FL_RIGHT_MOUSE == Fl::event_button())
				return 1;
			break;
		case FL_RELEASE:
			if (FL_RIGHT_MOUSE == Fl::event_button() && pxk->setup_copy && pxk->preset_copy)
			{
				if (id_layer[0] < 788) // master setting
					set_value((double) pxk->setup_copy->get_value(id_layer[0], pxk->selected_channel));
				else
					set_value((double) pxk->preset_copy->get_value(id_layer[0], id_layer[1]));
				do_callback();
				return 1;
			}
			break;
		case FL_MOUSEWHEEL:
			if (this == Fl::belowmouse())
			{
				int dy = Fl::event_dy();
				double v1 = value();
				double v2 = v1 + dy;
				if (v2 < 0)
					v2 = size() - 2;
				else if (v2 > size() - 2)
					v2 = 0;
				value(v2);
				do_callback();
				return 1;
			}
			break;
	}
	return Fl_Choice::handle(ev);
}

// ###################
// name and filter inputs
//
// ###################
int Input::handle(int ev)
{
	switch (ev)
	{
		case FL_KEYUP: // don't unfocus if we press enter
		case FL_KEYDOWN:
			if (Fl::event_key() == FL_Enter)
			{
				if (ev == FL_KEYDOWN)
					mark(0);
				return 1;
			}
			break;
	}
	return Fl_Input::handle(ev);
}

// ###################
// value in/output widget
//
// ###################
void Value_Input::set_id(int v, int l)
{
	id_layer[0] = v;
	id_layer[1] = l;
	callback((Fl_Callback*) cb, (void*) id_layer);
	pwid[v][l] = this;
}

void Value_Input::set_value(int v)
{
	value((double) v);
	ui->forma_out->value((double) v);
}

int Value_Input::get_value() const
{
	return (int) value();
}

int Value_Input::handle(int ev)
{
	if (!pwid_editing)
		return 0;
	switch (ev)
	{
		case FL_LEAVE:
			fl_cursor(FL_CURSOR_DEFAULT);
			break;
		case FL_MOUSEWHEEL:
			if (this == Fl::belowmouse())
			{
				int dy = Fl::event_dy() * -1;
				double v1 = value(); // current value
				double v2 = clamp(increment(v1, dy));
				if (v1 == v2)
					return 1;
				value(v2);
				do_callback();
				return 1;
			}
			break;
		case FL_PUSH:
			if (FL_RIGHT_MOUSE == Fl::event_button())
				return 1;
			break;
		case FL_RELEASE:
			if (FL_RIGHT_MOUSE == Fl::event_button() && pxk->setup_copy && pxk->preset_copy) // reset to initial value
			{
				int* la_id = pwid_editing->get_id_layer();
				if (la_id[0] < 788) // master setting
					value((double) pxk->setup_copy->get_value(la_id[0], pxk->selected_channel));
				else
					// preset
					value((double) pxk->preset_copy->get_value(la_id[0], la_id[1]));
				do_callback();
				return 1;
			}
			fl_cursor(FL_CURSOR_DEFAULT);
			if ((when() & FL_WHEN_ENTER_KEY) && changed())
			{
				do_callback();
				return 1;
			}
			break;
		case FL_KEYUP: // don't unfocus if we press enter
		case FL_KEYDOWN:
			if ((when() & FL_WHEN_CHANGED) && Fl::event_key() == FL_Enter)
			{
				if (ev == FL_KEYDOWN)
					input.mark(0);
				return 1;
			}
			break;
		case FL_DRAG:
			if (FL_BUTTON3 & Fl::event_state()) // prevent value dragging with right mouse button
				return 1;
			// prevent dragging when we are editing the preset browser or channel
			if (((int*) pwid_editing->get_id_layer())[0] == 897 || ((int*) pwid_editing->get_id_layer())[0] == 129)
				return 1;
			break;
		case FL_DND_ENTER:
			return 0;
	}
	return Fl_Value_Input::handle(ev);
}

// ###################
// Formatted output
// ###################
void Formatted_Output::set_value(int p, int l, int v)
{
	id = p;
	layer = l;
	value((double) v);
	redraw();
}

int Formatted_Output::format(char *buf)
{
	switch (id)
	{
		case 129: // selected channel
		{
			return sprintf(buf, "Channel %2d selected", (int) value() + 1);
		}
		case 140: // fx channel
		{
			if ((int) value() == -1)
				return sprintf(buf, "Master FX");
			return sprintf(buf, "Channel %2d FX", (int) value() + 1);
		}
		case 132: // channel pan
		{
			int v = (int) value();
			if (v == 64)
				return sprintf(buf, "Center");
			return sprintf(buf, "%2d %s", abs(v - 64), v < 64 ? "L" : "R");
		}
		case 259: // Master transpose
		case 1425:
		{
			const char* transpose_values[] =
			{ "C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B " };
			int v = (int) value();
			int p = v % 12;
			if (v < 0 && p != 0)
				p += 12;
			return sprintf(buf, "%s (%+4d semitones)", transpose_values[p], v);
		}
		case 1284: // link transpose
		case 1293:
			return sprintf(buf, "%+3d semitones", (int) value());
		case 257: // BPM
		{
			int v = (int) value();
			if (v == 0)
				return sprintf(buf, "External MIDI Clock");
			return sprintf(buf, "%d", v);
		}
		case 260: // Master tune
		case 1426:
		{
			int v = (int) value();
			const char* cents[] =
			{ "0", "1.2", "3.5", "4.7", "6.0", "7.2", "9.5", "10.7", "12.0", "14.2", "15.5", "17.7", "18.0", "20.2", "21.5",
					"23.7", "25.0", "26.2", "28.5", "29.7", "31.0", "32.2", "34.5", "35.7", "37.0", "39.2", "40.5", "42.7",
					"43.0", "45.2", "46.6", "48.7", "50.0", "51.2", "53.5", "54.7", "56.0", "57.2", "59.5", "60.7", "62.0",
					"64.2", "65.5", "67.7", "68.0", "70.2", "71.5", "73.7", "75.0", "76.2", "78.5", "79.7", "81.0", "82.2",
					"84.5", "85.7", "87.0", "89.2", "90.5", "92.7", "93.0", "95.2", "96.6", "98.7" };
			if (v >= 0)
				return sprintf(buf, "+%4s cents (%+3d/64)", cents[v], v);
			else
				return sprintf(buf, "-%4s cents (%+3d/64)", cents[abs(v)], v);
		}
		case 402: // Tempo control
		case 403:
		{
			int v = (int) value();
			if (v == -1)
				return sprintf(buf, "Pitch Wheel");
			if (v == -2)
				return sprintf(buf, "Mono Pressure");
			if (v == -3)
				return sprintf(buf, "Off");
			return sprintf(buf, "Ctrl %i", v);
		}
		case 1163: // FX Delay
		case 523:
		{
			int v = (int) value();
			const char* delay[] =
			{ "1/32  32nd", "1/16t 16th triplet", "1/32d dotted 32th", "1/16  16th", "1/8t  8th triplet", "1/16d dotted 16th",
					"1/8   8th", "1/4t  quarter triplet", "1/8d  dotted 8th", "1/4   quarter", "1/2t  half triplet",
					"1/4d  dotted quarter" };
			if (v < 0)
				return sprintf(buf, "%s", delay[abs(v) - 1]);
			return sprintf(buf, "%d ms", v * 5);
		}
		case 644: // Arp note
		case 651:
		case 650:
		case 661:
		case 1028:
		case 1034:
		case 1035:
		case 1043:
		{
			int v = (int) value();
			if (v)
				return sprintf(buf, "%s", rates[25 - v]);
			return sprintf(buf, "%s", "Off");
		}
		case 915: // initial controller amounts
		case 916:
		case 917:
		case 918:
		case 919:
		case 920:
		case 921:
		case 922:
		case 924:
		case 925:
		case 926:
		case 927:
		{
			int v = (int) value();
			if (v == -1)
				return sprintf(buf, "Current Controller Value");
			return sprintf(buf, "%d", v);
		}
		case 1029: // arp velocity
		case 645:
		{
			int v = (int) value();
			if (v == 0)
				return sprintf(buf, "As played");
			return sprintf(buf, "%d", v);
		}
		case 1030: // arp note gate
		case 1427: // chorus
		case 1428:
		{
			return sprintf(buf, "%3d %%", (int) value());
		}
		case 1410: // layer volume
		case 1282:
		case 1291:
		{
			return sprintf(buf, "%+3d db", (int) value());
		}
		case 1411: // layer pan
		case 1283:
		case 1292:
		{
			int v = (int) value();
			if (v == 0)
				return sprintf(buf, "Center");
			return sprintf(buf, "%2d %s", abs(v), v < 0 ? "L" : "R");
		}
		case 1431: // layer bend range
		{
			int v = (int) value();
			if (v == -1)
				return sprintf(buf, "Master Bend Range");
			else if (v == 0)
				return sprintf(buf, "Disabled");
			else
				return sprintf(buf, "+/- %2d semitones", v);
		}
		case 1432: // Glide rate
		{
			const unsigned char envunits1[] =
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4,
					4, 4, 5, 5, 5, 5, 6, 6, 7, 7, 7, 8, 8, 9, 9, 10, 11, 11, 12, 13, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23, 24,
					26, 28, 30, 32, 34, 36, 38, 41, 44, 47, 51, 55, 59, 64, 70, 76, 83, 91, 100, 112, 125, 142, 163, };
			const unsigned char envunits2[] =
			{ 00, 01, 02, 03, 04, 05, 06, 07, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 25, 26, 28, 29,
					32, 34, 36, 38, 41, 43, 46, 49, 52, 55, 58, 62, 65, 70, 74, 79, 83, 88, 93, 98, 04, 10, 17, 24, 31, 39, 47,
					56, 65, 74, 84, 95, 06, 18, 31, 44, 59, 73, 89, 06, 23, 42, 62, 82, 04, 28, 52, 78, 05, 34, 64, 97, 32, 67,
					06, 46, 90, 35, 83, 34, 87, 45, 06, 70, 38, 11, 88, 70, 56, 49, 48, 53, 65, 85, 13, 50, 97, 54, 24, 06, 02,
					15, 44, 93, 64, 60, 84, 41, 34, 70, 56, 03, 22, 28, 40, 87, 9, 65, 36, 69, };
			int v = (int) value();
			int msec = (envunits1[v] * 1000 + envunits2[v] * 10) / 5;
			return sprintf(buf, "%02d.%03d seconds/octave", msec / 1000, msec % 1000);
		}
		case 1433: // glide curve
		{
			int v = (int) value();
			if (v == 0)
				return sprintf(buf, "Linear");
			return sprintf(buf, "Exponential %d", v);
		}
		case 1435: // Sample Delay
		case 1667: // LFO Delay
		case 1672:
		case 1285:
		case 1294:
		{
			int v = (int) value();
			if (v < 0)
			{
				return sprintf(buf, "%s", rates[25 + v]);
			}
			else
				return sprintf(buf, "%d", v);
		}
		case 1538: // Cutoff frequency
		{
			int v = (int) value();
			int maxfreq = 0, mul = 0;
			int filter_selected = pwid[1537][layer]->get_value();
			if (filter_selected == 0 || filter_selected == 1 || filter_selected == 2)
			{
				maxfreq = 20000;
				mul = 1002;
			}
			else if (filter_selected == 8 || filter_selected == 9)
			{
				maxfreq = 18000;
				mul = 1003;
			}
			else if (filter_selected == 16 || filter_selected == 17 || filter_selected == 18 || filter_selected == 32
					|| filter_selected == 33 || filter_selected == 34 || filter_selected == 64 || filter_selected == 65
					|| filter_selected == 66 || filter_selected == 72)
			{
				maxfreq = 10000;
				mul = 1006;
			}
			else
				return sprintf(buf, "%d", v);

			int input = 255 - v;
			while (input-- > 0)
				maxfreq *= mul, maxfreq /= 1024;
			return sprintf(buf, "%5d Hz", maxfreq);
		}
		case 1665: // LFO rates
		case 1670:
		{
			int v = (int) value();
			if (v < 0)
				return sprintf(buf, "%s", rates[25 + v]);
			else
			{
				const unsigned char lfounits1[] =
				{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
						1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5,
						5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 10, 11, 11,
						11, 11, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 15, 15, 15, 16, 16, 17, 17, 17, 18, };

				const unsigned char lfounits2[] =
				{ 8, 11, 15, 18, 21, 25, 28, 32, 35, 39, 42, 46, 50, 54, 58, 63, 67, 71, 76, 80, 85, 90, 94, 99, 04, 10, 15, 20,
						25, 31, 37, 42, 48, 54, 60, 67, 73, 79, 86, 93, 00, 07, 14, 21, 29, 36, 44, 52, 60, 68, 77, 85, 94, 03, 12,
						21, 31, 40, 50, 60, 70, 81, 91, 02, 13, 25, 36, 48, 60, 72, 84, 97, 10, 23, 37, 51, 65, 79, 94, 8, 24, 39,
						55, 71, 88, 04, 21, 39, 57, 75, 93, 12, 32, 51, 71, 92, 13, 34, 56, 78, 00, 23, 47, 71, 95, 20, 46, 71, 98,
						25, 52, 80, 9, 38, 68, 99, 30, 61, 93, 26, 60, 94, 29, 65, 01, 38, 76, 14, };
				return sprintf(buf, "%2d.%02d Hz", lfounits1[v], lfounits2[v]);
			}
		}
		case 933: // preset controller amounts
		case 936:
		case 939:
		case 942:
		case 945:
		case 948:
		case 951:
		case 954:
		case 957:
		case 960:
		case 963:
		case 966:
		case 1923: // layer controller amounts
		case 1926:
		case 1929:
		case 1932:
		case 1935:
		case 1938:
		case 1941:
		case 1944:
		case 1947:
		case 1950:
		case 1953:
		case 1956:
		case 1959:
		case 1962:
		case 1965:
		case 1968:
		case 1971:
		case 1974:
		case 1977:
		case 1980:
		case 1983:
		case 1986:
		case 1989:
		case 1992:
		{
			int v = (int) value();
			switch (abs(v))
			{
				case 3:
					return sprintf(buf, "%0+4d ( 1 semitone)", v);
				case 6:
					return sprintf(buf, "%0+4d ( 2 semitones)", v);
				case 9:
					return sprintf(buf, "%0+4d ( 3 semitones approx.)", v);
				case 12:
					return sprintf(buf, "%0+4d ( 4 semitones approx.)", v);
				case 16:
					return sprintf(buf, "%0+4d ( 5 semitones)", v);
				case 19:
					return sprintf(buf, "%0+4d ( 6 semitones)", v);
				case 22:
					return sprintf(buf, "%0+4d ( 7 semitones)", v);
				case 25:
					return sprintf(buf, "%0+4d ( 8 semitones)", v);
				case 28:
					return sprintf(buf, "%0+4d ( 9  semitones)", v);
				case 31:
					return sprintf(buf, "%0+4d (10 semitones approx.)", v);
				case 35:
					return sprintf(buf, "%0+4d (11 semitones)", v);
				case 38:
					return sprintf(buf, "%0+4d (12 semitones) [Pat]", v);
				case 41:
					return sprintf(buf, "%0+4d (13 semitones)", v);
				case 44:
					return sprintf(buf, "%0+4d (14 semitones)", v);
				case 47:
					return sprintf(buf, "%0+4d (15 semitones)", v);
				case 50:
					return sprintf(buf, "%0+4d (16 semitones)", v);
				case 53:
					return sprintf(buf, "%0+4d (17 semitones approx.)", v);
				case 57:
					return sprintf(buf, "%0+4d (18 semitones)", v);
				case 60:
					return sprintf(buf, "%0+4d (19 semitones)", v);
				case 63:
					return sprintf(buf, "%0+4d (20 semitones)", v);
				case 66:
					return sprintf(buf, "%0+4d (21 semitones)", v);
				case 69:
					return sprintf(buf, "%0+4d (22 semitones)", v);
				case 72:
					return sprintf(buf, "%0+4d (23 semitones approx.)", v);
				case 76:
					return sprintf(buf, "%0+4d (24 semitones approx.)", v);
				case 79:
					return sprintf(buf, "%0+4d (25 semitones)", v);
				case 82:
					return sprintf(buf, "%0+4d (26 semitones)", v);
				case 88:
					return sprintf(buf, "%0+4d (27 semitones)", v);
				case 91:
					return sprintf(buf, "%0+4d (28 semitones)", v);
				case 95:
					return sprintf(buf, "%0+4d (29 semitones approx.)", v);
				case 98:
					return sprintf(buf, "%0+4d (30 semitones)", v);
				default:
					return sprintf(buf, "%0+4d", v);
			}
			break;
		}
		default:
			return sprintf(buf, "%d", (int) value());
	}
}

// ###################
//
// ###################
void Value_Output::set_id(int v, int l)
{
	id_layer[0] = v;
	id_layer[1] = l;
	minimax[0] = (int) minimum();
	minimax[1] = (int) maximum();
	callback((Fl_Callback*) cb, (void*) id_layer);
	pwid[v][l] = this;
}

void Value_Output::set_value(int v)
{
	value((double) v);
}

int Value_Output::get_value() const
{
	return (int) value();
}

int Value_Output::handle(int ev)
{
	switch (ev)
	{
		case FL_PUSH: // 1 = receive FL_DRAG and the matching (Fl::event_button()) FL_RELEASE event (becomes Fl::pushed())
			if (FL_RIGHT_MOUSE == Fl::event_button())
				return 1;
			if (Fl::event_clicks())
			{
				Fl::event_clicks(-1);
				if ((id_layer[0] >= 1921 && id_layer[0] <= 1992) || (id_layer[0] >= 933 && id_layer[0] <= 966)) // double click on patchcord amounts
				{

					if (value())
					{
						double_click_value = (int) value();
						value(0);
					}
					else
						value((double) double_click_value);
					do_callback();
					return 1;
				}
			}
			break;
		case FL_RELEASE:
			if (FL_RIGHT_MOUSE == Fl::event_button())
			{
				if (pxk->setup_copy && pxk->preset_copy && id_layer[0] != -1)
				{
					if (id_layer[0] < 788) // master setting
						value((double) pxk->setup_copy->get_value(id_layer[0], pxk->selected_channel));
					else
						value((double) pxk->preset_copy->get_value(id_layer[0], id_layer[1]));
					do_callback();
				}
				return 1;
			}
			break;
		case FL_MOUSEWHEEL:
			if (this == Fl::belowmouse())
			{
				int dy = Fl::event_dy();
				double v1 = value();
				double v2 = v1 - dy;
				if (v2 < minimum())
					v2 = maximum();
				else if (v2 > maximum())
					v2 = minimum();
				value(v2);
				do_callback();
				return 1;
			}
			break;
	}
	return Fl_Value_Output::handle(ev);
}

// ###################
//
// ###################
void Slider::set_id(int v, int l)
{
	id_layer[0] = v;
	id_layer[1] = l;
	if (v == 1410) // layer volume
	{
		minimax[0] = -96;
		minimax[1] = 10;
	}
	else
	{
		minimax[0] = (int) maximum();
		minimax[1] = (int) minimum();
	}
	callback((Fl_Callback*) cb, (void*) id_layer);
	pwid[v][l] = this;
}

void Slider::set_value(int v)
{
	if (id_layer[0] == 1410) // layer volume
	{
		prev_value = v;
		value(pow(v + 96, 3)); // cubic slider
		return;
	}
	value((double) v);
}

int Slider::get_value() const
{
	if (id_layer[0] == 1410) // layer volume
	{
		int val = (int) cbrt(value()) - 96;
		if (val != prev_value)
		{
			prev_value = val;
			return val;
		}
		else
			return -999;
	}
	return (int) value();
}

void Slider::draw()
{
	int X = x() + Fl::box_dx(box());
	int Y = y() + Fl::box_dy(box());
	int W = w() - Fl::box_dw(box());
	int H = h() - Fl::box_dh(box());
	double val;
	if (minimum() == maximum())
		val = 0.5;
	else
	{
		val = (value() - minimum()) / (maximum() - minimum());
		if (val > 1.0)
			val = 1.0;
		else if (val < 0.0)
			val = 0.0;
	}
	int ww = H;
	int xx, S;
	S = 17;
	xx = int(val * (ww - S) + .5);
	int xsl, ysl, wsl, hsl;
	ysl = Y + xx;
	hsl = S;
	xsl = X;
	wsl = W;
	fl_push_clip(X, Y, W, H);
	if (Fl::focus() == this)
		draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_SELECTION_COLOR);
	else
		draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_BACKGROUND2_COLOR);
	fl_pop_clip();
	if (wsl > 0 && hsl > 0)
		draw_box(FL_BORDER_BOX, xsl, ysl, wsl, hsl, FL_BACKGROUND_COLOR);
	draw_label(xsl, ysl, wsl, hsl);
}

// mousewheel support for slider
int Slider::handle(int ev)
{
	switch (ev)
	{
		case FL_ENTER: // 1 = receive FL_LEAVE and FL_MOVE events (widget becomes Fl::belowmouse())
			if (active_r())
			{
				take_focus();
				redraw();
				return 1;
			}
			return 0;
		case FL_FOCUS: // 1 = receive FL_KEYDOWN, FL_KEYUP, and FL_UNFOCUS events (widget becomes Fl::focus())
			// show value in value field and make ourselfes the editing widget
			if (pwid_editing != this && id_layer[0] != 0)
			{
				pwid_editing = this;
				ui->value_input->minimum((double) minimax[0]);
				ui->value_input->maximum((double) minimax[1]);
			}
			if (id_layer[0] == 1410) // layer volume fader
			{
				int v = cbrt(value()) - 96;
				ui->value_input->value((double) v);
				ui->forma_out->set_value(id_layer[0], id_layer[1], v);
			}
			else
			{
				ui->value_input->value((double) value());
				ui->forma_out->set_value(id_layer[0], id_layer[1], value());
			}
			return 1;
		case FL_UNFOCUS: // received when another widget gets the focus and we had the focus
			redraw();
			return 1;
		case FL_PUSH:
			if (FL_RIGHT_MOUSE == Fl::event_button())
				return 1;
			break;
		case FL_RELEASE:
			if (FL_RIGHT_MOUSE == Fl::event_button())
			{
				if (pxk->setup_copy && pxk->preset_copy)
				{
					if (id_layer[0] == 1410) // layer volume fader
					{
						int v = pxk->preset_copy->get_value(id_layer[0], id_layer[1]);
						value((double) pow(v + 96, 3));
						prev_value = v;
					}
					else
					{
						if (id_layer[0] < 788) // master setting
							value((double) pxk->setup_copy->get_value(id_layer[0], pxk->selected_channel));
						else
							value((double) pxk->preset_copy->get_value(id_layer[0], id_layer[1]));
					}
					do_callback();
				}
				return 1;
			}
			break;
		case FL_DRAG:
			if (FL_BUTTON3 & Fl::event_state())
				return 1;
			break;
		case FL_MOUSEWHEEL:
			if (this == Fl::belowmouse())
			{
				if (id_layer[0] == 1410) // layer volume
				{
					int dy = Fl::event_dy();
					int v = prev_value - dy;
					if (v < -96)
						v = -96;
					else if (v > 10)
						v = 10;
					if (v == prev_value)
						return 1;
					value(pow(v + 96, 3));
					do_callback();
					prev_value = v;
				}
				else
				{
					int dy = Fl::event_dy();
					double v1 = value();
					double v2 = clamp(increment(v1, dy));
					if (v1 == v2)
						return 1;
					value(v2);
					do_callback();
				}
				return 1;
			}
			break;
			// keyboard events
		case FL_KEYDOWN:
			// key press (Fl::event_key())
			if (id_layer[0] == 1410)
			{
				int key = Fl::event_key();
				int v = prev_value;
				switch (key)
				{
					case FL_Up:
						if (Fl::event_shift())
							v = 0;
						else
						{
							++v;
							if (v < -96)
								v = -96;
							else if (v > 10)
								v = 10;
						}
						break;
					case FL_Down:
						if (Fl::event_shift())
							v = 0;
						else
						{
							--v;
							if (v < -96)
								v = -96;
							else if (v > 10)
								v = 10;
						}
						break;
					case FL_Left:
						if (Fl::event_shift())
							v = -96;
						else
							return 0;
						break;
					case FL_Right:
						if (Fl::event_shift())
							v = 10;
						else
							return 0;
						break;
					default:
						return 0;
				}
				if (v != prev_value)
				{
					value(pow(v + 96, 3));
					do_callback();
					prev_value = v;
				}
				return 1;
			}
			break;
	}
	return Fl_Slider::handle(ev);
}

// ###################
//
// ###################
void Spinner::set_id(int v, int l)
{
	id_layer[0] = v;
	id_layer[1] = l;
	minimax[0] = (int) minimum();
	minimax[1] = (int) maximum();
	callback((Fl_Callback*) cb, (void*) id_layer);
	pwid[v][l] = this;
}

void Spinner::set_value(int v)
{
	value((double) v);
}

int Spinner::get_value() const
{
	return (int) value();
}

int Spinner::handle(int ev)
{
	switch (ev)
	{
		case FL_PUSH:
			if (FL_RIGHT_MOUSE == Fl::event_button())
				return 1;
			break;
		case FL_RELEASE:
			if (FL_RIGHT_MOUSE == Fl::event_button() && pxk->setup_copy && pxk->preset_copy)
			{
				if (id_layer[0] < 788) // master setting
					value((double) pxk->setup_copy->get_value(id_layer[0], pxk->selected_channel));
				else
					value((double) pxk->preset_copy->get_value(id_layer[0], id_layer[1]));
				do_callback();
				return 1;
			}
			break;
		case FL_MOUSEWHEEL:
			if (this == Fl::belowmouse()->parent())
			{
				int dy = Fl::event_dy() * -1;
				double v1 = value();
				double v2 = v1 + dy;
				if (v2 < minimum())
					v2 = minimum();
				else if (v2 > maximum())
					v2 = maximum();
				value(v2);
				do_callback();
				return 1;
			}
	}
	return Fl_Spinner::handle(ev);
}

// ###################
//
// ###################
void Counter::set_id(int v, int l)
{
	id_layer[0] = v;
	id_layer[1] = l;
	minimax[0] = (int) minimum();
	minimax[1] = (int) maximum();
	callback((Fl_Callback*) cb, (void*) id_layer);
	pwid[v][l] = this;
	lstep(10);
}

void Counter::set_value(int v)
{
	value((double) v);
}

int Counter::get_value() const
{
	return (int) value();
}

int Counter::handle(int ev)
{
	switch (ev)
	{
		case FL_PUSH:
			if (FL_RIGHT_MOUSE == Fl::event_button())
				return 1;
			break;
		case FL_RELEASE:
			if (FL_RIGHT_MOUSE == Fl::event_button() && pxk->setup_copy && pxk->preset_copy)
			{
				if (id_layer[0] < 788) // master setting
					value((double) pxk->setup_copy->get_value(id_layer[0], pxk->selected_channel));
				else
					value((double) pxk->preset_copy->get_value(id_layer[0], id_layer[1]));
				do_callback();
				return 1;
			}
			break;
		case FL_MOUSEWHEEL:
			if (this == Fl::belowmouse())
			{
				int dy = Fl::event_dy() * -1;
				double v1 = value(); // current value
				double v2 = clamp(increment(v1, dy));
				if (v1 == v2)
					return 1;
				value(v2);
				do_callback();
				return 1;
			}
	}
	return Fl_Counter::handle(ev);
}

// ###################
//
// ###################
void Group::set_id(int v, int l)
{
	id_layer[0] = v;
	id_layer[1] = l;
	minimax[0] = 0;
	switch (v)
	{
		case 129: // channel
			minimax[1] = 15;
			break;
		case 134: // multimode arp
			minimax[0] = -2;
			minimax[1] = 1;
			break;
		case 1026: // arp mode
		case 642:
			minimax[1] = 7;
			break;
		case 1036: // arp recycle
		case 652:
			minimax[1] = 2;
			break;
		case 1041: // arp pattern speed
		case 659:
			minimax[0] = -2;
			minimax[1] = 2;
			break;
		case 1412: // layer mix out
			minimax[1] = 3;
			break;
		default:
			minimax[1] = 0;
	}
	callback((Fl_Callback*) cb, (void*) id_layer);
	pwid[v][l] = this;
}

void Group::set_value(int v)
{
	//pmesg("Group::set_value(%d) (id:%d layer:%d)\n", v, id_layer[0], id_layer[1]);
	int childs = children();
	if (id_layer[0] == 1041 || id_layer[0] == 659 || id_layer[0] == 134) // arp pattern speed / multimode arp
	{
		if (childs > v + 2 && v >= -2)
			((Fl_Button*) child(v + 2))->setonly();
		else
			return;
	}
	else if (id_layer[0] == 133 || id_layer[0] == 930) // mix out/tempo offset
	{
		if (childs > v + 1 && v >= -1)
			((Fl_Button*) child(v + 1))->setonly();
		else
			return;
	}
	else
	{
		if (childs > v && v >= 0)
			((Fl_Button*) child(v))->setonly();
		else
			return;
	}
	dependency(v);
}

int Group::get_value() const
{
	int v = 0;
	for (unsigned char i = 0; i < children(); i++)
	{
		if (((Fl_Button*) array()[i])->value())
		{
			if (id_layer[0] == 1041 || id_layer[0] == 659 || id_layer[0] == 134)
				v = i - 2;
			else if (id_layer[0] == 133 || id_layer[0] == 930)
				v = i - 1;
			else
				v = i;
			break;
		}
	}
	dependency(v);
	return v;
}

int Group::handle(int ev)
{
	switch (ev)
	{
		case FL_PUSH:
			if (FL_RIGHT_MOUSE == Fl::event_button())
				return 1;
			break;
		case FL_RELEASE:
			if (FL_RIGHT_MOUSE == Fl::event_button() && pxk->setup_copy && pxk->preset_copy)
			{
				if (id_layer[0] < 788 && id_layer[0] != 129) // master setting
					set_value(pxk->setup_copy->get_value(id_layer[0], pxk->selected_channel));
				else
					set_value(pxk->preset_copy->get_value(id_layer[0], id_layer[1]));
				do_callback();
				return 1;
			}
			break;
	}
	return Fl_Group::handle(ev);
}

void Group::dependency(int v) const
{
	if (id_layer[0] == 129) // channel select
	{
		if (MULTI != pxk->midi_mode || v == pxk->selected_fx_channel || -1 == pxk->selected_fx_channel)
			ui->fx->activate();
		else
			ui->fx->deactivate();
	}
	else if (id_layer[0] == 133) // mix out
	{
		if (v == -1)
			for (int i = 0; i < 4; i++)
				ui->main->layer_strip[i]->mix_out->activate();
		else if (MULTI == pxk->midi_mode)
			for (int i = 0; i < 4; i++)
				ui->main->layer_strip[i]->mix_out->deactivate();
	}
	else if (id_layer[0] == 134) // arp mode
	{
		if ((v == 0 || v == -1) && ui->preset_editor->arp_rom->value() == 0) // preset arp
			ui->main->main_edit_arp->activate();
		else if (v == 1 && ui->main->arp_rom->value() == 0) // master arp
			ui->main->main_edit_arp->activate();
		else
			ui->main->main_edit_arp->deactivate();
	}
	else if (id_layer[0] == 1026) // preset arp mode
	{
		if (v != 7)
		{
			ui->preset_editor->g_pattern->deactivate();
			ui->preset_editor->g_pattern_speed->deactivate();
			ui->preset_editor->note->activate();
		}
		else
		{
			ui->preset_editor->g_pattern->activate();
			ui->preset_editor->g_pattern_speed->activate();
			ui->preset_editor->note->deactivate();
		}
	}
	else if (id_layer[0] == 642) // master arp mode
	{
		if (v != 7)
		{
			ui->main->g_pattern->deactivate();
			ui->main->g_pattern_speed->deactivate();
			ui->main->note->activate();
		}
		else
		{
			ui->main->g_pattern->activate();
			ui->main->g_pattern_speed->activate();
			ui->main->note->deactivate();
		}
	}
}

// ###################
//
// ###################
Fl_Knob::Fl_Knob(int xx, int yy, int ww, int hh, const char *l) :
		Fl_Valuator(xx, yy, ww, hh, l)
{
	id_layer[0] = 0;
	id_layer[1] = 0;
	a1 = 35;
	a2 = 325;
	_percent = 0.7;
	_scaleticks = 12;
}

void Fl_Knob::set_id(int v, int l)
{
	id_layer[0] = v;
	id_layer[1] = l;
	minimax[0] = (int) minimum();
	minimax[1] = (int) maximum();
	callback((Fl_Callback*) cb, (void*) id_layer);
	pwid[v][l] = this;
	switch (v)
	{
		case 259: // master transpose
			_scaleticks = 4;
			break;
		case 1425: // coarse tune
			_scaleticks = 6;
			break;
		case 1433: // glide curve
			_scaleticks = 8;
			break;
		case 1435: // delay
		case 1436: // offset
		case 1665: // lfo1 rate
		case 1670: // lfo2 rate
		case 1667: // lfo1 delay
		case 1672: // lfo2 delay
		case 1285: // link1 delay
		case 1294: // link2 delay
			_scaleticks = 16;
			break;
		case 1538: // cutoff
		case 1284: // link1 transpose
		case 1293: // link2 transpose
			_scaleticks = 24;
			break;
		case 1431: // bend
			_scaleticks = 13;
			break;
		case 1427: // chorus
		case 1428: // width
		case 517: // (master) fx sends
		case 518:
		case 519:
		case 527:
		case 524:
		case 525:
		case 526:
		case 528:
		case 1157: // fx sends
		case 1158:
		case 1159:
		case 1167:
		case 1164:
		case 1165:
		case 1166:
		case 1168:
		case 1668: // lfo1 variation
		case 1673: // lfo2 variation
		case 646: // arp gate
		case 1030:
			_scaleticks = 10;
			break;
		case 514: // (master) fxa decay
		case 1154: // fxa decay
			_scaleticks = 9;
			break;
		case 523: // (master) fxb delay
		case 1163: // fxb delay
			_scaleticks = 14;
			break;
		case 1028: // arp note delay
		case 644:
		case 651: // arp duration
		case 1035:
		case 1034: // pre delay
		case 650:
		case 1043: // post delay
		case 661:
			_scaleticks = 19;
			break;
		case 1031: // arp ext count
		case 647:
		case 648: // arp interval
		case 1032:
			_scaleticks = 15;
			break;
	}
}

void Fl_Knob::set_value(int v)
{
	if (id_layer[0] == 1665 || id_layer[0] == 1670) // lfo rates
		dependency(v);
	value((double) v);
}

int Fl_Knob::get_value() const
{
	int v = (int) value();
	if (id_layer[0] == 1665 || id_layer[0] == 1670) // lfo rates
		dependency(v);
	return v;
}

void Fl_Knob::dependency(int v) const
{
	if (id_layer[0] == 1665) // lfo1 rate
	{
		if (v < 0)
			ui->layer_editor[id_layer[1]]->lfo1_variation->deactivate();
		else
			ui->layer_editor[id_layer[1]]->lfo1_variation->activate();
	}
	else if (id_layer[0] == 1670) // lfo2 rate
	{
		if (v < 0)
			ui->layer_editor[id_layer[1]]->lfo2_variation->deactivate();
		else
			ui->layer_editor[id_layer[1]]->lfo2_variation->activate();
	}
}

void Fl_Knob::draw()
{
	int ox, oy, ww, hh, side;
	ox = x();
	oy = y();
	ww = w();
	hh = h();
	draw_label();
	fl_clip(ox, oy, ww, hh);
	if (ww > hh)
	{
		side = hh;
		ox = ox + (ww - side) / 2;
	}
	else
	{
		side = ww;
		oy = oy + (hh - side) / 2;
	}
	side = w() > h() ? hh : ww;
	// background
	fl_color(FL_BACKGROUND_COLOR);
	fl_rectf(ox, oy, side, side);
	// scale
	(active_r()) ?
			fl_color(fl_color_average(FL_BACKGROUND2_COLOR, FL_BACKGROUND_COLOR, .5)) :
			fl_color(fl_color_average(FL_BACKGROUND2_COLOR, FL_BACKGROUND_COLOR, .2));
	fl_pie(ox + 1, oy + 3, side - 2, side - 12, 0, 360);
	draw_scale(ox, oy, side);
	fl_pie(ox + 7, oy + 7, side - 14, side - 14, 0, 360);
	// shadow
	fl_color(fl_color_average(FL_BACKGROUND_COLOR, FL_BLACK, .9));
	fl_pie(ox + 8, oy + 12, side - 16, side - 16, 0, 360);
	fl_color(fl_color_average(FL_BACKGROUND_COLOR, FL_BLACK, .7));
	fl_pie(ox + 9, oy + 12, side - 18, side - 18, 0, 360);
	// knob edge
//	fl_color(active_r() ? FL_BACKGROUND2_COLOR : fl_color_average(FL_BACKGROUND2_COLOR, FL_BACKGROUND_COLOR, .5));
	fl_color(active_r() ? FL_BLACK : fl_color_average(FL_BLACK, FL_BACKGROUND_COLOR, .5));
	fl_pie(ox + 9, oy + 9, side - 18, side - 18, 0, 360);
	// top
	if (active_r())
		(this == Fl::focus()) ?
				fl_color(FL_SELECTION_COLOR) : fl_color(fl_color_average(FL_BACKGROUND2_COLOR, FL_BACKGROUND_COLOR, .8));
	else
		fl_color(fl_color_average(FL_BACKGROUND2_COLOR, FL_BACKGROUND_COLOR, .5));
	fl_pie(ox + 10, oy + 10, side - 20, side - 20, 0, 360);
//	if (active_r())
	{
		unsigned char rr, gg, bb;
		Fl::get_color((Fl_Color) fl_color(), rr, gg, bb);
		shadow(10, rr, gg, bb);
		fl_pie(ox + 10, oy + 10, side - 20, side - 20, 110, 150);
		fl_pie(ox + 10, oy + 10, side - 20, side - 20, 290, 330);
		shadow(17, rr, gg, bb);
		fl_pie(ox + 10, oy + 10, side - 20, side - 20, 120, 140);
		fl_pie(ox + 10, oy + 10, side - 20, side - 20, 300, 320);
		shadow(25, rr, gg, bb);
		fl_pie(ox + 10, oy + 10, side - 20, side - 20, 127, 133);
		fl_pie(ox + 10, oy + 10, side - 20, side - 20, 307, 313);
	}
	draw_cursor(ox, oy, side);
	fl_pop_clip();
}

void Fl_Knob::shadow(const int offs, const uchar r, uchar g, uchar b)
{
	int rr, gg, bb;
	rr = r + offs;
	rr = rr > 255 ? 255 : rr;
	rr = rr < 0 ? 0 : rr;
	gg = g + offs;
	gg = gg > 255 ? 255 : gg;
	gg = gg < 0 ? 0 : gg;
	bb = b + offs;
	bb = bb > 255 ? 255 : bb;
	bb = bb < 0 ? 0 : bb;
	fl_color((uchar) rr, (uchar) gg, (uchar) bb);
}

int Fl_Knob::handle(int ev)
{
	static int ox, oy, ww, hh, px, py;
	ox = x() + 10;
	oy = y() + 10;
	ww = w() - 20;
	hh = h() - 20;
	switch (ev)
	{
		case FL_ENTER: // 1 = receive FL_LEAVE and FL_MOVE events (widget becomes Fl::belowmouse())
			if (active_r())
			{
				take_focus();
				return 1;
			}
			return 0;
		case FL_PUSH: // 1 = receive FL_DRAG and the matching (Fl::event_button()) FL_RELEASE event (becomes Fl::pushed())
			if (this != Fl::belowmouse())
				return 0;
			handle_push();
			if (FL_RIGHT_MOUSE == Fl::event_button() && pxk->setup_copy && pxk->preset_copy)
			{
				if (id_layer[0] == 0) // master volume
					value((double) cfg->get_default(CFG_MASTER_VOLUME));
				else if (id_layer[0] < 788) // master setting
					value((double) pxk->setup_copy->get_value(id_layer[0], pxk->selected_channel));
				else
					value((double) pxk->preset_copy->get_value(id_layer[0], id_layer[1]));
				do_callback();
			}
			else
			{
				px = Fl::event_x();
				py = Fl::event_y();
			}
			return 1;
		case FL_RELEASE:
			handle_release();
			return 1;
		case FL_DRAG: // button state is in Fl::event_state() (FL_SHIFT FL_CAPS_LOCK FL_CTRL FL_ALT FL_NUM_LOCK FL_META FL_SCROLL_LOCK FL_BUTTON1 FL_BUTTON2 FL_BUTTON3)
			if (!(FL_BUTTON3 & Fl::event_state()))
			{
				double val = 0, angle, oldangle;
				int mx, my;
				switch (cfg->get_cfg_option(CFG_KNOBMODE))
				{
					case 0: // radial
						mx = Fl::event_x() - ox - ww / 2;
						my = Fl::event_y() - oy - hh / 2;
						angle = 270 - atan2((float) -my, (float) mx) * 180 / M_PI;
						oldangle = (a2 - a1) * (value() - minimum()) / (maximum() - minimum()) + a1;
						while (angle < oldangle - 180)
							angle += 360;
						while (angle > oldangle + 180)
							angle -= 360;
						if ((a1 < a2) ? (angle <= a1) : (angle >= a1))
							val = minimum();
						else if ((a1 < a2) ? (angle >= a2) : (angle <= a2))
							val = maximum();
						else
							val = minimum() + (maximum() - minimum()) * (angle - a1) / (a2 - a1);
						break;
					case 1: // horizontal
						mx = Fl::event_x() - px;
						val = (double) mx + value();
						px += mx;
						break;
					case 2: // vertical
						my = Fl::event_y() - py;
						val = value() - (double) my;
						py += my;
						break;
				}
				handle_drag(clamp(round(val)));
			}
			return 1;
		case FL_MOUSEWHEEL:
			if (this == Fl::belowmouse())
			{
				int dy = Fl::event_dy() * -1;
				double v1 = value(); // current value
				double v2 = clamp(increment(v1, dy));
				if (v1 == v2)
					return 1;
				value(v2);
				do_callback();
				return 1;
			}
			break;
			// keyboard events
		case FL_FOCUS: // 1 = receive FL_KEYDOWN, FL_KEYUP, and FL_UNFOCUS events (widget becomes Fl::focus())
			damage(2);
			// show value in value field and make ourselfes the editing widget
			if (pwid_editing != this && id_layer[0] != 0)
			{
				pwid_editing = this;
				ui->value_input->minimum((double) minimax[0]);
				ui->value_input->maximum((double) minimax[1]);
			}
			ui->value_input->value((double) value());
			ui->forma_out->set_value(id_layer[0], id_layer[1], value());
			return 1;
		case FL_UNFOCUS: // received when another widget gets the focus and we had the focus
			damage(2);
			return 1;
		case FL_KEYDOWN: // key press (Fl::event_key())
			if (this == Fl::focus())
			{
				int key = Fl::event_key();
				double v1 = value(); // current value
				double v2 = 0.;
				switch (key)
				{
					case FL_Down:
						if (Fl::event_shift())
							v2 = 0.;
						else
							v2 = clamp(increment(v1, -1));
						break;
					case FL_Up:
						if (Fl::event_shift())
							v2 = 0.;
						else
							v2 = clamp(increment(v1, 1));
						break;
					case FL_Left:
						if (Fl::event_shift())
							v2 = minimax[0];
						else
							return 0;
						break;
					case FL_Right:
						if (Fl::event_shift())
							v2 = minimax[1];
						else
							return 0;
						break;
					default:
						return 0;
				}
				if (v1 != v2)
				{
					value(v2);
					do_callback();
				}
				return 1;
			}
	}
	return Fl_Valuator::handle(ev);
}

void Fl_Knob::draw_scale(const int ox, const int oy, const int side)
{
	float x1, y1, x2, y2, rds, cx, cy, ca, sa;
	rds = side / 2;
	cx = ox + side / 2;
	cy = oy + side / 2;
	if (_scaleticks == 0)
		return;
	double a_step = (10.0 * 3.14159 / 6.0) / _scaleticks;
	double a_orig = -(3.14159 / 3.0);
	for (int a = 0; a <= _scaleticks; a++)
	{
		double na = a_orig + a * a_step;
		ca = cos(na);
		sa = sin(na);
		x1 = cx + (rds) * ca;
		y1 = cy - (rds) * sa;
		x2 = cx + (rds - 6) * ca;
		y2 = cy - (rds - 6) * sa;
		fl_color(FL_BACKGROUND_COLOR);
		fl_line(x1, y1, x2, y2);
	}
}

void Fl_Knob::draw_cursor(const int ox, const int oy, const int side)
{
	float rds, cur, cx, cy;
	double angle;
	// top
	if (active_r())
		(this == Fl::focus()) ?
				fl_color(fl_contrast(FL_FOREGROUND_COLOR, FL_SELECTION_COLOR)) :
				fl_color(fl_color_average(FL_BACKGROUND2_COLOR, FL_FOREGROUND_COLOR, .3));
	else
		fl_color(fl_color_average(FL_BACKGROUND2_COLOR, FL_FOREGROUND_COLOR, .4));
	rds = (side - 18) / 2.0;
	cur = _percent * rds / 2;
	cx = ox + side / 2;
	cy = oy + side / 2;
	angle = (a2 - a1) * (value() - minimum()) / (maximum() - minimum()) + a1;
	fl_push_matrix();
	fl_scale(1, 1);
	fl_translate(cx, cy);
	fl_rotate(-angle);
	fl_translate(0, rds - cur - 3.0);
	fl_begin_polygon();
	fl_vertex(-1., -cur);
	fl_vertex(-1., cur);
	fl_vertex(1., cur);
	fl_vertex(1., -cur);
	fl_end_polygon();
	fl_pop_matrix();
}

void Fl_Knob::cursor(const int pc)
{
	_percent = (float) pc / 100.0;
	if (_percent < 0.05)
		_percent = 0.05;
	if (_percent > 1.0)
		_percent = 1.0;
	if (visible())
		damage(FL_DAMAGE_CHILD);
}

void Fl_Knob::scaleticks(const int tck)
{
	_scaleticks = tck;
	if (_scaleticks < 0)
		_scaleticks = 0;
	if (_scaleticks > 31)
		_scaleticks = 31;
	if (visible())
		damage(FL_DAMAGE_ALL);
}

// ###################
//
// ###################
void Button::set_id(int v, int l)
{
	id_layer[0] = v;
	id_layer[1] = l;
	minimax[0] = 0;
	minimax[1] = 1;
	callback((Fl_Callback*) cb, (void*) id_layer);
	pwid[v][l] = this;
}

void Button::set_value(int v)
{
	if (id_layer[0] == 258 || id_layer[0] == 1669 || id_layer[0] == 1674 || id_layer[0] == 1033 || id_layer[0] == 649) // fx bypass / lfo syncs / arp syncs
		v ? v = 0 : v = 1;
	if (id_layer[0] == 258) // fx bypass
	{
		v ? ui->m_bypass->set() : ui->m_bypass->clear();
		v ? ui->b_pfx->color(fl_color_average(this->selection_color(), FL_BACKGROUND_COLOR, .6)) : ui->b_pfx->color(
						FL_BACKGROUND_COLOR);
		ui->b_pfx->redraw();
	}
	else if (id_layer[0] == 1025) // arp preset
	{
		if (v)
		{
			((Fl_Button*) ui->main->g_main_arp->child(2))->color(this->selection_color(),
					fl_color_average(this->selection_color(), FL_SELECTION_COLOR, .5));
		}
		else
		{
			((Fl_Button*) ui->main->g_main_arp->child(2))->color(FL_BACKGROUND2_COLOR, FL_SELECTION_COLOR);
		}
		ui->main->g_main_arp->redraw();
	}
	else if (id_layer[0] == 641) // arp master
	{
		if (v)
		{
			((Fl_Button*) ui->main->g_main_arp->child(3))->color(this->selection_color(),
					fl_color_average(this->selection_color(), FL_SELECTION_COLOR, .5));
		}
		else
		{
			((Fl_Button*) ui->main->g_main_arp->child(3))->color(FL_BACKGROUND2_COLOR, FL_SELECTION_COLOR);
		}
		ui->main->g_main_arp->redraw();
	}
	value(v);
}

int Button::get_value() const
{
	int v = value();
	if (id_layer[0] == 258) // fx bypass
	{
		v ? ui->m_bypass->set() : ui->m_bypass->clear();
		v ? ui->b_pfx->color(fl_color_average(this->selection_color(), FL_BACKGROUND_COLOR, .6)) : ui->b_pfx->color(
						FL_BACKGROUND_COLOR);
		ui->b_pfx->redraw();
	}
	if (id_layer[0] == 258 || id_layer[0] == 1669 || id_layer[0] == 1674 || id_layer[0] == 1033 || id_layer[0] == 649) // fx bypass / lfo syncs / arp syncs
		v ? v = 0 : v = 1;
	if (id_layer[0] == 137)
	{
		if (v)
			ui->g_preset->activate();
		else
			ui->g_preset->deactivate();
	}
	else if (id_layer[0] == 1025) // arp preset
	{
		if (v)
		{
			((Fl_Button*) ui->main->g_main_arp->child(2))->color(this->selection_color(),
					fl_color_average(this->selection_color(), FL_SELECTION_COLOR, .5));
		}
		else
		{
			((Fl_Button*) ui->main->g_main_arp->child(2))->color(FL_BACKGROUND2_COLOR, FL_SELECTION_COLOR);
		}
		ui->main->g_main_arp->redraw();
	}
	else if (id_layer[0] == 641) // arp master
	{
		if (v)
		{
			((Fl_Button*) ui->main->g_main_arp->child(3))->color(this->selection_color(),
					fl_color_average(this->selection_color(), FL_SELECTION_COLOR, .5));
		}
		else
		{
			((Fl_Button*) ui->main->g_main_arp->child(3))->color(FL_BACKGROUND2_COLOR, FL_SELECTION_COLOR);
		}
		ui->main->g_main_arp->redraw();
	}
	return v;
}

int Button::handle(int ev)
{
	switch (ev)
	{
		case FL_PUSH:
			if (FL_RIGHT_MOUSE == Fl::event_button())
				return 1;
			break;
		case FL_RELEASE:
			if (id_layer[0] != -1 && FL_RIGHT_MOUSE == Fl::event_button() && pxk->setup_copy && pxk->preset_copy)
			{
				if (id_layer[0] < 788) // master setting
					set_value(pxk->setup_copy->get_value(id_layer[0], pxk->selected_channel));
				else
					set_value(pxk->preset_copy->get_value(id_layer[0], id_layer[1]));
				do_callback();
				return 1;
			}
			break;
	}
	return Fl_Button::handle(ev);
}

// ###################
//
// ###################
int Fixed_Button::handle(int ev)
{
	if (ev == FL_DRAG)
		return 0;
	return Fl_Button::handle(ev);
}

// ###################
//
// ###################
void Choice::set_id(int v, int l)
{
	id_layer[0] = v;
	id_layer[1] = l;
	minimax[0] = 0;
	switch (v)
	{
		case 271: // beats mode
		case 658: // song start
		case 657: // midi transmit
			minimax[1] = 3;
			break;
		case 265: // velocity curve
			minimax[1] = 13;
			break;
		case 140: // fx channel
		case 272: // beats channel
		case 273: // beats trigger channel
			minimax[0] = -1;
			minimax[1] = 15;
			break;
		case 1437: // layer solo
			minimax[1] = 8;
			break;
		case 1438: // layer group
			minimax[1] = 23;
			break;
		case 1537: // filter type
			minimax[1] = 163;
			break;
		case 385: // midi mode
			minimax[1] = 2;
			break;
		case 513: // fxa
		case 1153:
			minimax[1] = 44;
			break;
		case 520: // fxb
		case 1160:
			minimax[1] = 32;
			break;
		case 141: // tempo channel
			minimax[1] = 15;
			break;
		case 402: // tempo ctrl up
		case 403: // tempo ctrl down
			minimax[0] = -3;
			minimax[1] = 31;
			break;
		case 923: // keyboard tuning
			minimax[1] = 11;
			break;
		case 1666: // lfo waveform
		case 1671:
			minimax[0] = -1;
			minimax[1] = 15;
			break;
	}
	callback((Fl_Callback*) cb, (void*) id_layer);
	pwid[v][l] = this;
}

void Choice::set_value(int v)
{
	//pmesg("Choice::set_value(%d) (id:%d layer:%d)\n", v, id_layer[0], id_layer[1]);
	if (id_layer[0] == 140) // FX channel
	{
		dependency(v);
		v += 1;
		value((double) v);
	}
	else if (id_layer[0] == 1153 || id_layer[0] == 1160 || id_layer[0] == 385 || id_layer[0] == 271)
	{
		value(v);
		dependency(v);
	}
	else if (id_layer[0] == 513 || id_layer[0] == 520) // master fx
	{
		dependency(v);
		value(v - 1);
	}
	else if (id_layer[0] == 1537) // filter type
	{
		for (char i = 0; i <= 50; i++)
			if (FM[i].id == v)
			{
				value(FM[i]._index);
				tooltip(FM[i].info);
				break;
			}
		if (v == 127)
			ui->main->layer_strip[id_layer[1]]->filter_knobs->deactivate();
		else
			ui->main->layer_strip[id_layer[1]]->filter_knobs->activate();
	}
	else if (id_layer[0] == 403 || id_layer[0] == 402) // tempo up/down ctrl
	{
		if (v < 1)
			value(v + 3);
		else
			value(v + 2);
	}
	else if (id_layer[0] == 272 || id_layer[0] == 273 || id_layer[0] == 1666 || id_layer[0] == 1671) // beat channels + lfo waveforms
		value(v + 1);
	else
		value(v);
}

int Choice::get_value() const
{
	if (id_layer[0] == 140) // FX channel
	{
		int val = (int) value();
		val -= 1;
		dependency(val);
		return val;
	}
	if (id_layer[0] == 1537) // filter type
	{
		int v = value();
		int i;
		for (i = 0; i <= 50; i++)
			if (FM[i]._index == v)
				break;
		int val = FM[i].id;
		if (val == 127)
			ui->main->layer_strip[id_layer[1]]->filter_knobs->deactivate();
		else
			ui->main->layer_strip[id_layer[1]]->filter_knobs->activate();
		return val;
	}
	int val = value();
	if (id_layer[0] == 1153 || id_layer[0] == 1160 || id_layer[0] == 385 || id_layer[0] == 271)
		dependency(val);
	if (id_layer[0] == 513 || id_layer[0] == 520) // master fx
	{
		val += 1;
		dependency(val);
	}
	if (id_layer[0] == 403 || id_layer[0] == 402) // tempo up/down ctrl
	{
		if (value() < 3)
			return value() - 3;
		else
			return value() - 2;
	}
	if (id_layer[0] == 272 || id_layer[0] == 273 || id_layer[0] == 1666 || id_layer[0] == 1671) // beat channels + lfo waveforms
		return val - 1;
	return val;
}

int Choice::handle(int ev)
{
	switch (ev)
	{
		case FL_PUSH: // 1 = receive FL_DRAG and the matching (Fl::event_button()) FL_RELEASE event (becomes Fl::pushed())
			if (FL_RIGHT_MOUSE == Fl::event_button())
				return 1;
			break;
		case FL_RELEASE:
			if (FL_RIGHT_MOUSE == Fl::event_button() && pxk->setup_copy && pxk->preset_copy)
			{
				if (id_layer[0] < 788) // master setting
					set_value((double) pxk->setup_copy->get_value(id_layer[0], pxk->selected_channel));
				else
					set_value((double) pxk->preset_copy->get_value(id_layer[0], id_layer[1]));
				do_callback();
				return 1;
			}
			break;
		case FL_MOUSEWHEEL:
			if (this == Fl::belowmouse())
			{
				int dy = Fl::event_dy();
				double v1 = value();
				double v2 = v1 + dy;
				if (v2 < 0)
					v2 = size() - 2;
				else if (v2 > size() - 2)
					v2 = 0;
				value(v2);
				// make sure we dont select submenus when scrolling
				const Fl_Menu_Item* item = mvalue();
				if (dy > 0) // scrolling down
				{
					if (item->label() == 0)
					{
						v2 += 2;
						if (v2 >= size())
							v2 = 0;
						value(v2);
						item = mvalue();
					}
					if (item->submenu())
						value(v2 + 1);
				}
				else if (dy < 0) // scrolling up
				{
					if (item->submenu())
					{
						v2 -= 1;
						if (v2 < 0)
							v2 = size() - 2;
						value(v2);
						item = mvalue();
					}
					if (item->label() == 0)
						value(v2 - 1);
				}
				do_callback();
				return 1;
			}
			break;
	}
	return Fl_Choice::handle(ev);
}

void Choice::dependency(int v) const
{
	if (id_layer[0] == 140) // FX channel
	{
		if (v != -1)
		{
			if (v == pxk->selected_channel)
				ui->fx->activate();
			else
				ui->fx->deactivate();
		}
		else
			ui->fx->activate();
		if ((v != -1 && pwid[1153][0] != 0) || (v == -1 && pwid[513][0] != 0))
			return;
		ui->fxa->clear();
		ui->fxb->clear();
		if (v != -1) // Master FX
		{
			ui->fxa->add("Master A");
			ui->fxb->add("Master B");
		}
		ui->fxa->add("Room 1");
		ui->fxa->add("Room 2");
		ui->fxa->add("Room 3");
		ui->fxa->add("Hall 1");
		ui->fxa->add("Hall 2");
		ui->fxa->add("Plate");
		ui->fxa->add("Delay");
		ui->fxa->add("Panning Delay");
		ui->fxa->add("Multitap 1");
		ui->fxa->add("Multitap Pan");
		ui->fxa->add("3 Tap");
		ui->fxa->add("3 Tap Pan");
		ui->fxa->add("Soft Room");
		ui->fxa->add("Warm Room");
		ui->fxa->add("Perfect Room");
		ui->fxa->add("Tiled Room");
		ui->fxa->add("Hard Plate");
		ui->fxa->add("Warm Hall");
		ui->fxa->add("Spacious Hall");
		ui->fxa->add("Bright Hall");
		ui->fxa->add("Brt Hall Pan");
		ui->fxa->add("Bright Plate");
		ui->fxa->add("BBall Court");
		ui->fxa->add("Gymnasium");
		ui->fxa->add("Cavern");
		ui->fxa->add("Concert 9");
		ui->fxa->add("Concert 10 Pan");
		ui->fxa->add("Reverse Gate");
		ui->fxa->add("Gate 2");
		ui->fxa->add("Gate Pan");
		ui->fxa->add("Concert 11");
		ui->fxa->add("MediumConcert");
		ui->fxa->add("Large Concert");
		ui->fxa->add("Lg Concert Pan");
		ui->fxa->add("Canyon");
		ui->fxa->add("DelayVerb 1");
		ui->fxa->add("DelayVerb 2");
		ui->fxa->add("DelayVerb 3");
		ui->fxa->add("DelayVerb4Pan");
		ui->fxa->add("DelayVerb5Pan");
		ui->fxa->add("DelayVerb 6");
		ui->fxa->add("DelayVerb 7");
		ui->fxa->add("DelayVerb 8");
		ui->fxa->add("DelayVerb 9");

		ui->fxb->add("Chorus 1");
		ui->fxb->add("Chorus 2");
		ui->fxb->add("Chorus 3");
		ui->fxb->add("Chorus 4");
		ui->fxb->add("Chorus 5");
		ui->fxb->add("Doubling");
		ui->fxb->add("Slapback"); // 6/7
		ui->fxb->add("Flange 1");
		ui->fxb->add("Flange 2");
		ui->fxb->add("Flange 3");
		ui->fxb->add("Flange 4");
		ui->fxb->add("Flange 5");
		ui->fxb->add("Flange 6");
		ui->fxb->add("Flange 7");
		ui->fxb->add("Big Chorus");
		ui->fxb->add("Symphonic");
		ui->fxb->add("Ensemble");
		ui->fxb->add("Delay"); // 17/18
		ui->fxb->add("Delay Stereo");
		ui->fxb->add("Delay Stereo 2");
		ui->fxb->add("Panning Delay");
		ui->fxb->add("Delay Chorus");
		ui->fxb->add("Pan Dly Chrs 1");
		ui->fxb->add("Pan Dly Chrs 2");
		ui->fxb->add("a");
		ui->fxb->add("b");
		if (v != -1)
		{
			ui->fxb->replace(25, "DualTap 1/3"); // 24/25
			ui->fxb->replace(26, "DualTap 1/4");
		}
		else
		{
			ui->fxb->replace(24, "DualTap 1/3");
			ui->fxb->replace(25, "DualTap 1/4");
		}
		ui->fxb->add("Vibrato");
		ui->fxb->add("Distortion 1"); // 27/28
		ui->fxb->add("Distortion 2");
		ui->fxb->add("DistortedFlange");
		ui->fxb->add("DistortedChorus");
		ui->fxb->add("DistortedDouble"); // 31/32

		if (v != -1)
		{
			// replace ids with preset ids
			pwid[513][0] = 0;
			pwid[514][0] = 0;
			pwid[515][0] = 0;
			pwid[516][0] = 0;
			pwid[517][0] = 0;
			pwid[518][0] = 0;
			pwid[519][0] = 0;
			pwid[527][0] = 0;

			pwid[520][0] = 0;
			pwid[521][0] = 0;
			pwid[522][0] = 0;
			pwid[523][0] = 0;
			pwid[524][0] = 0;
			pwid[525][0] = 0;
			pwid[526][0] = 0;
			pwid[528][0] = 0;

			ui->fxa->set_id(1153);
			ui->fxa_decay->set_id(1154);
			ui->fxa_damp->set_id(1155);
			ui->fxa_ba->set_id(1156);
			ui->fxa_send1->set_id(1157);
			ui->fxa_send2->set_id(1158);
			ui->fxa_send3->set_id(1159);
			ui->fxa_send4->set_id(1167);

			ui->fxb->set_id(1160);
			ui->fxb_feedback->set_id(1161);
			ui->fxb_lfo_rate->set_id(1162);
			ui->fxb_delay->set_id(1163);
			ui->fxb_send1->set_id(1164);
			ui->fxb_send2->set_id(1165);
			ui->fxb_send3->set_id(1166);
			ui->fxb_send4->set_id(1168);
		}
		else
		{
			// replace ids with master ids
			pwid[1153][0] = 0;
			pwid[1154][0] = 0;
			pwid[1155][0] = 0;
			pwid[1156][0] = 0;
			pwid[1157][0] = 0;
			pwid[1158][0] = 0;
			pwid[1159][0] = 0;
			pwid[1167][0] = 0;

			pwid[1160][0] = 0;
			pwid[1161][0] = 0;
			pwid[1162][0] = 0;
			pwid[1163][0] = 0;
			pwid[1164][0] = 0;
			pwid[1165][0] = 0;
			pwid[1166][0] = 0;
			pwid[1168][0] = 0;

			ui->fxa->set_id(513);
			ui->fxa_decay->set_id(514);
			ui->fxa_damp->set_id(515);
			ui->fxa_ba->set_id(516);
			ui->fxa_send1->set_id(517);
			ui->fxa_send2->set_id(518);
			ui->fxa_send3->set_id(519);
			ui->fxa_send4->set_id(527);

			ui->fxb->set_id(520);
			ui->fxb_feedback->set_id(521);
			ui->fxb_lfo_rate->set_id(522);
			ui->fxb_delay->set_id(523);
			ui->fxb_send1->set_id(524);
			ui->fxb_send2->set_id(525);
			ui->fxb_send3->set_id(526);
			ui->fxb_send4->set_id(528);
		}
	}
	else if (id_layer[0] == 271) // superbeats mode
	{
		if (v == 3) // master mode
			ui->main->g_riff->activate();
		else
			ui->main->g_riff->deactivate();
	}
	else if (id_layer[0] == 513) // Master fxa
		ui->g_fxa->activate();
	else if (id_layer[0] == 520) // Master fxb
		ui->g_fxb->activate();
	else if (id_layer[0] == 1153) // fxa type
	{
		if (text() && strcmp(text(), "Master A") == 0)
			ui->g_fxa->deactivate();
		else
			ui->g_fxa->activate();
	}
	else if (id_layer[0] == 1160) // fxb type
	{
		if (text() && strcmp(text(), "Master B") == 0)
			ui->g_fxb->deactivate();
		else
			ui->g_fxb->activate();
	}
	if (id_layer[0] == 1160 || id_layer[0] == 520) // fxb type
	{
		// disable lfo rate for algorithms that dont support lfo rate
		if (v == 6 || v == 7 || (v >= 18 && v <= 21) || v == 25 || v == 26 || v == 28 || v == 29 || v == 32)
			ui->fxb_lfo_rate->deactivate();
		else
			ui->fxb_lfo_rate->activate();
		// disable delay for algorithms that dont support delay
		if (v > 17 && v < 27)
			ui->fxb_delay->activate();
		else
			ui->fxb_delay->deactivate();
	}
	else if (id_layer[0] == 385) // MIDI Mode
	{
		if (v != MULTI) // not multimode
		{
			ui->fx_channel->deactivate();
			ui->main->channel_enable->set();
			ui->main->channel_enable->deactivate();
		}
		else
		{
			ui->fx_channel->activate();
			ui->main->channel_enable->activate();
			ui->main->channel_enable->value(pxk->setup->get_value(135, pxk->selected_channel));
		}
		if (v == OMNI)
		{
			ui->all_notes_off->deactivate();
			ui->m_all_notes_off->deactivate();
			ui->m_all_notes_off_all->deactivate();
		}
		else
		{
			ui->all_notes_off->activate();
			ui->m_all_notes_off->activate();
			ui->m_all_notes_off_all->activate();
		}
	}
}

// ###################
//
// ###################
void PCS_Choice::set_value(int v)
{
	if (!initialized && pxk->preset)
		init(pxk->preset->get_extra_controller());
	for (char i = 0; i < 78; i++)
		if (PatchS[i].id == v)
		{
			value(index[i]);
			return;
		}
}

int PCS_Choice::get_value() const
{
	for (char i = 0; i < 78; i++)
		if (index[i] == value())
			return PatchS[i].id;
	return 0;
}

// ###################
//
// ###################
void PCD_Choice::set_value(int v)
{
	for (char i = 0; i < 68; i++)
		if (PatchD[i].id == v)
		{
			value(index[i]);
			return;
		}
}

int PCD_Choice::get_value() const
{
	for (char i = 0; i < 68; i++)
		if (index[i] == value())
			return PatchD[i].id;
	return 0;
}

// ###################
//
// ###################
void PPCD_Choice::set_value(int v)
{
	for (char i = 0; i < 28; i++)
		if (PresetPatchD[i].id == v)
		{
			value(index[i]);
			return;
		}
}

int PPCD_Choice::get_value() const
{
	for (char i = 0; i < 28; i++)
		if (index[i] == value())
			return PresetPatchD[i].id;
	return 0;
}

// ###################
//
// ###################
void Envelope_Editor::draw_b_label(char butt, Fl_Color col)
{
	int ypos = ee_y0 + ee_h - 9;
	fl_color(col);
	switch (butt)
	{
		case VOLUME_SELECTED:
			fl_draw("V", copy_button[0] + 5, ypos);
			break;
		case FILTER_SELECTED:
			fl_draw("F", copy_button[1] + 5, ypos);
			break;
		case AUXILIARY_SELECTED:
			fl_draw("A", copy_button[2] + 5, ypos);
			break;
		case CPY_VOLUME:
			fl_draw("V", copy_button[3] + 5, ypos);
			break;
		case CPY_FILTER:
			fl_draw("F", copy_button[4] + 5, ypos);
			break;
		case CPY_AUXILIARY:
			fl_draw("A", copy_button[5] + 5, ypos);
			break;
		case SHAPE_A:
			fl_draw("Pr", shape_button[0] + 3, ypos);
			break;
		case SHAPE_B:
			fl_draw("Or", shape_button[1] + 3, ypos);
			break;
		case SHAPE_C:
			fl_draw("St", shape_button[2] + 3, ypos);
			break;
		case SHAPE_D:
			fl_draw("Pl", shape_button[3] + 3, ypos);
	}
}

void Envelope_Editor::draw()
{
	// box
	Fl_Box::draw_box();
	ee_w = this->w() - 2;
	ee_h = this->h() - 2;
	ee_x0 = this->x() + 1;
	ee_y0 = this->y() + 1;
	mode_button[0] = ee_x0 + ee_w - 164;
	mode_button[1] = mode_button[0] + 55;
	mode_button[2] = mode_button[1] + 55;
	mode_button[3] = mode_button[0] - 65; // oberlay
	mode_button[4] = mode_button[3] - 55; // comp
	copy_button[0] = ee_x0 + 5;
	copy_button[1] = copy_button[0] + 20;
	copy_button[2] = copy_button[1] + 20;
	copy_button[3] = copy_button[2] + 70;
	copy_button[4] = copy_button[3] + 20;
	copy_button[5] = copy_button[4] + 20;
	shape_button[0] = ee_x0 + ee_w - 22;
	shape_button[1] = shape_button[0] - 20;
	shape_button[2] = shape_button[1] - 20;
	shape_button[3] = shape_button[2] - 20;
	// title
	fl_font(FL_HELVETICA_BOLD, 14);
	fl_color(FL_INACTIVE_COLOR);
	switch (mode)
	{
		case VOLUME:
			fl_draw("VOL", ee_x0 + 5, ee_y0 + 17);
			break;
		case FILTER:
			fl_draw("FIL", ee_x0 + 5, ee_y0 + 17);
			break;
		case AUXILIARY:
			fl_draw("AUX", ee_x0 + 5, ee_y0 + 17);
			break;
	}
	// top bar
	fl_font(FL_COURIER, 10);
	Fl_Color fg;
	unsigned char i;
	for (i = 0; i < 5; i++)
	{
		// activated buttons
		if ((button_push && button_hover == i) || env[mode].mode == i || (i == 0 && mode != VOLUME && env[mode].repeat)
				|| (i == 3 && overlay) || (i == 4 && ui->syncview))
		{
			fl_color(FL_SELECTION_COLOR);
			fg = fl_contrast(FL_FOREGROUND_COLOR, FL_SELECTION_COLOR);
			draw_box(FL_DOWN_BOX, mode_button[i] - 2, ee_y0 + 3, 52, 17, fl_color());
		}
		else
		{
			fl_color(FL_BACKGROUND2_COLOR);
			fg = FL_FOREGROUND_COLOR;
			draw_box(FL_UP_BOX, mode_button[i] - 2, ee_y0 + 3, 52, 17, fl_color());
		}
		// text
		fl_color(fg);
		if (i == 0)
		{
			if (mode == VOLUME)
				fl_draw("Factory", mode_button[0] + 3, ee_y0 + 14);
			else
				fl_draw("Repeat", mode_button[0] + 7, ee_y0 + 14);
		}
		else if (i == 1)
			fl_draw("Time", mode_button[i] + 12, ee_y0 + 14);
		else if (i == 2)
			fl_draw("Tempo", mode_button[i] + 9, ee_y0 + 14);
		else if (i == 3)
			fl_draw("/\\\\//\\\\", mode_button[i] + 3, ee_y0 + 14);
		else if (i == 4)
			fl_draw("(( Y ))", mode_button[i] + 3, ee_y0 + 14);
	}
	// lower bar
	fl_font(FL_HELVETICA, 10);
	fl_color(FL_BACKGROUND2_COLOR);
	for (i = 0; i < 6; i++)
		if (i != mode && i != mode + 3)
		{
			if (button_push && button_hover == VOLUME_SELECTED + i)
			{
				draw_box(FL_BORDER_BOX, copy_button[i], ee_y0 + ee_h - 21, 17, 17, FL_SELECTION_COLOR);
				draw_b_label(VOLUME_SELECTED + i, fl_contrast(FL_FOREGROUND_COLOR, FL_SELECTION_COLOR));
			}
			else
			{
				draw_box(FL_BORDER_BOX, copy_button[i], ee_y0 + ee_h - 21, 17, 17, FL_BACKGROUND2_COLOR);
				draw_b_label(VOLUME_SELECTED + i, FL_FOREGROUND_COLOR);
			}
		}
	draw_box(FL_BORDER_BOX, copy_button[mode], ee_y0 + ee_h - 21, 17, 17, FL_SELECTION_COLOR);
	draw_b_label(VOLUME_SELECTED + mode, fl_contrast(FL_FOREGROUND_COLOR, FL_SELECTION_COLOR));
	// shapes
	for (i = 0; i < 4; i++)
	{
		if (button_push && button_hover == SHAPE_A + i)
		{
			draw_box(FL_BORDER_BOX, shape_button[i], ee_y0 + ee_h - 21, 17, 17, FL_SELECTION_COLOR);
			draw_b_label(SHAPE_A + i, fl_contrast(FL_FOREGROUND_COLOR, FL_SELECTION_COLOR));
		}
		else
		{
			draw_box(FL_BORDER_BOX, shape_button[i], ee_y0 + ee_h - 21, 17, 17, FL_BACKGROUND2_COLOR);
			draw_b_label(SHAPE_A + i, FL_FOREGROUND_COLOR);
		}
	}
	fl_color(FL_FOREGROUND_COLOR);
	int ypos = ee_y0 + ee_h - 9;
	fl_font(FL_COURIER, 10);
	fl_draw("Copy to", copy_button[2] + 22, ypos);
	fl_draw("Shape", ee_x0 + ee_w - 115, ypos);
	switch (zoomlevel)
	{
		case 1:
			fl_draw("1x", copy_button[5] + 25, ypos);
			break;
		case 2:
			fl_draw("2x", copy_button[5] + 25, ypos);
			break;
		case 4:
			fl_draw("4x", copy_button[5] + 25, ypos);
			break;
	}
	// center (coordinate system)
	// 0.0 position of coordinate system
	fl_line_style(FL_SOLID, 1);
	int x0 = ee_x0 + 5;
	float y0 = (float) ee_y0 + 25. + ((float) ee_h - 50.) / 2.;
	draw_box(FL_THIN_UP_BOX, x0 - 1, ee_y0 + 22., ee_w - 8, ee_h - 46, FL_BACKGROUND2_COLOR);
	fl_push_clip(x0 + 1, (float) ee_y0 + 24., ee_w - 12, ee_h - 50);
	fl_color(fl_color_average(FL_BACKGROUND2_COLOR, FL_FOREGROUND_COLOR, .8));
	// vertikale
	float x_step = ((float) (ee_w - 10) / 384.) * (float) zoomlevel; // 3*128 = 384
	float x_val = (float) x0 + 8. * x_step;
	while ((x_val - x0) / zoomlevel < (ee_w - 10) / zoomlevel - 1)
	{
		if (mode == VOLUME)
			fl_line(x_val, ee_y0 + 24, x_val, y0);
		else
			fl_line(x_val, ee_y0 + 24, x_val, ee_y0 + ee_h - 24);
		x_val += 8. * x_step;
	}
	// horizontale
	float y_step = ((float) ee_h - 50.) / 20.;
	for (char j = -9; j <= 9; j++)
	{
		if (j == 0)
		{
			if (mode == VOLUME)
				break;
			else
				continue;
		}
		fl_line(x0 + 1, y0 + y_step * j, x0 + ee_w - 11, y0 + y_step * j);
	}
	// nulllinie
	fl_color(fl_color_average(FL_BACKGROUND2_COLOR, FL_FOREGROUND_COLOR, .5));
	Fl_Color null = fl_color();
	fl_line(x0 + 1, y0, x0 + ee_w - 11, y0);
	fl_line_style(0);
	fl_pop_clip();
	// envelopes
	unsigned char r, g, b;
	Fl::get_color(FL_BACKGROUND2_COLOR, r, g, b);
	int luma = (r + r + b + g + g + g) / 6;
	if (overlay && active_r())
	{
		for (i = 0; i < modes; i++)
		{
			if (i == mode || (i == VOLUME && env[VOLUME].mode == FACTORY))
				continue;
			draw_envelope(i, x0, y0, luma);
		}
	}
	if (mode == VOLUME && env[VOLUME].mode == FACTORY)
	{
		fl_color(null);
		fl_font(FL_COURIER, 13);
		fl_draw("(using factory envelope)", x0 + 100, y0 + 15);
		return;
	}
	if (active_r())
		draw_envelope(mode, x0, y0, luma);
	if (!active_r())
		return;
	// value fields
	fl_color(FL_FOREGROUND_COLOR);
	// calc number of hovers
	int hovers = 0;
	for (i = 0; i < 6; i++)
		if (hover_list & (1 << i))
			hovers += 12;
	char info[15];
	int y_offset = 0;
	for (i = 0; i < 6; i++)
	{
		if (hover_list & (1 << i))
		{
			const char *tmp = 0;
			switch (i)
			{
				case 0:
					tmp = "A1";
					break;
				case 1:
					tmp = "D1";
					break;
				case 2:
					tmp = "R1";
					break;
				case 3:
					tmp = "A2";
					break;
				case 4:
					tmp = "D2";
					break;
				case 5:
					tmp = "R2";
					break;
			}
			const char *tmp_tempo = 0;
			if (env[mode].mode == TEMPO_BASED)
			{
				switch (env[mode].stage[i][0])
				{
					case 7:
						tmp_tempo = "1/64";
						break;
					case 12:
						tmp_tempo = "1/32t";
						break;
					case 14:
						tmp_tempo = "1/64d";
						break;
					case 19:
						tmp_tempo = "1/32";
						break;
					case 24:
						tmp_tempo = "1/16t";
						break;
					case 26:
						tmp_tempo = "1/32d";
						break;
					case 31:
						tmp_tempo = "1/16";
						break;
					case 36:
						tmp_tempo = "1/8t";
						break;
					case 38:
						tmp_tempo = "1/16d";
						break;
					case 43:
						tmp_tempo = "1/8";
						break;
					case 48:
						tmp_tempo = "1/4t";
						break;
					case 50:
						tmp_tempo = "1/8d";
						break;
					case 55:
						tmp_tempo = "1/4";
						break;
					case 60:
						tmp_tempo = "1/2t";
						break;
					case 62:
						tmp_tempo = "1/4d";
						break;
					case 67:
						tmp_tempo = "1/2";
						break;
					case 72:
						tmp_tempo = "1/1t";
						break;
					case 74:
						tmp_tempo = "1/2d";
						break;
					case 79:
						tmp_tempo = "1/1";
						break;
					case 84:
						tmp_tempo = "2/1t";
						break;
					case 86:
						tmp_tempo = "1/1d";
						break;
					case 91:
						tmp_tempo = "2/1";
						break;
					case 96:
						tmp_tempo = "4/1t";
						break;
					case 98:
						tmp_tempo = "2/1d";
						break;
					default:
						tmp_tempo = 0;
				}
			}
			if (tmp_tempo)
				snprintf(info, 15, "%s %5s%4d", tmp, tmp_tempo, env[mode].stage[i][1]);
			else
				snprintf(info, 15, "%s%4d%4d", tmp, env[mode].stage[i][0], env[mode].stage[i][1]);
			if (i == hover)
				fl_font(FL_COURIER_BOLD_ITALIC, 13);
			else
				fl_font(FL_COURIER, 13);
			// when we drag out of the visible area, keep the info text inside
			int __x = Fl::event_x() + 13;
			int __y = Fl::event_y() + 15 + y_offset;
			if (__x + 110 > ee_x0 + ee_w)
				__x = ee_x0 + ee_w - 110;
			else if (__x < ee_x0 + 13)
				__x = ee_x0 + 13;
			if (__y > ee_y0 + ee_h - 15 - hovers)
				__y = ee_y0 + ee_h - 15 - hovers + y_offset;
			else if (__y < ee_y0 + 35 + y_offset)
				__y = ee_y0 + 35 + y_offset;
			fl_draw(info, __x, __y);
			y_offset += 12;
		}
	}
}

void Envelope_Editor::draw_envelope(unsigned char type, int x0, int y0, int luma)
{
	fl_push_clip(ee_x0 + 1, ee_y0 + 21, ee_w - 2, ee_h - 42);
	// x-scaling
	float x_scale = (((float) ee_w - 10.) / 768.) * (float) zoomlevel;
	// y-scaling
	float y_scale = ((float) ee_h - 50.) / 200.;
	// calculate pixel position for 9x9 dragboxes
	dragbox[ATK_1][0] = x0 + env[type].stage[ATK_1][0] * x_scale;
	dragbox[ATK_1][1] = y0 - env[type].stage[ATK_1][1] * y_scale;
	dragbox[ATK_2][0] = dragbox[ATK_1][0] + env[type].stage[ATK_2][0] * x_scale;
	dragbox[ATK_2][1] = y0 - env[type].stage[ATK_2][1] * y_scale;
	dragbox[DCY_1][0] = dragbox[ATK_2][0] + env[type].stage[DCY_1][0] * x_scale;
	dragbox[DCY_1][1] = y0 - env[type].stage[DCY_1][1] * y_scale;
	dragbox[DCY_2][0] = dragbox[DCY_1][0] + env[type].stage[DCY_2][0] * x_scale;
	dragbox[DCY_2][1] = y0 - env[type].stage[DCY_2][1] * y_scale;
	dragbox[RLS_1][0] = dragbox[DCY_2][0] + env[type].stage[RLS_1][0] * x_scale;
	dragbox[RLS_1][1] = y0 - env[type].stage[RLS_1][1] * y_scale;
	dragbox[RLS_2][0] = dragbox[RLS_1][0] + env[type].stage[RLS_2][0] * x_scale;
	dragbox[RLS_2][1] = y0 - env[type].stage[RLS_2][1] * y_scale;
	// lines between dragboxes
	switch (type)
	{
		case VOLUME:
			fl_color(fl_color_average(fl_rgb_color(255, 124, 96), FL_BACKGROUND2_COLOR, .7));
			break;
		case FILTER:
			fl_color(fl_color_average(fl_rgb_color(93, 155, 229), FL_BACKGROUND2_COLOR, .7));
			break;
		case AUXILIARY:
			fl_color(fl_color_average(fl_rgb_color(72, 241, 94), FL_BACKGROUND2_COLOR, .7));
	}
	if (type != mode)
	{
		if (luma > 128)
			fl_color(fl_lighter(fl_color()));
		else
			fl_color(fl_darker(fl_color()));
	}
	// draw connections
	fl_line_style(FL_SOLID, 2);
	fl_line(x0 + 1, y0, dragbox[ATK_1][0], dragbox[ATK_1][1]);
	fl_line(dragbox[ATK_1][0], dragbox[ATK_1][1], dragbox[ATK_2][0], dragbox[ATK_2][1]);
	fl_line(dragbox[ATK_2][0], dragbox[ATK_2][1], dragbox[DCY_1][0], dragbox[DCY_1][1]);
	fl_line(dragbox[DCY_1][0], dragbox[DCY_1][1], dragbox[DCY_2][0], dragbox[DCY_2][1]);
	// vertical line at release
	fl_line_style(FL_DASH, 1);
	fl_line(dragbox[DCY_2][0], y0 - 97 * y_scale, dragbox[DCY_2][0], y0 + 97 * y_scale);
	fl_line_style(FL_SOLID, 2);
	fl_line(dragbox[DCY_2][0], dragbox[DCY_2][1], dragbox[RLS_1][0], dragbox[RLS_1][1]);
	fl_line(dragbox[RLS_1][0], dragbox[RLS_1][1], dragbox[RLS_2][0], dragbox[RLS_2][1]);
	// dragboxes
	if (type == mode)
	{
		fl_rectf(dragbox[ATK_1][0] - 4, dragbox[ATK_1][1] - 4, 9, 9);
		fl_rectf(dragbox[ATK_2][0] - 4, dragbox[ATK_2][1] - 4, 9, 9);
		fl_rectf(dragbox[DCY_1][0] - 4, dragbox[DCY_1][1] - 4, 9, 9);
		fl_rectf(dragbox[DCY_2][0] - 4, dragbox[DCY_2][1] - 4, 9, 9);
		fl_rectf(dragbox[RLS_1][0] - 4, dragbox[RLS_1][1] - 4, 9, 9);
		fl_rectf(dragbox[RLS_2][0] - 4, dragbox[RLS_2][1] - 4, 9, 9);
		// highlight selected
		if (hover >= 0)
		{
			fl_color(FL_SELECTION_COLOR);
			fl_rectf(dragbox[hover][0] - 4, dragbox[hover][1] - 4, 9, 9);
		}
	}
	fl_line_style(0);
	fl_pop_clip();
	return;
}

// envelope editor tooltips
static const char* tt0 =
		"Factory: Uses the factory preset envelope contained in each instrument. If you select this mode, the volume envelope parameters are disabled and the factory defined settings are used instead. Factory mode is useful for instruments containing multiple drums, since each drum can have its own envelope settings.";
static const char* tt00 =
		"Repeat: When the envelope repeat function is on, the attack (A1 & A2) and decay (D1 & D2) stages will continue to repeat as long as the key is held. As soon as the key is released, the envelope continues through its normal release stages (R1 & R2).";
static const char* tt1 =
		"Time: Defines the envelope rates from 0 to 127 (approximately 1 ms to 160 seconds). The master clock has no affect on timebased rates. If two adjacent segments have the same level in a time-based envelope, the segment will be skipped. Adjacent segments must have different levels for the rate control to work.";
static const char* tt2 =
		"Tempo: The envelope times vary based on the master tempo setting. Note values are displayed instead of a number when the time corresponds to an exact note value. Tempo-based envelopes are useful when using external sequencers and arpeggiators because the envelope rates compress and expand according to the master tempo setting, keeping the envelopes in sync with the sequence or arpeggio.";
static const char* tt3 = "Superimpose: Always show all voice envelopes.";
static const char* tt4 = "Keep envelope selection, zoom level and superimpose settings in sync between voices.";

static const char* bt0 = "Select volume-, filter- or auxillary envelope (shortcut: Mousewheel).";
static const char* bt1 = "Copy current envelope values to this envelope.";
static const char* bt2 = "Some common envelope shapes: Plucked, String, Organ and Percussion.";
static const char* st0 = "Drag: zoom\nMousewheel: cycle envelopes\nMousewheel over overlapping values: cycle selection";

int Envelope_Editor::handle(int ev)
{
	static int dx, dy, phover = -1, first_drag;
	if (!active_r())
		return 0;
	switch (ev)
	{
		case FL_LEAVE:
			fl_cursor(FL_CURSOR_DEFAULT);
			if (hover > -1)
			{
				hover = -1;
				redraw();
			}
			return 1;

		case FL_MOUSEWHEEL:
			if (this == Fl::belowmouse())
			{
				if (hover != -1)
				{
					// switch selected dragbox
					if (Fl::event_dy() > 0)
					{
						for (char i = hover + 1; i < 6; i++)
						{
							if (hover_list & (1 << i))
							{
								hover = i;
								redraw();
								break;
							}
						}
					}
					else
					{
						for (char i = hover - 1; i >= 0; i--)
						{
							if (hover_list & (1 << i))
							{
								hover = i;
								redraw();
								break;
							}
						}
					}
				}
				else
				{
					// switch mode
					if (Fl::event_dy() > 0)
						mode += 1;
					else
						mode += modes - 1;
					mode %= modes;
					if (ui->syncview)
						sync_view(layer);
					redraw();
				}
				return 1;
			}
			break;

		case FL_MOVE:
			Fl_Tooltip::enter_area(this, 0, 0, 0, 0, 0);
			button_hover = -1;
			phover = -1;
			hover_list = 0;
			// coordinate system
			if (Fl::event_inside(ee_x0 + 2, ee_y0 + 20, ee_w - 4, ee_h - 42))
			{
				Fl_Tooltip::enter_area(this, ee_x0 + 2, ee_y0 + 20, ee_w - 4, ee_h - 42, st0);
				if (mode == VOLUME && env[VOLUME].mode == FACTORY)
					return 1;
				for (unsigned char i = 0; i < 6; i++)
				{
					if (Fl::event_inside(dragbox[i][0] - 4, dragbox[i][1] - 4, 9, 9))
					{
						Fl_Tooltip::enter_area(this, 0, 0, 0, 0, 0);
						phover = i;
						if (hover == -1)
							hover = i;
						hover_list |= (1 << i);
					}
				}
				if (phover == -1 && hover != -1)
				{
					fl_cursor(FL_CURSOR_DEFAULT);
					hover = -1;
					redraw();
				}
				else if (hover != -1)
				{
					if (!(hover_list & (1 << hover)))
						hover = phover;
					fl_cursor(FL_CURSOR_CROSS);
					redraw();
				}
			}
			// mode buttons
			else if (Fl::event_inside(ee_x0, ee_y0 + 3, ee_w, 17))
			{
				fl_cursor(FL_CURSOR_DEFAULT);
				for (unsigned char i = 0; i < 5; i++)
					if (Fl::event_inside(mode_button[i] - 2, ee_y0 + 3, 52, 17))
					{
						const char* tt = 0;
						switch (i)
						{
							case 0:
								if (mode == VOLUME)
									tt = tt0;
								else
									tt = tt00;
								break;
							case 1:
								tt = tt1;
								break;
							case 2:
								tt = tt2;
								break;
							case 3:
								tt = tt3;
								break;
							case 4:
								tt = tt4;
								break;
						}
						Fl_Tooltip::enter_area(this, mode_button[i], 2, 60, 17, tt);
						button_hover = i;
						return 1;
					}
			}
			// extra buttons
			else if (Fl::event_inside(ee_x0, ee_y0 + ee_h - 21, ee_w, 18))
			{
				fl_cursor(FL_CURSOR_DEFAULT);
				for (unsigned char i = 0; i < 6; i++)
				{
					if (Fl::event_inside(copy_button[i], ee_y0 + ee_h - 21, 17, 17))
					{
						if (i > 2)
							Fl_Tooltip::enter_area(this, copy_button[i], ee_h - 21, 60, 17, bt1);
						else
							Fl_Tooltip::enter_area(this, copy_button[i], ee_h - 21, 60, 17, bt0);
						button_hover = i + VOLUME_SELECTED;
						return 1;
					}
				}
				for (unsigned char i = 0; i < 4; i++)
				{
					if (Fl::event_inside(shape_button[i], ee_y0 + ee_h - 21, 17, 17))
					{
						Fl_Tooltip::enter_area(this, copy_button[i], ee_h - 21, 60, 17, bt2);
						button_hover = i + SHAPE_A;
						return 1;
					}
				}
			}
			else
			{
				fl_cursor(FL_CURSOR_DEFAULT);
				hover = -1;
				redraw();
			}
			return 1;

		case FL_PUSH:
			first_drag = 1; // used only for zooming below
			if (FL_LEFT_MOUSE == Fl::event_button())
			{
				if (button_hover != -1) // mode buttons
				{
					button_push = true;
					redraw();
				}
				push_x = Fl::event_x();
				push_y = Fl::event_y();
			}
			return 1;

		case FL_RELEASE:
			fl_cursor(FL_CURSOR_DEFAULT);
			if (button_push)
			{
				switch (button_hover)
				{
					case FACTORY:
						if (mode == VOLUME)
						{
							if (env[mode].mode != FACTORY)
							{
								pxk->widget_callback(1793, FACTORY, layer);
								env[mode].mode = FACTORY;
							}
						}
						else if (mode == FILTER)
						{
							if (env[FILTER].repeat)
							{
								pxk->widget_callback(1833, 0, layer);
								env[FILTER].repeat = 0;
							}
							else
							{
								pxk->widget_callback(1833, 1, layer);
								env[FILTER].repeat = 1;
							}
						}
						else if (mode == AUXILIARY)
						{
							if (env[AUXILIARY].repeat)
							{
								pxk->widget_callback(1834, 0, layer);
								env[AUXILIARY].repeat = 0;
							}
							else
							{
								pxk->widget_callback(1834, 1, layer);
								env[AUXILIARY].repeat = 1;
							}
						}
						break;
					case TIME_BASED:
						if (env[mode].mode != TIME_BASED)
						{
							pxk->widget_callback(1793 + mode * 13, TIME_BASED, layer);
							env[mode].mode = TIME_BASED;
						}
						break;
					case TEMPO_BASED:
						if (env[mode].mode != TEMPO_BASED)
						{
							pxk->widget_callback(1793 + mode * 13, TEMPO_BASED, layer);
							env[mode].mode = TEMPO_BASED;
						}
						break;
					case OVERLAY:
						overlay ? overlay = false : overlay = true;
						if (ui->syncview)
							sync_view(layer);
						break;
					case SYNC_VOICE_VIEW:
						ui->syncview ? ui->syncview = 0 : ui->syncview = 1;
						if (ui->syncview)
							sync_view(layer);
						break;
					case VOLUME_SELECTED:
						mode = VOLUME;
						if (ui->syncview)
							sync_view(layer);
						break;
					case FILTER_SELECTED:
						mode = FILTER;
						if (ui->syncview)
							sync_view(layer);
						break;
					case AUXILIARY_SELECTED:
						mode = AUXILIARY;
						if (ui->syncview)
							sync_view(layer);
						break;
					case CPY_VOLUME:
						copy_envelope(mode, VOLUME);
						break;
					case CPY_FILTER:
						copy_envelope(mode, FILTER);
						break;
					case CPY_AUXILIARY:
						copy_envelope(mode, AUXILIARY);
						break;
					case SHAPE_A:
					case SHAPE_B:
					case SHAPE_C:
					case SHAPE_D:
						set_shape(mode, button_hover);
						break;
				}
			}
			hover_list = 0;
			button_push = false;
			redraw();
			return 1;

		case FL_DRAG:
			dx = Fl::event_x() - push_x;
			dy = push_y - Fl::event_y();
			if (hover != -1) // drag the box if we hover it
			{
				hover_list = (1 << hover);
				int dx_jump = 0;
				if (zoomlevel < 1)
					dx /= zoomlevel; // drag value is 2^n n>0
				else
				{
					if (dx % (int) zoomlevel != 0 && !(dx / zoomlevel)) // only drag +/-1 values
						dx = 0;
					else
					{
						dx_jump = dx % (int) zoomlevel;
						dx /= zoomlevel; // should be +/-1 now
					}
				}
				if (dx > 0 && env[mode].stage[hover][0] != 127)
				{
					if (env[mode].stage[hover][0] + dx < 127)
						env[mode].stage[hover][0] += dx;
					else
						env[mode].stage[hover][0] = 127;
					pxk->widget_callback(1793 + 1 + mode * 13 + hover * 2, env[mode].stage[hover][0], layer);
					push_x = Fl::event_x() - dx_jump;
				}
				else if (dx < 0 && env[mode].stage[hover][0] != 0)
				{
					if (env[mode].stage[hover][0] + dx >= 0)
						env[mode].stage[hover][0] += dx;
					else
						env[mode].stage[hover][0] = 0;
					pxk->widget_callback(1793 + 1 + mode * 13 + hover * 2, env[mode].stage[hover][0], layer);
					push_x = Fl::event_x() - dx_jump;
				}

				if (dy > 0 && env[mode].stage[hover][1] != 100)
				{
					if (env[mode].stage[hover][1] + dy < 100)
						env[mode].stage[hover][1] += dy;
					else
						env[mode].stage[hover][1] = 100;
					pxk->widget_callback(1793 + 2 + mode * 13 + hover * 2, env[mode].stage[hover][1], layer);
					push_y = Fl::event_y();
				}
				else if (dy < 0)
				{
					if (mode == VOLUME && env[mode].stage[hover][1] != 0)
					{
						if (env[mode].stage[hover][1] + dy > 0)
							env[mode].stage[hover][1] += dy;
						else
							env[mode].stage[hover][1] = 0;
						pxk->widget_callback(1793 + 2 + mode * 13 + hover * 2, env[mode].stage[hover][1], layer);
						push_y = Fl::event_y();
					}
					else if (mode != VOLUME && env[mode].stage[hover][1] != -100)
					{
						if (env[mode].stage[hover][1] + dy > -100)
							env[mode].stage[hover][1] += dy;
						else
							env[mode].stage[hover][1] = -100;
						pxk->widget_callback(1793 + 2 + mode * 13 + hover * 2, env[mode].stage[hover][1], layer);
						push_y = Fl::event_y();
					}
				}
				redraw();
			}
			else if (button_hover == -1) // zoom
			{
				if ((abs(dx) > 5 && first_drag) || abs(dx) % 35 > 30)
				{
					if (dx < 0 && zoomlevel > 1)
					{
						zoomlevel /= 2;
						redraw();
						if (ui->syncview)
							sync_view(layer);
						if (zoomlevel == 1)
						{
							push_x = Fl::event_x();
							first_drag = 1;
						}
						else
							first_drag = 0;
					}
					else if (dx > 0 && zoomlevel < 4)
					{
						zoomlevel *= 2;
						redraw();
						if (ui->syncview)
							sync_view(layer);
						if (zoomlevel == 4)
						{
							push_x = Fl::event_x();
							first_drag = 1;
						}
						else
							first_drag = 0;
					}
					else
						push_x = Fl::event_x();
				}
			}
			else if (button_hover >= 0) // lower button pushed
			{
				// extra buttons
				if (button_hover >= SHAPE_A)
				{
					if (!Fl::event_inside(shape_button[button_hover - SHAPE_A], ee_y0 + ee_h - 18, 16, 14))
						button_push = false;
					else
						button_push = true;
				}
				// copy
				else if (button_hover >= VOLUME_SELECTED)
				{
					if (!Fl::event_inside(copy_button[button_hover - VOLUME_SELECTED], ee_y0 + ee_h - 18, 16, 14))
						button_push = false;
					else
						button_push = true;
				}
				else // top buttons
				{
					if (!Fl::event_inside(mode_button[button_hover], ee_y0 + 5, 50, 14))
						button_push = false;
					else
						button_push = true;
				}
				redraw();
			}
			return 1;
		default:
			break;
	}
	return Fl_Box::handle(ev);
}

void Envelope_Editor::set_data(unsigned char type, int* stages, char mode, char repeat)
{
	//pmesg("Envelope_Editor::set_data(%d, int*, %d, %d)\n", type, mode, repeat);
	env[type].mode = mode;
	env[type].repeat = repeat;
	for (unsigned char i = 0; i < 6; i++)
	{
		env[type].stage[i][0] = *(stages + i * 2);
		env[type].stage[i][1] = *(stages + i * 2 + 1);
	}
	redraw();
}

void Envelope_Editor::copy_envelope(unsigned char src, unsigned char dst)
{
	if (dst == src)
		return;
	for (unsigned int i = 0; i < 6; i++)
	{
		env[dst].stage[i][0] = env[src].stage[i][0];
		env[dst].stage[i][1] = env[src].stage[i][1];
		if (dst == VOLUME && env[src].stage[i][1] < 0)
			env[dst].stage[i][1] = 0;
		pxk->widget_callback(1793 + 1 + dst * 13 + i * 2, env[dst].stage[i][0], layer);
		pxk->widget_callback(1793 + 1 + dst * 13 + i * 2 + 1, env[dst].stage[i][1], layer);
	}
	redraw();
}

void Envelope_Editor::set_shape(unsigned char dst, char shape)
{
	switch (shape)
	{
		case SHAPE_A: // prc
			env[dst].stage[0][0] = 0;
			env[dst].stage[0][1] = 100;
			env[dst].stage[1][0] = 32;
			env[dst].stage[1][1] = 0;
			env[dst].stage[2][0] = 32;
			env[dst].stage[2][1] = 0;
			env[dst].stage[3][0] = 0;
			env[dst].stage[3][1] = 100;
			env[dst].stage[4][0] = 0;
			env[dst].stage[4][1] = 0;
			env[dst].stage[5][0] = 0;
			env[dst].stage[5][1] = 0;
			break;
		case SHAPE_B: // organ
			env[dst].stage[0][0] = 0;
			env[dst].stage[0][1] = 100;
			env[dst].stage[1][0] = 0;
			env[dst].stage[1][1] = 100;
			env[dst].stage[2][0] = 3;
			env[dst].stage[2][1] = 0;
			env[dst].stage[3][0] = 0;
			env[dst].stage[3][1] = 100;
			env[dst].stage[4][0] = 127;
			env[dst].stage[4][1] = 100;
			env[dst].stage[5][0] = 0;
			env[dst].stage[5][1] = 0;
			break;
		case SHAPE_C: // string
			env[dst].stage[0][0] = 23;
			env[dst].stage[0][1] = 100;
			env[dst].stage[1][0] = 0;
			env[dst].stage[1][1] = 100;
			env[dst].stage[2][0] = 58;
			env[dst].stage[2][1] = 0;
			env[dst].stage[3][0] = 0;
			env[dst].stage[3][1] = 100;
			env[dst].stage[4][0] = 80;
			env[dst].stage[4][1] = 100;
			env[dst].stage[5][0] = 0;
			env[dst].stage[5][1] = 0;
			break;
		case SHAPE_D: // plucked
			env[dst].stage[0][0] = 0;
			env[dst].stage[0][1] = 100;
			env[dst].stage[1][0] = 64;
			env[dst].stage[1][1] = 50;
			env[dst].stage[2][0] = 48;
			env[dst].stage[2][1] = 0;
			env[dst].stage[3][0] = 0;
			env[dst].stage[3][1] = 100;
			env[dst].stage[4][0] = 0;
			env[dst].stage[4][1] = 0;
			env[dst].stage[5][0] = 0;
			env[dst].stage[5][1] = 0;
			break;
	}
	redraw();
	if (!pxk) // not there on init
		return;
	for (unsigned char i = 0; i < 6; i++)
	{
		pxk->widget_callback(1793 + 1 + dst * 13 + i * 2, env[dst].stage[i][0], layer);
		pxk->widget_callback(1793 + 1 + dst * 13 + i * 2 + 1, env[dst].stage[i][1], layer);
	}
}

void Envelope_Editor::set_layer(char l)
{
	layer = l;
}

void Envelope_Editor::sync_view(char l, char m, float z, bool o)
{
	if (l == layer) // set other
	{
		for (int i = 0; i < 4; i++)
		{
			if (i == layer)
				continue;
			ui->layer_editor[i]->envelope_editor->sync_view(layer, mode, zoomlevel, overlay);
		}
	}
	else // update ourselfes
	{
		if (ui->syncview)
		{
			mode = m;
			zoomlevel = z;
			overlay = o;
		}
	}
}

// Piano keyboard widget
void Piano::draw()
{
	if (damage() & FL_DAMAGE_ALL)
	{
		draw_ranges();
		switch (mode)
		{
			case KEYRANGE:
				draw_piano();
				// make sure we draw the transpose position
				active_keys[72 - transpose[selected_transpose_layer]] = -1;
				draw_highlights();
				draw_case();
				break;
			case VELOCITY:
				draw_curve(VELOCITY);
				break;
			case REALTIME:
				draw_curve(REALTIME);
				break;
		}
		return;
	}
	if (damage() & (FL_DAMAGE_ALL | D_RANGES))
		draw_ranges();
	if (damage() & (FL_DAMAGE_ALL | D_KEYS))
		draw_piano();
	if (damage() & (FL_DAMAGE_ALL | D_HIGHLIGHT))
		draw_highlights();
	if (damage() & (FL_DAMAGE_ALL | D_CASE))
		draw_case();
}

const char* tt_p0 =
		"Mousewheel: cycle range type\n\n1/2/3/4: Layer keyrange & key crossfade\nP: Preset arp keyrange\nM: Master arp keyrange\n1/2: Link 1/2 keyrange";
const char* tt_p1 = "Mousewheel: cycle range type\n\n1/2/3/4: Layer velocity range & velocity crossfade";
const char* tt_p2 = "Mousewheel: cycle range type\n\n1/2/3/4: Layer real-time range & real-time crossfade";
const char* tt_p3 = "Left: play\nRight: latch\nDrag on 'case': set velocity";

int Piano::handle(int ev)
{
	static unsigned char i = 0;
	static unsigned char j = 0;
	static int mx = 0;
	mx = Fl::event_x() - w_black / 2; // dragbox beneath mousepointer
	switch (ev)
	{
		case FL_LEAVE:
			if (hovered_key != NONE)
			{
				if (mode == KEYRANGE)
					activate_key(-1, hovered_key);
				hovered_key = NONE;
				previous_hovered_key = NONE;
			}
			fl_cursor(FL_CURSOR_DEFAULT);
			return 1;

		case FL_MOUSEWHEEL:
			// switch mode
			char tmp;
			if (Fl::event_dy() > 0)
				tmp = mode + 1;
			else
				tmp = mode + modes - 1;
			tmp %= modes;
			((Fl_Button*) ui->pi_mode->child(tmp))->setonly();
			set_mode(tmp);
			return 1;
		case FL_MOVE:
			Fl_Tooltip::enter_area(this, 0, 0, 0, 0, 0);
			if (mode == KEYRANGE && Fl::event_inside(keyboard_x0, keyboard_y0 - 10, keyboard_w, 10)) // case
				Fl_Tooltip::enter_area(this, keyboard_x0, keyboard_y0 + h_white, keyboard_w, 120, tt_p3);
			else if (Fl::event_inside(keyboard_x0, keyboard_y0 + h_white, keyboard_w, 120)) // ranges
			{
				if (mode == KEYRANGE)
					Fl_Tooltip::enter_area(this, keyboard_x0, keyboard_y0 + h_white, keyboard_w, 120, tt_p0);
				else if (mode == VELOCITY)
					Fl_Tooltip::enter_area(this, keyboard_x0, keyboard_y0 + h_white, keyboard_w, 120, tt_p1);
				else if (mode == REALTIME)
					Fl_Tooltip::enter_area(this, keyboard_x0, keyboard_y0 + h_white, keyboard_w, 120, tt_p2);
				if (hovered_key != NONE)
				{
					if (mode == KEYRANGE)
						activate_key(-1, hovered_key);
					hovered_key = NONE;
					previous_hovered_key = NONE;
				}
				bool changed = false;
				for (i = 0; i < 8; i++)
				{
					for (j = 0; j < 4; j++)
					{
						if (Fl::event_inside(dragbox[mode][i][j][0], dragbox[mode][i][j][1], w_black, 8))
						{
							fl_cursor(FL_CURSOR_CROSS);
							highlight_dragbox[i][j] = 1;
							changed = true;
						}
						else if (highlight_dragbox[i][j])
						{
							highlight_dragbox[i][j] = 0;
							changed = true;
						}
					}
				}
				if (changed)
				{
					Fl_Tooltip::enter_area(this, 0, 0, 0, 0, 0);
					damage(D_RANGES);
				}
				return 1;
			}
			else if (mode == KEYRANGE && Fl::event_inside(keyboard_x0, keyboard_y0, keyboard_w, h_white)) // keys
				calc_hovered(Fl::event_x(), Fl::event_y() - keyboard_y0);
			else if (hovered_key != NONE)
			{
				if (mode == KEYRANGE)
					activate_key(-1, hovered_key);
				hovered_key = NONE;
				previous_hovered_key = NONE;
			}
			fl_cursor(FL_CURSOR_DEFAULT);
			return 1;

		case FL_PUSH:
			if (this != Fl::belowmouse())
				return 0;
			push_x = 0;
			pushed = NONE;
			pushed_range = NONE;
			switch (Fl::event_button())
			{
				case FL_LEFT_MOUSE:
					// play the piano
					if (Fl::event_inside(keyboard_x0, keyboard_y0, keyboard_w, h_white) && mode == KEYRANGE)
					{
						pushed = PIANO;
						// press key
						previous_hovered_key = hovered_key;
						if (active_keys[hovered_key] > 1)
						{
							// ok, we press the key here so you might ask:
							// why do you first send a note-off?
							// answer: you cannot push a key twice without
							// releasing it first.
							midi->write_event(NOTE_OFF, hovered_key, 0);
							midi->write_event(NOTE_ON, hovered_key, key_velocity);
						}
						else
							midi->write_event(NOTE_ON, hovered_key, key_velocity);
						active_keys[hovered_key] = 1;
						damage(D_HIGHLIGHT);
					}
					else if (Fl::event_inside(keyboard_x0 - 10, keyboard_y0 + h_white, keyboard_w + 15, 120))
					{
						// pushed ranges
						for (i = 0; i < 8; i++)
							for (j = 0; j < 4; j++)
								if (Fl::event_inside(dragbox[mode][i][j][0], dragbox[mode][i][j][1], w_black, 8))
								{
									if (mode == KEYRANGE)
									{
										if (j % 2 == 0)
											hovered_key = prev_key_value[mode][i][j];
										else if (j == LOW_FADE)
											hovered_key = prev_key_value[mode][i][LOW_FADE] + prev_key_value[mode][i][LOW_KEY];
										else
											hovered_key = prev_key_value[mode][i][HIGH_KEY] - prev_key_value[mode][i][HIGH_FADE];
										previous_hovered_key = hovered_key;
										if (active_keys[hovered_key] < 1)
											active_keys[hovered_key] = 1;
										damage(D_HIGHLIGHT);
									}
									pushed = i; // layer/arp
									pushed_range = j; // key
									return 1;
								}
					}
					// set velocity
					else if (Fl::event_inside(keyboard_x0, keyboard_y0 - 10, keyboard_w, 10) && mode == KEYRANGE)
						push_x = Fl::event_x();
					break;

				case FL_MIDDLE_MOUSE:
					// set transpose, middle-c == key 72
					if (Fl::event_inside(keyboard_x0, keyboard_y0, keyboard_w, h_white) && mode == KEYRANGE)
					{
						transpose[selected_transpose_layer] = 72 - hovered_key;
						pxk->widget_callback(1429, transpose[selected_transpose_layer], selected_transpose_layer);
						damage(D_KEYS | D_HIGHLIGHT);
					}
					break;

				case FL_RIGHT_MOUSE:
					if (Fl::event_inside(keyboard_x0, keyboard_y0, keyboard_w, h_white) && mode == KEYRANGE)
					{
						// press + hold key
						if (active_keys[hovered_key] > 1)
						{
							midi->write_event(NOTE_OFF, hovered_key, 0);
							active_keys[hovered_key] = -1;
						}
						else
						{
							midi->write_event(NOTE_ON, hovered_key, key_velocity);
							active_keys[hovered_key] = 2;
						}
						damage(D_HIGHLIGHT);
					}
					break;
			}
			return 1;

		case FL_DRAG:
			switch (Fl::event_button())
			{
				case FL_LEFT_MOUSE:
					if (pushed == PIANO)
					{
						// dragging on keyboard
						play_hovered_key = 1;
						calc_hovered(Fl::event_x(), Fl::event_y() - keyboard_y0);
						return 1;
					}
					if (pushed > NONE)
					{
						// fade value contraints are simple, just keep them inside the keyrange
						if (pushed_range == LOW_FADE || pushed_range == HIGH_FADE)
						{
							if (mx < dragbox[mode][pushed][LOW_KEY][0])
								mx = dragbox[mode][pushed][LOW_KEY][0];
							else if (mx > dragbox[mode][pushed][HIGH_KEY][0])
								mx = dragbox[mode][pushed][HIGH_KEY][0];
							calc_hovered(mx + w_black / 2, 1);
						}
						// key values
						else
						{
							// low key
							if (pushed_range == LOW_KEY)
							{
								// arps return here
								if (pushed == PRESET_ARP || pushed == MASTER_ARP || pushed == LINK_ONE || pushed == LINK_TWO)
								{
									if (mx < keyboard_x0 + 3)
										mx = keyboard_x0 + 3;
									else if (mx > dragbox[0][pushed][HIGH_KEY][0])
										mx = dragbox[0][pushed][HIGH_KEY][0];
									calc_hovered(mx + w_black / 2, 1);
									dragbox[0][pushed][pushed_range][0] = mx;
									damage(D_RANGES);
									return 1;
								}
								// lower edge
								if (mx < keyboard_x0 + 3)
									mx = keyboard_x0 + 3;
								else if (mx > dragbox[mode][pushed][HIGH_KEY][0])
									mx = dragbox[mode][pushed][HIGH_KEY][0];
								calc_hovered(mx + w_black / 2, 1);
								// high key aus der ecke schubsen
								if (dragbox[mode][pushed][HIGH_KEY][0] <= keyboard_x0 + 7
										&& dragbox[mode][pushed][LOW_KEY][0] < keyboard_x0 + 7)
								{
									dragbox[mode][pushed][HIGH_KEY][0] = keyboard_x0 + 14;
									new_key_value[mode][pushed][HIGH_KEY] = 2;
									commit_changes();
								}
								// drag the fade value with me
								dragbox[mode][pushed][LOW_FADE][0] += mx - dragbox[mode][pushed][LOW_KEY][0];
								if (dragbox[mode][pushed][LOW_FADE][0] > dragbox[mode][pushed][HIGH_KEY][0])
								{
									dragbox[mode][pushed][LOW_FADE][0] = dragbox[mode][pushed][HIGH_KEY][0];
									new_key_value[mode][pushed][LOW_FADE] = prev_key_value[mode][pushed][HIGH_KEY] - hovered_key;
								}
								// move the high key fade value away
								if (dragbox[mode][pushed][HIGH_FADE][0] < mx)
								{
									dragbox[mode][pushed][HIGH_FADE][0] = mx;
									new_key_value[mode][pushed][HIGH_FADE] = prev_key_value[mode][pushed][HIGH_KEY] - hovered_key;
								}
							}
							// high key

							else
							{
								// arps return
								if (pushed == PRESET_ARP || pushed == MASTER_ARP || pushed == LINK_ONE || pushed == LINK_TWO)
								{
									if (mx > keyboard_w + keyboard_x0 - 9)
										mx = keyboard_w + keyboard_x0 - 9;
									else if (mx < dragbox[0][pushed][LOW_KEY][0])
										mx = dragbox[0][pushed][LOW_KEY][0];
									calc_hovered(mx + w_black / 2, 1);
									dragbox[0][pushed][pushed_range][0] = mx;
									damage(D_RANGES);
									return 1;
								}
								// upper edge
								if (mx > keyboard_w + keyboard_x0 - 9)
									mx = keyboard_w + keyboard_x0 - 9;
								else if (mx < dragbox[mode][pushed][LOW_KEY][0])
									mx = dragbox[mode][pushed][LOW_KEY][0];
								calc_hovered(mx + w_black / 2, 1);
								// drag the fade value with me
								dragbox[mode][pushed][HIGH_FADE][0] += mx - dragbox[mode][pushed][HIGH_KEY][0];
								if (dragbox[mode][pushed][HIGH_FADE][0] < dragbox[mode][pushed][LOW_KEY][0])
								{
									dragbox[mode][pushed][HIGH_FADE][0] = dragbox[mode][pushed][LOW_KEY][0];
									new_key_value[mode][pushed][HIGH_FADE] = hovered_key - prev_key_value[mode][pushed][LOW_KEY];
								}
								// move the low key fade value away
								if (dragbox[mode][pushed][LOW_FADE][0] > mx)
								{
									dragbox[mode][pushed][LOW_FADE][0] = mx;
									new_key_value[mode][pushed][LOW_FADE] = hovered_key - prev_key_value[mode][pushed][LOW_KEY];
								}
							}
						}
						dragbox[mode][pushed][pushed_range][0] = mx;
						if (mode == KEYRANGE)
							damage(D_RANGES | D_HIGHLIGHT);
						else
							damage(FL_DAMAGE_ALL);
					}
					// velocity setup

					else if (push_x)
					{
						int dx = Fl::event_x() - push_x;
						if (dx > 0)
						{
							if (key_velocity + dx <= 127)
								key_velocity += dx;
							else
								key_velocity = 127;
						}
						else if (dx < 0)
						{
							if (key_velocity + dx >= 0)
								key_velocity += dx;
							else
								key_velocity = 0;
						}
						push_x = Fl::event_x();
						damage(D_CASE);
					}
			}
			return 1;

		case FL_RELEASE:
			fl_cursor(FL_CURSOR_DEFAULT);
			switch (Fl::event_button())
			{
				case FL_LEFT_MOUSE:
					if (mode == KEYRANGE && pushed == PIANO)
					{
						midi->write_event(NOTE_OFF, hovered_key, 0);
						active_keys[hovered_key] = -1;
						hovered_key = NONE;
						previous_hovered_key = NONE;
						play_hovered_key = 0;
						pushed = NONE;
						damage(D_HIGHLIGHT);
						return 1;
					}
					// dragbox dragged: snap to grid
					if (pushed > NONE && pushed != PIANO)
					{
						if (mode == KEYRANGE)
						{
							if (pushed_range % 2 == 0
									&& dragbox[mode][pushed][pushed_range][0] == dragbox[mode][pushed][pushed_range + 1][0])
							{
								if (taste_x0[hovered_key][1] == 0)
								{
									dragbox[mode][pushed][pushed_range][0] = taste_x0[hovered_key][0] + 3;
									dragbox[mode][pushed][pushed_range + 1][0] = taste_x0[hovered_key][0] + 3;
								}
								else
								{
									dragbox[mode][pushed][pushed_range][0] = taste_x0[hovered_key][0];
									dragbox[mode][pushed][pushed_range + 1][0] = taste_x0[hovered_key][0];
								}
							}
							else
							{
								if (taste_x0[hovered_key][1] == 0)
									dragbox[mode][pushed][pushed_range][0] = taste_x0[hovered_key][0] + 3;
								else
									dragbox[mode][pushed][pushed_range][0] = taste_x0[hovered_key][0];
							}
							damage(D_HIGHLIGHT);
						}
						highlight_dragbox[pushed][pushed_range] = 0;
						pushed = NONE;
						damage(D_RANGES);
					}
			}
			return 1;
	}
	return Fl_Box::handle(ev);
}

void Piano::draw_ranges()
{
	fl_push_clip(keyboard_x0 - 10, keyboard_y0 + h_white, keyboard_w + 15, 120);
	fl_color(FL_BACKGROUND_COLOR);
	fl_rectf(keyboard_x0 - 10, keyboard_y0 + h_white + 1, keyboard_w + 15, 119);
	fl_color(FL_FOREGROUND_COLOR);
	fl_font(FL_HELVETICA, 10);
	static unsigned char show_layers;
	ui->eall ? show_layers = 1 : show_layers = 4;
	char buf[4];
	unsigned char i;
	for (i = 0; i < show_layers; i++)
	{
		snprintf(buf, 4, "%d", i + 1);
		fl_draw(buf, keyboard_x0 - 9, dragbox[mode][i][0][1] + 7);
	}
	if (mode == KEYRANGE)
	{
		// arps
		snprintf(buf, 4, "P");
		fl_draw(buf, keyboard_x0 - 9, dragbox[0][4][0][1] + 7);
		snprintf(buf, 4, "M");
		fl_draw(buf, keyboard_x0 - 9, dragbox[0][5][0][1] + 7);
		// links
		snprintf(buf, 4, "1");
		fl_draw(buf, keyboard_x0 - 9, dragbox[0][6][0][1] + 7);
		snprintf(buf, 4, "2");
		fl_draw(buf, keyboard_x0 - 9, dragbox[0][7][0][1] + 7);
	}
	// draw grid
	fl_color(fl_color_average(FL_BACKGROUND_COLOR, FL_FOREGROUND_COLOR, .95));
	if (mode == KEYRANGE)
	{
		for (i = 0; i < 128; i++)
			if (taste_x0[i][1] == 0)
			{
				fl_line_style(FL_SOLID, 1);
				fl_line(taste_x0[i][0] + 7, dragbox[0][0][0][1] + 4, taste_x0[i][0] + 7, dragbox[0][7][0][1]);
			}
			else
			{
				fl_line_style(FL_SOLID, 2);
				fl_line(taste_x0[i][0] + 4, dragbox[0][0][0][1] + 8, taste_x0[i][0] + 4, dragbox[0][7][0][1] + 4);
			}
	}
	else
	{
		fl_line_style(FL_SOLID, 1);
		int offset = keyboard_x0 + 4;
		for (int i = 8; i < 128; i += 8)
			fl_line(offset + 7 * i, dragbox[0][0][0][1] + 4, offset + 7 * i, dragbox[0][3][0][1]);
	}

	Fl_Color ranges = fl_color_average(FL_BACKGROUND2_COLOR, FL_BACKGROUND_COLOR, .6);
	// draw selected ranges
	for (i = 0; i < show_layers; i++) // for all 4 layers
	{
		// key ranges
		fl_color(ranges);
		fl_rectf(dragbox[mode][i][LOW_KEY][0], dragbox[mode][i][LOW_KEY][1],
				dragbox[mode][i][HIGH_KEY][0] - dragbox[mode][i][LOW_KEY][0] + w_black, 4);
		// fade ranges
		// we want the selected range on top
		if (highlight_dragbox[i][0] || highlight_dragbox[i][1])
		{
			fl_color(ranges);
			fl_rectf(dragbox[mode][i][HIGH_FADE][0], dragbox[mode][i][HIGH_FADE][1],
					dragbox[mode][i][HIGH_KEY][0] - dragbox[mode][i][HIGH_FADE][0] + w_black, 4);
			fl_color(FL_SELECTION_COLOR);
			fl_rectf(dragbox[mode][i][LOW_KEY][0], dragbox[mode][i][LOW_FADE][1],
					dragbox[mode][i][LOW_FADE][0] - dragbox[mode][i][LOW_KEY][0] + w_black, 4);
		}
		else if (highlight_dragbox[i][2] || highlight_dragbox[i][3])
		{
			fl_color(ranges);
			fl_rectf(dragbox[mode][i][LOW_KEY][0], dragbox[mode][i][LOW_FADE][1],
					dragbox[mode][i][LOW_FADE][0] - dragbox[mode][i][LOW_KEY][0] + w_black, 4);
			fl_color(FL_SELECTION_COLOR);
			fl_rectf(dragbox[mode][i][HIGH_FADE][0], dragbox[mode][i][HIGH_FADE][1],
					dragbox[mode][i][HIGH_KEY][0] - dragbox[mode][i][HIGH_FADE][0] + w_black, 4);
		}
		else
		{
			fl_color(ranges);
			fl_rectf(dragbox[mode][i][LOW_KEY][0], dragbox[mode][i][LOW_FADE][1],
					dragbox[mode][i][LOW_FADE][0] - dragbox[mode][i][LOW_KEY][0] + w_black, 4);
			fl_rectf(dragbox[mode][i][HIGH_FADE][0], dragbox[mode][i][HIGH_FADE][1],
					dragbox[mode][i][HIGH_KEY][0] - dragbox[mode][i][HIGH_FADE][0] + w_black, 4);
		}
		// put handles on top
		for (unsigned char j = 0; j < 4; j++) // for all 4 values
		{
			if (highlight_dragbox[i][j])
				fl_color(FL_SELECTION_COLOR);
			else
				fl_color(ranges);
			if (j % 2)
				fl_rectf(dragbox[mode][i][j][0], dragbox[mode][i][j][1], w_black, 8);
			else
				fl_rectf(dragbox[mode][i][j][0], dragbox[mode][i][j][1], w_black, 8);
		}
	}
	if (mode == KEYRANGE)
	{
		// arp ranges
		fl_color(ranges);
		fl_rectf(dragbox[0][4][LOW_KEY][0], dragbox[0][4][LOW_KEY][1] + 2,
				dragbox[0][4][HIGH_KEY][0] - dragbox[0][4][LOW_KEY][0] + w_black, 4);
		fl_rectf(dragbox[0][5][LOW_KEY][0], dragbox[0][5][LOW_KEY][1] + 2,
				dragbox[0][5][HIGH_KEY][0] - dragbox[0][5][LOW_KEY][0] + w_black, 4);
		// link ranges
		fl_rectf(dragbox[0][6][LOW_KEY][0], dragbox[0][6][LOW_KEY][1] + 2,
				dragbox[0][6][HIGH_KEY][0] - dragbox[0][6][LOW_KEY][0] + w_black, 4);
		fl_rectf(dragbox[0][7][LOW_KEY][0], dragbox[0][7][LOW_KEY][1] + 2,
				dragbox[0][7][HIGH_KEY][0] - dragbox[0][7][LOW_KEY][0] + w_black, 4);
		// arp handles
		if (highlight_dragbox[4][LOW_KEY])
			fl_color(FL_SELECTION_COLOR);
		else
			fl_color(ranges);
		fl_rectf(dragbox[0][4][LOW_KEY][0], dragbox[0][4][LOW_KEY][1], w_black, 8);
		if (highlight_dragbox[4][HIGH_KEY])
			fl_color(FL_SELECTION_COLOR);
		else
			fl_color(ranges);
		fl_rectf(dragbox[0][4][HIGH_KEY][0], dragbox[0][4][HIGH_KEY][1], w_black, 8);
		if (highlight_dragbox[5][LOW_KEY])
			fl_color(FL_SELECTION_COLOR);
		else
			fl_color(ranges);
		fl_rectf(dragbox[0][5][LOW_KEY][0], dragbox[0][5][LOW_KEY][1], w_black, 8);
		if (highlight_dragbox[5][HIGH_KEY])
			fl_color(FL_SELECTION_COLOR);
		else
			fl_color(ranges);
		fl_rectf(dragbox[0][5][HIGH_KEY][0], dragbox[0][5][HIGH_KEY][1], w_black, 8);
		// link handles
		if (highlight_dragbox[6][LOW_KEY])
			fl_color(FL_SELECTION_COLOR);
		else
			fl_color(ranges);
		fl_rectf(dragbox[0][6][LOW_KEY][0], dragbox[0][6][LOW_KEY][1], w_black, 8);
		if (highlight_dragbox[6][HIGH_KEY])
			fl_color(FL_SELECTION_COLOR);
		else
			fl_color(ranges);
		fl_rectf(dragbox[0][6][HIGH_KEY][0], dragbox[0][6][HIGH_KEY][1], w_black, 8);
		if (highlight_dragbox[7][LOW_KEY])
			fl_color(FL_SELECTION_COLOR);
		else
			fl_color(ranges);
		fl_rectf(dragbox[0][7][LOW_KEY][0], dragbox[0][7][LOW_KEY][1], w_black, 8);
		if (highlight_dragbox[7][HIGH_KEY])
			fl_color(FL_SELECTION_COLOR);
		else
			fl_color(ranges);
		fl_rectf(dragbox[0][7][HIGH_KEY][0], dragbox[0][7][HIGH_KEY][1], w_black, 8);
	}
	fl_line_style(0);
	fl_pop_clip();
}

void Piano::draw_piano()
{
	fl_push_clip(keyboard_x0, keyboard_y0, keyboard_w + 1, h_white);
	fl_color(FL_BACKGROUND_COLOR);
	fl_rectf(keyboard_x0, keyboard_y0, keyboard_w + 1, h_white);
	unsigned char tmp;
	// white keys
	fl_color(fl_color_average(FL_BACKGROUND2_COLOR, FL_BACKGROUND_COLOR, .6));
	unsigned char i;
	for (i = 0; i < 11; i++)
	{
		tmp = 12 * i;
		fl_rectf(taste_x0[0 + tmp][0] + 1, keyboard_y0, w_white - 2, h_white);
		fl_rectf(taste_x0[2 + tmp][0] + 1, keyboard_y0, w_white - 2, h_white);
		fl_rectf(taste_x0[4 + tmp][0] + 1, keyboard_y0, w_white - 2, h_white);
		fl_rectf(taste_x0[5 + tmp][0] + 1, keyboard_y0, w_white - 2, h_white);
		fl_rectf(taste_x0[7 + tmp][0] + 1, keyboard_y0, w_white - 2, h_white);
		if (i == 10)
			break;
		fl_rectf(taste_x0[9 + tmp][0] + 1, keyboard_y0, w_white - 2, h_white);
		fl_rectf(taste_x0[11 + tmp][0] + 1, keyboard_y0, w_white - 2, h_white);
	}

	// black keys
	fl_color(FL_BACKGROUND_COLOR);
	for (i = 0; i < 11; i++)
	{
		tmp = 12 * i;
		fl_rectf(taste_x0[1 + tmp][0], keyboard_y0, w_black, h_black);
		fl_rectf(taste_x0[3 + tmp][0], keyboard_y0, w_black, h_black);
		fl_rectf(taste_x0[6 + tmp][0], keyboard_y0, w_black, h_black);
		if (i == 10)
			break;
		fl_rectf(taste_x0[8 + tmp][0], keyboard_y0, w_black, h_black);
		fl_rectf(taste_x0[10 + tmp][0], keyboard_y0, w_black, h_black);
	}
	fl_pop_clip();
}

void Piano::draw_highlights()
{
	Fl_Color color_white = fl_color_average(FL_BACKGROUND2_COLOR, FL_BACKGROUND_COLOR, .6);
	for (unsigned char key = 0; key < 128; key++)
		if (active_keys[key] != 0)
		{
			if (taste_x0[key][1] == 1) // black keys
			{
				if (key == 72 - transpose[selected_transpose_layer] && active_keys[key] == -1)
					fl_color(fl_color_average(FL_BACKGROUND_COLOR, FL_GREEN, .8));
				else
				{
					if (active_keys[key] == 2)
						fl_color(fl_color_average(FL_BACKGROUND_COLOR, FL_MAGENTA, .7));
					else
					{
						if (pushed == PIANO && key == hovered_key)
							fl_color(FL_SELECTION_COLOR);
						else if (active_keys[key] < 0)
							fl_color(FL_BACKGROUND_COLOR);
						else
							fl_color(fl_color_average(FL_SELECTION_COLOR, FL_BACKGROUND_COLOR, .5));
					}
				}
				fl_rectf(taste_x0[key][0], keyboard_y0, w_black, h_black);
			}
			else // white keys
			{
				if (key == 72 - transpose[selected_transpose_layer] && active_keys[key] == -1)
					fl_color(fl_color_average(color_white, FL_GREEN, .8));
				else
				{
					if (active_keys[key] == 2)
						fl_color(fl_color_average(color_white, FL_MAGENTA, .7));
					else
					{
						if (pushed == PIANO && key == hovered_key)
							fl_color(FL_SELECTION_COLOR);
						else if (active_keys[key] < 0) // deactivate key
							fl_color(color_white);
						else
							fl_color(fl_color_average(FL_SELECTION_COLOR, color_white, .5));
					}
				}
				fl_rectf(taste_x0[key][0] + 1, keyboard_y0, w_white - 2, h_white);
				// repaint black keys if a white key is hovered
				unsigned char tmp;
				if (key + 1 < 127 && key + 1 != hovered_key)
				{
					tmp = (key + 1) % 12;
					if (tmp == 1 || tmp == 3 || tmp == 6 || tmp == 8 || tmp == 10)
					{
						if (key + 1 == 72 - transpose[selected_transpose_layer] && active_keys[key + 1] != 2)
							fl_color(fl_color_average(FL_BACKGROUND_COLOR, FL_GREEN, .8));
						else
						{
							if (active_keys[key + 1] > 0)
							{
								if (active_keys[key + 1] == 2)
									fl_color(fl_color_average(FL_BACKGROUND_COLOR, FL_MAGENTA, .7));
								else
									fl_color(FL_SELECTION_COLOR);
							}
							else
								fl_color(FL_BACKGROUND_COLOR);
						}
						fl_rectf(taste_x0[key + 1][0], keyboard_y0, w_black, h_black);
					}
				}
				if (key - 1 > 0)
				{
					tmp = (key - 1) % 12;
					if (tmp == 1 || tmp == 3 || tmp == 6 || tmp == 8 || tmp == 10)
					{
						if (key - 1 == hovered_key)
							if (pushed == PIANO)
								fl_color(FL_SELECTION_COLOR);
							else
								fl_color(fl_color_average(FL_SELECTION_COLOR, color_white, .5));
						else if (key - 1 == 72 - transpose[selected_transpose_layer] && active_keys[key - 1] != 2)
							fl_color(fl_color_average(FL_BACKGROUND_COLOR, FL_GREEN, .8));
						else
						{
							if (active_keys[key - 1] > 0)
							{
								if (active_keys[key - 1] == 2)
									fl_color(fl_color_average(FL_BACKGROUND_COLOR, FL_MAGENTA, .7));
								else
									fl_color(fl_color_average(FL_SELECTION_COLOR, color_white, .5));
							}
							else
								fl_color(FL_BACKGROUND_COLOR);
						}
						fl_rectf(taste_x0[key - 1][0], keyboard_y0, w_black, h_black);
					}
				}
			}
			if (active_keys[key] < 0)
				active_keys[key] = 0;
		}
}

void Piano::draw_case()
{
	fl_push_clip(keyboard_x0, keyboard_y0 - 11, keyboard_w + 1, 10);
	fl_color(FL_BACKGROUND2_COLOR);
	fl_rectf(keyboard_x0, keyboard_y0 - 11, keyboard_w + 1, 10);
	fl_color(FL_FOREGROUND_COLOR);
	fl_font(FL_HELVETICA, 10);
	unsigned char offset = 7 * (w_white - 1); // octave
	char buf[9];
	for (unsigned char i = 0; i < 11; i++)
	{
		snprintf(buf, 9, "C%d", i - 2);
		fl_draw(buf, keyboard_x0 + i * offset, keyboard_y0 - 2);
	}
	snprintf(buf, 9, "pd");
	fl_draw(buf, keyboard_x0 + keyboard_w - 15, keyboard_y0 - 2);
	// velocity
	snprintf(buf, 9, "Velo %3d", key_velocity);
	fl_draw(buf, keyboard_x0 + 30, keyboard_y0 - 2);
	fl_pop_clip();
}

void Piano::draw_curve(int type)
{
	fl_push_clip(keyboard_x0, keyboard_y0 - 11, keyboard_w + 1, 10 + h_white);
	fl_color(FL_BACKGROUND_COLOR);
	fl_rectf(keyboard_x0, keyboard_y0 - 11, keyboard_w + 1, 10 + h_white);
	fl_color(fl_color_average(FL_BACKGROUND2_COLOR, FL_BACKGROUND_COLOR, .6));
	fl_polygon(keyboard_x0 + 110, keyboard_h + keyboard_y0, keyboard_w + keyboard_x0 - 3, keyboard_h + keyboard_y0,
			keyboard_w + keyboard_x0 - 3, keyboard_y0 - 11);
	fl_color(FL_FOREGROUND_COLOR);
	fl_font(FL_HELVETICA, 10);
	switch (type)
	{
		case VELOCITY:
			fl_draw("VELOCITY CROSSFADE RANGE", keyboard_x0, keyboard_y0 - 2);
			break;
		case REALTIME:
			fl_draw("REALTIME CROSSFADE RANGE", keyboard_x0, keyboard_y0 - 2);
			break;
	}
	// values
	char buf[30];
	fl_font(FL_COURIER, 8);
	for (unsigned char i = 0; i < 4; i++)
	{
		snprintf(buf, 30, "L%d  %3d %3d  %3d %3d", i + 1, new_key_value[mode][i][LOW_KEY], new_key_value[mode][i][LOW_FADE],
				new_key_value[mode][i][HIGH_KEY], new_key_value[mode][i][HIGH_FADE]);
		fl_draw(buf, keyboard_x0 + 3, keyboard_y0 + 8 + i * 8);
	}
	fl_pop_clip();
}

void Piano::calc_hovered(int x, int y)
{
	if (x <= keyboard_x0)
		hovered_key = 0;
	else if (x >= keyboard_x0 + keyboard_w)
		hovered_key = 127;
	else
	{
		hovered_key = 0;
		if (y < h_black) // black and white-key area
		{
			while (hovered_key < 127)
			{
				++hovered_key;
				if (taste_x0[hovered_key][0] >= x || hovered_key == 127)
				{
					if (hovered_key != 127)
						--hovered_key;
					if (hovered_key == 0)
						break;
					if (taste_x0[hovered_key - 1][1] && taste_x0[hovered_key - 1][0] + w_black >= x)
						--hovered_key;
					break;
				}
			}
		}
		else // only white-keys area (lower area of keyboard)
		{
			while (hovered_key < 127)
			{
				++hovered_key;
				if (taste_x0[hovered_key][0] >= x)
				{
					--hovered_key;
					if (hovered_key == 0)
						break;
					if (taste_x0[hovered_key][1] == 1)
						--hovered_key;
					break;
				}
			}
		}
	}
	if (previous_hovered_key != hovered_key)
	{
		if (play_hovered_key == 1)
		{
			midi->write_event(NOTE_ON, hovered_key, key_velocity);
			if (previous_hovered_key != NONE)
				midi->write_event(NOTE_OFF, previous_hovered_key, 0);
		}
		else if (pushed > NONE)
		{
			if (pushed == PRESET_ARP || pushed == MASTER_ARP)
				new_key_value[0][pushed][pushed_range] = hovered_key;
			else
			{
				if (pushed_range == HIGH_KEY || pushed_range == LOW_KEY)
					new_key_value[mode][pushed][pushed_range] = hovered_key;
				else if (pushed_range == LOW_FADE)
					new_key_value[mode][pushed][pushed_range] = hovered_key - new_key_value[mode][pushed][LOW_KEY];
				else if (pushed_range == HIGH_FADE)
					new_key_value[mode][pushed][pushed_range] = new_key_value[mode][pushed][HIGH_KEY] - hovered_key;
			}
			commit_changes();
		}
		if (mode == KEYRANGE)
		{
			if (active_keys[hovered_key] < 1)
				active_keys[hovered_key] = 1;
			if (previous_hovered_key != NONE)
				if (active_keys[previous_hovered_key] < 2 || play_hovered_key == 1)
					active_keys[previous_hovered_key] = -1;
			damage(D_HIGHLIGHT);
		}
	}
	previous_hovered_key = hovered_key;
}

void Piano::select_transpose_layer(char l)
{
	active_keys[72 - transpose[selected_transpose_layer]] = -1;
	selected_transpose_layer = l;
	active_keys[72 - transpose[selected_transpose_layer]] = -1;
	damage(D_HIGHLIGHT);
}

void Piano::set_mode(char m)
{
	mode = m;
	if (mode == KEYRANGE)
	{
		ui->g_transpose_layer->activate();
		active_keys[72 - transpose[selected_transpose_layer]] = -1;
	}
	else
		ui->g_transpose_layer->deactivate();
	damage(FL_DAMAGE_ALL);
}

// called by midi->process_not_sysex to highlight incoming midi events
// on the keyboard
void Piano::activate_key(char value, unsigned char key)
{
	if (active_keys[key] == value || (active_keys[key] == 2 && value == -1) || (pushed != NONE && key == hovered_key))
		return;
	active_keys[key] += value;
	if (active_keys[key] > 3)
		active_keys[key] = 3;
	else if (active_keys[key] < 1)
		active_keys[key] = -1;
	if (visible_r() && mode == KEYRANGE)
	{
		damage(D_HIGHLIGHT);
		redraw();
	}
}

void Piano::reset_active_keys()
{
	for (int i = 0; i < 128; i++)
		if (active_keys[i] > 0)
			active_keys[i] = -1;
	if (visible_r() && mode == KEYRANGE)
	{
		damage(D_HIGHLIGHT);
		redraw();
	}
}

// map keys to 2-d space
void Piano::set_range_values(unsigned char md, unsigned char layer, unsigned char low_k, unsigned char low_f,
		unsigned char high_k, unsigned char high_f)
{
	//pmesg("Piano::set_range_values(%d, %d, %d, %d, %d, %d)\n", md, layer, low_k, low_f, high_k, high_f);
	if (low_f + low_k > 127)
		low_f = 0;
	if (high_k - high_f < 0)
		high_f = 0;
	dragbox[md][layer][LOW_KEY][0] = taste_x0[low_k][0];
	dragbox[md][layer][LOW_FADE][0] = taste_x0[low_k + low_f][0];
	dragbox[md][layer][HIGH_KEY][0] = taste_x0[high_k][0];
	dragbox[md][layer][HIGH_FADE][0] = taste_x0[high_k - high_f][0];
	for (unsigned char i = 0; i < 4; i++) // center on white keys
	{
		if ((dragbox[md][layer][i][0] - keyboard_x0) % 12 == 0)
			dragbox[md][layer][i][0] += 3; // center on white keys
	}
	// remember current values
	prev_key_value[md][layer][LOW_KEY] = low_k;
	prev_key_value[md][layer][LOW_FADE] = low_f;
	prev_key_value[md][layer][HIGH_KEY] = high_k;
	prev_key_value[md][layer][HIGH_FADE] = high_f;
	new_key_value[md][layer][LOW_KEY] = low_k;
	new_key_value[md][layer][LOW_FADE] = low_f;
	new_key_value[md][layer][HIGH_KEY] = high_k;
	new_key_value[md][layer][HIGH_FADE] = high_f;
	if (visible_r())
		damage(D_RANGES);
}

void Piano::set_transpose(char l1, char l2, char l3, char l4)
{
	//pmesg("Piano::set_transpose(%d, %d, %d, %d)\n", l1, l2, l3, l4);
	transpose[0] = l1;
	transpose[1] = l2;
	transpose[2] = l3;
	transpose[3] = l4;
	active_keys[72 - transpose[selected_transpose_layer]] = -1;
	if (visible_r() && mode == KEYRANGE)
		damage(D_HIGHLIGHT);
}

void Piano::commit_changes()
{
	//pmesg("Piano::commit_changes()\n");
	if (pushed < PRESET_ARP)
	{
		for (unsigned char range = 0; range < 4; range++)
		{
			if (prev_key_value[mode][pushed][range] != new_key_value[mode][pushed][range])
			{
				int id = 1413;
				switch (range)
				{
					case LOW_KEY:
						pxk->widget_callback(id + LOW_KEY + mode * 4, new_key_value[mode][pushed][range], pushed);
						break;
					case LOW_FADE:
						pxk->widget_callback(id + LOW_FADE + mode * 4, new_key_value[mode][pushed][range], pushed);
						break;
					case HIGH_KEY:
						pxk->widget_callback(id + HIGH_KEY + mode * 4, new_key_value[mode][pushed][range], pushed);
						break;
					case HIGH_FADE:
						pxk->widget_callback(id + HIGH_FADE + mode * 4, new_key_value[mode][pushed][range], pushed);
				}
				prev_key_value[mode][pushed][range] = new_key_value[mode][pushed][range];
			}
		}
	}
	else
	{
		if (prev_key_value[0][pushed][pushed_range] != new_key_value[0][pushed][pushed_range])
		{
			switch (pushed)
			{
				case PRESET_ARP:
					if (pushed_range == LOW_KEY)
						pxk->widget_callback(1039, new_key_value[0][pushed][pushed_range], 0);
					else
						pxk->widget_callback(1040, new_key_value[0][pushed][pushed_range], 0);
					break;
				case MASTER_ARP:
					if (pushed_range == LOW_KEY)
						pxk->widget_callback(655, new_key_value[0][pushed][pushed_range], 0);
					else
						pxk->widget_callback(656, new_key_value[0][pushed][pushed_range], 0);
					break;
				case LINK_ONE:
					if (pushed_range == LOW_KEY)
						pxk->widget_callback(1286, new_key_value[0][pushed][pushed_range], 0);
					else
						pxk->widget_callback(1287, new_key_value[0][pushed][pushed_range], 0);
					break;
				case LINK_TWO:
					if (pushed_range == LOW_KEY)
						pxk->widget_callback(1295, new_key_value[0][pushed][pushed_range], 0);
					else
						pxk->widget_callback(1296, new_key_value[0][pushed][pushed_range], 0);
					break;
			}
			prev_key_value[0][pushed][pushed_range] = new_key_value[0][pushed][pushed_range];
		}
	}
}

// MiniPiano keyboard widget
void MiniPiano::draw()
{
	if (damage() & FL_DAMAGE_ALL)
	{
		draw_piano();
		draw_highlights();
		draw_case();
		return;
	}
	if (damage() & (FL_DAMAGE_ALL | D_KEYS))
		draw_piano();
	if (damage() & (FL_DAMAGE_ALL | D_HIGHLIGHT))
		draw_highlights();
	if (damage() & (FL_DAMAGE_ALL | D_CASE))
		draw_case();
}

void MiniPiano::draw_case()
{
	fl_color(FL_BACKGROUND2_COLOR);
	fl_rectf(key_x, keyboard_y0, key_w, 10);
	fl_color(FL_FOREGROUND_COLOR);
	fl_font(FL_HELVETICA, 10);
	// velocity
	char buf[9];
	snprintf(buf, 9, "Velo %3d", key_velocity);
	fl_draw(buf, key_x + 3, keyboard_y0 + 8);
	// position
	snprintf(buf, 4, "C%d", octave - 1);
	fl_draw(buf, key_x + key_w / 2 - 8, keyboard_y0 + 8);
	// octave shift
	fl_polygon(key_x + key_w / 2 - 15, keyboard_y0 + 2, key_x + key_w / 2 - 15, keyboard_y0 + 6, key_x + key_w / 2 - 30,
			keyboard_y0 + 4);
	fl_polygon(key_x + key_w / 2 + 15, keyboard_y0 + 2, key_x + key_w / 2 + 15, keyboard_y0 + 6, key_x + key_w / 2 + 30,
			keyboard_y0 + 4);
}

void MiniPiano::draw_highlights()
{
	Fl_Color color_white = fl_color_average(FL_BACKGROUND2_COLOR, FL_BACKGROUND_COLOR, .6);
	for (unsigned char key = 12 * octave; key <= (12 * octave + 24); key++)
		if (active_keys[key] != 0)
		{
			int mapped_key = key - octave * 12;
			if (taste_x0[key][1] == 1) // black keys (FL_BACKGROUND_COLOR)
			{
				if (active_keys[key] == 2)
					fl_color(fl_color_average(FL_BACKGROUND_COLOR, FL_MAGENTA, .7));
				else
				{
					if (pushed == PIANO && key == hovered_key)
						fl_color(FL_SELECTION_COLOR);
					else if (active_keys[key] < 0)
						fl_color(FL_BACKGROUND_COLOR);
					else
						fl_color(fl_color_average(FL_SELECTION_COLOR, FL_BACKGROUND_COLOR, .5));
				}
				fl_rectf(taste_x0[mapped_key][0], key_y, w_black, h_black);
			}
			else
			{
				if (active_keys[key] == 2)
					fl_color(fl_color_average(color_white, FL_MAGENTA, .7));
				else
				{
					if (pushed == PIANO && key == hovered_key)
						fl_color(FL_SELECTION_COLOR);
					else if (active_keys[key] < 0) // deactivate key
						fl_color(color_white);
					else
						fl_color(fl_color_average(FL_SELECTION_COLOR, color_white, .5));
				}
				fl_rectf(taste_x0[mapped_key][0], key_y, w_white - 2, h_white);
				// repaint black keys if a white key is hovered
				if (mapped_key == 24)
					continue;
				int tmp;
				if (key + 1 < 127)
				{
					tmp = (key + 1) % 12;
					if (tmp == 1 || tmp == 3 || tmp == 6 || tmp == 8 || tmp == 10)
					{
						if (active_keys[key + 1] > 0)
						{
							if (active_keys[key + 1] == 2)
								fl_color(fl_color_average(FL_BACKGROUND_COLOR, FL_MAGENTA, .7));
							else
								fl_color(FL_SELECTION_COLOR);
						}
						else
							fl_color(FL_BACKGROUND_COLOR);
						fl_rectf(taste_x0[mapped_key + 1][0], key_y, w_black, h_black);
					}
				}
				if (key - 1 > 0)
				{
					tmp = (key - 1) % 12;
					if (tmp == 1 || tmp == 3 || tmp == 6 || tmp == 8 || tmp == 10)
					{
						if (key - 1 == hovered_key)
							if (pushed == PIANO)
								fl_color(FL_SELECTION_COLOR);
							else
								fl_color(fl_color_average(FL_SELECTION_COLOR, color_white, .5));
						else if (active_keys[key - 1] > 0)
						{
							if (active_keys[key - 1] == 2)
								fl_color(fl_color_average(FL_BACKGROUND_COLOR, FL_MAGENTA, .7));
							else
								fl_color(fl_color_average(FL_SELECTION_COLOR, color_white, .5));
						}
						else
							fl_color(FL_BACKGROUND_COLOR);
						fl_rectf(taste_x0[mapped_key - 1][0], key_y, w_black, h_black);
					}
				}
			}
			if (active_keys[key] < 0)
				active_keys[key] = 0;
		}
}

void MiniPiano::draw_piano()
{
	// get our position and key koordinates
	keyboard_x0 = this->x();
	keyboard_y0 = this->y();
	keyboard_w = this->w();
	keyboard_h = this->h();

	// keys start/end here
	key_x = keyboard_x0 + 3;
	key_w = keyboard_w - 6;
	// tasten- hoehen/-breiten
	h_white = (float) keyboard_h - 15.;
	w_white = (float) (key_w + 14.) / 15.;
	h_black = h_white * 5. / 8.;
	w_black = w_white * 7. / 13.;
	// spalten
	float s = 14. / 15.;
	// start of keys (y-axis)
	key_y = (float) keyboard_y0 + 11;
	fl_push_clip(key_x, key_y, key_w, h_white);
	fl_color(FL_BACKGROUND_COLOR);
	fl_rectf(key_x, key_y, key_w, h_white);
	static float offset = 1.;
	unsigned char i;
	for (i = 0; i < 11; i++)
	{
		// for each octave on the keyboard
		unsigned char octave = i * 12;
		taste_x0[0 + octave][0] = key_x + offset;
		taste_x0[0 + octave][1] = 0.;
		taste_x0[1 + octave][0] = key_x + w_white - s - w_black / 2. + offset;
		taste_x0[1 + octave][1] = 1.;
		taste_x0[2 + octave][0] = key_x + w_white - s + offset;
		taste_x0[2 + octave][1] = 0.;
		taste_x0[3 + octave][0] = key_x + w_white - s - w_black / 2. + (w_white - s) + offset;
		taste_x0[3 + octave][1] = 1.;
		taste_x0[4 + octave][0] = key_x + 2. * (w_white - s) + offset;
		taste_x0[4 + octave][1] = 0.;
		taste_x0[5 + octave][0] = key_x + 3. * (w_white - s) + offset;
		taste_x0[5 + octave][1] = 0.;
		taste_x0[6 + octave][0] = key_x + w_white - s - w_black / 2. + 3. * (w_white - s) + offset;
		taste_x0[6 + octave][1] = 1.;
		taste_x0[7 + octave][0] = key_x + 4. * (w_white - s) + offset;
		taste_x0[7 + octave][1] = 0.;
		if (i == 10) // keyboard is smaller than full 10 full octaves
			break;
		taste_x0[8 + octave][0] = key_x + w_white - s - w_black / 2. + 4. * (w_white - s) + offset;
		taste_x0[8 + octave][1] = 1.;
		taste_x0[9 + octave][0] = key_x + 5. * (w_white - s) + offset;
		taste_x0[9 + octave][1] = 0.;
		taste_x0[10 + octave][0] = key_x + w_white - s - w_black / 2. + 5. * (w_white - s) + offset;
		taste_x0[10 + octave][1] = 1.;
		taste_x0[11 + octave][0] = key_x + 6. * (w_white - s) + offset;
		taste_x0[11 + octave][1] = 0.;
		offset += 7. * (w_white - s);
	}
	offset = 1.;
	// keys
	int octa;
	// white keys
	fl_color(fl_color_average(FL_BACKGROUND2_COLOR, FL_BACKGROUND_COLOR, .6));
	for (i = 0; i < 3; i++)
	{
		octa = 12 * i;
		fl_rectf(taste_x0[0 + octa][0], key_y, w_white - 2, h_white);
		if (i == 2)
			continue;
		fl_rectf(taste_x0[2 + octa][0], key_y, w_white - 2, h_white);
		fl_rectf(taste_x0[4 + octa][0], key_y, w_white - 2, h_white);
		fl_rectf(taste_x0[5 + octa][0], key_y, w_white - 2, h_white);
		fl_rectf(taste_x0[7 + octa][0], key_y, w_white - 2, h_white);
		fl_rectf(taste_x0[9 + octa][0], key_y, w_white - 2, h_white);
		fl_rectf(taste_x0[11 + octa][0], key_y, w_white - 2, h_white);
	}
	// black keys
	fl_color(FL_BACKGROUND_COLOR);
	for (i = 0; i < 2; i++)
	{
		octa = 12 * i;
		fl_rectf(taste_x0[1 + octa][0], key_y, w_black, h_black);
		fl_rectf(taste_x0[3 + octa][0], key_y, w_black, h_black);
		fl_rectf(taste_x0[6 + octa][0], key_y, w_black, h_black);
		fl_rectf(taste_x0[8 + octa][0], key_y, w_black, h_black);
		fl_rectf(taste_x0[10 + octa][0], key_y, w_black, h_black);
	}
	fl_pop_clip();
}

const char* mp_tt =
		"Left: play\nRight: latch\nMousewheel: octave shift\nMiddle on key: set 'B' note & velocity\nMiddle on case: set 'B' velocity\nDrag on case: set velocity";

int MiniPiano::handle(int ev)
{
	switch (ev)
	{
		case FL_LEAVE:
			if (hovered_key != NONE)
			{
				activate_key(-1, hovered_key);
				hovered_key = NONE;
				previous_hovered_key = NONE;
			}
			return 1;
		case FL_MOVE:
			Fl_Tooltip::enter_area(this, 0, 0, 0, 0, 0);
			if (Fl::event_inside(key_x, key_y, key_w, h_white))
				calc_hovered(Fl::event_x(), Fl::event_y() - key_y);
			else if (hovered_key != NONE)
			{
				activate_key(-1, hovered_key);
				hovered_key = NONE;
				previous_hovered_key = NONE;
			}
			else
				Fl_Tooltip::enter_area(this, keyboard_x0, keyboard_y0, keyboard_w, keyboard_h, mp_tt);
			return 1;
		case FL_PUSH:
			if (this != Fl::belowmouse())
				return 0;
			push_x = 0;
			pushed = NONE;
			switch (Fl::event_button())
			{
				case FL_LEFT_MOUSE:
					// play the piano
					if (Fl::event_inside(key_x, key_y, key_w, h_white))
					{
						pushed = PIANO;
						// press key
						previous_hovered_key = hovered_key;
						if (active_keys[hovered_key] > 1)
						{
							// ok, we press the key here so you might ask:
							// why do you first send a note-off?
							// answer: you cannot push a key twice without
							// releasing it first.
							midi->write_event(NOTE_OFF, hovered_key, 0);
							midi->write_event(NOTE_ON, hovered_key, key_velocity);
						}
						else
							midi->write_event(NOTE_ON, hovered_key, key_velocity);
						active_keys[hovered_key] = 1;
						damage(D_HIGHLIGHT);
					}
					// set velocity or shift octave
					else if (Fl::event_inside(keyboard_x0, keyboard_y0, keyboard_w, keyboard_h - h_white))
					{
						if (Fl::event_inside(keyboard_x0 + keyboard_w / 2 - 30, keyboard_y0, 30, keyboard_h / 3))
							shift_octave(-1);
						else if (Fl::event_inside(keyboard_x0 + keyboard_w / 2, keyboard_y0, 30, keyboard_h / 3)) // shift octave
							shift_octave(1);
						else
							// set velo
							push_x = Fl::event_x();
					}
					break;
				case FL_RIGHT_MOUSE:
					if (Fl::event_inside(key_x, key_y, key_w, h_white))
					{
						// press + hold key
						if (active_keys[hovered_key] > 1)
						{
							midi->write_event(NOTE_OFF, hovered_key, 0);
							active_keys[hovered_key] = -1;
						}
						else
						{
							midi->write_event(NOTE_ON, hovered_key, key_velocity);
							active_keys[hovered_key] = 2;
						}
						damage(D_HIGHLIGHT);
					}
					break;
					// set note and velocity for 'B' shortcut
					// see Double_Window handler
				case FL_MIDDLE_MOUSE:
					if (Fl::event_inside(key_x, key_y, key_w, h_white))
						scut_b_key = hovered_key;
					scut_b_velo = key_velocity;
					return 1;
			}
			return 1;

		case FL_DRAG:
			if (FL_LEFT_MOUSE == Fl::event_button())
			{
				if (pushed == PIANO)
				{
					// dragging on keyboard
					play_hovered_key = 1;
					calc_hovered(Fl::event_x(), Fl::event_y() - key_y);
					return 1;
				}
				// velocity setup
				else if (push_x)
				{
					int dx = Fl::event_x() - push_x;
					if (dx > 0)
					{
						if (key_velocity + dx <= 127)
							key_velocity += dx;
						else
							key_velocity = 127;
					}
					else if (dx < 0)
					{
						if (key_velocity + dx >= 0)
							key_velocity += dx;
						else
							key_velocity = 0;
					}
					push_x = Fl::event_x();
					damage(D_CASE);
				}
			}
			return 1;
		case FL_RELEASE:
			if (FL_LEFT_MOUSE == Fl::event_button() && pushed == PIANO)
			{
				midi->write_event(NOTE_OFF, hovered_key, 0);
				active_keys[hovered_key] = -1;
				hovered_key = NONE;
				previous_hovered_key = NONE;
				play_hovered_key = 0;
				pushed = NONE;
				damage(D_HIGHLIGHT);
				return 1;
			}
			return 1;
		case FL_MOUSEWHEEL:
			if (this == Fl::belowmouse())
			{
				shift_octave(Fl::event_dy());
				return 1;
			}
			return 0;
	}
	return Fl_Box::handle(ev);
}

void MiniPiano::shift_octave(int shift)
{
	octave += shift;
	if (octave < 0)
		octave = 0;
	else if (octave > 8)
		octave = 8;
	redraw();
}

void MiniPiano::calc_hovered(int x, int y)
{
	if (x <= key_x)
		hovered_key = octave * 12;
	else if (x >= key_x + key_w - w_white)
		hovered_key = 24 + octave * 12;
	else
	{
		hovered_key = 0;
		if (y < h_black) // black and white-key area
		{
			while (hovered_key < 24)
			{
				++hovered_key;
				if (taste_x0[hovered_key][0] >= x)
				{
					--hovered_key;
					if (hovered_key == 0)
						break;
					if (taste_x0[hovered_key - 1][0] + w_black >= x)
						--hovered_key;
					break;
				}
			}
		}
		else // only white-keys area (lower area of keyboard)
		{
			while (++hovered_key < 25)
			{
				if (taste_x0[hovered_key][0] >= x)
				{
					--hovered_key;
					if (hovered_key == 0)
						break;
					if (taste_x0[hovered_key][1] == 1)
						--hovered_key;
					break;
				}
			}
		}
		hovered_key += octave * 12;
	}
	if (previous_hovered_key != hovered_key)
	{
		if (play_hovered_key == 1)
		{
			midi->write_event(NOTE_ON, hovered_key, key_velocity);
			if (previous_hovered_key != NONE)
				midi->write_event(NOTE_OFF, previous_hovered_key, 0);
		}
		if (active_keys[hovered_key] < 1)
			active_keys[hovered_key] = 1;
		if (previous_hovered_key != NONE)
			if (active_keys[previous_hovered_key] < 2 || play_hovered_key == 1)
				active_keys[previous_hovered_key] = -1;
		damage(D_HIGHLIGHT);
	}
	previous_hovered_key = hovered_key;
}

// called by midi->process_not_sysex to highlight incoming midi events
// on the keyboard
void MiniPiano::activate_key(int value, int key)
{
	if (active_keys[key] == value || (active_keys[key] == 2 && value == -1))
		return;
	active_keys[key] += value;
	if (active_keys[key] > 3)
		active_keys[key] = 3;
	else if (active_keys[key] < 1)
		active_keys[key] = -1;
	if (key > 12 * octave + 24 || key < 12 * octave)
		return;
	damage(D_HIGHLIGHT);
}

void MiniPiano::reset_active_keys()
{
	for (unsigned char i = 0; i < 128; i++)
		if (active_keys[i] > 0)
			active_keys[i] = -1;
	damage(D_HIGHLIGHT);
}

// ###################
//
// ###################
int Pitch_Slider::handle(int ev)
{
	switch (ev)
	{
		case FL_PUSH:
			if (FL_RIGHT_MOUSE == Fl::event_button())
				hold = true;
			else
				hold = false;
			break;
		case FL_RELEASE:
			if (!hold)
			{
				value(8192.);
				do_callback();
				return 1;
			}
			break;
	}
	return Fl_Slider::handle(ev);
}

// ###################
//
// ###################
void Step_Type::set_step(int step)
{
	s = step;
}

int Step_Type::handle(int ev)
{
	switch (ev)
	{
		case FL_PUSH:
			if (FL_RIGHT_MOUSE == Fl::event_button())
				return 1;
			break;
		case FL_RELEASE:
			if (FL_RIGHT_MOUSE == Fl::event_button() && pxk->arp)
			{
				pxk->arp->reset_step(s);
				return 1;
			}
			break;
	}
	return Fl_Group::handle(ev);
}

// ###################
//
// ###################
void Step_Value::set_id(int i, int step)
{
	id = i;
	s = step;
}

int Step_Value::handle(int ev)
{
	switch (ev)
	{
		case FL_PUSH:
			if (FL_RIGHT_MOUSE == Fl::event_button())
				return 1;
			break;
		case FL_RELEASE:
			if (FL_RIGHT_MOUSE == Fl::event_button() && pxk->arp)
			{
				value((double) pxk->arp->get_value(id, s));
				do_callback();
				return 1;
			}
			break;
		case FL_MOUSEWHEEL:
			if (this == Fl::belowmouse())
			{
				int dy = Fl::event_dy();
				double v1 = value();
				double v2 = v1 - dy;
				if (v2 < minimum())
					v2 = maximum();
				else if (v2 > maximum())
					v2 = minimum();
				value(v2);
				do_callback();
				return 1;
			}
			break;
	}
	return Fl_Value_Output::handle(ev);
}

int Step_Value::format(char *buf)
{
	if (id == 786)
		return snprintf(buf, 6, "%s", rates[25 - (int) value()]);
	return sprintf(buf, "%d", (int) value());
}

// ###################
//
// ###################
void Step_Offset::set_step(int step)
{
	s = step;
}

void Step_Offset::set_root(char r)
{
	root = r;
	redraw();
}

int Step_Offset::handle(int ev)
{
	switch (ev)
	{
		case FL_ENTER: // 1 = receive FL_LEAVE and FL_MOVE events (widget becomes Fl::belowmouse())
			if (active_r())
			{
				take_focus();
				redraw();
				return 1;
			}
			return 0;
		case FL_FOCUS: // 1 = receive FL_KEYDOWN, FL_KEYUP, and FL_UNFOCUS events (widget becomes Fl::focus())
			return 1;
		case FL_UNFOCUS: // received when another widget gets the focus and we had the focus
			redraw();
			return 1;
		case FL_PUSH:
			if (FL_RIGHT_MOUSE == Fl::event_button())
				return 1;
			take_focus();
			break;
		case FL_RELEASE:
			if (FL_RIGHT_MOUSE == Fl::event_button() && pxk->arp)
			{
				int val = pxk->arp->get_value(784, s);
				if (val > -49)
					value((double) val);
				else
					value(0.);
				do_callback();
				return 1;
			}
			break;
		case FL_MOUSEWHEEL:
			if (this == Fl::belowmouse())
			{
				int dy = Fl::event_dy();
				double v1 = value();
				double v2 = clamp(increment(v1, dy));
				if (v1 == v2)
					return 1;
				value(v2);
				do_callback();
				return 1;
			}
			break;
		case FL_DRAG:
			if (FL_BUTTON3 & Fl::event_state())
				return 1;
	}
	int sxx = x(), syy = y(), sww = w(), shh = h();
	syy += 18;
	shh -= 18;
	return Fl_Slider::handle(ev, sxx + Fl::box_dx(box()), syy + Fl::box_dy(box()), sww - Fl::box_dw(box()),
			shh - Fl::box_dh(box()));
}

void Step_Offset::draw(int X, int Y, int W, int H)
{
	double val;
	if (minimum() == maximum())
		val = 0.5;
	else
	{
		val = (value() - minimum()) / (maximum() - minimum());
		if (val > 1.0)
			val = 1.0;
		else if (val < 0.0)
			val = 0.0;
	}
	int ww = H;
	int xx, S;
	S = 17;
	xx = int(val * (ww - S) + .5);
	int xsl, ysl, wsl, hsl;
	ysl = Y + xx;
	hsl = S;
	xsl = X;
	wsl = W;
	fl_push_clip(X, Y, W, H);
	if (Fl::focus() == this)
		draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_SELECTION_COLOR);
	else
		draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_BACKGROUND2_COLOR);
	fl_pop_clip();
	if (wsl > 0 && hsl > 0)
		draw_box(FL_BORDER_BOX, xsl, ysl, wsl, hsl, FL_BACKGROUND_COLOR);
	draw_label(xsl, ysl, wsl, hsl);
}

void Step_Offset::draw()
{

	int sxx = x(), syy = y(), sww = w(), shh = h();
	int bxx = x(), byy = y(), bww = w(), bhh;
	syy += 18; // height of value output
	bhh = 18;
	shh -= 18;
	draw(sxx + Fl::box_dx(box()), syy + Fl::box_dy(box()), sww - Fl::box_dw(box()), shh - Fl::box_dh(box()));
	draw_box(FL_FLAT_BOX, bxx, byy, bww, bhh, FL_BACKGROUND_COLOR); // value box
	const char* transpose_values[] =
	{ "C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B " };
	int v = (int) value();
	int p = v % 12;
	if (v < 0 && p != 0)
		p += 12;
	p += root;
	p %= 12;
	char buf[6];
	snprintf(buf, 6, "%s%+3d", transpose_values[p], v);
	fl_font(textfont(), textsize());
	fl_color(active_r() ? textcolor() : fl_inactive(textcolor()));
	fl_draw(buf, bxx, byy, bww, bhh, FL_ALIGN_CLIP);
}

// Text_Display
void Text_Display::resize(int X, int Y, int W, int H)
{
	if (w() != W)
		wrap_mode(1, W / c_w - 4);
	Fl_Text_Display::resize(X, Y, W, H);
}
