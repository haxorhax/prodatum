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
#include "cfg.H"
#include "debug.H"

extern PD_UI* ui;
extern PXK* pxk;

// ms to wait between name requests on init and when a WAIT is received
unsigned char request_delay;
// colors used in ui and widgets
unsigned char colors[5];

Cfg::Cfg(const char* n)
{
	pmesg("Cfg::Cfg(%s)  \n", n);
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
	defaults[CFG_DEVICE_ID] = 127;
	defaults[CFG_AUTOCONNECT] = 1;
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
	defaults[CFG_BG] = 212; // 170 140
	defaults[CFG_BG2] = 50; // 5 215
	defaults[CFG_RR] = 126; // 82 68
	defaults[CFG_GG] = 132; // 92 74
	defaults[CFG_BB] = 142; // 87 77
	defaults[CFG_COLORED_BG] = 1;
	defaults[CFG_SHINY_KNOBS] = 0;
	defaults[CFG_LOG_SYSEX_OUT] = 0;
	defaults[CFG_LOG_SYSEX_IN] = 0;
	defaults[CFG_LOG_EVENTS_OUT] = 0;
	defaults[CFG_LOG_EVENTS_IN] = 0;
	defaults[CFG_WINDOW_WIDTH] = 843;
	defaults[CFG_WINDOW_HEIGHT] = 615;
	// load config
	config_name = 0;
	if (n != 0)
	{
		snprintf(_name, 64, "%s", n);
		config_name = _name;
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
		char config_path[PATH_MAX];
		snprintf(config_path, PATH_MAX, "%s/%s", config_dir, config_name);
		std::ifstream file(config_path);
		if (!file.is_open()) // new config
		{
			for (int i = 0; i < NOOPTION; i++)
				option[i] = defaults[i];
			return;
		}
		int check_file, check = 1;
		for (unsigned char i = 0; i < NOOPTION; i++)
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
			for (unsigned char i = 0; i < NOOPTION; i++)
				option[i] = defaults[i];
		}
		request_delay = option[CFG_SPEED] * 25 + 25;
	}
	else // no config
	{
		for (unsigned char i = 0; i < NOOPTION; i++)
			option[i] = defaults[i];
	}
}

Cfg::~Cfg()
{
	pmesg("Cfg::~Cfg()  \n");
	// save config
	if (config_name != 0)
	{
		char config_path[PATH_MAX];
		snprintf(config_path, PATH_MAX, "%s/%s", config_dir, config_name);
		std::ofstream file(config_path, std::ios::trunc);
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
		for (int i = 0; i < NOOPTION; i++)
		{
			file << option[i] << " ";
			check += option[i] * ((i % 5) + 1);
		}
		file << check << std::endl;
		file << export_dir << std::endl;
		file.close();
	}
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

const char* Cfg::get_config_name() const
{
	pmesg("Cfg::get_config_name()  \n");
	return config_name;
}

bool Cfg::set_export_dir(const char* dir)
{
	pmesg("Cfg::set_export_dir(%s)  \n", dir);
	struct stat sbuf;
	if (stat(dir, &sbuf) == -1)
	{
		fl_alert("Warning: Directory must exist.\nUsing previous directory.");
		return false;
	}
	else
	{
		char buf[PATH_MAX];
		snprintf(buf, PATH_MAX, "%s/___prodatum-filecheck___", dir);
		FILE *fp = fopen(buf, "w");
		if (fp == NULL)
		{
			if (errno == EACCES)
				fl_alert("Warning: You don't have write permission at %s.\nUsing previous directory.", dir);
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

void Cfg::apply()
{
	pmesg("Cfg::apply()\n");
	ui->syncview = option[CFG_SYNCVIEW];
	// UI INIT
	// midi options
	ui->device_id->value(option[CFG_DEVICE_ID]);
	ui->r_user_id->value(option[CFG_DEVICE_ID]);
	option[CFG_AUTOCONNECT] ? ui->autoconnect->set() : ui->autoconnect->clear();
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
}

void Cfg::set_color(int type, int value)
{
	pmesg("Cfg::set_color(%d, %d)\n", type, value);
	switch (type)
	{
		case CFG_BG:
		case CFG_BG2:
		case CFG_RR:
		case CFG_GG:
		case CFG_BB:
			option[type] = value;
			colors[type - CFG_BG] = value; // Knobs
			break;
		case CFG_SHINY_KNOBS:
			if (value == 1)
				ui->shiny_knobs = true;
			else
				ui->shiny_knobs = false;
			option[CFG_SHINY_KNOBS] = value;
			break;
		case DEFAULT:
			ui->c_bg->value(getset_default(CFG_BG));
			ui->c_bg2->value(getset_default(CFG_BG2));
			ui->c_rr->value(getset_default(CFG_RR));
			ui->c_gg->value(getset_default(CFG_GG));
			ui->c_bb->value(getset_default(CFG_BB));
			ui->c_cbg->value(getset_default(CFG_COLORED_BG));
			(getset_default(CFG_SHINY_KNOBS)) ? ui->shiny_knobs = true : ui->shiny_knobs = false;
			ui->c_sk->value(option[CFG_SHINY_KNOBS]);
			break;
		case CURRENT:
			ui->c_bg->value(option[CFG_BG]);
			ui->c_bg2->value(option[CFG_BG2]);
			ui->c_rr->value(option[CFG_RR]);
			ui->c_gg->value(option[CFG_GG]);
			ui->c_bb->value(option[CFG_BB]);
			ui->c_cbg->value(option[CFG_COLORED_BG]);
			(option[CFG_SHINY_KNOBS]) ? ui->shiny_knobs = true : ui->shiny_knobs = false;
			ui->c_sk->value(option[CFG_SHINY_KNOBS]);
			break;
		default:
			break;
	}
	Fl::set_color(FL_BACKGROUND2_COLOR, option[CFG_BG2], option[CFG_BG2], option[CFG_BG2]);
	if (!option[CFG_COLORED_BG])
	{
		Fl::set_color(FL_BACKGROUND_COLOR, option[CFG_BG], option[CFG_BG], option[CFG_BG]);
		Fl::set_color(FL_SELECTION_COLOR, option[CFG_RR], option[CFG_GG], option[CFG_BB]);
		Fl::set_color(FL_INACTIVE_COLOR, fl_color_average(FL_BACKGROUND_COLOR, FL_WHITE, .7f));
	}
	else // colored background
	{
		Fl::set_color(FL_BACKGROUND_COLOR, option[CFG_RR], option[CFG_GG], option[CFG_BB]);
		Fl::set_color(FL_SELECTION_COLOR, option[CFG_BG], option[CFG_BG], option[CFG_BG]);
		int luma = (option[CFG_RR] + option[CFG_RR] + option[CFG_BB] + option[CFG_GG] + option[CFG_GG] + option[CFG_GG])
				/ 6;
		if (luma > 128)
			Fl::set_color(FL_INACTIVE_COLOR, fl_color_average(FL_BACKGROUND_COLOR, FL_BLACK, .75f));
		else
			Fl::set_color(FL_INACTIVE_COLOR, fl_color_average(FL_BACKGROUND_COLOR, FL_WHITE, .75f));
	}
	Fl::set_color(FL_FOREGROUND_COLOR, option[CFG_BG2], option[CFG_BG2], option[CFG_BG2]);
	Fl_Tooltip::textcolor(FL_BACKGROUND_COLOR);
	Fl_Tooltip::color(FL_BACKGROUND2_COLOR);
	Fl::reload_scheme();
	// update highlight buttons
	if (pxk && pxk->preset)
		pxk->preset->update_highlight_buttons();
	if (pxk && pxk->setup)
		pxk->setup->update_highlight_buttons();
}
