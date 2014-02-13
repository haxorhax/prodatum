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

#include <string>
#include "pxk.H"
#include "cfg.H"

extern PD_UI* ui;
MIDI* midi = 0;
Cfg* cfg = 0;

// to open another device:
// delete PXK, new PXK :)
PXK::PXK(const char* file, char auto_c)
{
	// initialize
	midi = 0;
	device_id = -1;
	device_code = -1;
	synchronized = false;
	preset = 0;
	preset_copy = 0;
	setup = 0;
	setup_copy = 0;
	setup_names = 0;
	arp = 0;
	for (unsigned char i = 0; i <= 5; i++)
		rom[i] = 0;
	// populate ports && load config
	if (LoadConfig(file) && PopulatePorts())
	{
		if (auto_c == 0)
		{
			// show open_device
			ui->open_device->show();
			Fl::wait(.1);
			return;
		}
		//Synchronize(cfg->get_cfg_option(CFG_DEVICE_ID));
	}
}

PXK::~PXK()
{
	unsigned char i;
	for (i = 0; i < 4; i++)
		// unmute eventually muted voices
		;//mute(0, i); // TODO
	if (preset)
		delete preset;
	if (preset_copy)
		delete preset_copy;
	for (i = 0; i <= roms; i++)
		delete rom[i];
	if (setup)
		delete setup;
	if (setup_copy)
		delete setup_copy;
	if (setup_names)
		delete[] setup_names;
	if (cfg)
	{
		delete cfg;
		cfg = 0;
	}
	if (midi)
	{
		delete midi;
		midi = 0;
	}
}

bool PXK::LoadConfig(const char* name) const
{
	pmesg("PXK::LoadConfig()\n");
	if (cfg)
	{
		delete cfg;
		cfg = 0;
	}
	cfg = new Cfg(name, ui->autoconnect->value());
	if (!cfg)
		return false;
	cfg->apply();
	cfg->set_color(CURRENT, 0);
	return true;
}

bool PXK::PopulatePorts() const
{
	pmesg("PXK::PopulatePorts()\n");
	if (midi)
	{
		delete midi;
		midi = 0;
	}
	midi = new MIDI();
	if (!midi)
		return false;
	return true;
}

/* if autoconnection is enabled but the device is not powered
 we end up with an unconnected main window. this shows
 the open dialog if there is no answer*/
static void check_connection(void* p)
{
	if (((PXK*) p)->device_code <= 0 && !ui->init->shown())
		ui->open_device->show();
}

void PXK::Synchronize(unsigned char id)
{
	midi->request_device_inquiry(id);
	Fl::add_timeout(.5, check_connection, (void*) this);
}

void PXK::r_sysex(const unsigned char* sysex, const int len) const
{
	// log sysex messages
	static unsigned int count = 0;
	if (ui->log_sysex_in->value())
	{
		char* buf = new char[2 * len + 18];
		int n = snprintf(buf, 18, "\nIS.%lu::", ++count);
		for (int i = 0; i < len; i++)
			sprintf(n + buf + 2 * i, "%02X", sysex[i]);
		ui->logbuf->append(buf);
		delete[] buf;
	}
}

void PXK::incoming_inquiry_data(const unsigned char* data, int len)
{
	Fl::remove_timeout(check_connection);
	device_code = data[7] * 128 + data[6];
	if (device_code == 516) // talking to an PXK!
	{
		device_id = data[2];
		midi->set_device_id(device_id); // so further sysex comes through
		midi->edit_parameter_value(405, request_delay);
		midi->request_hardware_config();
		snprintf(os_rev, 5, "%c%c%c%c", data[10], data[11], data[12], data[13]);
		member_code = data[9] * 128 + data[8];
		switch (member_code)
		{
			case 2: // AUDITY
				ui->main->b_audit->hide();
				ui->m_audit->hide();
				ui->main->g_riff->deactivate();
				ui->preset_editor->g_riff->deactivate();
				ui->main->post_d->hide();
				ui->main->pre_d->label("Delay");
				ui->preset_editor->post_d->hide();
				ui->preset_editor->pre_d->label("Delay");
				break;
			default:
				ui->main->b_audit->show();
				ui->m_audit->show();
				ui->main->g_riff->activate();
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
void PXK::incoming_hardware_config(const unsigned char* data, int len)
{
	user_presets = data[8] * 128 + data[7];
	roms = data[9];
	rom_id_map.clear();
	rom_id_map[0] = 0;
	rom[0] = new ROM(0, user_presets);
	for (unsigned char j = 1; j <= roms; j++)
	{
		int idx = 11 + (j - 1) * 6;
		rom[j] = new ROM(data[idx + 1] * 128 + data[idx], data[idx + 3] * 128 + data[idx + 2],
				data[idx + 5] * 128 + data[idx + 4]);
		rom_id_map[data[idx + 1] * 128 + data[idx]] = j;
	}
	create_device_info();
	if (roms != 0)
	{
		ui->open_device->hide();
		midi->request_setup_dump();
	}
}
void PXK::create_device_info()
{
	pmesg("PXK::create_device_info()\n");
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
			snprintf(buf, 512, "\n - %s: %d sam, %d prg", rom[i]->name(), rom[i]->get_attribute(INSTRUMENT),
					rom[i]->get_attribute(PRESET));
			info += buf;
		}
	}
	ui->device_info->copy_label(info.data());
}
const char* PXK::get_name(int code) const
{
	pmesg("PXK::get_name(code: %d)\n", code);
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
