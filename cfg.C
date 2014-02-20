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
	// defaults
#ifdef WIN32
	set_export_dir(getenv("USERPROFILE"));
	snprintf(config_dir, PATH_MAX, "%s/prodatum", getenv("APPDATA"));
#else
	set_export_dir(getenv("HOME"));
	snprintf(config_dir, PATH_MAX, "%s/.prodatum", export_dir);
#endif
	defaults.resize(NOOPTION, 0);
	option.resize(NOOPTION, 0);
	defaults[CFG_MIDI_OUT] = -1;
	defaults[CFG_MIDI_IN] = -1;
	defaults[CFG_MIDI_THRU] = -1;
	defaults[CFG_CONTROL_CHANNEL] = 0;
	defaults[CFG_AUTOMAP] = 1;
	defaults[CFG_DEVICE_ID] = device_id;
	defaults[CFG_SPEED] = 1;
	defaults[CFG_CLOSED_LOOP_UPLOAD] = 0;
	defaults[CFG_CLOSED_LOOP_DOWNLOAD] = 0;
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
	defaults[CFG_WINDOW_WIDTH] = 843;
	defaults[CFG_WINDOW_HEIGHT] = 615;
	defaults[CFG_BGR] = 129;
	defaults[CFG_BGG] = 132;
	defaults[CFG_BGB] = 149;
	defaults[CFG_BG2R] = 35;
	defaults[CFG_BG2G] = 42;
	defaults[CFG_BG2B] = 59;
	defaults[CFG_FGR] = 220;
	defaults[CFG_FGG] = 238;
	defaults[CFG_FGB] = 235;
	defaults[CFG_SLR] = 164;
	defaults[CFG_SLG] = 182;
	defaults[CFG_SLB] = 147;
	defaults[CFG_INR] = 96;
	defaults[CFG_ING] = 101;
	defaults[CFG_INB] = 91;
	// check/create cfg dir
	struct stat sbuf;
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
	// load config
	char _fname[PATH_MAX];
	if (device_id == -1)
	{
		snprintf(_fname, 64, "%s/default.cfg", config_dir); // read default.cfg
		std::ifstream file(_fname);
		if (file.is_open())
		{
			file >> device_id;
			file.close();
		}
	}
	snprintf(_fname, 64, "%s/%d.cfg", config_dir, device_id); // load actual config
	std::ifstream file(_fname);
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
	std::ofstream defaults(_file, std::ios::trunc);
	if (!defaults.is_open())
	{
		fl_alert("Warning:\nCould not write the config file.");
		fprintf(stderr, "Warning:\nCould not write the config file.");
#ifdef WIN32
		fflush(stderr);
#endif
		return;
	}
	defaults << option[CFG_DEVICE_ID] << " ";
	defaults.close();
	// save actual config
	snprintf(_file, PATH_MAX, "%s/%d.cfg", config_dir, option[CFG_DEVICE_ID]);
	std::ofstream config(_file, std::ios::trunc);
	if (!config.is_open())
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
		config << option[i] << " ";
		check += option[i] * ((i % 5) + 1);
	}
	config << check << std::endl;
	config << export_dir << std::endl;
	config.close();
}

void Cfg::set_cfg_option(int opt, int value)
{
	pmesg("Cfg::set_cfg_option(%d, %d)  \n", opt, value);
	if (opt < NOOPTION && opt >= 0)
		option[opt] = value;
}

int Cfg::get_cfg_option(int opt) const
{
	pmesg("Cfg::get_cfg_option(%d)  \n", opt);
	if (opt < NOOPTION && opt >= 0)
		if (opt == CFG_SPEED)
			return option[CFG_SPEED] * option[CFG_SPEED] * 10;
		return option[opt];
	return 0;
}

int Cfg::getset_default(int opt)
{
	pmesg("Cfg::getset_default(%d)  \n", opt);
	if (opt < NOOPTION && opt >= 0)
	{
		option[opt] = defaults[opt];
		return defaults[opt];
	}
	return 0;
}

const char* Cfg::get_config_dir() const
{
	pmesg("Cfg::get_config_dir()  \n");
	return config_dir;
}

const char* Cfg::get_export_dir() const
{
	pmesg("Cfg::get_export_dir()  \n");
	return export_dir;
}

bool Cfg::set_export_dir(const char* dir)
{
	//pmesg("Cfg::set_export_dir(%s)\n", dir);
	struct stat sbuf;
	if (stat(dir, &sbuf) == -1)
	{
		fl_alert("Directory must exist.\nUsing previous directory.");
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
				fl_alert("You don't have write permission at %s.\nUsing previous directory.", dir);
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
	pmesg("Cfg::apply()\n");
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
	if (colors_only)
		return;
	ui->syncview = option[CFG_SYNCVIEW];
	// UI INIT
	// midi options
	ui->device_id->value(option[CFG_DEVICE_ID]);
	ui->r_user_id->value(option[CFG_DEVICE_ID]);
	ui->midi_ctrl_ch->value(option[CFG_CONTROL_CHANNEL]);
	ui->midi_automap->value(option[CFG_AUTOMAP]);
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
	ui->main_window->size(option[CFG_WINDOW_WIDTH], option[CFG_WINDOW_HEIGHT]);
	Fl::wait();
}
