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
	std::vector<std::string> preset_list;
	std::vector<int> preset_saves;
	std::vector<std::string> arp_list;
	std::vector<int> arp_saves;
	std::string output_dir;
	std::string setup_file;
	int preset_dump_rom;
	bool save_in_progress;
	bool started_request;
	bool pending_cancel;
	int machine_id;

private:
	char device_id;
	volatile bool synchronized;
public:
	void ConnectPorts();
	bool Synchronize();
	void Loading(bool upload = false);
	void log_add(const unsigned char*, const unsigned int, unsigned char) const;
	bool Synchronized() const;
	void new_preset(int, const unsigned char*, int);
	void new_arp(int, const unsigned char*);
	void clear_preset_handler();
	bool preset_transfer_complete();
	/// maps controller values to CC widget numbers (device -> UI)
	std::map<int, int> cc_to_ctrl;
private:
	/// maps CC widget numbers to actual controller values (UI -> device)
	std::map<int, int> ctrl_to_cc;
	unsigned char nak_count;
	unsigned char ack_count;
public:
	void incoming_generic_name(const unsigned char*);
	void incoming_ACK(int);
	void incoming_NAK(int);
//	void incoming_ERROR(int, int);
	void widget_callback(int, int, int layer = -2);
	void cc_callback(int, int);
	void display_status(const char*);
	void Join();
	void reset();

	/*
	 * device specific
	 */
public:
	int device_code; // PXK device code
	int member_code; // 2 = AUDITY
private:
	char os_rev[5]; // OS revision
	int user_presets; // available user presets
	void create_device_info();
public:
	bool inquired;
	void Inquire(int);
	void incoming_inquiry_data(const unsigned char*);
	void incoming_hardware_config(const unsigned char*);

	/*
	 * setup specific
	 */
public:
	// these are used by lot's of widgets
	Setup_Dump* setup;
	const Setup_Dump* setup_copy;
	char selected_channel;
	char selected_preset_rom;
	int selected_preset;
	int selected_arp;
	char selected_multisetup;
	char setup_offset;
private:
	unsigned char* setup_names;
	bool setup_names_changed;
	bool cc_changed;
	void update_fx_values(int, int) const;
	void update_cc_sliders();
	void update_control_map();
public:
	char midi_mode;
	char selected_fx_channel;
	const Setup_Dump* setup_init;
	void save_setup(int, const char*);
	void incoming_setup_dump(const unsigned char*, int);
	unsigned char load_setup_names(unsigned char);
	void set_setup_name(unsigned char, const unsigned char*);
	void save_setup_names(bool force = false) const;
	void load_setup();
	void store_play_as_initial();
	void import_setup();
	void export_setup();


	/*
	 * rom specific
	 */
private:
	char rom_index[5];
	const char* get_name(int) const;
public:
	ROM* rom[5];
	unsigned char roms; // number of roms
	unsigned char get_rom_index(char) const;

	/*
	 * preset specific
	 */
private:
	int mute_volume[4]; // volume of muted voices
	bool is_solo[4];
	bool randomizing;
	int preset_offset;
	
public:
	Preset_Dump* preset;
	const Preset_Dump* preset_copy;
	int test_checksum(const unsigned char*, int, int);
	void show_preset();
	int selected_layer;
	void mute(int state, int layer);
	void solo(int state, int layer);
	void incoming_preset_dump(const unsigned char*, int, bool=false);
	void load_export(const char*);
	void start_over();
	void randomize();
	void bulk_preset_download();
	void bulk_preset_upload();
	int get_preset_and_increment();

	/*
	 * arp specific
	 */
private:
	int arp_offset;

public:
	Arp_Dump* arp;
	void incoming_arp_dump(const unsigned char*, int);
	int get_arp_and_increment();

public:
	/// CTOR
	PXK();
	void Boot(bool, int __id = -1);
	/// DTOR
	~PXK();

	void bulk_pattern_download();
	void bulk_pattern_upload();
};

#endif /* PXK_H_ */
