/*
 * pxk.H
 *
 *  Created on: 12 Feb 2014
 *      Author: vvd
 */

#ifndef PXK_H_
#define PXK_H_

#include <map>
#include <vector>
#include <string>

#include "ui.H"
#include "data.H"

/**
 * Enum for the three MIDI modes
 */
enum
{
	OMNI, POLY, MULTI
};

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

class PXK
{
	/*
	 * general
	 */
public:
	std::vector<std::string> status_message;
private:
	char device_id;
	volatile bool synchronized;
public:
	void LoadConfig(bool, int id = -1);
	void ConnectPorts();
	bool Synchronize();
	void Loading(bool upload = false);
	void log_add(const unsigned char*, const unsigned int, unsigned char) const;
	bool Synchronized() const;
	/// maps controller values to CC widget numbers (device -> UI)
	std::map<int, int> cc_to_ctrl;
private:
	/// maps CC widget numbers to actual controller values (UI -> device)
	std::map<int, int> ctrl_to_cc;
	unsigned char nak_count;
public:
	void incoming_generic_name(const unsigned char*);
	void incoming_ACK(int);
	void incoming_NAK(int);
//	void incoming_ERROR(int, int);
	void widget_callback(int, int, int layer = -2);
	void cc_callback(int, int);
	void display_status(const char*);
	void Join();

	/*
	 * device specific
	 */
public:
	int device_code; // PXK device code
private:
	int member_code; // 2 = AUDITY
	char os_rev[5]; // OS revision
	int user_presets; // available user presets
	void create_device_info();
public:
	bool inquired;
	void Inquire(unsigned char);
	void incoming_inquiry_data(const unsigned char*, int);
	void incoming_hardware_config(const unsigned char*, int);

	/*
	 * setup specific
	 */
public: // these are used by lot's of widgets
	Setup_Dump* setup;
	const Setup_Dump* setup_copy;
	char selected_channel;
	char selected_preset_rom;
	int selected_preset;
private:
	char selected_multisetup;
	unsigned char* setup_names;
	bool cc_changed;
	void update_fx_values(int, int) const;
	void update_cc_sliders();
	void update_control_map();
	void set_setup_name(unsigned char, const unsigned char*);
	void save_setup_names() const;
public:
	char midi_mode;
	char selected_fx_channel;
	const Setup_Dump* setup_init;
	void save_setup(int, const char*);
	void incoming_setup_dump(const unsigned char*, int);
	unsigned char load_setup_names(unsigned char);
	void load_setup();
	void store_play_as_initial();

	/*
	 * rom specific
	 */
private:
	char rom_index[5];
	const char* get_name(int) const;
public:
	ROM* rom[5];
	unsigned char roms; // number of roms
	char get_rom_index(char) const;

	/*
	 * preset specific
	 */
private:
	int mute_volume[4]; // volume of muted voices
	bool is_solo[4];
	int test_checksum(const unsigned char*, int, int);
	bool randomizing;
public:
	Preset_Dump* preset;
	const Preset_Dump* preset_copy;
	void show_preset();
	int selected_layer;
	void mute(int state, int layer);
	void solo(int state, int layer);
	void incoming_preset_dump(const unsigned char*, int);
	void load_export(const char*);
	void start_over();
	void randomize();

	/*
	 * arp specific
	 */
private:

public:
	Arp_Dump* arp;
	void incoming_arp_dump(const unsigned char*, int);


public:
	/// CTOR
	PXK(bool, int __id = -1);
	/// DTOR
	~PXK();
};

#endif /* PXK_H_ */
