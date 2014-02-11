//    This file is part of prodatum.
//    Copyright 2011 Jan Eidtmann
//
//    prodatum is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    prodatum is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with prodatum.  If not, see <http://www.gnu.org/licenses/>.

// $Id$

#include <string.h>
#include <errno.h>
#include <fstream>
#include <sys/stat.h>
#include <FL/fl_ask.H>
#include <stdlib.h>
#include "config.h"
#include "cfg.H"
#include "debug.H"

// ms to wait between name requests on init and when a WAIT is received
int request_delay;
// colors used in ui and widgets
unsigned char colors[5];

Cfg::Cfg(const char* n, int ac)
{
	pmesg("Cfg::Cfg(%s, %d)  \n", n, ac);
	// defaults
#ifdef WIN32
	set_export_dir(getenv("USERPROFILE"));
	snprintf(config_dir, PATH_MAX, "%s/prodatum", getenv("APPDATA"));
#else
	set_export_dir(getenv("HOME"));
	snprintf(config_dir, PATH_MAX, "%s/.prodatum", export_dir);
#endif
	snprintf(config_name, 32, "%s", n);
	defaults.resize(NOOPTION, 0);
	defaults[CFG_MIDI_OUT] = -1;
	defaults[CFG_MIDI_IN] = -1;
	defaults[CFG_MIDI_THRU] = -1;
	defaults[CFG_CONTROL_CHANNEL] = 0;
	defaults[CFG_AUTOMAP] = 1;
	defaults[CFG_DEVICE_ID] = 0;
	defaults[CFG_AUTOCONNECT] = ac;
	defaults[CFG_SPEED] = 0;
	defaults[CFG_CLOSED_LOOP_UPLOAD] = 0;
	defaults[CFG_CLOSED_LOOP_DOWNLOAD] = 0;
	defaults[CFG_TOOLTIPS] = 1;
	defaults[CFG_KNOBMODE] = 1;
	defaults[CFG_CONFIRM_EXIT] = 0;
	defaults[CFG_CONFIRM_RAND] = 1;
	defaults[CFG_CONFIRM_DISMISS] = 1;
	defaults[CFG_SYNCVIEW] = 0;
	defaults[CFG_DRLS] = 1;
	defaults[CFG_BG] = 170; // 170 140
	defaults[CFG_BG2] = 15; // 5 215
	defaults[CFG_RR] = 92; // 82 68
	defaults[CFG_GG] = 102; // 92 74
	defaults[CFG_BB] = 97; // 87 77
	defaults[CFG_COLORED_BG] = 1;
	defaults[CFG_SHINY_KNOBS] = 0;
	defaults[CFG_LOG_SYSEX_OUT] = 0;
	defaults[CFG_LOG_SYSEX_IN] = 0;
	defaults[CFG_LOG_EVENTS_OUT] = 0;
	defaults[CFG_LOG_EVENTS_IN] = 0;
	defaults[CFG_WINDOW_WIDTH] = 843;
	defaults[CFG_WINDOW_HEIGHT] = 615;
	// load config
	struct stat sbuf;
	if (stat(config_dir, &sbuf) == -1)
	{

		if (mkdir(config_dir, S_IRWXU| S_IRWXG | S_IROTH | S_IXOTH) == -1)
		{
			char message[256];
			snprintf(message, 256, "Could not create configuration directory:\n%s - %s\n", config_dir, strerror(errno));
			pmesg(message);
			fl_alert("%s", message);
			throw 1;
		}
	}
	// load config
	option.resize(NOOPTION, 0);
	char config_path[PATH_MAX];
	snprintf(config_path, PATH_MAX, "%s/%s", config_dir, config_name);
	std::ifstream file(config_path);
	if (!file.is_open())
	{
		//ui->message("Could not load the config,\nusing defaults.");
		for (int i = 0; i < NOOPTION; i++)
			option[i] = defaults[i];
		return;
	}
	int check_file, check = 1;
	for (int i = 0; i < NOOPTION; i++)
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
		fl_message("Configuration updated, using default values.\n"
				"Sorry for the inconvenience! I've opted for a\n"
				"brainless but uber-fast configuration parser.");
		for (int i = 0; i < NOOPTION; i++)
			option[i] = defaults[i];
	}
	request_delay = option[CFG_SPEED] * 25 + 25;
}

Cfg::~Cfg()
{
	pmesg("Cfg::~Cfg()  \n");
	// save config
	char config_path[PATH_MAX];
	snprintf(config_path, PATH_MAX, "%s/%s", config_dir, config_name);
	std::ofstream file(config_path, std::ios::trunc);
	if (!file.is_open())
	{
		fl_message("Warning:\nCould not write the config file.");
		return;
	}
	// calc checksum
	int check = 1;
	for (int i = 0; i < NOOPTION; i++)
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
	pmesg("Cfg::set_cfg_option(%d, %d)  \n", opt, value);
	if (opt < NOOPTION && opt >= 0)
		option[opt] = value;
	if (opt == CFG_SPEED)
		request_delay = value * 25 + 25;
}

int Cfg::get_cfg_option(int opt) const
{
	pmesg("Cfg::get_cfg_option(%d)  \n", opt);
	if (opt < NOOPTION && opt >= 0)
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

void Cfg::set_export_dir(const char* dir)
{
	pmesg("Cfg::set_export_dir(%s)  \n", dir);
	// add trailing slash
	if (dir[strlen(dir) - 1] != '/' && dir[strlen(dir) - 1] != '\\')
		snprintf(export_dir, PATH_MAX, "%s/", dir);
	else
		snprintf(export_dir, PATH_MAX, "%s", dir);
}
