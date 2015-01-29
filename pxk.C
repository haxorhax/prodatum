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

#include <string.h>
#include <fstream>
#include <FL/fl_ask.H>
#include <FL/Fl_Tooltip.H>
#include "pxk.H"

extern const char* VERSION;
extern PD_UI* ui;
extern PXK* pxk;

// buffer spaces
#ifdef SYNCLOG
extern unsigned int write_space;
extern unsigned int read_space;
extern unsigned int max_write;
extern unsigned int max_read;
#endif

extern bool got_answer;
extern FilterMap FM[51];
extern MIDI* midi;
Cfg* cfg = 0;

volatile bool join_bro = false;
volatile static bool name_set_incomplete;
volatile static int init_progress;

void PXK::widget_callback(int id, int value, int layer)
{
	//pmesg("PXK::widget_callback(%d, %d, %d) \n", id, value, layer);
	if (!synchronized)
		return;
	// browser returns -1 when user clicked on empty space
	if (value == -1 && (id == 278 || id == 643 || id == 897 || id == 928 || id == 1027 || id == 1409))
	{
		int current = 0;
		// reselect the previous selection for them
		switch (id)
		{
			case (278): // master riff
			case (643): // master arp
				current = setup->get_value(id);
				break;
			case 897: // preset
				current = setup->get_value(130, selected_channel);
				break;
			case 1409: // intrument
				current = preset->get_value(id, layer);
				break;
			default: // arp/riff
				current = preset->get_value(id);
		}
		if (layer == -2)
			pwid[id][0]->set_value(current);
		else
			pwid[id][layer]->set_value(current);
		return;
	}
	// channel changed
	if (id == 129)
	{
		if (ui->b_copy_p->value())
		{
			ui->b_copy_p->value(0);
			ui->b_copy_p->do_callback();
		}
		else if (ui->b_save_p->value())
		{
			ui->b_save_p->value(0);
			ui->b_save_p->do_callback();
		}
		selected_channel = value;
		// select multimode channel (to make pan and channel related controls work)
		midi->edit_parameter_value(id, selected_channel);
		// select basic channel (to get the edit buffer of the channels preset below)
		midi->edit_parameter_value(139, selected_channel);
		// update midi input filter
		midi->set_channel_filter(selected_channel);
		// request preset data
		selected_preset_rom = setup->get_value(138, selected_channel);
		selected_preset = setup->get_value(130, selected_channel);
		ui->preset_rom->set_value(selected_preset_rom);
		ui->preset->set_value(selected_preset);
		if (cfg->get_cfg_option(CFG_CLOSED_LOOP_DOWNLOAD))
			midi->request_preset_dump();
		else
			midi->request_preset_dump(50 + cfg->get_cfg_option(CFG_SPEED));
		// update channel controls (pan etc)
		if (midi_mode == MULTI)
			pwid[135][0]->set_value(setup->get_value(135, selected_channel)); // MIDI enable
		else
			pwid[140][0]->set_value(selected_channel); // FX channel
		pwid[131][0]->set_value(setup->get_value(131, selected_channel));
		pwid[132][0]->set_value(setup->get_value(132, selected_channel));
		pwid[133][0]->set_value(setup->get_value(133, selected_channel));
		pwid[134][0]->set_value(setup->get_value(134, selected_channel));
		pwid[137][0]->set_value(setup->get_value(137, selected_channel));
		ui->global_minipiano->reset_active_keys();
		ui->main->minipiano->reset_active_keys();
		ui->piano->reset_active_keys();
		ui->arp_mp->reset_active_keys();
		return;
	}
	if (id == 897) // preset_select (this is effectively id 130 (MULTIMODE_PRESET))
	{
		// do nothing if the preset didn't change
		if (setup->set_value(130, value, selected_channel))
		{
			selected_preset = value;
			pmesg("callback value: %d\n", value);
			midi->write_event(0xb0, 0, selected_preset_rom, selected_channel);
			midi->write_event(0xb0, 32, selected_preset / 128, selected_channel);
			midi->write_event(0xc0, selected_preset % 128, 0, selected_channel);
			if (cfg->get_cfg_option(CFG_CLOSED_LOOP_DOWNLOAD))
				midi->request_preset_dump();
			else
				midi->request_preset_dump(50 + cfg->get_cfg_option(CFG_SPEED));
		}
		return;
	}
	// select editing layer
	if (layer != selected_layer && layer != -2 && !ui->eall)
	{
		selected_layer = layer;
		midi->edit_parameter_value(898, selected_layer);
	}
	// volume, honor mute state ")
	if (id == 1410 && mute_volume[selected_layer] != -100)
	{
		mute_volume[selected_layer] = value;
		return;
	}
	// update changes locally
	int ret = 0;
	if (id > 898)
	{
		if (ui->eall)
		{
			ret |= preset->set_value(id, value, 0);
			ret |= preset->set_value(id, value, 1);
			ret |= preset->set_value(id, value, 2);
			ret |= preset->set_value(id, value, 3);
		}
		else
			ret = preset->set_value(id, value, selected_layer);
	}
	else
		ret = setup->set_value(id, value, selected_channel);
	// update changes remotely but only if we really changed something
	if (ret == 1)
		midi->edit_parameter_value(id, value);
	else
		return;

	// id's which have deps and need further updates below
	switch (id)
	{
		case 135: // channel enable
			if (value == 0)
			{
				ui->main->minipiano->reset_active_keys();
				ui->global_minipiano->reset_active_keys();
				ui->piano->reset_active_keys();
			}
			return;
		case 138: // preset rom select
			selected_preset_rom = value;
			midi->write_event(0xb0, 0, selected_preset_rom, selected_channel);
			midi->write_event(0xb0, 32, selected_preset / 128, selected_channel);
			midi->write_event(0xc0, selected_preset % 128, 0, selected_channel);
			// wait a bit to let it sink...
			midi->request_preset_dump(50 + cfg->get_cfg_option(CFG_SPEED));
			return;
		case 140: // FX channel
			selected_fx_channel = value;
			if (midi_mode != MULTI || selected_fx_channel != -1)
				preset->show_fx();
			else
				setup->show_fx();
			return;
		case 385: // MIDI Mode
		{
			ui->piano->reset_active_keys();
			ui->global_minipiano->reset_active_keys();
			ui->main->minipiano->reset_active_keys();
			int prev_mode = midi_mode;
			midi_mode = value;
			if (midi_mode != MULTI)
			{
				if (prev_mode == MULTI)
				{
					pwid[140][0]->set_value(selected_channel);
					preset->show_fx();
				}
			}
			else // multimode
			{
				pwid[140][0]->set_value(selected_fx_channel);
				if (selected_fx_channel != -1)
					preset->show_fx();
				else
					setup->show_fx();
			}
			ui->main->mix_out->get_value();
			return;
		}
		case 1537: // Filter type
			for (char i = 0; i <= 50; i++)
				if (FM[i].id == value)
				{
					Fl_Tooltip::exit((Fl_Widget*) pwid[id][layer]);
					ui->main->layer_strip[layer]->filter->tooltip(FM[i].info);
					break;
				}
			return;
		case 513: // fx types
		case 520:
		case 1153:
		case 1160:
			update_fx_values(id, value);
			return;
	}
	// controller mapping changed
	if ((id > 390 && id < 402) || (id > 405 && id < 410))
		update_control_map();
	// update cc controler values when we change initial amounts
	else if ((id > 914 && id < 928) && id != 923)
		((Fl_Slider*) ui->main->ctrl_x[(id > 922) ? id - 915 : id - 914])->value((double) value);
}

void PXK::cc_callback(int controller, int value)
{
	//pmesg("PXK::cc_callback(%d, %d) \n", controller, value);
	midi->write_event(0xb0, ctrl_to_cc[controller], value, selected_channel);
	if (!cc_changed && controller < 13)
	{
		ui->main->b_store->activate();
		cc_changed = true;
	}
}

// to open another device:
// delete PXK, new PXK :)
PXK::PXK()
{
	pmesg("PXK::PXK()\n");
	// initialize
	device_id = -1;
	inquired = false;
	device_code = -1;
	synchronized = false;
	roms = 0;
	preset = 0;
	preset_copy = 0;
	setup = 0;
	setup_copy = 0;
	setup_init = 0;
	selected_multisetup = -1;
	selected_fx_channel = -1;
	midi_mode = -1;
	setup_names = 0;
	setup_names_changed = false;
	arp = 0;
	nak_count = 0;
	cc_changed = false;
	randomizing = false;
	for (unsigned char i = 0; i < 4; i++)
	{
		mute_volume[i] = -100;
		is_solo[i] = 0;
	}
	for (unsigned char i = 0; i < 5; i++)
	{
		rom[i] = 0;
		rom_index[i] = -1;
	}
}

void PXK::Boot(bool autoconnect, int __id)
{
	if (!cfg)
	{
		cfg = new Cfg(__id);
		cfg->apply();
	}
	else if (__id != cfg->get_cfg_option(CFG_DEVICE_ID))
	{
		delete cfg;
		cfg = new Cfg(__id);
		cfg->apply();
	}
	if (cfg->get_cfg_option(CFG_MIDI_IN) != -1 && cfg->get_cfg_option(CFG_MIDI_OUT) != -1)
	{
		if (autoconnect)
		{
			ConnectPorts();
			Inquire(cfg->get_cfg_option(CFG_DEVICE_ID));
		}
		else
			ui->open_device->showup();
	}
	else if (midi->in() && midi->out())
		Inquire(cfg->get_cfg_option(CFG_DEVICE_ID));
	else
		ui->open_device->showup();
}

PXK::~PXK()
{
	pmesg("PXK::~PXK()\n");
	save_setup_names();
	// unmute eventually muted voices
	mute(0, 0);
	mute(0, 1);
	mute(0, 2);
	mute(0, 3);
	ui->device_info->label(0);
	// clear browsers and rom choices
	for (unsigned char i = 0; i < 4; i++)
	{
		ui->layer_editor[i]->instrument->reset();
		ui->layer_editor[i]->instrument_rom->menu(0);
		ui->layer_editor[i]->patchcords->reset_sources();
		ui->main->layer_strip[i]->instrument->label(0);
	}
	ui->preset_editor->patchcords->reset_sources();
	ui->preset_editor->patchcords->reset_destinations();
	ui->preset->reset();
	ui->preset_rom->menu(0);
	ui->preset_editor->riff->reset();
	ui->preset_editor->riff_rom->menu(0);
	ui->preset_editor->arp->reset();
	ui->preset_editor->arp_rom->menu(0);
	ui->preset_editor->l1->reset();
	ui->preset_editor->l1_rom->menu(0);
	ui->preset_editor->l2->reset();
	ui->preset_editor->l2_rom->menu(0);
	ui->main->riff->reset();
	ui->main->riff_rom->menu(0);
	ui->main->arp->reset();
	ui->main->arp_rom->menu(0);
	ui->multisetups->clear();
	ui->copy_browser->reset();
	ui->copy_arp_rom->menu(0);
	ui->copy_arp_pattern_browser->reset();
	for (unsigned char i = 0; i <= roms; i++)
		if (rom[i])
		{
			delete rom[i];
			rom[i] = 0;
		}
	if (preset)
		delete preset;
	if (preset_copy)
		delete preset_copy;
	if (setup)
		delete setup;
	if (setup_copy)
		delete setup_copy;
	if (setup_init)
	{
		setup_init->upload();
		delete setup_init;
	}
	if (setup_names)
		delete[] setup_names;
	if (cfg)
		delete cfg;
	preset = 0;
	preset_copy = 0;
	setup = 0;
	setup_copy = 0;
	setup_init = 0;
	setup_names = 0;
	cfg = 0;
}

void PXK::ConnectPorts()
{
	pmesg("PXK::ConnectPorts()\n");
	if (midi->connect_out(cfg->get_cfg_option(CFG_MIDI_OUT)) && midi->connect_in(cfg->get_cfg_option(CFG_MIDI_IN)))
	{
		ui->midi_outs->label(ui->midi_outs->text(cfg->get_cfg_option(CFG_MIDI_OUT)));
		ui->midi_outs->value(cfg->get_cfg_option(CFG_MIDI_OUT));
		ui->midi_ins->label(ui->midi_ins->text(cfg->get_cfg_option(CFG_MIDI_IN)));
		ui->midi_ins->value(cfg->get_cfg_option(CFG_MIDI_IN));
		if (midi->connect_thru(cfg->get_cfg_option(CFG_MIDI_THRU)))
		{
			ui->midi_ctrl->label(ui->midi_ctrl->text(cfg->get_cfg_option(CFG_MIDI_THRU)));
			ui->midi_ctrl->value(cfg->get_cfg_option(CFG_MIDI_THRU));
		}
	}
	else
		ui->open_device->showup();
}

static void sync_bro(void* p)
{
	if (join_bro)
		goto Club;
	// general variables
	static bool timed_out = false;
	static int name = 0;
	static char _label[64];
	static unsigned char countdown;
#ifdef SYNCLOG
	char logbuffer[128];
#endif
	// preset name variables
	static unsigned char rom_nr = 0;
	static unsigned char type = PRESET;
	static int names = 0;
	// setup name variables
	static char setups_to_load = -1;
	static bool requested = false;
	if (setups_to_load == -1)
		setups_to_load = pxk->load_setup_names(99);
	if (setups_to_load)
	{
		if (pxk->setup_init == 0)
		{
			if (!requested)
			{
				ui->init_progress->label("Syncing multisetup names...");
				ui->init_progress->maximum((float) setups_to_load);
				ui->init_progress->value(.0);
				init_progress = 0;
				ui->init->position(ui->main_window->x() + (ui->main_window->w() / 2) - (ui->init->w() / 2),
						ui->main_window->y() + 80);
				ui->init->show();
#ifdef SYNCLOG
				ui->init_log->append("sync_bro: Requesting initial setup dump\n");
#endif
				midi->request_setup_dump();
				requested = true;

				countdown = 50; // ~ 1/2 s.
				goto Exit;
			}
#ifdef SYNCLOG
			ui->init_log->append("*");
#endif
			if (--countdown == 0) // timeout
			{
#ifdef SYNCLOG
				ui->init_log->append("\nsync_bro: request for initial setup timed out. Giving up.\n");
#endif
				timed_out = true;
				goto Club;
			}
			goto Exit;
		} // got init setup
		if (name == 0)
		{
#ifdef SYNCLOG
			ui->init_log->append("# OK\n\nsync_bro: Loading setup names\n");
#endif
			requested = false;
		}
		if (name <= setups_to_load)
		{
			if (!requested && name < setups_to_load)
			{
				pxk->load_setup_names(name++);
				requested = true;
				got_answer = false;
				countdown = 50;
				goto Exit;
			}
			if (!got_answer)
			{
#ifdef SYNCLOG
				ui->init_log->append("*");
#endif
				if (--countdown == 0) // timeout
				{
#ifdef SYNCLOG
					ui->init_log->append("\nsync_bro: timeout syncing setup name. Giving up.\n");
#endif
					timed_out = true;
					goto Club;
				}
				goto Exit;
			}
			else
			{
#ifdef SYNCLOG
				ui->init_log->append("#");
#endif
				if (name == setups_to_load)
				{
#ifdef SYNCLOG
					ui->init_log->append(" OK\n");
#endif
					pxk->load_setup_names(setups_to_load);
					setups_to_load = 0;
				}
				ui->init_progress->value((float) init_progress);
				requested = false;
				goto Exit;
			}
		}
	}
	// setup initialization complete
	if (rom_nr <= pxk->roms)
	{
		if (type <= RIFF) // for every type
		{
			if (type == SETUP || type == DEMO || (rom_nr == 0 && (type == INSTRUMENT || type == RIFF)))
			{
				names = 0;
				type++;
				goto Exit;
			}
			if (type == RIFF && pxk->member_code == 2) // audity has no riffs
			{
				names = 0;
				type = PRESET; // next rom
				rom_nr++;
				goto Exit;
			}
			if (names == 0)
			{
				name = 0;
				names = pxk->rom[rom_nr]->disk_load_names(type);
				if (names != -1)  // not available on disk
				{
					name_set_incomplete = true;
					if (names == 0) // unknown number of names available
					{
						if (type == ARP)
							names = MAX_ARPS; // max requests
						else if (type == RIFF)
							names = MAX_RIFFS;
					}
					const char* _type = 0;
					switch (type)
					{
						case PRESET:
							_type = "preset";
							break;
						case INSTRUMENT:
							_type = "instrument";
							break;
						case ARP:
							_type = "arp (estimated progress)";
							break;
						case RIFF:
							_type = "riff (estimated progress)";
							break;
					}
					if (rom_nr == 0)
						snprintf(_label, 64, "Syncing flash %s names...", _type);
					else
						snprintf(_label, 64, "Syncing %s %s names...", pxk->rom[rom_nr]->name(), _type);
					ui->init_progress->label(_label);
					ui->init_progress->maximum((float) names);
					ui->init_progress->value(.0);
					init_progress = 0;
					if (!ui->init->shown())
					{
						ui->init->position(ui->main_window->x() + (ui->main_window->w() / 2) - (ui->init->w() / 2),
								ui->main_window->y() + 80);
						ui->init->show();
					}
#ifdef SYNCLOG
					// init log
					snprintf(logbuffer, 128, "\nsync_bro: Loading %s\n", _label);
					ui->init_log->append(logbuffer);
#endif
				}
			} // if (names == 0)
			if (name_set_incomplete && name <= names)
			{
				if (!requested && name < names)
				{
					if (type == ARP && rom_nr != 0)
						pxk->rom[rom_nr]->load_name(type, name++);
					else // turbo
					{
						if (name == 0)
							got_answer = true;
						if (!got_answer)
						{
							name_set_incomplete = false;
							goto Exit;
						}
						ui->init_progress->value((float) init_progress);
						while (name < names)
						{
							pxk->rom[rom_nr]->load_name(type, name++);
							if (name % 12 == 0)
							{
#ifdef SYNCLOG
								ui->init_log->append("R12");
#endif
								Fl::repeat_timeout(.12 + (double) (cfg->get_cfg_option(CFG_SPEED) / 1000.), sync_bro, p);
								got_answer = false;
								return;
							}
						}
					}
					requested = true;
					got_answer = false;
					countdown = 50;
					goto Exit;
				}
				if (!got_answer)
				{
#ifdef SYNCLOG
					ui->init_log->append("*");
#endif
					if (--countdown == 0) // timeout
					{
						if (type != ARP && type != RIFF) // ok for ARPs and RIFFs
						{
#ifdef SYNCLOG
							ui->init_log->append("\nsync_bro: timed out syncing ROM name. Giving up.\n");
#endif
							timed_out = true;
							goto Club;
						}
#ifdef SYNCLOG
						ui->init_log->append("\nsync_bro: timed out syncing RIFF/ARP name. Skipping.\n");
#endif
						name_set_incomplete = false; // stop requesting arp/riff names
					}
					goto Exit;
				}
				else
				{
#ifdef SYNCLOG
					ui->init_log->append("#");
#endif
					if (name == names)
					{
#ifdef SYNCLOG
						ui->init_log->append(" OK\n");
#endif
						name_set_incomplete = false;
					}
					ui->init_progress->value((float) init_progress);
					requested = false;
					goto Exit;
				}
			} // if (name_set_incomplete)
			else
			{
				if (name != 0)
					pxk->rom[rom_nr]->save(type);
				type++;
				names = 0; // next type
				if (name != 0)
					goto Wait;
				goto Exit;
			}
		} // if (type <= RIFF) // for every type
		else
		{
			names = 0;
			type = PRESET; // next rom
			rom_nr++;
			goto Exit;
		}
	} // if (rom_nr < pxk->roms)
	else
		*(bool*) p = true;

	Exit: if (!*(bool*) p)
		Fl::repeat_timeout(.01, sync_bro, p);
	else
	{
		Club: if (pxk->setup_init)
		{
			pxk->setup_init->upload();
			if (join_bro)
			{
				delete pxk->setup_init;
				pxk->setup_init = 0;
				goto Wait;
				// wait for upload
			}
		}
#ifdef SYNCLOG
		snprintf(logbuffer, 128,
				"\nmin. read buffer space: %u (max. pkt len %u)\nmin. write buffer space: %u (max. pkt len %u)\n", read_space,
				max_read, write_space, max_write);
		ui->init_log->append(logbuffer);
		read_space = RINGBUFFER_READ;
		write_space = RINGBUFFER_WRITE;
		max_read = 0;
		max_write = 0;
#endif
		// reset static variables
		name = 0;
		countdown = 33;
		rom_nr = 0;
		type = PRESET;
		names = 0;
		setups_to_load = -1;
		requested = false;
		ui->init->hide();
		ui->main_window->showup(); // make main active (important!)
		if (timed_out)
		{
			timed_out = false;
			fl_alert("Sync failed. Please send the init log to rdxesy@@yahoo.de and check your cables & MIDI drivers etc.");
#ifdef SYNCLOG
			ui->init_log_w->showup();
#endif
		}
		else if (join_bro)
		{
			int id = cfg->get_cfg_option(CFG_DEVICE_ID);
			delete pxk;
			pxk = new PXK();
			pxk->Boot(false, id);
			ui->open_device->showup();
			pxk->Inquire(id);
		}
		else
		{
			midi->filter_loose();
			if (pxk->setup_init)
				pxk->load_setup();
			else
				midi->request_setup_dump();
			midi->master_volume(cfg->get_cfg_option(CFG_MASTER_VOLUME));
		}
	}
	return;
	Wait: Fl::repeat_timeout(.5 + (double) (cfg->get_cfg_option(CFG_SPEED) / 1000.), sync_bro, p);
}

void PXK::Join()
{
	if (!join_bro)
	{
		ui->device_info->label(0);
		join_bro = true;
	}
}

bool PXK::Synchronize()
{
	if (synchronized)
		return false;
	pmesg("PXK::Synchronize()\n");
	display_status("Synchronizing...");
#ifdef SYNCLOG
	char buf[64];
	snprintf(buf, 64, "PXK::Synchronize() %d[ms]\n\n", cfg->get_cfg_option(CFG_SPEED));
	ui->init_log->append(buf);
#endif
	midi->filter_strict(); // filter everything but sysex for sync
	init_progress = 0;
	name_set_incomplete = false;
	Fl::add_timeout(0, sync_bro, (void*) &synchronized);
	return true;
}

bool PXK::Synchronized() const
{
	return synchronized;
}

void PXK::log_add(const unsigned char* sysex, const unsigned int len, unsigned char io) const
{
	//pmesg("PXK::log_add(sysex, %d, %d)\n", len, io);
	bool log = false;
	char* buf = 0;
	if ((io == 1 && cfg->get_cfg_option(CFG_LOG_SYSEX_IN)) || (io == 0 && cfg->get_cfg_option(CFG_LOG_SYSEX_OUT)))
	{
		log = true;
		buf = new char[2 * len + 18];
	}
	unsigned char n = 0;
	if (log)
	{
		if (io == 1)
		{
			static unsigned int count_i = 0;
			n = snprintf(buf, 16, "\nI.%u::", ++count_i);
		}
		else
		{
			static unsigned int count_o = 0;
			n = snprintf(buf, 16, "\nO.%u::", ++count_o);
		}
	}
	for (unsigned int i = 0; i < len; i++)
	{
		if (log)
			sprintf(n + buf + 2 * i, "%02hhX", sysex[i]);
		if (io == 1)
			ui->scope_i->Add(sysex[i]);
		else
			ui->scope_o->Add(sysex[i]);
	}
	if (buf)
	{
		ui->logbuf->append(buf);
		delete[] buf;
	}
}

/* if autoconnection is enabled but the device is not powered
 we end up with an unconnected main window. this shows
 the open dialog if there is no answer*/
static void check_connection(void* p)
{
	if (*(char*) p == -1)
	{
		ui->open_device->showup();
		pxk->inquired = false;
	}
}

void PXK::Inquire(int id)
{
	pmesg("PXK::Inquire(%d)\n", id);
	if (!synchronized)
	{
		join_bro = false;
		if (id == -1)
			device_id = 0x7f;
		else
			device_id = id;
		unsigned char sid = id & 0xff;
		unsigned char s[] =
		{ 0xf0, 0x7e, sid, 0x06, 0x01, 0xf7 };
		midi->write_sysex(s, 6);
		midi->write_sysex(s, 6);
		inquired = true;
		device_code = -1;
		Fl::add_timeout(.6, check_connection, (void*) &device_code);
	}
}

void request_hardware_config_timeout(void*)
{
	midi->request_hardware_config();
}

void PXK::incoming_inquiry_data(const unsigned char* data)
{
	pmesg("PXK::incoming_inquiry_data(data)\n");
	if (!inquired || synchronized)
		return;
	device_code = data[7] * 128 + data[6];
	if (device_code == 516 && (data[2] == device_id || device_id == 127)) // talking to our PXK!
	{
		inquired = false;
		if (device_id == 127)
		{
			device_id = data[2];
			ui->device_id->value((double) data[2]);
			// load user config if we scanned using ID 127
			delete cfg;
			cfg = new Cfg(device_id);
			cfg->apply();
		}
		midi->set_device_id((unsigned char) device_id); // so further sysex comes through
		Fl::add_timeout(.1, request_hardware_config_timeout);
		snprintf(os_rev, 5, "%c%c%c%c", data[10], data[11], data[12], data[13]);
		member_code = data[9] * 128 + data[8];
		switch (member_code)
		{
			case 2: // AUDITY
				ui->main->b_audit->deactivate();
				ui->m_audit->hide();
				ui->main->g_riff->deactivate();
				ui->main->g_superbeats->deactivate();
				ui->preset_editor->g_riff->deactivate();
				ui->main->post_d->hide();
				ui->main->pre_d->label("Delay");
				ui->preset_editor->post_d->hide();
				ui->preset_editor->pre_d->label("Delay");
				break;
			default:
				ui->main->b_audit->activate();
				ui->m_audit->show();
				ui->main->g_riff->activate();
				ui->main->g_superbeats->activate();
				ui->preset_editor->g_riff->activate();
				ui->main->post_d->show();
				ui->main->pre_d->label("Pre D");
				ui->preset_editor->post_d->show();
				ui->preset_editor->pre_d->label("Pre D");
		}
	}
	else
		device_code = -1;
}

unsigned char PXK::get_rom_index(char id) const
{
	if (id == 0)
		return 0;
	if (rom_index[1] == id)
		return 1;
	if (rom_index[2] == id)
		return 2;
	if (rom_index[3] == id)
		return 3;
	if (rom_index[4] == id)
		return 4;
	return 5;
}

void PXK::incoming_hardware_config(const unsigned char* data)
{
	pmesg("PXK::incoming_hardware_config(data)\n");
	user_presets = data[8] * 128 + data[7];
	roms = data[9];
	rom_index[0] = 0;
	rom[0] = new ROM(0, user_presets);
	for (unsigned char j = 1; j <= roms; j++)
	{
		int idx = 11 + (j - 1) * 6;
		rom[j] = new ROM(data[idx + 1] * 128 + data[idx], data[idx + 3] * 128 + data[idx + 2],
				data[idx + 5] * 128 + data[idx + 4]);
		rom_index[j] = data[idx + 1] * 128 + data[idx];
	}
	create_device_info();
	if (roms != 0)
	{
		if (!ui->open_device->shown_called())
			Synchronize();
		else
			ui->connect->activate();
	}
}

unsigned char PXK::load_setup_names(unsigned char start)
{
	//pmesg("PXK::load_setup_names(%d)\n", start);
	char available_setups = 64;
	if (member_code == 2)
		available_setups = 16;
	// try to load from disk
	if (start == 99)
	{
		if (setup_names)
		{
			delete[] setup_names;
			setup_names = 0;
		}
		char filename[PATH_MAX];
		snprintf(filename, PATH_MAX, "%s/n_set_0_%d", cfg->get_config_dir(), cfg->get_cfg_option(CFG_DEVICE_ID));
		std::fstream file(filename, std::ios::in | std::ios::binary | std::ios::ate);
		if (file.is_open())
		{
			int size = file.tellg();
			if (size != available_setups * 16)
			{
				file.close();
				return available_setups - 1;
			}
			setup_names = new unsigned char[available_setups * 16];
			file.seekg(0, std::ios::beg);
			file.read((char*) setup_names, available_setups * 16);
			file.close();
			ui->multisetups->clear();
			char buf[21];
			for (char i = 0; i < available_setups; i++)
			{
				snprintf(buf, 21, "%02d: %s", i, setup_names + i * 16);
				ui->multisetups->add(buf);
			}
			return 0;
		}
		return available_setups - 1;
	}
	// midi load setup names
	if (start < available_setups - 1)
	{
		midi->copy(C_SETUP, start, -1);
		midi->request_name(SETUP, start, 0);
	}
	else // last setup name
	{
		set_setup_name(available_setups - 1, (unsigned char*) "Factory Setup   ");
		save_setup_names(true);
		ui->multisetups->clear();
		char buf[21];
		for (char i = 0; i < available_setups; i++)
		{
			snprintf(buf, 21, "%02d: %s", i, setup_names + i * 16);
			ui->multisetups->add(buf);
		}
	}
	return 1;
}

void PXK::incoming_preset_dump(const unsigned char* data, int len)
{
	pmesg("PXK::incoming_preset_dump(data, %d) \n", len);
	//static int packets;
	static unsigned char dump[1615];
	static int dump_pos = 0;
	static int packet_size = 0;
	static bool closed_loop;
	static int number;
	if (dump_pos + len > 1615)
	{
		fl_message("Preset dump larger than expected!\nPlease report!");
		return;
	}
	// preset dump header
	if (data[6] == 0x01 && dump_pos == 0) // closed loop (requires ACKs)
	{
		//pmesg("PXK::incoming_preset_dump(len: %d) header (closed)\n", len);
		midi->ack(0);
		closed_loop = true;
		dump_pos = len;
		number = data[7] + 128 * data[8];
		if (number >= 8192)
			number -= 16384;
		memcpy(dump, data, len);
	}
	else if (data[6] == 0x03 && dump_pos == 0) // open loop
	{
		//pmesg("PXK::incoming_preset_dump(len: %d) header (open)\n", len);
		closed_loop = false;
		dump_pos = len;
		number = data[7] + 128 * data[8];
		if (number >= 8192)
			number -= 16384;
		memcpy(dump, data, len);
	}
	// preset dump data message
	else
	{
		// didn't receive a header
		if (!dump_pos)
			return;
		if (closed_loop)
		{
			//pmesg("PXK::incoming_preset_dump(len: %d) data (closed)\n", len);
			// calculate checksum
			int sum = 0;
			for (int i = 9; i < len - 2; i++)
				sum += data[i];
			unsigned char checksum = ~sum;
			// compare checksums
			if (checksum % 128 != data[len - 2])
				midi->nak(data[8] * 128 + data[7]);
			else
			{
				midi->ack(data[8] * 128 + data[7]);
				memcpy(dump + dump_pos, data, len);
				dump_pos += len;
				if (len < 253) // last packet
				{
					delete preset;
					if (randomizing)
					{
						preset = new Preset_Dump(dump_pos, dump, packet_size);
						preset->set_changed(true);
						randomizing = false;
					}
					else
						preset = new Preset_Dump(dump_pos, dump, packet_size, true);
					// store a copy
					delete preset_copy;
					preset_copy = new Preset_Dump(dump_pos, dump, packet_size);
					dump_pos = 0;
				}
				else
					packet_size = len;
			}
		}
		else
		{
			//pmesg("PXK::incoming_preset_dump(len: %d) data (open)\n", len);
			memcpy(dump + dump_pos, data, len);
			dump_pos += len;
			if (len < 253) // last packet
			{
				delete preset;
				if (randomizing)
				{
					preset = new Preset_Dump(dump_pos, dump, packet_size);
					preset->set_changed(true);
					randomizing = false;
				}
				else
					preset = new Preset_Dump(dump_pos, dump, packet_size, true);
				// store a copy
				delete preset_copy;
				preset_copy = new Preset_Dump(dump_pos, dump, packet_size);
				dump_pos = 0;
			}
			else
				packet_size = len;
		}
	}
	if (dump_pos == 0)
		show_preset();
}

void PXK::incoming_setup_dump(const unsigned char* data, int len)
{
	pmesg("PXK::incoming_setup_dump(data, %d) \n", len);
	selected_multisetup = data[0x4A]; // needed to select it in the multisetup choice
	if (!synchronized)
	{
#ifdef SYNCLOG
		char* __buffer = (char*) malloc(48 * sizeof(char));
		snprintf(__buffer, 48, "\nPXK::incoming_setup_dump(len: %d)\n", len);
		ui->init_log->append(__buffer);
		free(__buffer);
#endif
		setup_init = new Setup_Dump(len, data);
		return;
	}
	if (setup)
	{
		delete setup;
		delete setup_copy;
	}
	setup = new Setup_Dump(len, data);
	setup_copy = new Setup_Dump(len, data);
	load_setup();
}

static void check_loading(void*)
{
	if (!got_answer)
	{
		ui->supergroup->clear_output();
		fl_alert("Device did not respond to our request.");
	}
}

void PXK::Loading(bool upload)
{
	pmesg("PXK::Loading() \n");
	Fl::remove_timeout(check_loading);
	display_status("Loading program...");
	ui->supergroup->set_output();
	got_answer = false;
	if (upload)
		Fl::add_timeout((2000. + cfg->get_cfg_option(CFG_SPEED)) / 1000., check_loading);
	else
	{
		if (cfg->get_cfg_option(CFG_CLOSED_LOOP_DOWNLOAD))
			Fl::add_timeout((1900. + cfg->get_cfg_option(CFG_SPEED)) / 1000., check_loading);
		else
			Fl::add_timeout((1500. + 8 * cfg->get_cfg_option(CFG_SPEED)) / 1000., check_loading);
	}
}

void load_setup_timeout(void* s)
{
	// load names into the copy browser
	ui->copy_arp_rom->set_value(0);
	ui->copy_arp_pattern_browser->load_n(ARP, 0);
	// show setup
	((Setup_Dump*) s)->show();
	// select basic channel on startup
	pwid[129][0]->set_value(((Setup_Dump*) s)->get_value(139));
	((Fl_Button*) ui->main->channel_select->child(((Setup_Dump*) s)->get_value(139)))->take_focus();
	ui->main->channel_select->do_callback();
}

void PXK::load_setup()
{
	pmesg("PXK::load_setup() \n");
	display_status("Loading multisetup...");
	if (setup_init)
	{
		setup = new Setup_Dump(setup_init->get_dump_size(), setup_init->get_data());
		setup_copy = new Setup_Dump(setup_init->get_dump_size(), setup_init->get_data());
		delete setup_init;
		setup_init = 0;
	}
	midi_mode = setup->get_value(385);
	selected_fx_channel = setup->get_value(140);
	// select first entry in the reset rom choice
	ui->r_rom_rom->value(0);
	// get realtime controller assignments
	update_control_map();
	// multisetups
	ui->multisetups->select(selected_multisetup + 1);
	ui->multisetups->activate();
	char n[17];
	snprintf(n, 17, "%s", setup_names + 16 * selected_multisetup);
	while (n[strlen(n) - 1] == ' ')
		n[strlen(n) - 1] = '\0';
	ui->s_name->value(n);
	// expensive stuff in timeout
	Fl::add_timeout(.1, load_setup_timeout, setup);
}

void PXK::incoming_generic_name(const unsigned char* data)
{
#ifdef SYNCLOG
	if (!synchronized && ((data[6] % 0xF) <= RIFF))
	{
		char* __name = (char*) malloc(17 * sizeof(char));
		snprintf(__name, 17, "%s", data + 11);
		char* __buffer = (char*) malloc(128 * sizeof(char));
		snprintf(__buffer, 128, "\nPXK::incoming_generic_name(type:%d) %d-%d \"%s\"\n", data[6] % 0xF,
				data[7] + 128 * data[8], data[9] + 128 * data[10], __name);
		ui->init_log->append(__buffer);
		free(__buffer);
		free(__name);
	}
#endif
	// might be the case when sync gets cancelled:
	// there might be some incoming messages left
	// that the OS will hand to portmidi when we connect
	// to the port
	if (rom[0] == 0)
		return;
	if (data[11] < 0x20 || data[11] > 0x7E) // garbage
	{
		name_set_incomplete = false;
		return;
	}
	unsigned char type = data[6] % 0xF;
	if (type < PRESET || type > RIFF)
	{
		pmesg("*** unknown name type %d\n", type);
		display_status("*** Received unknown name type.");
		name_set_incomplete = false;
		return;
	}
	int rom_id = data[9] + 128 * data[10];
	if (get_rom_index(rom_id) == 5)
	{
		pmesg("*** ROM %d does not exist\n", data[9] + 128 * data[10]);
		display_status("*** Received unknown name type.");
		name_set_incomplete = false;
		return;
	}
	if (type == RIFF && data[11] == 0x66 && data[12] == 0x66) // "ff"
	{
		name_set_incomplete = false;
		return;
	}
	int number = data[7] + 128 * data[8];
	//pmesg("PXK::incoming_generic_name(data) (#:%d-%d, type:%d)\n", number, data[9] + 128 * data[10], type);
	if (type == SETUP)
		set_setup_name(number, data + 11);
	else if (0 == rom[get_rom_index(rom_id)]->set_name(type, number, data + 11))
		name_set_incomplete = false;
	++init_progress;
}

void PXK::incoming_arp_dump(const unsigned char* data, int len)
{
	//pmesg("PXK::incoming_arp_dump(data, %d)\n", len);
	if (!synchronized || ui->init->shown()) // init
	{
#ifdef SYNCLOG
		char* __name = (char*) malloc(13 * sizeof(char));
		snprintf(__name, 13, "%s", data + 14);
		char* __buffer = (char*) malloc(128 * sizeof(char));
		snprintf(__buffer, 128, "\nPXK::incoming_arp_dump(len:%d) %d-%d \"%s\"\n ", len, data[6] + 128 * data[7],
				data[len - 3] + 128 * data[len - 2], __name);
		ui->init_log->append(__buffer);
		free(__buffer);
		free(__name);
#endif
		// might be the case when sync gets cancelled:
		// there might be some incoming messages left
		// that the OS will hand to portmidi when we connect
		// to the port
		if (rom[0] == 0)
			return;
		if (data[14] < 0x20 || data[14] > 0x7E) // garbage // not ascii, not a "real" arp dump
		{
			name_set_incomplete = false;
			return;
		}
		// some roms dont have arpeggios and return "(not instld)"
		if (strncmp((const char*) data + 14, "(not", 4) == 0)
		{
			name_set_incomplete = false;
			return;
		}
		int number = data[6] + 128 * data[7];
		int rom_id = data[len - 3] + 128 * data[len - 2];
		if (get_rom_index(rom_id) != 5)
		{
			rom[get_rom_index(rom_id)]->set_name(ARP, number, data + 14);
			++init_progress;
		}
	}
	else // this is a dump we like to edit ")
	{
		delete arp;
		arp = new Arp_Dump(len, data);
		display_status("Arp pattern loaded.");
	}
}

void PXK::incoming_ACK(int packet)
{
	pmesg("PXK::incoming_ACK(packet: %d) \n", packet);
//	display_status("Received ACK.");
	if (preset)
		preset->upload(++packet);
	nak_count = 0;
}

void PXK::incoming_NAK(int packet)
{
	pmesg("PD:incoming_NAK:(packet: %d) \n", packet);
//	display_status("Received NAK. Retrying...");
	if (preset && nak_count < 3)
	{
		preset->upload(packet);
		++nak_count;
	}
	else
		fl_message("Closed Loop Upload failed!");
}

//void PXK::incoming_ERROR(int cmd, int sub)
//{
//	pmesg("PXK::incoming_ERROR(cmd: %X, subcmd: %X) \n", cmd, sub);
//	display_status("Received ERROR.");
//
//}

void PXK::set_setup_name(unsigned char number, const unsigned char* name)
{
	//pmesg("PXK::set_setup_name(%d, %s) \n", number, name);
	char available_setups = 64;
	if (member_code == 2)
		available_setups = 16;
	if (!setup_names)
		setup_names = new unsigned char[available_setups * 16];
	memcpy(setup_names + 16 * number, name, 16);
	if (synchronized) // user changed setup name
		setup_names_changed = true;
}

void PXK::save_setup_names(bool force) const
{
//	pmesg("PXK::save_setup_names() \n");
	if ((synchronized && setup_names_changed) || force)
	{
		char available_setups = 64;
		if (member_code == 2)
			available_setups = 16;
		char filename[PATH_MAX];
		snprintf(filename, PATH_MAX, "%s/n_set_0_%d", cfg->get_config_dir(), device_id);
		std::fstream file(filename, std::ios::out | std::ios::binary | std::ios::trunc);
		file.write((char*) setup_names, available_setups * 16);
		file.close();
	}
}

void PXK::update_cc_sliders()
{
	//pmesg("PXK::update_cc_sliders() \n");
	if (!preset)
		return;
	ui->main->b_store->deactivate();
	cc_changed = false;
	int tmp = preset->get_value(915);
	if (tmp != -1)
		((Fl_Slider*) ui->main->ctrl_x[1])->value((double) tmp);
	tmp = preset->get_value(916);
	if (tmp != -1)
		((Fl_Slider*) ui->main->ctrl_x[2])->value((double) tmp);
	tmp = preset->get_value(917);
	if (tmp != -1)
		((Fl_Slider*) ui->main->ctrl_x[3])->value((double) tmp);
	tmp = preset->get_value(918);
	if (tmp != -1)
		((Fl_Slider*) ui->main->ctrl_x[4])->value((double) tmp);
	tmp = preset->get_value(919);
	if (tmp != -1)
		((Fl_Slider*) ui->main->ctrl_x[5])->value((double) tmp);
	tmp = preset->get_value(920);
	if (tmp != -1)
		((Fl_Slider*) ui->main->ctrl_x[6])->value((double) tmp);
	tmp = preset->get_value(921);
	if (tmp != -1)
		((Fl_Slider*) ui->main->ctrl_x[7])->value((double) tmp);
	tmp = preset->get_value(922);
	if (tmp != -1)
		((Fl_Slider*) ui->main->ctrl_x[8])->value((double) tmp);
	tmp = preset->get_value(924);
	if (tmp != -1)
		((Fl_Slider*) ui->main->ctrl_x[9])->value((double) tmp);
	tmp = preset->get_value(925);
	if (tmp != -1)
		((Fl_Slider*) ui->main->ctrl_x[10])->value((double) tmp);
	tmp = preset->get_value(926);
	if (tmp != -1)
		((Fl_Slider*) ui->main->ctrl_x[11])->value((double) tmp);
	tmp = preset->get_value(927);
	if (tmp != -1)
		((Fl_Slider*) ui->main->ctrl_x[12])->value((double) tmp);
}

void PXK::update_control_map()
{
	//pmesg("PXK::update_control_map() \n");
	// cc to controller
	cc_to_ctrl.clear();
	cc_to_ctrl[setup->get_value(391)] = 1;
	cc_to_ctrl[setup->get_value(392)] = 2;
	cc_to_ctrl[setup->get_value(393)] = 3;
	cc_to_ctrl[setup->get_value(394)] = 4;
	cc_to_ctrl[setup->get_value(395)] = 5;
	cc_to_ctrl[setup->get_value(396)] = 6;
	cc_to_ctrl[setup->get_value(397)] = 7;
	cc_to_ctrl[setup->get_value(398)] = 8;
	cc_to_ctrl[setup->get_value(406)] = 9;
	cc_to_ctrl[setup->get_value(407)] = 10;
	cc_to_ctrl[setup->get_value(408)] = 11;
	cc_to_ctrl[setup->get_value(409)] = 12;
	// fottswitches
	cc_to_ctrl[setup->get_value(399)] = 13;
	cc_to_ctrl[setup->get_value(400)] = 14;
	cc_to_ctrl[setup->get_value(401)] = 15;
	// vice versa
	ctrl_to_cc.clear();
	ctrl_to_cc[1] = setup->get_value(391);
	ctrl_to_cc[2] = setup->get_value(392);
	ctrl_to_cc[3] = setup->get_value(393);
	ctrl_to_cc[4] = setup->get_value(394);
	ctrl_to_cc[5] = setup->get_value(395);
	ctrl_to_cc[6] = setup->get_value(396);
	ctrl_to_cc[7] = setup->get_value(397);
	ctrl_to_cc[8] = setup->get_value(398);
	ctrl_to_cc[9] = setup->get_value(406);
	ctrl_to_cc[10] = setup->get_value(407);
	ctrl_to_cc[11] = setup->get_value(408);
	ctrl_to_cc[12] = setup->get_value(409);
	// footswitches
	ctrl_to_cc[13] = setup->get_value(399);
	ctrl_to_cc[14] = setup->get_value(400);
	ctrl_to_cc[15] = setup->get_value(401);
}

void show_preset_timeout(void* p)
{
	((Preset_Dump*) p)->show();
}

void PXK::show_preset()
{
	pmesg("PXK::show_preset() \n");
	ui->set_eall(0);
	selected_layer = 5;
	update_cc_sliders();
	ui->undo_b->deactivate();
	ui->redo_b->deactivate();
	// clear solo/mute buttons
	for (int i = 0; i < 4; i++)
	{
		is_solo[i] = 0;
		ui->solo_b[i]->value(0);
		ui->main->layer_strip[i]->solo_b->value(0);
		mute_volume[i] = -100;
		ui->mute_b[i]->value(0);
		ui->main->layer_strip[i]->mute_b->value(0);
	}
	Fl::add_timeout(0.1, show_preset_timeout, preset);
}

int PXK::test_checksum(const unsigned char* data, int size, int packet_size)
{
	pmesg("PXK::test_checksum(data, %d, %d) \n", size, packet_size);
	const int chunks = (size - DUMP_HEADER_SIZE) / packet_size;
	const int tail = (size - DUMP_HEADER_SIZE) % packet_size - 11;
	int offset = DUMP_HEADER_SIZE + 9;
	int sum;
	unsigned char checksum;
	int errors = 0;
	for (int j = 0; j < chunks; j++)
	{
		sum = 0;
		for (int i = 0; i < (packet_size - 11); i++)
			sum += data[offset + i];
		checksum = ~sum;
		if (data[offset + packet_size - 11] != checksum % 128)
			++errors;
		offset += packet_size;
	}
	sum = 0;
	for (int i = 0; i < tail; i++)
		sum += data[offset + i];
	checksum = ~sum;
	if (data[offset + tail] != checksum % 128)
		++errors;
	sum = 0;
	if (errors)
		sum = fl_choice("Checksum test failed! Import anyway?\n"
				"Note: This may work and cause exceptional results.", "Import", "No", 0);
	return sum;
}

void PXK::load_export(const char* filename)
{
	pmesg("PXK::load_export(%s) \n", filename);
	if (!setup)
	{
		display_status("*** Must be connected.");
		return;
	}
	if (preset->is_changed())
	{
		if (fl_choice("Unsaved changes will be lost! Continue import?", "Cancel", "Continue", 0) != 1)
			return;
	}
	// open the file
#ifdef __linux
	int offset = 0;
	while (strncmp(filename + offset, "/", 1) != 0)
		++offset;
	char n[PATH_MAX];
	snprintf(n, PATH_MAX, "%s", filename + offset);
	while (n[strlen(n) - 1] == '\n' || n[strlen(n) - 1] == '\r' || n[strlen(n) - 1] == ' ')
		n[strlen(n) - 1] = '\0';
	std::ifstream file(n, std::ifstream::binary);
#else
	std::ifstream file(filename, std::ifstream::binary);
#endif
	if (!file.is_open())
	{
		fl_message("Could not open the file. Do you have read permissions?\n"
				"Note: You can only drop a single file.");
		return;
	}
	// check and load file
	int size;
	file.seekg(0, std::ios::end);
	size = file.tellg();
	file.seekg(0, std::ios::beg);
	if (size < 1605 || size > 1615)
	{
		display_status("*** File format unsupported.");
		file.close();
		return;
	}
	unsigned char* sysex = new unsigned char[size];
	file.read((char*) sysex, size);
	file.close();
	if (!(sysex[0] == 0xf0 && sysex[1] == 0x18 && sysex[2] == 0x0f && sysex[4] == 0x55 && sysex[size - 1] == 0xf7))
	{
		display_status("*** File format unsupported.");
		delete[] sysex;
		return;
	}
	// find packet size
	int pos = DUMP_HEADER_SIZE; // start after the header
	while (++pos < size) // calculate packet size
		if (sysex[pos] == 0xf7)
			break;
	if (0 != test_checksum(sysex, size, pos - DUMP_HEADER_SIZE + 1))
	{
		delete[] sysex;
		display_status("File failed checksum test.");
		return;
	}
	// load preset
	display_status("Uploading...");
	if (preset)
	{
		delete preset;
		delete preset_copy;
	}
	preset = new Preset_Dump(size, sysex, pos - DUMP_HEADER_SIZE + 1);
	preset_copy = new Preset_Dump(size, sysex, pos - DUMP_HEADER_SIZE + 1);
	// set edited
	//preset->set_changed(true);
	// upload to edit buffer
	preset->move(-1);
	preset->upload(0, cfg->get_cfg_option(CFG_CLOSED_LOOP_UPLOAD));
	show_preset();
	delete[] sysex;
}

void PXK::create_device_info()
{
	//pmesg("PXK::create_device_info()\n");
// if we cancelled initialisation of roms, return here
	if (!rom[1])
		return;
	char buf[512];
	snprintf(buf, 512, "%s (%s) with %d ROM%s", get_name(member_code), os_rev, roms,
			(roms > 1 || roms == 0) ? "s:" : ":");
	std::string info;
	info += buf;
	if (roms == 0)
	{
		snprintf(buf, 512, "\nNo ROM installed");
		info += buf;
	}
	else
	{
		for (unsigned char i = 1; i <= roms; i++)
		{
			snprintf(buf, 512, "\n * %s: %d instr, %d prgs", rom[i]->name(), rom[i]->get_attribute(INSTRUMENT),
					rom[i]->get_attribute(PRESET));
			info += buf;
		}
	}
	ui->device_info->copy_label(info.data());
#ifdef SYNCLOG
	ui->init_log->remove(0, ui->init_log->length());
	const char* OS;
#if defined(OSX)
	OS = "Mac OS X";
#elif defined(WIN32)
	OS = "Microsoft Windows";
#else
	OS = "GNU/Linux";
#endif
	snprintf(buf, 512, "prodatum %s on %s\nClosed loop up/down %d/%d\n\n", VERSION, OS,
			cfg->get_cfg_option(CFG_CLOSED_LOOP_UPLOAD), cfg->get_cfg_option(CFG_CLOSED_LOOP_DOWNLOAD));
	ui->init_log->append(buf);
	ui->init_log->append(info.data());
	ui->init_log->append("\n\n");
#endif
}
const char* PXK::get_name(int code) const
{
	//pmesg("PXK::get_name(code: %d)\n", code);
	switch (code)
	{
		case 0x02:
			return "Audity 2000";
		case 0x03:
			return "Proteus 2000";
		case 0x04:
			return "B-3";
		case 0x05:
			return "XL-1";
		case 0x06:
			return "Virtuoso 2000";
		case 0x07:
			return "Mo'Phatt";
		case 0x08:
			return "B-3 Turbo";
		case 0x09:
			return "XL-1 Turbo";
		case 0x0a:
			return "Mo'Phatt Turbo";
		case 0x0b:
			return "Planet Earth";
		case 0x0c:
			return "Planet Earth Turbo";
		case 0x0d:
			return "XL-7";
		case 0x0e:
			return "MP-7";
		case 0x0f:
			return "Proteus 2500";
		case 0x10:
			return "Orbit 3";
		case 0x11:
			return "PK-6";
		case 0x12:
			return "XK-6";
		case 0x13:
			return "MK-6";
		case 0x14:
			return "Halo";
		case 0x15:
			return "Proteus 1000";
		case 0x16:
			return "Vintage Pro";
		case 0x17:
			return "Vintage Keys";
		case 0x18:
			return "PX-7";
		default:
			static char buf[20];
			snprintf(buf, 20, "Unknown (%X)", code);
			return buf;
	}
}

/**
 * displays text in the status bar for 1 second
 */
static void clear_status(void*)
{
	ui->status->label(0);
}

void PXK::display_status(const char* message)
{
	Fl::remove_timeout(clear_status);
	Fl::add_timeout(1.5, clear_status);
	ui->status->copy_label(message);
}

void PXK::update_fx_values(int id, int value) const
{
	//pmesg("PXK::update_fx_values(%d, %d) \n", id, value);
	if (id == 513 || id == 1153) // fxa
	{
		int decay = 0;
		int damp = 0;
		switch (value)
		{
			case 1:
				decay = 40;
				damp = 96;
				break;
			case 2:
				decay = 44;
				damp = 64;
				break;
			case 3:
			case 34:
				decay = 48;
				damp = 96;
				break;
			case 4:
				decay = 56;
				damp = 64;
				break;
			case 5:
				decay = 56;
				damp = 80;
				break;
			case 6:
				decay = 56;
				damp = 64;
				break;
			case 7:
				decay = 56;
				damp = 120;
				break;
			case 8:
				decay = 36;
				damp = 120;
				break;
			case 9:
			case 10:
			case 11:
			case 12:
				decay = 24;
				damp = 64;
				break;
			case 13:
			case 14:
			case 15:
			case 16:
			case 17:
				decay = 48;
				damp = 64;
				break;
			case 18:
				decay = 40;
				damp = 64;
				break;
			case 19:
				decay = 37;
				damp = 120;
				break;
			case 20:
				decay = 60;
				damp = 120;
				break;
			case 21:
				decay = 30;
				damp = 120;
				break;
			case 22:
				decay = 45;
				damp = 120;
				break;
			case 23:
				decay = 48;
				damp = 0;
				break;
			case 24:
				decay = 72;
				damp = 0;
				break;
			case 25:
				decay = 80;
				damp = 0;
				break;
			case 26:
			case 33:
				decay = 64;
				damp = 96;
				break;
			case 27:
				decay = 32;
				damp = 96;
				break;
			case 28:
				decay = 0;
				damp = 0;
				break;
			case 29:
				decay = 0;
				damp = 96;
				break;
			case 30:
				decay = 0;
				damp = 64;
				break;
			case 31:
			case 32:
				decay = 60;
				damp = 120;
				break;
			case 35:
				decay = 80;
				damp = 0;
				break;
			case 36:
				decay = 64;
				damp = 120;
				break;
			case 37:
				decay = 60;
				damp = 8;
				break;
			case 38:
				decay = 60;
				damp = 104;
				break;
			case 39:
				decay = 40;
				damp = 104;
				break;
			case 40:
				decay = 48;
				damp = 112;
				break;
			case 41:
				decay = 52;
				damp = 112;
				break;
			case 42:
				decay = 40;
				damp = 80;
				break;
			case 43:
				decay = 32;
				damp = 56;
				break;
			case 44:
				decay = 56;
				damp = 32;
				break;

		}
		ui->fxa_decay->value((double) decay);
		ui->fxa_damp->value((double) damp);
		if (id == 513)
		{
			setup->set_value(514, decay);
			setup->set_value(515, damp);
		}
		else
		{
			preset->disable_add_undo = true;
			preset->set_value(1154, decay);
			preset->set_value(1155, damp);
			preset->disable_add_undo = false;
		}
	}
	else if (id == 520 || id == 1160) // fxb
	{
		int feedback = 0;
		int lfo = 0;
		int delay = 0;
		switch (value)
		{
			case 1:
				lfo = 3;
				break;
			case 2:
				feedback = 4;
				lfo = 11;
				break;
			case 3:
				feedback = 8;
				lfo = 4;
				break;
			case 4:
				feedback = 16;
				lfo = 11;
				break;
			case 5:
				feedback = 64;
				lfo = 2;
				break;
			case 8:
				feedback = 88;
				lfo = 3;
				break;
			case 9:
				feedback = 64;
				lfo = 1;
				break;
			case 10:
				feedback = 64;
				lfo = 6;
				break;
			case 11:
				feedback = 104;
				lfo = 5;
				break;
			case 12:
				feedback = 72;
				lfo = 2;
				break;
			case 13:
				feedback = 16;
				lfo = 24;
				break;
			case 14:
				feedback = 112;
				lfo = 1;
				break;
			case 15:
				feedback = 16;
				lfo = 4;
				break;
			case 16:
				feedback = 48;
				lfo = 24;
				break;
			case 17:
				feedback = 64;
				lfo = 9;
				break;
			case 18:
				feedback = 32;
				delay = 50;
				break;
			case 19:
				feedback = 32;
				delay = 60;
				break;
			case 20:
			case 21:
				feedback = 32;
				delay = 80;
				break;
			case 22:
				feedback = 16;
				lfo = 9;
				delay = 40;
				break;
			case 23:
				feedback = 24;
				lfo = 24;
				delay = 24;
				break;
			case 24:
				feedback = 24;
				lfo = 3;
				delay = 50;
				break;
			case 25:
			case 26:
				feedback = 32;
				delay = 100;
				break;
			case 27:
				lfo = 70;
				break;
			case 28:
				feedback = 100;
				break;
			case 29:
				feedback = 70;
				lfo = 1;
				break;
			case 30:
				feedback = 90;
				lfo = 6;
				break;
			case 31:
				feedback = 20;
				lfo = 4;
				break;
		}
		ui->fxb_feedback->value((double) feedback);
		ui->fxb_lfo_rate->value((double) lfo);
		ui->fxb_delay->value((double) delay);
		if (id == 513)
		{
			setup->set_value(521, feedback);
			setup->set_value(522, lfo);
			setup->set_value(523, delay);
		}
		else
		{
			preset->disable_add_undo = true;
			preset->set_value(1161, feedback);
			preset->set_value(1162, lfo);
			preset->set_value(1163, delay);
			preset->disable_add_undo = false;
		}
	}
}

void PXK::mute(int state, int layer)
{
	//pmesg("PXK::mute(%d, %d)\n", state, layer);
	if (!preset)
		return;
	if (state)
	{
		if (mute_volume[layer] == -100)
		{
			int tmp = preset->get_value(1410, layer);
			midi->edit_parameter_value(898, layer);
			selected_layer = layer;
			midi->edit_parameter_value(1410, -96);
			mute_volume[layer] = tmp;
			if (is_solo[layer])
				solo(0, layer);
			ui->mute_b[layer]->value(1);
			ui->main->layer_strip[layer]->mute_b->value(1);
		}
	}
	else if (mute_volume[layer] != -100)
	{
		int tmp = mute_volume[layer];
		if (!is_solo[layer])
			for (int i = 0; i < 4; i++)
			{
				is_solo[i] = 0;
				ui->solo_b[i]->value(0);
				ui->main->layer_strip[i]->solo_b->value(0);
			}
		mute_volume[layer] = -100;
		midi->edit_parameter_value(898, layer);
		selected_layer = layer;
		midi->edit_parameter_value(1410, tmp);
		ui->mute_b[layer]->value(0);
		ui->main->layer_strip[layer]->mute_b->value(0);
	}
	if (ui->eall)
	{
		midi->edit_parameter_value(898, -1);
		selected_layer = 0;
	}
}

void PXK::solo(int state, int layer)
{
	//pmesg("PXK::solo(%d, %d)\n", state, layer);
	if (state)
	{
		is_solo[layer] = 1;
		ui->solo_b[layer]->value(1);
		ui->main->layer_strip[layer]->solo_b->value(1);
		for (int i = 0; i < 4; i++)
		{
			if (layer == i)
			{
				mute(0, i);
				continue;
			}
			if (is_solo[i])
			{
				is_solo[i] = 0;
				ui->solo_b[i]->value(0);
				ui->main->layer_strip[i]->solo_b->value(0);
			}
			mute(1, i);
		}
	}
	else if (is_solo[layer])
	{
		is_solo[layer] = 0;
		ui->solo_b[layer]->value(0);
		ui->main->layer_strip[layer]->solo_b->value(0);
		if (mute_volume[layer] == -100)
			for (int i = 0; i < 4; i++)
				mute(0, i);
	}
}

void switch_channel_timeout(void*)
{
	midi->edit_parameter_value(139, pxk->selected_channel);
}

void PXK::start_over()
{
	pmesg("PXK::start_over() \n");
	if (!preset || !preset_copy || !preset->is_changed() || dismiss(0) != 1)
		return;
	// select a different basic channel (erases edit buffer)
	midi->edit_parameter_value(139, (selected_channel + 1) % 15);
	// let it think..
	Fl::add_timeout(.1, switch_channel_timeout);
	delete preset;
	preset = preset_copy->clone();
	show_preset();
}

void PXK::randomize()
{
	pmesg("PXK::randomize() \n");
	if (!setup)
	{
		pxk->display_status("*** Must be connected.");
		return;
	}
	if (!(cfg->get_cfg_option(CFG_CONFIRM_RAND) && !fl_choice("Randomize preset?", "Cancel", "Randomize", 0)))
	{
		ui->set_eall(0);
		randomizing = true;
		midi->randomize();
	}
}

void PXK::save_setup(int dst, const char* newname)
{
	pmesg("PXK::save_setup(%d, %s) \n", dst, newname);
	if (!setup)
	{
		ui->do_save->activate();
		return;
	}
	if (dst == selected_multisetup)
	{
		fl_message("Target must be different from currently\nselected setup.");
		ui->do_save->activate();
		return;
	}
	selected_multisetup = dst;
	char name[17];
	snprintf(name, 17, "%s                ", newname);
	bool updated = false;
	for (unsigned char i = 0; i < 16; i++)
		if (name[i] < (0x20 & 0x7f) || name[i] > (0x7e & 0x7f))
		{
			name[i] = ' ';
			updated = true;
		}
	// update text input
	if (updated)
	{
		char n[17];
		snprintf(n, 17, "%s", name);
		while (n[strlen(n) - 1] == ' ')
			n[strlen(n) - 1] = '\0';
		ui->s_name->value(n);
	}
	// update device
	for (int i = 0; i < 16; i++)
	{
		midi->edit_parameter_value(142 + i, name[i]);
		setup->set_value(142 + i, name[i]);
	}
	midi->copy(C_SETUP, -1, dst);
	// update memory
	setup->set_value(388, dst);
	set_setup_name(dst, (const unsigned char*) name);
	// update setup choice
	char n[21];
	snprintf(n, 21, "%02d: %s", dst, name);
	ui->multisetups->text(dst + 1, n);
	ui->multisetups->select(dst + 1);
	ui->multisetups->redraw();
	ui->do_save->activate();
}

void PXK::store_play_as_initial()
{
	if (!preset)
		return;
	pmesg("PXK::store_play_as_initial() \n");
	ui->main->b_store->deactivate();
	cc_changed = false;
	widget_callback(915, (int) ((Fl_Slider*) ui->main->ctrl_x[1])->value());
	widget_callback(916, (int) ((Fl_Slider*) ui->main->ctrl_x[2])->value());
	widget_callback(917, (int) ((Fl_Slider*) ui->main->ctrl_x[3])->value());
	widget_callback(918, (int) ((Fl_Slider*) ui->main->ctrl_x[4])->value());
	widget_callback(919, (int) ((Fl_Slider*) ui->main->ctrl_x[5])->value());
	widget_callback(920, (int) ((Fl_Slider*) ui->main->ctrl_x[6])->value());
	widget_callback(921, (int) ((Fl_Slider*) ui->main->ctrl_x[7])->value());
	widget_callback(922, (int) ((Fl_Slider*) ui->main->ctrl_x[8])->value());
	widget_callback(924, (int) ((Fl_Slider*) ui->main->ctrl_x[9])->value());
	widget_callback(925, (int) ((Fl_Slider*) ui->main->ctrl_x[10])->value());
	widget_callback(926, (int) ((Fl_Slider*) ui->main->ctrl_x[11])->value());
	widget_callback(927, (int) ((Fl_Slider*) ui->main->ctrl_x[12])->value());
	for (int i = 915; i < 928; i++)
		if (pwid[i][0] && i != 923)
			pwid[i][0]->set_value(preset->get_value(i));
}
