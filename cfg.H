// $Id$
#ifndef CFG_H_
#define CFG_H_
/**
 \defgroup pd_cfg prodatum Configurations
 @{
 */
#ifdef WIN32
#	include <direct.h>
#	define mkdir(x,m) _mkdir(x)
#else
#	define mkdir(x,m) mkdir(x,m)
#endif

#include "config.h"
#define MAX_ARPS 300
#define MAX_RIFFS 1000
#include <vector>

/**
 * Enum of config options
 */
enum CONFIG
{
	CFG_MIDI_OUT,
	CFG_MIDI_IN,
	CFG_MIDI_THRU,
	CFG_CONTROL_CHANNEL,
	CFG_AUTOMAP,
	CFG_DEVICE_ID,
	CFG_AUTOCONNECT,
	CFG_SPEED,
	CFG_CLOSED_LOOP_UPLOAD,
	CFG_CLOSED_LOOP_DOWNLOAD,
	CFG_TOOLTIPS,
	CFG_KNOBMODE,
	CFG_CONFIRM_EXIT,
	CFG_CONFIRM_RAND,
	CFG_CONFIRM_DISMISS,
	CFG_SYNCVIEW,
	CFG_DRLS,
	CFG_BG,
	CFG_BG2,
	CFG_RR,
	CFG_GG,
	CFG_BB,
	CFG_COLORED_BG,
	CFG_SHINY_KNOBS,
	CFG_LOG_SYSEX_OUT,
	CFG_LOG_SYSEX_IN,
	CFG_LOG_EVENTS_OUT,
	CFG_LOG_EVENTS_IN,
	CFG_WINDOW_WIDTH,
	CFG_WINDOW_HEIGHT,
	NOOPTION
};

/**
 * Enum of colors
 */
enum
{
	BG, BG2, RR, GG, BB, DEFAULT, CURRENT
};

/**
 * Configuration class.
 * loads, saves and manages all configuration options
 */
class Cfg
{
	/// config name
	char config_name[64];
	/// configuration directory
	char config_dir[PATH_MAX];
	/// path to export directory
	char export_dir[PATH_MAX];
	std::vector<int> defaults;
	std::vector<int> option;

public:
	/**
	 * CTOR parses config file
	 */
	Cfg(const char* n, char ac);
	/**
	 * DTOR saves config file
	 */
	~Cfg();
	/// returns the config directory path
	const char* get_config_dir() const;
	const char* get_export_dir() const;
	bool set_export_dir(const char* dir);
	/**
	 * updates a configuration value
	 * @param option the parameter to update
	 * @param value the new value for the parameter
	 */
	void set_cfg_option(int option, int value);
	/**
	 * get a configuration option
	 * @param option parameter to get
	 * @return parameter value
	 */
	int get_cfg_option(int option) const;
	int getset_default(int option);
	void apply();
	void set_color(int, int);
};

#endif /* CFG_H_ */
/** @} */
