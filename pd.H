// $Id$
#ifndef PD_H_
#define PD_H_
/**
    \defgroup pd_pd prodatum CPU
    @{
*/
#include <sys/time.h>
#include <map>
#include <vector>
#include <string>

#include "data.H"

/**
 * Enum for the various copy commands of the device
 */
enum
{
	C_PRESET = 0x20,
	C_PRESET_COMMON,
	C_ARP,
	C_FX,
	C_PRESET_LINK,
	C_LAYER,
	C_LAYER_COMMON,
	C_LAYER_FILTER,
	C_LAYER_LFO,
	C_LAYER_ENVELOPE,
	C_LAYER_PATCHCORD,
	C_ARP_PATTERN,
	C_SETUP,
	SAVE_PRESET
};

/**
 * Enum for the three MIDI modes
 */
enum
{
	OMNI, POLY, MULTI
};

enum
{
	AUDITY = 2
};

/**
 * prodatum's control class.
 * Evaluates incoming sysex messages and holds data objects like preset and
 * setup dumps and ROM informations.
 * @see ROM()
 * @see Preset_Dump()
 * @see Setup_Dump()
 * @see FX_Dump()
 */
class PD
{
	/// device code of opened device
	int device_code;
	/// member code of opened device
	int member_code;
	/// OS revision string for opened device
	char os_rev[5];
	/// number of available user presets in this device
	int user_presets;
	/// pointer for setup names
	unsigned char* setup_names;
	/// number of installed ROMs in the opened device
	int roms;
	/// temporary storage for volume changes of muted voices
	int mute_volume[4];
	/// keeps track of solo'ed voices
	int is_solo[4];
	/// currently selected setup number
	int selected_multisetup;
	/// keeps track of incoming name request replies, so we can make sure
	/// to re-request missing names
	int name_counter[5][7];
	int names_to_download;
	int nak_count;
	bool randomizing;
	Setup_Dump* setup_init;
	/// creates device infoormations for an detected device
	void create_device_info();
	/// sets realtime controllers to initial values
	void update_cc_sliders();
	bool cc_changed;
	/// updates internal controller mappings on controller assignment changes
	void update_control_map();
	/**
	 * saves setup names to a file
	 * @param device_id the device id of the device that these setup names
	 * belong to
	 */
	void save_setup_names(int device_id);
	/**
	 * initialize a setup name
	 * @param number setup number
	 * @param name the name
	 */
	void set_setup_name(int number, const unsigned char* name);
	/**
	 * get a devices name
	 * @param the device code
	 * @returns the device name
	 */
	const char* get_name(int code) const;
	/**
	 * updates fx paramaters depending on the fx algorithm
	 * @param id (master/preset)fxa/fxb
	 * @param value fx type
	 */
	void update_fx_values(int id, int value) const;
	int test_checksum(const unsigned char* data, int size, int packet_size);
public:
	/// CTOR initializes variables
	PD();
	/// DTOR frees memory and saves setup names
	~PD();
	// these are used everywhere, so we just make them public! ;)
	/// currently selected MIDI channel
	int selected_channel;
	int selected_preset;
	int selected_preset_rom;
	/// currently active MIDI mode
	int midi_mode;
	/// currently selected layer
	int selected_layer;
	/// currently selected FX channel
	int selected_fx_channel;
	/**
	 * UI -> Device/Data controller.
	 * Receives user widget input and forwards commands to the device.
	 * Updates internal state.
	 * @param id the parameter ID
	 * @param value the parameter value
	 * @param layer the layer number for the parameter (-2 for layer independent
	 * parameters)
	 */
	void widget_callback(int id, int value, int layer = -2);
	/**
	 * UI CC -> device controller
	 * Receives controller inputs from the UI and forwards them
	 * to the device.
	 * @param controller the controller number
	 * @param value the controller value
	 */
	void cc_callback(int controller, int value);
	/// stores current controller settings as initial amounts
	void store_play_as_initial();
	/// maps controller values to CC widget numbers (device -> UI)
	std::map<int, int> cc_to_ctrl;
	/// maps CC widget numbers to actual controller values (UI -> device)
	std::map<int, int> ctrl_to_cc;
	/**
	 * request a setup name from the device or load a setup name file
	 * @param start the number of the setup to request
	 * @param from_disk_only wether we should try to only load a setup name
	 * file and skip the device request if the load fails
	 */
	int load_setup_names(int start, bool from_disk_only);
	/**
	 * local copy of the currently loaded setup on the device
	 * @see Setup_Dump()
	 */
	Setup_Dump* setup;
	/**
	 * copy of the currently loaded setup.
	 * the copy is not editable and is used to reset controller values
	 * to their initial amount ("right-click" feature)
	 * @see Setup_Dump()
	 */
	const Setup_Dump* setup_copy;
	/**
	 * local copy of the currently selected program/preset
	 * @see Preset_Dump()
	 */
	Preset_Dump* preset;
	/**
	 * copy of the currently loaded program/preset.
	 * the copy is not editable and is used to reset controller values
	 * to their initial amount ("right-click" feature)
	 * @see Preset_Dump()
	 */
	const Preset_Dump* preset_copy;
	/**
	 * preset data that has been loaded from the library.
	 * unused yet
	 * @see Preset_Dump()
	 */
	Preset_Dump* preset_library;
	/**
	 * array of ROM()s for the currently opened device.
	 * @see ROM()
	 */
	ROM* rom[5];
	/**
	 * user edit arp dump
	 */
	Arp_Dump* arp;
	/**
	 * tries to open a device with pre-configured values.
	 * if autoconnect is enabled and MIDI ports have been configured,
	 * this method tries to open the device on those ports. shows the
	 * "Open device..." dialog if it fails to do so
	 */
	void connect();
	/**
	 * initializes all ROM's and requests a setup dump when finished.
	 * triggers the loading of all names for the currently opened device.
	 * these may be saved already and loaded from disk, if not, name requests
	 * will be send out and a watchdog is launched to track the requests.
	 * once all names have been loaded (one way or the other), a setup dump
	 * is requested
	 */
	void initialize();
	void init_arp_riff_names();
	void loading();
	std::vector<std::string> status_message;
	void display_status(const char* message, bool top = false);
	/**
	 * saves current setup dump to the device.
	 * @param dst the slot to save to (0-62)
	 * @param newname the name for the multisetup
	 */
	void save_setup(int dst, const char* newname);
	/**
	 * cancel initialization process and clean up the mess
	 */
	void cancel_init();
	/**
	 * set tv_start
	 * @see tv_start
	 */
	//void start_stopwatch();
	/**
	 * set tv_stop and update the UI
	 * @see tv_stop
	 */
	//void stop_stopwatch();
	/// handles incoming device inuqiry sysex message
	void incoming_inquiry_data(const unsigned char* data, int len);
	/// handles incoming hardware config sysex message
	void incoming_hardware_config(const unsigned char* data, int len);
	/// handles incoming setup dump sysex message
	void incoming_setup_dump(const unsigned char* data, int len);
	void load_setup();
	/// handles incoming preset dump sysex message
	void incoming_preset_dump(const unsigned char* data, int len);
	/// handles incoming arp dump sysex message
	void incoming_arp_dump(const unsigned char* data, int len);
	/// handles incoming program change map dump sysex message
	//void incoming_pc_dump(const unsigned char* data, int len);
	/// handles incoming generic name sysex message
	void incoming_generic_name(const unsigned char* data);
	/// handles incoming parameter value sysex message
	//	void incoming_parameter_value(int parameter, int value);
	/// handles incoming ERROR sysex message
	void incoming_ERROR(int cmd, int sub);
	/// handles incoming ACK sysex message
	void incoming_ACK(int packet);
	/// handles incoming NAK sysex message
	void incoming_NAK(int packet);
	/// handles incoming CANCEL sysex message
	void incoming_CANCEL();
	/// handles incoming WAIT sysex message
	void incoming_WAIT();
	/// handles incoming EOF sysex message
	void incoming_EOF();
	/**
	 * reset all edited values to initial.
	 * resets all parameter values to the program/preset default values
	 * and updates the UI
	 */
	void start_over();
	void randomize();
	/// load an exported program
	int load_export(const char*, bool library = false); //, bool keep = false);
	void show_preset(bool lib = false);
	/**
	 * mute/unmute a layer
	 * @param state wether to mute or unmute
	 * @param layer the voice to mute/unmute
	 */
	void mute(int state, int layer);
	/**
	 * solo/unsolo a layer
	 * @param state wether to solo or unsolo
	 * @param layer the voice to solo/unsolo
	 */
	void solo(int state, int layer);
};

#endif /* PD_H_ */
/** @} */
