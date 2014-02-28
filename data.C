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
#include <sys/stat.h>
#include <FL/fl_ask.H>

#include "data.H"
#include "midi.H"
#include "cfg.H"
#include "pxk.H"
#include "ui.H"
#include "debug.H"

extern MIDI* midi;
extern Cfg* cfg;
extern PXK* pxk;
extern PD_UI* ui;
extern PD_Arp_Step* arp_step[32];

/**
 * calculates integer value of two nibbelized MIDI bytes
 * @param lsb least significant byte of value
 * @param msb most significant byte of value
 * @returns integer value
 */
static int inline unibble(const unsigned char* lsb, const unsigned char* msb)
{
	int raw_value;
	raw_value = (*msb * 128) + *lsb;
	if (raw_value >= 8192)
		raw_value -= 16384;
	return raw_value;
}

// ###############
// Preset_Dump class
// #################
Preset_Dump::Preset_Dump(int dump_size, const unsigned char* dump_data, int p_size, bool update)
{
	pmesg("Preset_Dump::Preset_Dump(size: %d, data, packet_size: %d)\n", dump_size, p_size);
	disable_add_undo = false;
	data_is_changed = false;
	data = 0;
	size = dump_size;
	packet_size = p_size;
	(size > 1607) ? extra_controller = 4 : extra_controller = 0;
	(size == 1605) ? a2k = 1 : a2k = 0;
	number = pxk->selected_preset;
	rom_id = pxk->selected_preset_rom;
	if (dump_data)
	{
		data = new unsigned char[size];
		memcpy(data, dump_data, size);
		snprintf((char*) name, 17, "%s", data + DUMP_HEADER_SIZE + 9);
	}
	// silently update outside name changes
	if (update)
	{
		if (rom_id == 0 && number >= 0 && pxk->rom[0])
		{
			pxk->rom[0]->set_name(PRESET, number, name);
			ui->preset->load_n(PRESET, 0, number);
			if (ui->copy_arp_rom->value() == 0)
				ui->copy_browser->load_n(PRESET, 0, number);
		}
	}
}

Preset_Dump::~Preset_Dump()
{
	pmesg("Preset_Dump::~Preset_Dump()\n");
	delete[] data;
}

bool Preset_Dump::is_changed() const
{
	return data_is_changed;
}

void Preset_Dump::set_changed(bool set)
{
	if (set)
		data_is_changed = true;
	else
	{
		data_is_changed = false;
		undo_s.clear();
		redo_s.clear();
	}
}

int Preset_Dump::get_extra_controller() const
{
	return extra_controller;
}

void Preset_Dump::clone(Preset_Dump* dst) const
{
	memcpy(dst->data, data, size);
}

const char* Preset_Dump::get_name() const
{
	static char n[17];
	snprintf(n, 17, "%s", name);
	n[3] = '_';
	while (n[strlen(n) - 1] == ' ')
		n[strlen(n) - 1] = '\0';
	return (const char*) n;
}

int Preset_Dump::get_number() const
{
	return number;
}

int Preset_Dump::get_rom_id() const
{
	return rom_id;
}

int Preset_Dump::get_value(int id, int layer) const
{
	//pmesg("Preset_Dump::get_value(id: %d, layer: %d)\n", id, layer);
	int offset = 0;
	idmap(id, layer, offset);
	if (offset == 0)
		return -999;
	return unibble(data + offset, data + offset + 1);
}

int Preset_Dump::set_value(int id, int value, int layer)
{
	//pmesg("Preset_Dump::set_value(id: %d, value: %d, layer: %d)\n", id,
	//		value, layer);
	int offset = 0;
	idmap(id, layer, offset);
	if (offset == 0)
		return 0;
	if (value < 0)
		value += 16384;
	// check wether value is the same
	if (unibble((const unsigned char*) data + offset, (const unsigned char*) data + offset + 1) == value)
		return 0;
	// save undo
	if (!disable_add_undo && id > 914 && !(ui->eall && layer > 0))
	{
		// but not all steps and not if we are undoing
		if (!undo_s.empty())
		{
			parameter current = undo_s.front();
			if (current.id != id || current.layer != layer)
				add_undo(id, value, layer);
		}
		else
			add_undo(id, value, layer);
	}
	// for names, only one byte is used per character...
	data[offset] = value % 128;
	if (id > 914) // ...otherwise 2
		data[offset + 1] = value / 128;
	if (!data_is_changed)
		data_is_changed = true;
	return 1;
}

// type 0 = category, type 1 = name
void Preset_Dump::set_name(const char* val, int type, int position)
{
	unsigned char len;
	unsigned char offset;
	unsigned char i;
	if (type == 0) //category
	{
		len = 3;
		offset = 0;
	}
	else if (type == 1) // name
	{
		len = 12;
		offset = 4;
	}
	else
		return;
	// check input
	char buf[30];
	bool updated = false;
	snprintf(buf, len + 1, "%s              ", val);
	for (i = 0; i < len; i++)
		if (buf[i] < (0x20 & 0x7f) || buf[i] > (0x7e & 0x7f))
		{
			buf[i] = 0x5f;
			updated = true;
			break;
		}
	// update internal data
	for (i = 0; i < len; i++)
	{
		name[i + offset] = buf[i];
		set_value(899 + i + offset, name[i + offset]);
	}
	// update ui
	if (updated)
	{
		while (buf[strlen(buf) - 1] == ' ')
			buf[strlen(buf) - 1] = '\0';
		if (type == 0)
		{
			ui->n_cat_m->value(buf);
			ui->n_cat_m->position(position);
		}
		else
		{
			ui->n_name_m->value(buf);
			ui->n_name_m->position(position);
		}
	}
	snprintf(buf, 30, "%02d.%03d.%d %s", rom_id, number % 128, number / 128, name);
	ui->main->preset_name->copy_label((char*) buf);
	// update the name on the device, just for fun
	for (i = offset; i < len + offset; i++)
		midi->edit_parameter_value(899 + i, *(name + i));
}

void Preset_Dump::show() const
{
	//pmesg("Preset_Dump::show()\n");
	char buf[30];
	snprintf(buf, 30, "%02d.%03d.%d %s", rom_id, number % 128, number / 128, name);
	ui->main->preset_name->copy_label((char*) buf);
	snprintf(buf, 30, "%02d.%03d.%d", rom_id, number % 128, number / 128);
	ui->n_cat_m->copy_label(buf);
	snprintf(buf, 17, "%s", name);
	while (buf[strlen(buf) - 1] == ' ')
		buf[strlen(buf) - 1] = '\0';
	ui->n_name_m->value((const char*) buf + 4);
	//buf[3] = '\0';
	while (strlen(buf) > 3 || buf[strlen(buf) - 1] == ' ')
		buf[strlen(buf) - 1] = '\0';
	ui->n_cat_m->value((const char*) buf);
	// first we wanna load the names into the browsers
	// so they are available for selection
	for (unsigned char j = 0; j < 4; j++)
		if (pwid[1439][j]) // instruments
			pwid[1439][j]->set_value(get_value(1439, j));
	if (pwid[929][0]) // riffs
		pwid[929][0]->set_value(get_value(929));
	if (pwid[1042][0]) // arp
		pwid[1042][0]->set_value(get_value(1042));
	if (pwid[1299][0]) // link 1
		pwid[1299][0]->set_value(get_value(1299));
	if (pwid[1300][0]) // link 2
		pwid[1300][0]->set_value(get_value(1300));
	Fl::wait(.1);
	for (int i = 915; i <= 1300; i++) // preset params
	{
		if ((i > 1152 && i < 1169) || i == 929) // skip fx & riff rom
			continue;
		if (pwid[i][0])
			pwid[i][0]->set_value(get_value(i));
	}
	for (int i = 1409; i <= 1992; i++) // layer params
	{
		// skip roms
		if (i == 1439 || i == 1042 || i == 1299 || i == 1300)
			continue;
		for (int j = 0; j < 4; j++)
			if (pwid[i][j])
				pwid[i][j]->set_value(get_value(i, j));
	}
	update_piano();
	update_envelopes();
	if (pxk->midi_mode != MULTI || pxk->selected_fx_channel != -1)
		show_fx();
}

void Preset_Dump::show_fx() const
{
	//pmesg("Preset_Dump::show_fx()\n");
	for (int i = 1153; i < 1169; i++)
		if (pwid[i][0])
			pwid[i][0]->set_value(get_value(i));
}

void Preset_Dump::update_piano() const
{
	// update piano
	int id = 1413;
	for (int m = 0; m < 3; m++) // modes
		for (int i = 0; i < 4; i++) // layers
			ui->piano->set_range_values(m, i, get_value(id + m * 4, i), get_value(id + 1 + m * 4, i),
					get_value(id + 2 + m * 4, i), get_value(id + 3 + m * 4, i));
	// arp ranges
	ui->piano->set_range_values(0, 4, get_value(1039), 0, get_value(1040), 0);
	if (pxk->setup)
		ui->piano->set_range_values(0, 5, pxk->setup->get_value(655), 0, pxk->setup->get_value(656), 0);
	// link ranges
	ui->piano->set_range_values(0, 6, get_value(1286), 0, get_value(1287), 0);
	ui->piano->set_range_values(0, 7, get_value(1295), 0, get_value(1296), 0);
	// transpose
	ui->piano->set_transpose(get_value(1429, 0), get_value(1429, 1), get_value(1429, 2), get_value(1429, 3));
}

void Preset_Dump::update_envelopes() const
{
	//pmesg("Preset_Dump::update_envelopes()\n");
	static int stages[12];
	int mode = 0;
	int repeat = -1;
	int id = 1793;
	for (int l = 0; l < 4; l++) // for all 4 layers
	{
		for (int m = 0; m < 3; m++) // for all three envelopes
		{
			mode = get_value(id + m * 13, l);
			for (int i = 0; i < 6; i++)
			{
				*(stages + i * 2) = get_value(id + m * 13 + 2 * i + 1, l);
				*(stages + i * 2 + 1) = get_value(id + m * 13 + 2 * i + 2, l);
			}
			switch (m)
			{
				case 1:
					repeat = get_value(1833, l);
					break;
				case 2:
					repeat = get_value(1834, l);
					break;
				default: // volume env has no repeat
					repeat = -1;
			}
			ui->layer_editor[l]->envelope_editor->set_data(m, stages, mode, repeat);
		}
	}
}

void Preset_Dump::upload(int packet, int closed, bool show)
{
	pmesg("Preset_Dump::upload(packet: %d, closed: %d)\n", packet, closed);
	if (!data)
		return;
	const static int chunks = (size - DUMP_HEADER_SIZE) / packet_size;
	const static int tail = (size - DUMP_HEADER_SIZE) % packet_size;
	static int offset;
	static int chunk_size = 0;
	static int closed_loop = 0;
	static int previous_packet = -1;
	static bool show_preset = false;
	if (packet == 0 && closed != -1)
	{
		closed_loop = closed;
		show_preset = show;
		update_checksum();
		if (closed_loop)
			pxk->Loading(true);
	}
	if (closed_loop)
	{
		// first we send the dump header
		if (packet == 0)
		{
			data[3] = (unsigned char) (cfg->get_cfg_option(CFG_DEVICE_ID) & 0xFF);
			data[6] = 0x01;
			offset = 0;
			chunk_size = DUMP_HEADER_SIZE;
		}
		else
		{
			if (previous_packet != packet)
			{
				if (packet == 1)
					offset += DUMP_HEADER_SIZE;
				else
					offset += packet_size;
			}
			data[offset + 3] = (unsigned char) (cfg->get_cfg_option(CFG_DEVICE_ID) & 0xFF);
			data[offset + 6] = 0x02;
			chunk_size = packet_size;
			if (packet == chunks + 1)
				chunk_size = tail;
		}
		// ack of tail...send eof
		if (packet == chunks + 2)
		{
			ui->supergroup->clear_output();
			midi->eof();
			if (show_preset)
				pxk->show_preset();
			if (unibble(data + 7, data + 8) == -1)
				pxk->display_status("Upload successful.");
			else
				pxk->display_status("Program saved.");
		}
		else
			midi->write_sysex(data + offset, chunk_size);
		previous_packet = packet;
	}
	else
	{
		for (int i = 0; i < chunks + 2; i++)
		{
			if (i == 0)
			{
				data[3] = (unsigned char) (cfg->get_cfg_option(CFG_DEVICE_ID) & 0xFF);
				data[6] = 0x03;
				offset = 0;
				chunk_size = DUMP_HEADER_SIZE;
			}
			else
			{
				if (i == 1)
					offset += DUMP_HEADER_SIZE;
				else
					offset += packet_size;
				data[offset + 3] = (unsigned char) (cfg->get_cfg_option(CFG_DEVICE_ID) & 0xFF);
				data[offset + 6] = 0x04;
				chunk_size = packet_size;
				if (i == chunks + 1)
					chunk_size = tail;
			}
			midi->write_sysex(data + offset, chunk_size);
			if (i == chunks + 1)
			{
				if (show_preset)
					pxk->show_preset();
				if (unibble(data + 7, data + 8) == -1)
					pxk->display_status("Upload successful.");
				else
					pxk->display_status("Program saved.");
			}
		}
	}
}

void Preset_Dump::save_file()
{
	pmesg("Preset_Dump::save_file() \n");
	char path[PATH_MAX];
	snprintf(path, PATH_MAX, "%s/%s.syx", cfg->get_export_dir(), get_name());
	// check if file exists and ask for confirmation
	struct stat sbuf;
	if (stat(path, &sbuf) == 0 && fl_choice("Overwrite existing file?", "No", "Overwrite", 0) == 0)
		return;
	// write
	std::ofstream file(path, std::ofstream::binary | std::ios::trunc);
	if (!file.is_open())
	{
		fl_message("Could not write the file.\nDo you have permission to write?");
		return;
	}
	update_checksum(); // save a valid dump
	file.write((const char*) data, size);
	file.close();
	pxk->display_status("Saved to export directory.");
}

void Preset_Dump::move(int new_number)
{
	pmesg("Preset_Dump::move(position: %d)\n", new_number);
	if (!data)
		return;
	number = new_number;
	rom_id = 0;
	if (new_number < 0)
		new_number += 16384;
	// preset number
	data[7] = new_number % 128;
	data[8] = new_number / 128;
	// rom id
	data[33] = 0;
	data[34] = 0;
}

// recursively copy any parameter range in chunks
void Preset_Dump::copy_layer_parameter_range(int start, int end, int src, int dst)
{
	// reverse order so instrument rom is set before instrument
	if (start < end)
	{
		int tmp = start;
		start = end;
		end = tmp;
	}
	pmesg("Preset_Dump::copy_layer_parameter_range(%d,%d,%d,%d)\n", start, end, src, dst);
	unsigned char m[255];
	m[0] = 0xf0;
	m[1] = 0x18;
	m[2] = 0x0f;
	m[3] = cfg->get_cfg_option(CFG_DEVICE_ID);
	m[4] = 0x55;
	m[5] = 0x01;
	int limit = 0;
	if (start - end > 41)
	{
		// find real ids
		int real = 0;
		int i;
		for (i = start; i >= end; i--)
		{
			if (get_value(i, src) != -999)
				++real;
			if (real == 41)
				break;
		}
		limit = i;
	}
	else
		limit = end;

	int real = -1;
	// dont keep undos for copy operations
	disable_add_undo = true;
	for (int i = start; i >= limit; i--)
	{
		int v = get_value(i, src);
		if (v != -999)
			++real;
		else
			continue;
		int offset = 7 + 4 * real;
		set_value(i, v, dst);
		if (pwid[i][dst])
			pwid[i][dst]->set_value(v);
		if (v < 0)
			v += 16384;
		m[offset] = i % 128;
		m[offset + 1] = i / 128;
		m[offset + 2] = v % 128;
		m[offset + 3] = v / 128;
		if (i == limit)
			m[offset + 4] = 0xf7;
	}
	disable_add_undo = false;
	m[6] = (real + 1) * 2;
	midi->write_sysex(m, 8 + m[6] * 2);
	if (limit != end)
		copy_layer_parameter_range(limit + 1, end, src, dst);
}

void Preset_Dump::copy(int type, int src, int dst)
{
	pmesg("Preset_Dump::copy(%X,%d,%d)\n", type, src, dst);
	if (type >= C_LAYER && type <= C_LAYER_PATCHCORD)
	{
		midi->edit_parameter_value(898, dst);
		pxk->selected_layer = dst;
	}
	switch (type)
	{
		case C_PRESET:
		{
			midi->copy(C_PRESET, number, dst, 0, 0, rom_id);
			// update names and browsers
			pxk->rom[0]->set_name(PRESET, dst, pxk->rom[pxk->get_rom_index(rom_id)]->get_name(PRESET, number));
			if (ui->preset_rom->value() == 0)
				ui->preset->load_n(PRESET, 0, dst);
			ui->copy_browser->load_n(PRESET, 0, dst);
			pxk->display_status("Copied.");
		}
			break;
		case C_PRESET_COMMON:
			break;
		case C_ARP:
		{
			// note: we cannot copy the arp into the edit buffer
			// so we only allow copying if a user preset is selected
			if (!pxk->preset_copy->get_rom_id())
			{
				midi->copy(C_ARP, dst, pxk->preset_copy->get_number(), 0, 0, ui->copy_arp_rom->get_value());
				// wait a bit for the device to update the edit buffer
				mysleep(100);
				midi->request_preset_dump(-1, 0);
			}
			pxk->display_status("Copied.");
			break;
		}
		case C_FX:
			break;
		case C_LAYER:
			copy_layer_parameter_range(1409, 1992, src, dst);
			update_piano();
			update_envelopes();
			pxk->display_status("Copied.");
			break;
		case C_LAYER_COMMON:
			copy_layer_parameter_range(1409, 1439, src, dst);
			pxk->display_status("Copied.");
			break;
		case C_LAYER_FILTER:
			copy_layer_parameter_range(1537, 1539, src, dst);
			pxk->display_status("Copied.");
			break;
		case C_LAYER_LFO:
			copy_layer_parameter_range(1665, 1674, src, dst);
			pxk->display_status("Copied.");
			break;
		case C_LAYER_ENVELOPE:
			copy_layer_parameter_range(1793, 1834, src, dst);
			update_envelopes();
			pxk->display_status("Copied.");
			break;
		case C_LAYER_PATCHCORD:
			copy_layer_parameter_range(1921, 1992, src, dst);
			pxk->display_status("Copied.");
			break;
		case C_ARP_PATTERN:
		{
			char source_rom;
			if (ui->preset_editor->arp->visible_r())
			{
				src = ui->preset_editor->arp->get_value();
				source_rom = pxk->rom[ui->preset_editor->arp_rom->value()]->get_attribute(ID);
			}
			else
			{
				src = ui->main->arp->get_value();
				source_rom = pxk->rom[ui->main->arp_rom->value()]->get_attribute(ID);
			}
			midi->copy(C_ARP_PATTERN, src, dst, 0, 0, source_rom);
			// update names
			pxk->rom[0]->set_name(ARP, dst, pxk->rom[pxk->get_rom_index(source_rom)]->get_name(ARP, src));
			ui->copy_arp_pattern_browser->load_n(ARP, 0, dst);
			if (ui->preset_editor->arp_rom->value() == 0)
				ui->preset_editor->arp->load_n(ARP, 0, dst);
			if (ui->main->arp_rom->value() == 0)
				ui->main->arp->load_n(ARP, 0, dst);
			pxk->display_status("Copied.");
			break;
		}
		case SAVE_PRESET:
		{
			// update internal files and ui
			pxk->rom[0]->set_name(PRESET, dst, name);
			ui->preset->load_n(PRESET, 0, dst);
			if (ui->copy_arp_rom->value() == 0)
				ui->copy_browser->load_n(PRESET, 0, dst);
			// upload preset
			move(dst);
			upload(0, cfg->get_cfg_option(CFG_CLOSED_LOOP_UPLOAD));
			set_changed(false);
		}
	}
}

void Preset_Dump::add_undo(int id, int value, int layer)
{
	if (disable_add_undo)
		return;
	//pmesg("Preset_Dump::add_undo(%d, %d, %d)\n", id, value, layer);
	parameter changed, prev_changed;
	// envelopes are special because we change 2 values at once
	// and on undo we want to change both back at once
	// everything else just doesn't feel right
	if ((id > 1793 && id < 1833) && id != 1806 && id != 1819) // envelopes
	{
		int second_id = 0;
		if (id <= 1805 || id >= 1820)
		{
			if (id % 2 != 0)
				id -= 1;
			second_id = id + 1;
		}
		else
		{
			if (id % 2 == 0)
				id -= 1;
			second_id = id + 1;
		}
		// ^^ id is rate, second_id is lvl
		if (!undo_s.empty())
		{
			prev_changed = undo_s.front();
			if (prev_changed.id == second_id && prev_changed.layer == layer)
				return; // dont save all steps
		}
		// save current values
		changed.layer = layer;
		// first value
		changed.id = id;
		changed.value = get_value(id, layer);
		undo_s.push_front(changed);
		// second value
		changed.id = second_id;
		changed.value = get_value(second_id, layer);
		undo_s.push_front(changed);
	}
	else
	{
		// save current value
		changed.layer = layer;
		changed.id = id;
		changed.value = get_value(id, layer);
		undo_s.push_front(changed);
	}
	redo_s.clear();
	ui->redo_b->deactivate();
	ui->undo_b->activate();
}

void Preset_Dump::undo()
{
	if (!undo_s.empty())
	{
		//pmesg("Preset_Dump::undo()\n");
		// 10000 undos max! ")
		if (undo_s.size() > 10000)
			undo_s.pop_back();
		bool repeat = true;
		Undo:
		// save current value to redo
		parameter p = undo_s.front();
		parameter c;
		c.layer = p.layer;
		c.id = p.id;
		c.value = get_value(p.id, p.layer);
		// undo
		pxk->widget_callback(p.id, p.value, p.layer);
		// focus the widget
		//((Fl_Widget*)pwid[p.id][p.layer])->take_focus();
		// update stack
		undo_s.pop_front();
		redo_s.push_front(c);
		if (undo_s.empty())
			ui->undo_b->deactivate();
		else if (repeat && (p.id > 1793 && p.id < 1833) && p.id != 1806 && p.id != 1819) // envelopes
		{
			repeat = false;
			goto Undo;
		}
		// update UI
		ui->redo_b->activate();
		update_ui_from_xdo(p.id, p.value, p.layer);
	}
	else
		pxk->display_status("*** Nothing to Undo.");
}

void Preset_Dump::redo()
{
	if (!redo_s.empty())
	{
		//pmesg("Preset_Dump::redo()\n");
		bool repeat = true;
		Redo:
		// save current value for undo
		parameter p = redo_s.front();
		parameter c;
		c.layer = p.layer;
		c.id = p.id;
		c.value = get_value(p.id, p.layer);
		undo_s.push_front(c);
		// redo
		pxk->widget_callback(p.id, p.value, p.layer);
		// update stack
		redo_s.pop_front();
		if (redo_s.empty())
			ui->redo_b->deactivate();
		else if (repeat && (p.id > 1793 && p.id < 1833) && p.id != 1806 && p.id != 1819) // envelopes
		{
			repeat = false;
			goto Redo;
		}
		// update UI
		ui->undo_b->activate();
		update_ui_from_xdo(p.id, p.value, p.layer);
	}
	else
		pxk->display_status("*** Nothing to redo.");
}

void Preset_Dump::update_ui_from_xdo(int id, int value, int layer) const
{
	if (id > 1792 && id < 1835)
		update_envelopes();
	else if ((id > 1412 && id < 1425) || id == 1429)
		update_piano();
	else
	{
		pwid_editing = pwid[id][layer];
		int* minimax = pwid_editing->get_minimax();
		ui->value_input->minimum((double) minimax[0]);
		ui->value_input->maximum((double) minimax[1]);
		pwid[1][0]->set_value(value);
		pwid[id][layer]->set_value(value);
		ui->forma_out->set_value(id, layer, value);
		ui->forma_out->redraw();
	}
}

// maps Parameter IDs from the device to data position in preset dump
void Preset_Dump::idmap(const int& id, const int& layer, int& id_mapped) const
{
	if (!data
			|| !((id >= 899 && id <= 970) || (id >= 1025 && id <= 1043) || (id >= 1153 && id <= 1168)
					|| (id >= 1281 && id <= 1300) || (id >= 1409 && id <= 1439) || (id >= 1537 && id <= 1539)
					|| (id >= 1665 && id <= 1674) || (id >= 1793 && id <= 1834) || (id >= 1921 && id <= 1992)))
		return;
	if (!extra_controller && (id >= 967 && id <= 970))
		return;
	if (a2k && id == 1043) //  a2k has no arp post delay
		return;
	int pos;
	if (id - 970 <= 0) // Preset Common General Edit Parameters
	{
		pos = id - 899;
		if (pos < 16) // name chars
		{
			id_mapped = DUMP_HEADER_SIZE + 9 + pos;
			return;
		}
	}

	else if (id - 1043 <= 0) // Preset Common Arpeggiator Edit Parameters
		pos = id - 1025 + 68 + extra_controller;

	else if (id - 1168 <= 0) // Preset Common Effects Edit Parameters
		pos = id - 1153 + 87 + extra_controller - a2k;

	else if (id - 1300 <= 0) // Preset Common Links Edit Parameters
		pos = id - 1281 + 103 + extra_controller - a2k;
	// layer parameters start here
	else if (id - 1439 <= 0) // Preset Layer General Edit Parameters
		pos = id - 1409 + 123 + extra_controller - a2k + layer * 158;

	else if (id - 1539 <= 0) // Preset Layer Filter Edit Parameters
		pos = id - 1537 + 154 + extra_controller - a2k + layer * 158;

	else if (id - 1674 <= 0) // Preset Layer LFOs Edit Parameters
		pos = id - 1665 + 157 + extra_controller - a2k + layer * 158;

	else if (id - 1834 <= 0) // Preset Layer Envelope Edit Parameters
		pos = id - 1793 + 167 + extra_controller - a2k + layer * 158;

	else
		// Preset Layer PatchCords Edit Parameters
		pos = id - 1921 + 209 + extra_controller - a2k + layer * 158;

	pos = (pos - 16) * 2 + 16;
	id_mapped = DUMP_HEADER_SIZE + packet_size * (pos / (packet_size - 11)) + (pos % (packet_size - 11)) + 9;
	if (id_mapped > size - 4)
	{
		pmesg("*** Preset_Dump::idmap value out of bounds: id %d, layer %d, offset %d\n", id, layer, id_mapped);
		id_mapped = 0;
		return;
	}
}

// updates checksums in the dump
void Preset_Dump::update_checksum()
{
	pmesg("Preset_Dump::update_checksum()\n");
	if (!data)
		return;
	const static unsigned char chunks = (size - DUMP_HEADER_SIZE) / packet_size;
	const static int tail = (size - DUMP_HEADER_SIZE) % packet_size - 11;
	int offset = DUMP_HEADER_SIZE + 9;
	int sum;
	unsigned char checksum;
	for (unsigned char j = 0; j < chunks; j++)
	{
		sum = 0;
		for (int i = 0; i < (packet_size - 11); i++)
			sum += data[offset + i];
		checksum = ~sum;
		data[offset + packet_size - 11] = checksum % 128;
		offset += packet_size;
	}
	sum = 0;
	for (int i = 0; i < tail; i++)
		sum += data[offset + i];
	checksum = ~sum;
	data[offset + tail] = checksum % 128;
}

// #################
// Arp_Dump class
// #################
Arp_Dump::Arp_Dump(int dump_size, const unsigned char* dump_data)
{
	pmesg("Arp_Dump::Arp_Dump(size: %d, data)\n", dump_size);
	size = dump_size;
	data = new unsigned char[dump_size];
	memcpy(data, dump_data, dump_size);
	number = data[6] + 128 * data[7];
	snprintf((char*) name, 17, "%s                 ", data + 14);
	// tell device that we want to edit this pattern
	midi->edit_parameter_value(769, number);
	show();
}

Arp_Dump::~Arp_Dump()
{
	pmesg("Arp_Dump::~Arp_Dump()\n");
	delete[] data;
}

int Arp_Dump::get_number() const
{
	pmesg("Arp_Dump::get_number()\n");
	return number;
}

void Arp_Dump::show() const
{
	//pmesg("Arp_Dump::show()\n");
	// fill name field
	update_name(name);
	int offset;
	int velocity;
	int duration;
	int repeat;
	int tmp;
	unsigned char* dat = data + 26;
	// load step values
	for (unsigned char i = 0; i < 32; i++)
	{
		tmp = i * 8;
		offset = unibble(dat + tmp, dat + tmp + 1);
		velocity = unibble(dat + tmp + 2, dat + tmp + 3);
		duration = unibble(dat + tmp + 4, dat + tmp + 5);
		repeat = unibble(dat + tmp + 6, dat + tmp + 7);
		arp_step[i]->set_values(offset, velocity, duration, repeat);
	}
	update_sequence_length_information();
	if (!ui->g_arp_edit->visible())
	{
		ui->g_arp_edit->show();
		ui->g_main->hide();
		Fl::focus(ui->g_arp_edit);
	}
}

void Arp_Dump::update_sequence_length_information() const
{
	//pmesg("Arp_Dump::update_sequence_length_information()\n");
	int tick[19] =
	{ 6, 8, 9, 12, 16, 18, 24, 32, 36, 48, 64, 72, 96, 128, 144, 192, 256, 288, 384 };
	unsigned char i = 0;
	int total = 0;
	while (i < 32 && !arp_step[i]->End->value())
	{
		if (!arp_step[i]->skip->value())
			total += tick[(int) arp_step[i]->duration->value() - 1] * (int) arp_step[i]->repeat->value();
		++i;
	}
	int beat = total / 48;
	int ticks = total % 48;
	char buf[8];
	snprintf(buf, 8, "%d:%d", beat, ticks);
	ui->pattern_length->value(buf);
}

int Arp_Dump::get_value(int id, int step) const
{
	pmesg("Arp_Dump::get_value(id: %d, step: %d)\n", id, step);
	int offset = (id - 784) * 2 + 26 + step * 8;
	int value = unibble(data + offset, data + offset + 1);
	if (id == 787) // repeat
		value += 1;
	return value;
}

void Arp_Dump::reset_step(int step) const
{
	pmesg("Arp_Dump::reset_step(step: %d)\n", step);
	int tmp = step * 8;
	unsigned char* dat = data + 26;
	int offset = unibble(dat + tmp, dat + tmp + 1);
	int velocity = unibble(dat + tmp + 2, dat + tmp + 3);
	int duration = unibble(dat + tmp + 4, dat + tmp + 5);
	int repeat = unibble(dat + tmp + 6, dat + tmp + 7);
	arp_step[step]->set_values(offset, velocity, duration, repeat);
	arp_step[step]->op->do_callback();
	arp_step[step]->velocity->do_callback();
	arp_step[step]->duration->do_callback();
	arp_step[step]->repeat->do_callback();
}

void Arp_Dump::rename(const char* newname) const
{
	pmesg("Arp_Dump::rename(%s)\n", newname);
	unsigned char buf[17];
	snprintf((char*) buf, 17, "%s                 ", newname);
	for (unsigned char i = 0; i < 12; i++)
	{
		if (buf[i] < (0x20 & 0x7f) || buf[i] > (0x7e & 0x7f))
			buf[i] = 0x32;
		midi->edit_parameter_value(771 + i, buf[i]);
	}
	update_name(buf);
}

void Arp_Dump::update_name(const unsigned char* np) const
{
	//pmesg("Arp_Dump::update_name(%s)\n", np);
	pxk->rom[0]->set_name(ARP, number, np);
	ui->copy_arp_pattern_browser->load_n(ARP, 0, number);
	if (ui->preset_editor->arp_rom->value() == 0)
		ui->preset_editor->arp->load_n(ARP, 0, number);
	if (ui->main->arp_rom->value() == 0)
		ui->main->arp->load_n(ARP, 0, number);
	// fill name field
	char n[13];
	snprintf(n, 13, "%s", np);
	while (n[strlen(n) - 1] == ' ')
		n[strlen(n) - 1] = '\0';
	ui->arp_name->value(n);
}

void Arp_Dump::reset_pattern() const
{
	pmesg("Arp_Dump::reset_pattern()\n");
	midi->write_sysex(data, (size_t) size);
	show();
}

// #################
// Setup_Dump class
// #################
Setup_Dump::Setup_Dump(int dump_size, const unsigned char* dump_data)
{
	pmesg("Setup_Dump::Setup_Dump(size: %d, data)\n", dump_size);
	size = dump_size;
	(size > 689) ? extra_controller = 4 : extra_controller = 0;
	data = 0;
	if (dump_data)
	{
		data = new unsigned char[size];
		memcpy(data, dump_data, size);
		setup_dump_info[0] = data[7] * 128 + data[6]; // # general
		setup_dump_info[1] = data[9] * 128 + data[8]; // # master
		setup_dump_info[2] = data[11] * 128 + data[10]; // # master fx
		setup_dump_info[3] = data[13] * 128 + data[12]; // # reserved
		setup_dump_info[4] = data[15] * 128 + data[14]; // # non-chan
		setup_dump_info[5] = data[17] * 128 + data[16]; // # midi chnls
		setup_dump_info[6] = data[19] * 128 + data[18]; // # chnl params
	}
}

Setup_Dump::~Setup_Dump()
{
	pmesg("Setup_Dump::~Setup_Dump()\n");
	delete[] data;
}

int Setup_Dump::get_value(int id, int channel) const
{
	//pmesg("Setup_Dump::get_value(id: %d, ch: %d)\n", id, channel);
	int offset = 0;
	idmap(id, channel, offset);
	if (offset == 0)
		return -999;
	return unibble(data + offset, data + offset + 1);
}

int Setup_Dump::get_dump_size() const
{
	//pmesg("Setup_Dump::get_dump_size()\n");
	return size;
}

const unsigned char* Setup_Dump::get_data() const
{
	//pmesg("Setup_Dump::get_data()\n");
	return data;
}

int Setup_Dump::set_value(int id, int value, int channel)
{
	//pmesg("Setup_Dump::set_value(id: %d, value: %d, ch: %d)\n", id, value, channel);
	if (id == 388 && data)
	{
		data[0x4A] = value;
		return 0;
	}
	// update changes in memory
	int offset = 0;
	idmap(id, channel, offset);
	if (offset == 0)
		return 0;
	if (value < 0)
		value += 16384;
	// check wether value is the same
	if (unibble((const unsigned char*) data + offset, (const unsigned char*) data + offset + 1) == value)
		return 0;
	data[offset] = value % 128;
	if (id < 142 || id > 157) // for name only one byte per char
		data[offset + 1] = value / 128;
	return 1;
}

void Setup_Dump::show() const
{
	pmesg("Setup_Dump::show()\n");
	int skip = 0;
	if (pxk->midi_mode != MULTI)
		skip = 140;
	// first fill the arp browser
	if (pwid[660][0])
		pwid[660][0]->set_value(get_value(660));
	for (int i = 140; i <= 787; i++)
	{
		// skip fx and browsers
		if (pwid[i][0] && (!((i > 512 && i < 529) || i == skip || i == 660)))
			pwid[i][0]->set_value(get_value(i));
	}
	if (pxk->midi_mode == MULTI)
	{
		if (pxk->selected_fx_channel == -1)
			show_fx();
		ui->main->mix_out->activate();
	}
	else
		ui->main->mix_out->deactivate();
}

void Setup_Dump::show_fx() const
{
	pmesg("Setup_Dump::show_fx()\n");
	//	ui->main->fx->activate();
	for (int i = 513; i < 529; i++)
		if (pwid[i][0])
			pwid[i][0]->set_value(get_value(i));
}

void Setup_Dump::upload() const
{
	pmesg("Setup_Dump::upload()\n");
	midi->write_sysex(data, size);
	// let it eat
	mysleep(800);
}

// maps Parameter IDs from the device to data position in preset dump
void Setup_Dump::idmap(const int& id, const int& channel, int& id_mapped) const
{
	//pmesg(100, "Setup_Dump::idmap(id: %d, ch: %d, offset)\n", id, channel);
	if (!data
			|| !((id >= 130 && id <= 157) || (id >= 257 && id <= 269) || (id >= 271 && id <= 278) || (id >= 385 && id <= 414)
					|| (id >= 513 && id <= 528) || (id >= 641 && id <= 661)) || id == 136 || id == 261 || id == 262 || id == 263
			|| id == 387 || id == 389 || id == 390)
		return;
	if (!extra_controller && (id >= 411 && id <= 414))
		return;
	// channel parameters
	if (id >= 130 && id <= 138)
		id_mapped = 2 * (id - 130) + 36
				+ 2
						* (setup_dump_info[SDI_GENERAL] + setup_dump_info[SDI_MIDI] + setup_dump_info[SDI_FX]
								+ setup_dump_info[SDI_RESERVED] + setup_dump_info[SDI_NON_CHNL])
				+ channel * (2 * setup_dump_info[SDI_CHNL_PARAMS]);
	// multimode parameters
	else if (id >= 139 && id <= 141)
		id_mapped = 2 * (id - 139) + 36
				+ 2 * (setup_dump_info[SDI_GENERAL] + setup_dump_info[SDI_MIDI] + setup_dump_info[SDI_FX] + 21);
	// name
	else if (id >= 142 && id <= 157)
		id_mapped = id - 122;
	// general parameters
	else if (id >= 257 && id <= 278)
	{
		id_mapped = 2 * (id - 257) + 36;
		if (id > 263)
			id_mapped -= 6;
	}
	// midi parameters
	else if (id >= 385 && id <= 422)
	{
		id_mapped = 2 * (id - 385) + 36 + 2 * setup_dump_info[SDI_GENERAL];
		if (id > 390)
			id_mapped -= 6;
	}
	// fx parameters
	else if (id >= 513 && id <= 528)
		id_mapped = 2 * (id - 513) + 36 + 2 * (setup_dump_info[SDI_GENERAL] + setup_dump_info[SDI_MIDI]);
	// arp parameters
	else if (id >= 641 && id <= 661)
		id_mapped = 2 * (id - 641) + 36
				+ 2 * (setup_dump_info[SDI_GENERAL] + setup_dump_info[SDI_MIDI] + setup_dump_info[SDI_FX]);
	else
		pmesg("*** Setup_Dump::idmap: Request for unknown ID: %d\n", id);
	if (id_mapped > size - 3)
	{
		pmesg("*** Setup_Dump::idmap value out of bounds: id %d, channel %d, offset %d\n", id, channel, id_mapped);
		id_mapped = 0;
		return;
	}
}

// ###############
// ROM class
// #################
ROM::ROM(int i, int pr, int in)
{
	pmesg("ROM::ROM(%d, %d, %d)  \n", i, pr, in);
	id = i;
	presets = pr;
	instruments = in;
	riffs = 0;
	arps = 0;
	instrument_names = 0;
	preset_names = 0;
	arp_names = 0;
	riff_names = 0;
	for (unsigned char i = 0; i < 7; i++)
		on_disk[i] = false;
	const char* rom_name = name();
	ui->preset_rom->add(rom_name);
	ui->preset_editor->l1_rom->add(rom_name);
	ui->preset_editor->l2_rom->add(rom_name);
	// some roms have no arps but we have to add them anyway
	// so the rom indizes are still working correctly.
	ui->preset_editor->arp_rom->add(rom_name);
	ui->main->arp_rom->add(rom_name);
	ui->copy_arp_rom->add(rom_name);
	// reset rom choice
	if (id != 0)
	{
		ui->r_rom_rom->add(rom_name);
		const Fl_Menu_Item* item;
		item = ui->preset_editor->arp_rom->find_item(rom_name);
		const_cast<Fl_Menu_Item*>(item)->hide();
		item = ui->main->arp_rom->find_item(rom_name);
		const_cast<Fl_Menu_Item*>(item)->hide();
		item = ui->copy_arp_rom->find_item(rom_name);
		const_cast<Fl_Menu_Item*>(item)->hide();
		for (int i = 0; i < 4; i++)
			ui->layer_editor[i]->instrument_rom->add(rom_name);
		ui->preset_editor->riff_rom->add(rom_name);
		ui->main->riff_rom->add(rom_name);
		device_id = -1; // roms are not device specific
	}
	else
	{
		arps = 100;
		device_id = cfg->get_cfg_option(CFG_DEVICE_ID);
	}
}

ROM::~ROM()
{
	pmesg("ROM::~ROM()  \n");
	// clear rom choices once
	if (id == 0)
	{
		for (unsigned char i = 0; i < 4; i++)
			ui->layer_editor[i]->instrument_rom->clear();
		ui->preset_rom->clear();
		ui->preset_editor->arp_rom->clear();
		ui->preset_editor->riff_rom->clear();
		ui->preset_editor->l1_rom->clear();
		ui->preset_editor->l2_rom->clear();
		ui->main->riff_rom->clear();
		ui->main->arp_rom->clear();
		ui->copy_arp_rom->clear();
	}
	if (pxk->Synchronized())
	{
		const char* path = cfg->get_config_dir();
		char filename[PATH_MAX];
		// instruments
		if (id != 0 && instrument_names && !on_disk[INSTRUMENT])
		{
			snprintf(filename, PATH_MAX, "%s/n_ins_%d", path, id);
			std::fstream file(filename, std::ios::out | std::ios::binary | std::ios::trunc);
			file.write((char*) instrument_names, instruments * 16);
			file.close();
		}
		// riffs
		if (id != 0 && riff_names && !on_disk[RIFF])
		{
			snprintf(filename, PATH_MAX, "%s/n_rff_%d", path, id);
			std::fstream file(filename, std::ios::out | std::ios::binary | std::ios::trunc);
			file.write((char*) riff_names, riffs * 16);
			file.close();
		}
		// presets
		if (preset_names && !on_disk[PRESET])
		{
			if (id == 0)
				snprintf(filename, PATH_MAX, "%s/n_prs_%d_%d", path, id, device_id);
			else
				snprintf(filename, PATH_MAX, "%s/n_prs_%d", path, id);
			std::fstream file(filename, std::ios::out | std::ios::binary | std::ios::trunc);
			file.write((char*) preset_names, presets * 16);
			file.close();
		}
		// arps
		if (arp_names && !on_disk[ARP])
		{
			if (id == 0)
				snprintf(filename, PATH_MAX, "%s/n_arp_%d_%d", path, id, device_id);
			else
				snprintf(filename, PATH_MAX, "%s/n_arp_%d", path, id);
			std::fstream file(filename, std::ios::out | std::ios::binary | std::ios::trunc);
			file.write((char*) arp_names, arps * 16);
			file.close();
		}
	}
	delete[] instrument_names;
	delete[] preset_names;
	delete[] arp_names;
	delete[] riff_names;
}

void ROM::load_name(unsigned char type, int number)
{
	//pmesg("ROM(%d)::load_name(type %d, number %d) \n", id, type, number);
	// for rom arps we need to use arp dumps
	if (type == ARP && id != 0)
		midi->request_arp_dump(number, id);
	else
		midi->request_name(type, number, id);
}

int ROM::disk_load_names(unsigned char type)
{
	//pmesg("ROM::disk_load_names(type: %d)  \n", type);
	const char* path = cfg->get_config_dir();
	char filename[PATH_MAX];
	int* number;
	unsigned char** data;
	switch (type)
	{
		case INSTRUMENT:
			snprintf(filename, PATH_MAX, "%s/n_ins_%d", path, id);
			number = &instruments;
			data = &instrument_names;
			break;
		case PRESET:
			if (id == 0)
				snprintf(filename, PATH_MAX, "%s/n_prs_%d_%d", path, id, device_id);
			else
				snprintf(filename, PATH_MAX, "%s/n_prs_%d", path, id);
			number = &presets;
			data = &preset_names;
			break;
		case ARP:
			if (id == 0)
				snprintf(filename, PATH_MAX, "%s/n_arp_%d_%d", path, id, device_id);
			else
				snprintf(filename, PATH_MAX, "%s/n_arp_%d", path, id);
			number = &arps;
			data = &arp_names;
			break;
		case RIFF:
			snprintf(filename, PATH_MAX, "%s/n_rff_%d", path, id);
			number = &riffs;
			data = &riff_names;
			break;
		default:
			return -1;
	}
	std::fstream file(filename, std::ios::in | std::ios::binary | std::ios::ate);
	if (file.is_open())
	{
		int size = file.tellg();
		if (size % 16)
		{
			file.close();
			return *number;
		}
		*number = size / 16;
		*data = new unsigned char[size];
		file.seekg(0, std::ios::beg);
		file.read((char*) *data, size);
		file.close();
		if (id != 0 || *number == 1)
			on_disk[type] = true;
		// show this rom in arp selections
		if (type == ARP && id != 0)
		{
			if (*number > 1)
			{
				const char* rom_name = ROM::name();
				const Fl_Menu_Item* item;
				item = ui->preset_editor->arp_rom->find_item(rom_name);
				const_cast<Fl_Menu_Item*>(item)->show();
				item = ui->main->arp_rom->find_item(rom_name);
				const_cast<Fl_Menu_Item*>(item)->show();
				item = ui->copy_arp_rom->find_item(rom_name);
				const_cast<Fl_Menu_Item*>(item)->show();
			}
		}
		return -1;
	}
	return *number;
}

int ROM::set_name(int type, int number, const unsigned char* name)
{
	//pmesg("ROM::set_name(type: %d, #: %d, name) (id: %d) \n", type, number, id);
	switch (type)
	{
		case INSTRUMENT:
			if (!instrument_names)
				instrument_names = new unsigned char[16 * instruments];
			if (number >= instruments)
			{
				pmesg("*** ROM::set_name out of bounds: rom: %d type %d, number %d\n", id, type, number);
				return 0;
			}
			memcpy(instrument_names + 16 * number, name, 16);
			break;
		case PRESET:
			if (!preset_names)
				preset_names = new unsigned char[16 * presets];
			if (number >= presets)
			{
				pmesg("*** ROM::set_name out of bounds: rom: %d type %d, number %d\n", id, type, number);
				return 0;
			}
			memcpy(preset_names + 16 * number, name, 16);
			break;
		case ARP:
			if (!arp_names)
			{
				arp_names = new unsigned char[16 * MAX_ARPS];
				memset(arp_names, ' ', 16 * MAX_ARPS);
			}
			if ((id != 0 && number >= MAX_ARPS) || (id == 0 && number >= arps))
			{
				pmesg("*** ROM::set_name out of bounds: rom: %d type %d, number %d\n", id, type, number);
				return 0;
			}
			if (id != 0)
			{
				++arps;
				// show this rom in arp selections
				if (number == 0)
				{
					const char* rom_name = ROM::name();
					const Fl_Menu_Item* item;
					item = ui->preset_editor->arp_rom->find_item(rom_name);
					const_cast<Fl_Menu_Item*>(item)->show();
					item = ui->main->arp_rom->find_item(rom_name);
					const_cast<Fl_Menu_Item*>(item)->show();
					item = ui->copy_arp_rom->find_item(rom_name);
					const_cast<Fl_Menu_Item*>(item)->show();
				}
			}
			memcpy(arp_names + 16 * number, name, 12);
			break;
		case RIFF:
			if (!riff_names)
				riff_names = new unsigned char[16 * MAX_RIFFS];
			if (number >= MAX_RIFFS)
			{
				pmesg("*** ROM::set_name out of bounds: rom: %d type %d, number %d\n", id, type, number);
				return 0;
			}
			++riffs;
			memcpy(riff_names + 16 * number, name, 16);
			break;
		default:
			pmesg("*** ROM::set_name unknown type: rom: %d type %d, number %d\n", id, type, number);
			pxk->display_status("ROM::set_name() Unknown ROM.");
			return 0;
	}
	return 1;
}

const unsigned char* ROM::get_name(int type, int number) const
{
	//pmesg("ROM::get_name(type: %d, #: %d)  \n", type, number);
	switch (type)
	{
		case INSTRUMENT:
			if (!instrument_names || number >= instruments)
			{
				pmesg("*** ROM::get_name out of bounds: rom: %d type INSTRUMENT, number %d\n", id, number);
				return 0;
			}
			return instrument_names + 16 * number;
		case PRESET:
			if (!preset_names || number >= presets)
			{
				pmesg("*** ROM::get_name out of bounds: rom: %d type PRESET, number %d\n", id, number);
				return 0;
			}
			return preset_names + 16 * number;
		case ARP:
			if (!arp_names || number >= arps)
			{
				pmesg("*** ROM::get_name out of bounds: rom: %d type ARP, number %d\n", id, number);
				return 0;
			}
			return arp_names + 16 * number;
		case RIFF:
			if (!riff_names || number >= riffs)
			{
				pmesg("*** ROM::get_name out of bounds: rom: %d type RIFF, number %d\n", id, number);
				return 0;
			}
			return riff_names + 16 * number;
		default:
			pmesg("*** ROM::get_name unknown type: rom: %d type %d, number %d\n", id, type, number);
			pxk->display_status("ROM::get_name() Unknown ROM.");
			return 0;
	}
}

int ROM::get_attribute(int type) const
{
	//pmesg("ROM::get_attribute(type: %d)  \n", type);
	switch (type)
	{
		case ID:
			return id;
		case INSTRUMENT:
			return instruments;
		case PRESET:
			return presets;
		case ARP:
			return arps;
		case RIFF:
			return riffs;
		default:
			return -1;
	}
}

const char* ROM::name() const
{
	switch (id)
	{
		case 0:
			return "User";
		case 2:
			return "XTREM";
		case 3:
			return "Audity";
		case 4:
			return "Composer";
		case 5:
			return "Protozoa";
		case 6:
			return "B3";
		case 7:
			return "XL-1";
		case 8:
			return "ZR-76";
		case 9:
			return "World Exp.";
		case 10:
			return "Orch 1";
		case 11:
			return "Orch 2";
		case 13:
		case 15:
			return "Mo'Phatt";
		case 14:
			return "XL-2";
		case 16:
			return "Ensoniq";
		case 17:
			return "PROM1";
		case 18:
			return "Vintage";
		case 19:
			return "DRUM";
		case 64:
			return "Holy Grail";
		case 65:
			return "TSCY";
		case 66:
			return "Siedlaczek";
		case 67:
			return "Beat";
		default:
			static char buf[20];
			snprintf(buf, 20, "Unknown (%d)", id);
			return buf;
	}
}
