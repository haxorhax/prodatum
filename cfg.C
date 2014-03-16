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

#ifdef WIN32
#include <stdio.h>
#else
#include <unistd.h>
#endif

#include <string.h>
#include <errno.h>
#include <fstream>
#include <sys/stat.h>
#include <FL/fl_ask.H>
#include <FL/Fl_Tooltip.H>
#include <stdlib.h>

#include "ui.H"

extern PD_UI* ui;
extern PXK* pxk;

Cfg::Cfg(int device_id)
{
	pmesg("Cfg::Cfg(%d)  \n", device_id);
	// default export dir
#ifdef WIN32
	set_export_dir(getenv("USERPROFILE"));
#else
	set_export_dir(getenv("HOME"));
#endif
	// check for portable config dir
	struct stat sbuf;
	config_dir[0] = 0;
	if (stat("./prodatum-config", &sbuf) == 0)
	{
		// check write permissions
		FILE *fp = fopen("./prodatum-config/.___prdtmchck", "w");
		if (fp == NULL)
		{
			if (errno == EACCES)
				fl_alert("You don't have write permission at ./prodatum-config/.");
		}
		else
		{
			fclose(fp);
#ifdef WIN32
			_unlink("./prodatum-config/.___prdtmchck");
#else
			unlink("./prodatum-config/.___prdtmchck");
#endif
			snprintf(config_dir, PATH_MAX, "./prodatum-config");
			ui->pref_info_portable->show();
			ui->open_info_portable->show();
		}
	}
	if (config_dir[0] == 0)
	{
		ui->pref_info_portable->hide();
		ui->open_info_portable->hide();
#ifdef WIN32
		snprintf(config_dir, PATH_MAX, "%s/prodatum", getenv("APPDATA"));
#else
		snprintf(config_dir, PATH_MAX, "%s/.prodatum", export_dir);
#endif
		// check/create cfg dir
		if (stat(config_dir, &sbuf) == -1)
		{
			if (mkdir(config_dir, S_IRWXU| S_IRWXG | S_IROTH | S_IXOTH) == -1)
			{
				fl_alert("Could not create configuration directory:\n%s - %s\n", config_dir, strerror(errno));
				fprintf(stderr, "Could not create configuration directory:\n%s - %s\n", config_dir, strerror(errno));
#ifdef WIN32
				fflush(stderr);
#endif
			}
		}
	}
	// defaults
	defaults.resize(NOOPTION, 0);
	option.resize(NOOPTION, 0);
	defaults[CFG_MIDI_OUT] = -1;
	defaults[CFG_MIDI_IN] = -1;
	defaults[CFG_MIDI_THRU] = -1;
	defaults[CFG_CONTROL_CHANNEL] = 0;
	defaults[CFG_AUTOMAP] = 1;
	defaults[CFG_DEVICE_ID] = device_id;
	defaults[CFG_MASTER_VOLUME] = 80;
	defaults[CFG_SPEED] = -1;
	defaults[CFG_CLOSED_LOOP_UPLOAD] = 1;
	defaults[CFG_CLOSED_LOOP_DOWNLOAD] = 1;
	defaults[CFG_TOOLTIPS] = 1;
	defaults[CFG_KNOBMODE] = 1;
	defaults[CFG_CONFIRM_EXIT] = 0;
	defaults[CFG_CONFIRM_RAND] = 1;
	defaults[CFG_CONFIRM_DISMISS] = 1;
	defaults[CFG_SYNCVIEW] = 0;
	defaults[CFG_DRLS] = 1;
	defaults[CFG_LOG_SYSEX_OUT] = 1;
	defaults[CFG_LOG_SYSEX_IN] = 1;
	defaults[CFG_LOG_EVENTS_OUT] = 0;
	defaults[CFG_LOG_EVENTS_IN] = 0;
	defaults[CFG_WINDOW_WIDTH] = 845;
	defaults[CFG_WINDOW_HEIGHT] = 620;
	defaults[CFG_BGR] = 108;
	defaults[CFG_BGG] = 118;
	defaults[CFG_BGB] = 121;
	defaults[CFG_BG2R] = 45;
	defaults[CFG_BG2G] = 50;
	defaults[CFG_BG2B] = 51;
	defaults[CFG_FGR] = 204;
	defaults[CFG_FGG] = 204;
	defaults[CFG_FGB] = 194;
	defaults[CFG_SLR] = 204;
	defaults[CFG_SLG] = 204;
	defaults[CFG_SLB] = 194;
	defaults[CFG_INR] = 204;
	defaults[CFG_ING] = 204;
	defaults[CFG_INB] = 194;
	defaults[CFG_KNOB_COLOR1] = -1;
	defaults[CFG_KNOB_COLOR2] = -1;

	// load config
	char _fname[PATH_MAX];
	int sysex_id = device_id;
	std::ifstream file;
	if (sysex_id == -1)
	{
		snprintf(_fname, PATH_MAX, "%s/default.cfg", config_dir); // read default.cfg
		file.open(_fname);
		if (file.is_open())
		{
			file >> sysex_id;
			file.close();
		}
	}
	snprintf(_fname, PATH_MAX, "%s/%d.cfg", config_dir, sysex_id); // load actual config
	file.open(_fname);
	unsigned char i;
	if (!file.is_open()) // new config
	{
		for (i = 0; i < NOOPTION; i++)
			option[i] = defaults[i];
		return;
	}
	int check_file, check = 1;
	for (i = 0; i < NOOPTION; i++)
	{
		file >> option[i];
		check += option[i] * ((i % 5) + 1);
	}
	// checksum
	file >> check_file;
	// get export directory
	char buf[PATH_MAX];
	file.getline(0, 0);
	file.getline(buf, PATH_MAX);
	if (!file.fail())
		set_export_dir(buf);
	file.close();
	if (check_file != check)
	{
		fl_message("Configuration format changed, using default values.\n"
				"Sorry for the inconvenience.");
#ifdef WIN32
		_unlink(_fname);
#else
		unlink(_fname);
#endif
		for (i = 0; i < NOOPTION; i++)
			option[i] = defaults[i];
	}
}

Cfg::~Cfg()
{
	pmesg("Cfg::~Cfg()  \n");
	if (!pxk->Synchronized())
		return;
	// save default
	char _file[PATH_MAX];
	snprintf(_file, PATH_MAX, "%s/default.cfg", config_dir);
	std::ofstream file;
	file.open(_file, std::ios::trunc);
	if (!file.is_open())
	{
		fl_alert("Warning:\nCould not write the config file.");
		fprintf(stderr, "Warning:\nCould not write the config file.");
#ifdef WIN32
		fflush(stderr);
#endif
		return;
	}
	file << option[CFG_DEVICE_ID] << " ";
	file.close();
	// save actual config
	snprintf(_file, PATH_MAX, "%s/%d.cfg", config_dir, option[CFG_DEVICE_ID]);
	file.open(_file, std::ios::trunc);
	if (!file.is_open())
	{
		fl_alert("Warning:\nCould not write the config file.");
		fprintf(stderr, "Warning:\nCould not write the config file.");
#ifdef WIN32
		fflush(stderr);
#endif
		return;
	}
	// calc checksum
	int check = 1;
	for (unsigned char i = 0; i < NOOPTION; i++)
	{
		file << option[i] << " ";
		check += option[i] * ((i % 5) + 1);
	}
	file << check << std::endl;
	file << export_dir << std::endl;
	file.close();
}

void Cfg::set_cfg_option(int opt, int value)
{
	//pmesg("Cfg::set_cfg_option(%d, %d)  \n", opt, value);
	if (opt < NOOPTION && opt >= 0)
		option[opt] = value;
}

int Cfg::get_cfg_option(int opt) const
{
	//pmesg("Cfg::get_cfg_option(%d)  \n", opt);
	if (opt < NOOPTION && opt >= 0)
	{
		if (opt == CFG_SPEED)
			return option[CFG_SPEED] * option[CFG_SPEED] * 10;
		return option[opt];
	}
	return 0;
}

int Cfg::get_default(int opt) const
{
	if (opt < NOOPTION && opt >= 0)
		return defaults[opt];
	return 0;
}

int Cfg::getset_default(int opt)
{
	//pmesg("Cfg::getset_default(%d)  \n", opt);
	if (opt < NOOPTION && opt >= 0)
	{
		option[opt] = defaults[opt];
		return defaults[opt];
	}
	return 0;
}

const char* Cfg::get_config_dir() const
{
	//pmesg("Cfg::get_config_dir()  \n");
	return config_dir;
}

const char* Cfg::get_export_dir() const
{
	//pmesg("Cfg::get_export_dir()  \n");
	return export_dir;
}

bool Cfg::set_export_dir(const char* dir)
{
	//pmesg("Cfg::set_export_dir(%s)\n", dir);
	struct stat sbuf;
	if (stat(dir, &sbuf) == -1)
	{
		fl_alert("Directory must exist.");
		return false;
	}
	else
	{
		char buf[PATH_MAX];
		snprintf(buf, PATH_MAX, "%s/.___prdtmchck", dir);
		FILE *fp = fopen(buf, "w");
		if (fp == NULL)
		{
			if (errno == EACCES)
				fl_alert("You don't have write permission at %s.", dir);
			return false;
		}
		else
		{
			fclose(fp);
#ifdef WIN32
			_unlink(buf);
#else
			unlink(buf);
#endif
		}
	}
	snprintf(export_dir, PATH_MAX, "%s", dir);
	return true;
}

void Cfg::apply(bool colors_only)
{
	//pmesg("Cfg::apply()\n");
	ui->set_color(FL_BACKGROUND_COLOR, (unsigned char) option[CFG_BGR], (unsigned char) option[CFG_BGG],
			(unsigned char) option[CFG_BGB]);
	ui->set_color(FL_BACKGROUND2_COLOR, (unsigned char) option[CFG_BG2R], (unsigned char) option[CFG_BG2G],
			(unsigned char) option[CFG_BG2B]);
	ui->set_color(FL_FOREGROUND_COLOR, (unsigned char) option[CFG_FGR], (unsigned char) option[CFG_FGG],
			(unsigned char) option[CFG_FGB]);
	ui->set_color(FL_SELECTION_COLOR, (unsigned char) option[CFG_SLR], (unsigned char) option[CFG_SLG],
			(unsigned char) option[CFG_SLB]);
	ui->set_color(FL_INACTIVE_COLOR, (unsigned char) option[CFG_INR], (unsigned char) option[CFG_ING],
			(unsigned char) option[CFG_INB]);
	ui->set_knobcolor(0, (char) option[CFG_KNOB_COLOR1]);
	ui->set_knobcolor(1, (char) option[CFG_KNOB_COLOR2]);
	if (colors_only)
		return;
	ui->syncview = option[CFG_SYNCVIEW];
	// UI INIT
	// midi options
	if (option[CFG_DEVICE_ID] != -1)
	{
		ui->device_id->value(option[CFG_DEVICE_ID]);
		ui->r_user_id->value(option[CFG_DEVICE_ID]);
	}
	ui->main->master_volume->value(option[CFG_MASTER_VOLUME]);
	ui->midi_ctrl_ch->value(option[CFG_CONTROL_CHANNEL]);
	ui->midi_automap->value(option[CFG_AUTOMAP]);
	if (option[CFG_SPEED] != -1)
		ui->speed->value(option[CFG_SPEED]);
	ui->confirm->value(option[CFG_CONFIRM_EXIT]);
	ui->confirm_rand->value(option[CFG_CONFIRM_RAND]);
	ui->confirm_dismiss->value(option[CFG_CONFIRM_DISMISS]);
	((Fl_Button*) ui->g_knobmode->child(option[CFG_KNOBMODE]))->setonly();
	option[CFG_CLOSED_LOOP_UPLOAD] ? ui->closed_loop_upload->set() : ui->closed_loop_upload->clear();
	option[CFG_CLOSED_LOOP_DOWNLOAD] ? ui->closed_loop_download->set() : ui->closed_loop_download->clear();
	ui->export_dir->value(get_export_dir());
	// UI misc
	if (option[CFG_TOOLTIPS])
	{
		ui->tooltips->set();
		Fl_Tooltip::enable();
	}
	else
	{
		ui->tooltips->clear();
		Fl_Tooltip::disable();
	}
	if (option[CFG_DRLS])
	{
		ui->drls->value(1);
		ui->value_input->when(FL_WHEN_ENTER_KEY);
	}
	else
	{
		ui->drls->value(0);
		ui->value_input->when(FL_WHEN_CHANGED);
	}
	// log
	ui->log_sysex_out->value(option[CFG_LOG_SYSEX_OUT]);
	ui->log_sysex_in->value(option[CFG_LOG_SYSEX_IN]);
	ui->log_events_out->value(option[CFG_LOG_EVENTS_OUT]);
	ui->log_events_in->value(option[CFG_LOG_EVENTS_IN]);
	ui->main_window->resize(ui->main_window->x(), ui->main_window->y(), option[CFG_WINDOW_WIDTH],
			option[CFG_WINDOW_HEIGHT]);
}
