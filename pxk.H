/*
 * pxk.H
 *
 *  Created on: 12 Feb 2014
 *      Author: vvd
 */

#ifndef PXK_H_
#define PXK_H_

#include <map>

#include "ui.H"
#include "data.H"
#include "debug.H"
//#include "midi.H"


class PXK
{
	/*
	 * general
	 */
public:

private:
	char device_id;
	unsigned char request_delay;
	bool synchronized;
public:
	bool LoadConfig(const char*) const;
	bool PopulatePorts() const;
	void Synchronize(unsigned char);
	void r_sysex(const unsigned char*, const int) const;
	void r_event(const unsigned char*);
private:
	/// maps controller values to CC widget numbers (device -> UI)
	std::map<int, int> cc_to_ctrl;
	/// maps CC widget numbers to actual controller values (UI -> device)
	std::map<int, int> ctrl_to_cc;
public:
	void incoming_generic_name(const unsigned char* data);
	void incoming_ERROR(int cmd, int sub);
	void incoming_ACK(int packet);
	void incoming_NAK(int packet);
	void incoming_CANCEL();
	void incoming_WAIT();
	void incoming_EOF();
	void widget_callback(int id, int value, int layer = -2);
	void cc_callback(int controller, int value);
	void display_status(const char* message, bool top = false);

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
	void incoming_inquiry_data(const unsigned char* data, int len);
	void incoming_hardware_config(const unsigned char* data, int len);

	/*
	 * setup specific
	 */
private:
	Setup_Dump* setup;
	const Setup_Dump* setup_copy;
	char selected_multisetup;
	unsigned char* setup_names;
	char selected_channel;
	char selected_preset_rom;
	int selected_preset;
	char midi_mode;
	char selected_fx_channel;
	void store_play_as_initial();
	void update_fx_values(int id, int value) const;
	void update_cc_sliders();
	void update_control_map();
public:
	void incoming_setup_dump(const unsigned char* data, int len);

	/*
	 * rom specific
	 */
private:
	ROM* rom[5];
	unsigned char roms; // number of roms
	std::map<int, int> rom_id_map;
	const char* get_name(int code) const;

	/*
	 * preset specific
	 */
private:
	Preset_Dump* preset;
	const Preset_Dump* preset_copy;
	unsigned int mute_volume[4]; // volume of muted voices
	bool is_solo[4];
public:
	int selected_layer;
	void mute(int state, int layer);
	void solo(int state, int layer);
	void incoming_preset_dump(const unsigned char* data, int len);

	/*
	 * arp specific
	 */
private:
	Arp_Dump* arp;
public:
	void incoming_arp_dump(const unsigned char* data, int len);


public:
	/// CTOR
	PXK(const char*, char);
	/// DTOR
	~PXK();
};

#endif /* PXK_H_ */
