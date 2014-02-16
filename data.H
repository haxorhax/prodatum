// $Id$
#ifndef DATA_H_
#define DATA_H_
/**
 \defgroup pd_data prodatum Data Classes
 @{
 */
#include <deque>

#define DUMP_HEADER_SIZE 36

/**
 * Enum for generic name IDs used by the device
 */
enum
{
	ID, PRESET, INSTRUMENT, ARP, SETUP, DEMO, RIFF
};

/**
 * Preset Dump class.
 * holds data and informations of a preset dump
 */
class Preset_Dump
{
	/// size of the complete dump
	int size;
	/// size of packets
	int packet_size;
	/// program/preset number
	int number;
	/// ROM ID of this program/preset
	int rom_id;
	/// name of program/preset
	unsigned char name[17];
	/// wether this dump contains 4 extra controllers found on p2k modules
	int extra_controller;
	/// wether we are an audity 2000 dump
	int a2k;
	/// true if data has been edited
	bool data_is_changed;
	/// raw dump data
	unsigned char* data;
	/// undo struct
	struct parameter
	{
		int id;
		int value;
		int layer;
	};
	/// undo stack
	std::deque<parameter> undo_s;
	/// redo stack
	std::deque<parameter> redo_s;
	/// push undo parameter on stack
	void add_undo(int id, int value, int layer);
	void update_ui_from_xdo(int id, int value, int layer) const;
	/**
	 * maps parameter IDs to offset values in the dump
	 * @param id parameter ID
	 * @param layer the layer of the parameter (0-3)
	 * @param id_mapped reference variable for the calculated offset value
	 */
	void idmap(const int& id, const int& layer, int& id_mapped) const;
	/// hands over preset data to the piano widget
	void update_piano() const;
	/// hands over preset data to the envelope widget
	void update_envelopes() const;
	/// updates the checksums of the dump
	void update_checksum();
	/**
	 * Copy a given range of parameters from one layer to another.
	 * works will all ranges (validity checks are being done internally)
	 * @param start first parameter ID
	 * @param end last parameter ID
	 * @param src source layer
	 * @param dst destination layer
	 */
	void copy_layer_parameter_range(int start, int end, int src, int dst);

public:
	/**
	 * CTOR for Preset Dump
	 * @param dump_size the size of the dump in bytes
	 * @param dump_data pointer to the dump data
	 * @param p_size number of bytes in a packet
	 * @param update wether to automatically update name files and browsers
	 * from name extracted from the dump
	 */
	Preset_Dump(int dump_size, const unsigned char* dump_data, int p_size, bool update = false);
	/**
	 * DTOR just frees the memory
	 */
	~Preset_Dump();
	/// return data status
	bool is_changed() const;
	/// return extra_controller
	int get_extra_controller() const;
	/// undo edit
	void undo();
	/// redo edit
	void redo();
	/// if true, nothing is pushed on the undo stack
	bool disable_add_undo;
	/**
	 * clone this preset dump
	 * @param dst pointer to destination (will be deleted)
	 */
	void clone(Preset_Dump* dst) const;
	///
	void set_changed(bool);
	/// returns the preset name
	const char* get_name() const;
	/// returns the preset number
	int get_number() const;
	/// returns the ROM ID
	int get_rom_id() const;
	/**
	 * get the value of a parameter.
	 * extract a value for a given (layer-)parameter from the dump
	 * @param id parameter ID
	 * @param layer layer of the parameter
	 * @returns integer value for the parameter
	 */
	int get_value(int id, int layer = 0) const;
	/**
	 * set the value of a parameter.
	 * update a given (layer-)parameter to the specified value
	 * @param id parameter ID
	 * @param value the new value for the (layer-)parameter
	 * @param layer layer of the parameter
	 */
	int set_value(int id, int value, int layer = -2);
	/// set category and name
	void set_name(const char* val, int type, int position);
	/**
	 * update UI widgets with the parameter values of this dump.
	 * this calls \c get_value() for all parameters (except FX parameters)
	 * and sets the corresponding widget in the UI.
	 */
	void show() const;
	/**
	 * update UI-FX widgets with the parameter values of this dump.
	 * this calls \c get_value() for all FX parameters
	 * and sets the corresponding widget in the UI.
	 */
	void show_fx() const;
	/**
	 * move preset to a new location.
	 * @param new_number the new location for the preset
	 */
	void move(int new_number);
	/**
	 * copy various parameters from one layer to another.
	 * this is also used to save a preset to the device.
	 * @param type what to copy (preset, fx settings, complete layer, ..)
	 * @param src the source layer
	 * @param dst the destination layer
	 */
	void copy(int type, int src, int dst);
	/**
	 * upload this preset dump to the device.
	 * this is used to save the preset. it will update the checksums and upload
	 * the preset in either closed or open loop fashion (depending on what
	 * has been configured).
	 * @param packet the packet number to upload
	 * @param closed wether to use close or open loop style uploads
	 * @param show wether to show the uploaded dump when
	 * the upload was successfull
	 */
	void upload(int packet, int closed = -1, bool show = false);
	/// save dump to disk
	void save_file();
	// get methods needed to clone preset dumps
	int get_dump_size() const;
	const unsigned char* get_data() const;
};

/**
 * Arp Dump class.
 * holds data and informations of a arp dump
 */
class Arp_Dump
{
	int size;
	int number;
	unsigned char name[17];
	unsigned char* data;
	void show() const;
	void update_name(const unsigned char* np) const;
public:
	Arp_Dump(int dump_size, const unsigned char* dump_data);
	~Arp_Dump();
	void update_sequence_length_information() const;
	int get_number() const;
	int get_value(int id, int step) const;
	/// rename arp
	void rename(const char* newname) const;
	void reset_step(int step) const;
	void reset_pattern() const;
};

/**
 * Setup Dump class.
 * holds data and informations of a setup dump
 */
class Setup_Dump
{
	/// size of the complete dump
	int size;
	/// raw dump data
	unsigned char* data;
	/// wether this dump contains 4 extra controllers found on p2k modules
	int extra_controller;
	/**
	 * maps parameter IDs to offset values in the dump
	 * @param id parameter ID
	 * @param channel the channel for the parameter
	 * @param id_mapped reference variable for the calculated offset value
	 */
	void idmap(const int& id, const int& channel, int& id_mapped) const;
	/**
	 * Enum for various types of data in the setup dump.
	 */
	enum
	{
		SDI_GENERAL, SDI_MIDI, SDI_FX, SDI_RESERVED, SDI_NON_CHNL, SDI_CHNLS, SDI_CHNL_PARAMS
	};
	/// array holds vital informations about the current setup
	int setup_dump_info[7];

public:
	/**
	 * CTOR for Setup Dump
	 * @param dump_size the size of the dump in bytes
	 * @param dump_data pointer to the dump data
	 */
	Setup_Dump(int dump_size, const unsigned char* dump_data);
	/**
	 * DTOR just frees the memory
	 */
	~Setup_Dump();
	/**
	 * get the value of a parameter.
	 * extract a value for a given setup parameter from the dump
	 * @param id parameter ID
	 * @param hannel the channel number for the parameter
	 * @returns integer value for the parameter
	 */
	int get_value(int id, int channel = -1) const;
	/**
	 * set the value of a parameter.
	 * update a given (channel-)parameter to the specified value
	 * @param id parameter ID
	 * @param value the new value for the (layer-)parameter
	 * @param channel parameter channel to update
	 */
	int set_value(int id, int value, int channel = -1);
	/**
	 * update UI widgets with the parameter values of this dump.
	 * this calls \c get_value() for all setup parameters
	 * and sets the corresponding widget in the UI.
	 * @param midi_mode the currently active midimode
	 */
	void show() const;
	/**
	 * update UI FX widgets with master FX settings.
	 * this calls \c get_value() for all Master FX parameters
	 * and sets the corresponding widget in the UI.
	 */
	void show_fx() const;
	/**
	 * uploads the dump to the device to save it.
	 * currently not used
	 */
	void upload() const;
	// get methods needed to copy setup dumps
	int get_dump_size() const;
	const unsigned char* get_data() const;
};

/**
 * Program change map Dump class.
 * holds data and informations of a program change map dump
 */
class PC_Dump
{

};

/**
 * ROM class.
 * holds data and informations of a ROM module
 */
class ROM
{
	/// rom ID
	int id;
	/// for user data (programs, arps) we need to know which device we belong to
	char device_id;
	/// if data (PROGRAM, INSTRUMENT,..) is on disk already, this gets true
	bool on_disk[7];
	/// number of available instruments in this ROM
	int instruments;
	/// number of available presets in this ROM
	int presets;
	/// number of available arps in this ROM
	int arps;
	/// number of available riffs in this ROM
	int riffs;
	/// storage pointer to the array of instrument names
	unsigned char* instrument_names;
	/// storage pointer to the array of preset names
	unsigned char* preset_names;
	/// storage pointer to the array of arp names
	unsigned char* arp_names;
	/// storage pointer to the array of riff names
	unsigned char* riff_names;


public:
	/**
	 * CTOR for the ROM class.
	 * @param id the ROM ID
	 * @param presets the number of presets the ROM holds
	 * @param instruments the number of instruments the ROM holds
	 */
	ROM(int id, int presets = 0, int instruments = 0);
	/**
	 * DTOR of ROM class.
	 * triggers saving of ROM-name files (if not already saved and ID != 0)
	 * and frees memory
	 */
	~ROM();
	/**
	 * loads name files or triggers name request commands if loading fails
	 * @param type of name file (PRESET; INSTRUMENT,..)
	 * @param number index number of name to request
	 */
	void load_name(unsigned char type, int number);
	/**
	 * load a name file from disk.
	 * @param type of name file (PRESET; INSTRUMENT,..)
	 * @returns number of available names
	 */
	int disk_load_names(unsigned char type);
	/**
	 * set the name of type X with number Z to name Y
	 * @param type of name (PRESET; INSTRUMENT,..)
	 * @param number the number of the name
	 * @param name the name
	 */
	int set_name(int type, int number, const unsigned char* name);
	/// returns an attribute like ROM ID ...
	int get_attribute(int type) const;
	/// returns the name of this ROM
	const char* name() const;
	/**
	 * return the name for type X with number Y
	 * @param type of name file (PRESET; INSTRUMENT,..)
	 * @param number the number of the name
	 * @returns pointer to the name
	 */
	const unsigned char* get_name(int type, int number = 0) const;
};

#endif /*DATA_H_*/
/** @} */
