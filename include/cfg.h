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
#define MAX_ARPS 400
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
	CFG_MASTER_VOLUME,
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
	CFG_LOG_SYSEX_OUT,
	CFG_LOG_SYSEX_IN,
	CFG_LOG_EVENTS_OUT,
	CFG_LOG_EVENTS_IN,
	CFG_WINDOW_WIDTH,
	CFG_WINDOW_HEIGHT,
	CFG_BGR,
	CFG_BGG,
	CFG_BGB,
	CFG_BG2R,
	CFG_BG2G,
	CFG_BG2B,
	CFG_FGR,
	CFG_FGG,
	CFG_FGB,
	CFG_SLR,
	CFG_SLG,
	CFG_SLB,
	CFG_INR,
	CFG_ING,
	CFG_INB,
	CFG_KNOB_COLOR1,
	CFG_KNOB_COLOR2,
	NOOPTION
};

/**
 * Configuration class.
 * loads, saves and manages all configuration options
 */
class Cfg
{
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
	Cfg(int id = -1);
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
	int get_cfg_option(int) const;
	int get_default(int) const;
	int getset_default(int);
	void apply(bool colors_only = false);
};

#endif /* CFG_H_ */
/** @} */
