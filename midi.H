#ifndef MIDI_H_
#define MIDI_H_
/**
 \defgroup pd_midi prodatum MIDI I/O
 @{
 */
#include <vector>
#include <portmidi.h>
#include <porttime.h>
#include <pmutil.h>

#define MIDI_SYSEX 0xf0
#define MIDI_EOX 0xf7
#define NOTE_OFF 0x80
#define NOTE_ON 0x90

#ifdef WIN32
#	include <windows.h>
#	define mysleep(x) Sleep(x)
#else
#	include <unistd.h>
#	define mysleep(x) usleep(x * 1000)
#endif

/**
 * prodatum MIDI class.
 * opens and closes MIDI ports, starts and stops the MIDI sender/receiver,
 * allocates buffers for reading and writing MIDI data, populates the UI
 * with available MIDI ports and offers many methods to send all kinds of
 * sysex commands and MIDI events
 */
class MIDI
{
	/// vector for available readable MIDI ports
	std::vector<int> ports_in;
	/// vector for writable MIDI ports
	std::vector<int> ports_out;
	/// opened writable MIDI port
	int selected_port_out;
	/// opened readable MIDI port
	int selected_port_in;
	/// opened controller MIDI port
	int selected_port_thru;
	/// method to start the MIDI sender/receiver
	int start_timer();
	/// method to stop the MIDI sender/receiver
	void stop_timer();
	/// populates the UI with available system MIDI ports
	void populate_ports();
public:
	/**
	 * CTOR for the MIDI class
	 * populates available MIDI ports to the UI and allocates storage for
	 * the ringbuffers, initializes default values
	 */
	MIDI();
	/**
	 * CTOR for the MIDI class
	 * frees allocated ringbuffers, closes all opened MIDI ports and stops
	 * the MIDI sender/receiver
	 */
	~MIDI();
	/// allows for switching the device ID on the fly
	void set_device_id(int id);
	/// sets the incoming channel filter for the control port
	void set_control_channel_filter(int channel) const;
	/// sets the incoming channel filter
	void set_channel_filter(int channel) const;
	/**
	 * opens writable MIDI port, starts MIDI timer if not running yet.
	 * @param out the MIDI port to open
	 */
	int connect_out(int out);
	/**
	 * opens readable MIDI port, starts MIDI timer if not running yet.
	 * @param in the MIDI port to open
	 */
	int connect_in(int in);
	void filter_loose() const;
	void filter_strict() const;
	/**
	 * opens readable MIDI controller port, starts MIDI timer if not running yet.
	 * @param thru the MIDI port to open
	 */
	int connect_thru(int thru);
	/**
	 * puts a sysex message into the write buffer.
	 * @param sysex the sysex data
	 * @param size the size of the message in bytes
	 */
	void write_sysex(const unsigned char* sysex, unsigned int size) const;
	/**
	 * puts a MIDI event into the write buffer
	 * @param status MIDI status byte
	 * @param value1 first MIDI data byte
	 * @param value2 second MIDI data byte
	 * @param channel the channel to send the event on
	 */
	void write_event(int status, int value1, int value2, int channel = -1) const;
	/**
	 * send an acknowledgement for a packet.
	 * @param packet the packet to acknowledge
	 */
	void ack(int packet) const;
	/**
	 * send an negative acknowledgement for a packet.
	 * @param packet the packet to negative acknowledge
	 */
	void nak(int packet) const;
	/// sends a cancel command
//	void cancel() const;
	/// send WAIT command
//	void wait() const;
	/// sends a EOF command
	void eof() const;
	/// sends a device inquiry
//	void request_device_inquiry(int id = -1) const;
	/**
	 * sends a preset dump request.
	 * @param preset the preset number
	 * @param rom_id the ROM ID of the preset
	 */
	void request_preset_dump(int preset, int rom_id) const;
	/// sends a setup dump request
	void request_setup_dump() const;
	//	/// sends an FX dump request
	//	void request_fx_dump(int preset, int rom_id) const;
	/// sends an arp dump request
	void request_arp_dump(int number, int rom_id) const;
	/**
	 * sends a parameter value request
	 * @param id the parameter ID
	 * @param channel the channel of the parameter
	 */
	//void request_parameter_value(int id, int channel = -1) const;
	/**
	 * sends a generic name request.
	 * @param type the type of name (PRESET, INSTRUMENT, ..)
	 * @param number the items number
	 * @param rom_ID the ROM ID of the item
	 */
	void request_name(int type, int number, int rom_ID) const;
	/**
	 * sends a parameter value edit command.
	 * @param id the parameter ID
	 * @param value the new value for the parameter
	 */
	void edit_parameter_value(int id, int value) const;
	/**
	 * renames an item on the device.
	 * @param type the type of the item to rename (PRESET or ARP)
	 * @param number the number of the item to rename
	 * @param name the new name
	 */
	//void rename(int type, int number, const unsigned char* name) const;
	/// sends a hardware configuration request
	void request_hardware_config() const;
	/**
	 * sets the master volume of the device
	 * @param volume the new volume
	 */
	void master_volume(int volume) const;
	/**
	 * sends a copy command.
	 * @param cmd the type of copy command (copy preset/arp, patchcords, layer,..)
	 * @param src the source program
	 * @param dst the destination program
	 * @param src_l the source layer
	 * @param dst_l the destination layer
	 * @param rom_id the ROM ID of the source
	 */
	void copy(int cmd, int src, int dst, int src_l = 0, int dst_l = 0, int rom_id = 0) const;
	/// toggles audition on the device
	void audit() const;
	/// sends a randomize preset command
	void randomize() const;
	bool in();
	bool out();
};

#endif /*MIDI_H_*/
/** @} */
