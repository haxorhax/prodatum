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

#ifdef HAVE_CONFIG_H
#  include <config.H>
#endif

#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <FL/fl_ask.H>

#include "ringbuffer.H"
#include "midi.H"
#include "pd.H"
#include "ui.H"
#include "cfg.H"

extern PD* pd;
extern PD_UI* ui;
extern Cfg* cfg;

#include "debug.H"

/**
 * midi core implementation (sender/receiver/decoder).
 * this is where all MIDI bytes (outgoing and incoming) pass through.
 * this thread should never lock
 * in the linux version a pipe is used to notify \c process_midi_in
 * for new messages.
 */
static void process_midi(PtTimestamp, void*);
static bool timer_running = false;
/*! \fn process_midi_in
 * connects the MIDI receiver with the main thread.
 * all incoming MIDI messages are passed to this function via the
 * read buffer. runs in the main thread.
 * in the linux version this gets notified by \c process_midi if a new message
 * is available. on windows and mac this is a timeout function, repeated
 * until we close our MIDI ports
 */
#ifndef WINDOWS
static void process_midi_in(int fd, void*);
static int p[2];
#else
static void process_midi_in(void*);
#endif

/**
 * updates some MIDI engine informations that are shown in the UI
 */
static int overflows = 0;
static PmError pmerror = pmNoError;

static PortMidiStream *port_in;
static PortMidiStream *port_out;
static PortMidiStream *port_thru; // controller port (eg keyboard)
static bool thru_active;
static bool midi_active;
static bool process_midi_exit_flag;
static jack_ringbuffer_t *read_buffer;
static jack_ringbuffer_t *write_buffer;
static int midi_device_id;
static bool seed_randomizer = true;
static bool requested = false;
static int requested_names = 0;
// used by watchdog in PD
timeval tv_incoming;
bool time_incoming_midi = false;
bool automap;
// set in cfg
extern int request_delay;

static void process_midi(PtTimestamp, void*)
{
	if (!midi_active)
	{
		if (!jack_ringbuffer_read_space(write_buffer))
		{
			process_midi_exit_flag = true;
			return;
		}
	}
	PmEvent ev;
	static unsigned char event[4]; // 3 midi bytes, one byte to distinguish device (0) and controller (1) events
	static int data, shift;
	// we only want to put full sysex messages in our ringbuffer
	// these are the buffers to store sysex chunks locally
	static unsigned char local_read_buffer[SYSEX_MESSAGE_BUFFER];
	static unsigned char local_write_buffer[SYSEX_MESSAGE_BUFFER];
	static size_t position = 0;
	static bool receiving_sysex = false;
	static bool result_out = false;
	do
	{
		// check if theres something from the device and write it to the read_buffer
		while (midi_active && Pm_Poll(port_in))
		{
			pmerror = (PmError) Pm_Read(port_in, &ev, 1);
			if (pmerror < 0)
			{
				receiving_sysex = false;
				break;
			}
			// got 4 bytes
			for (shift = 0; shift <= 24; shift += 8)
			{
				// byte for byte inspection
				data = (ev.message >> shift) & 0xFF;
				if (data == MIDI_SYSEX)
				{
					receiving_sysex = false;
					// filter sysex
					// e-mu proteus
					if (((ev.message >> 8) & 0xFF) == 0x18 && ((ev.message
							>> 16) & 0xFF) == 0x0F && ((ev.message >> 24)
							& 0xFF) == midi_device_id)
						receiving_sysex = true;
					// universal sysex
					else if (((ev.message >> 8) & 0xFF) == 0x7E)
						receiving_sysex = true;
					position = 0;
				}
				// check for truncated sysex
				if (receiving_sysex && (((data & 0x80) == 0 || data == 0xF7)
						|| position == 0))
				{
					// copy data
					if (position < SYSEX_MESSAGE_BUFFER)
					{
						local_read_buffer[position] = data;
						++position;
						// hand over a complete sysex message to the main thread
						if (data == MIDI_EOX)
						{
							if (jack_ringbuffer_write_space(read_buffer)
									>= position)
							{
								jack_ringbuffer_write(read_buffer,
										local_read_buffer, position);
#ifndef WINDOWS
								if (!write(p[1], " ", 1) == 1)
									pmesg(1, "*** Could not write into pipe!\n");
#endif
								if (time_incoming_midi)
									gettimeofday(&tv_incoming, 0);
							}
							else
								overflows |= 1;
							receiving_sysex = false;
							break;
						}
					}
					else
					{
						++position;
						if (data == MIDI_EOX)
						{
							pmesg(
									1,
									"*** SYSEX_MESSAGE_BUFFER (%d) too small (%d) for reader\n",
									SYSEX_MESSAGE_BUFFER, position);
							overflows |= 2;
							receiving_sysex = false;
							break;
						}
					}
				}
				// voice message
				else if (shift == 0 && data >= 0x80 && data <= 0xEF)
				{
					receiving_sysex = false;
					if (jack_ringbuffer_write_space(read_buffer) >= 4)
					{
						event[0] = Pm_MessageStatus(ev.message);
						event[1] = Pm_MessageData1(ev.message);
						event[2] = Pm_MessageData2(ev.message);
						event[3] = 0;
						jack_ringbuffer_write(read_buffer, event, 4);
#ifndef WINDOWS
						if (!write(p[1], " ", 1) == 1)
							pmesg(1, "*** Could not write into pipe!\n");
#endif
					}
					break; // dont loop through the other bytes
				}
				else
				{
					// can be no realtime message here (which could have been inserted
					// into a sysex stream) as we filter those
					receiving_sysex = false;
					break;
				}
			}
		}

		// check if theres something from the controller
		if (thru_active)
		{
			if (Pm_Poll(port_thru))
			{
				pmerror = (PmError) Pm_Read(port_thru, &ev, 1);
				if (!(pmerror < 0)) // no error
				{
					event[0] = Pm_MessageStatus(ev.message);
					// voice messages
					if (event[0] >= 0x80 && event[0] <= 0xEF)
					{
						// automap
						if (automap && pd->midi_mode != OMNI)
							event[0] = (event[0] & ~0xf)
									| (pd->selected_channel & 0xff);
						event[1] = Pm_MessageData1(ev.message);
						event[2] = Pm_MessageData2(ev.message);
						event[3] = 1;
						// write to ringbuffer for internal processing
						if (jack_ringbuffer_write_space(read_buffer) >= 4)
						{
							jack_ringbuffer_write(read_buffer, event, 4);
#ifndef WINDOWS
							if (!write(p[1], " ", 1) == 1)
								pmesg(1, "*** Could not write into pipe!\n");
#endif
						}
						ev.message = Pm_Message(event[0], event[1], event[2]);
					}
					// forward message
					pmerror = (PmError) Pm_Write(port_out, &ev, 1);
				}
			}
		}

		// check if theres some MIDI to write on the bus
		result_out = false;
		if (jack_ringbuffer_read(write_buffer, &local_write_buffer[0], 1) == 1)
		{
			result_out = true;
			if (local_write_buffer[0] == MIDI_SYSEX)
			{
				int i = 0;
				do
				{
					jack_ringbuffer_read(write_buffer,
							local_write_buffer + ++i, 1);
				} while (local_write_buffer[i] != MIDI_EOX);
				pmerror = Pm_WriteSysEx(port_out, 0, local_write_buffer);
			}
			else
			{
				if (jack_ringbuffer_read(write_buffer, event + 1, 2) == 2)
				{
					ev.message = Pm_Message(local_write_buffer[0], event[1],
							event[2]);
					pmerror = Pm_Write(port_out, &ev, 1);
				}
			}
		}
	} while (result_out); // repeat until all our queued sysex has been written
}

static void show_error(void)
{
	pmesg(40, "show_error()\n");
	static char buf[256];
	if (pmerror == -10000)
		Pm_GetHostErrorText(buf, 256);
	else if (pmerror < 0)
		snprintf(buf, 256, "%s", Pm_GetErrorText(pmerror));
	else if (overflows & 2) // internal buffer overflows
		snprintf(buf, 256, "ERROR: SysEx message too large!\nSkipping message.");
	else if (overflows & 1) // read buffer overflows + sysex lost
		snprintf(buf, 256,
				"Warning: SysEx incoming too fast! Message(s) lost!\n");
	pmerror = pmNoError;
	overflows = 0;
	fl_message("%s", buf);
}

#ifndef WINDOWS
static void process_midi_in(int fd, void*)
#else
static void process_midi_in(void*)
#endif
{
	static unsigned long count = 0;
	static unsigned long count_events = 0;
	unsigned char poll = 0;
	if (pmerror < 0 || overflows)
		show_error();
	while (midi_active && jack_ringbuffer_peek(read_buffer, &poll, 1))
	{
#ifndef WINDOWS
		char buf;
		if (!read(fd, &buf, 1) == 1) // this is fatal but very unlikely
		{
			pmesg(1, "*** Could not read from pipe!\n");
			pd->display_status("*** Could not read from pipe!", true);
		}
#endif
		if (poll == MIDI_SYSEX)
		{
			unsigned char sysex[SYSEX_MESSAGE_BUFFER];
			int len = -1;
			do
			{
				jack_ringbuffer_read(read_buffer, sysex + ++len, 1);
			} while (sysex[len] != MIDI_EOX);
			++len;
			// log sysex messages
			++count;
			if (ui->log_sysex_in->value())
			{
				char* buf = new char[2 * len + 17];
				int n = snprintf(buf, 15, "\nIS.%lu ", count);
				for (int i = 0; i < len; i++)
					sprintf(n + buf + 2 * i, "%02X", sysex[i]);
				ui->logbuf->append(buf);
				delete[] buf;
				ui->log->insert_position(ui->logbuf->length());
				if (!ui->scroll_lock->value())
					ui->log->show_insert_position();
			}
			// e-mu sysex
			if (sysex[1] == 0x18)
			{
				switch (sysex[5])
				{
				case 0x70: // error
					pd->incoming_ERROR(sysex[7] * 128 + sysex[6],
							sysex[9] * 128 + sysex[8]);
					break;

				case 0x7f: // ACK
					pd->incoming_ACK(sysex[7] * 128 + sysex[6]);
					break;

				case 0x7e: // NAK
					pd->incoming_NAK(sysex[7] * 128 + sysex[6]);
					break;

				case 0x7c: // WAIT
					pd->incoming_WAIT();
					Pt_Sleep(request_delay);
					break;

				case 0x7b: // EOF
					pd->incoming_EOF();
					break;

				case 0x7d: // CANCEL
					pd->incoming_CANCEL();
					break;

				case 0x09: // hardware configuration
					if (requested)
					{
						requested = false;
						pd->incoming_hardware_config(sysex, len);
					}
					break;

				case 0x0b: // generic name
					if (requested_names > 0)
					{
						--requested_names;
						pd->incoming_generic_name(sysex);
					}
					break;

				case 0x1c: // setup dumps
					if (requested)
					{
						requested = false;
						pd->incoming_setup_dump(sysex, len);
					}
					break;

				case 0x18: // arp pattern dump
					pd->incoming_arp_dump(sysex, len);
					break;

					//				case 0x16: // program change/preset map dump
					//					pd->incoming_pc_dump(sysex, len);
					//					break;

				case 0x10: // preset dumps
					if (requested)
						switch (sysex[6])
						{
						case 0x01: // dump header (closed)
						case 0x03: // dump header (open)
							pd->incoming_preset_dump(sysex, len);
							break;
						case 0x02: // dump data (closed)
						case 0x04: // dump data (open)
							pd->incoming_preset_dump(sysex, len);
							if (len < 253) // last packet
								requested = false;
							break;
						}
					break;
				case 0x40: // remote front panel control command
					break;

				default:
					pd->display_status("Received unrecognized sysex.", true);
					pmesg(10,
							"process_midi_in: received unrecognized message:\n");
					for (int i = 0; i < len; i++)
						pmesg(10, "%X ", sysex[i]);
					pmesg(10, "\n");
				}
			}
			// universal sysex
			else if (sysex[1] == 0x7e)
			{
				pmesg(90,
						"process_midi_in: received MIDI standard universal message: ");
				// device inquiry
				if (sysex[3] == 0x06 && sysex[4] == 0x02 && sysex[5] == 0x18)
				{
					if (requested)
					{
						pmesg(90, "device inquiry response\n");
						requested = false;
						pd->incoming_inquiry_data(sysex, len);
					}
				}
			}
			else
			{
				pd->display_status("Received data from another planet.", true);
				pmesg(10,
						"process_midi_in: received data from another planet!!!:\n");
				for (int i = 0; i < len; i++)
					pmesg(40, "%X ", sysex[i]);
				pmesg(10, "\n");
			}
		} // if (poll == MIDI_SYSEX)

		else
		{
			unsigned char event[4];
			jack_ringbuffer_read(read_buffer, event, 4);
			if (event[3] == 0) // device event
			{
				switch (event[0] >> 4)
				{
				case 0x8: // note off
					ui->piano->activate_key(-1, event[1]);
					ui->main->minipiano->activate_key(-1, event[1]);
					ui->global_minipiano->activate_key(-1, event[1]);
					break;
				case 0x9: // note-on
					if (event[2] == 0)
					{
						ui->piano->activate_key(-1, event[1]);
						ui->main->minipiano->activate_key(-1, event[1]);
						ui->global_minipiano->activate_key(-1, event[1]);
					}
					else
					{
						ui->piano->activate_key(1, event[1]);
						ui->main->minipiano->activate_key(1, event[1]);
						ui->global_minipiano->activate_key(1, event[1]);
					}
					break;
				case 0xb: // controller event
				{
					if (pd->cc_to_ctrl.find(event[1]) != pd->cc_to_ctrl.end())
					{
						int controller = pd->cc_to_ctrl[event[1]];
						if (controller <= 12)
							// sliders
							((Fl_Slider*) ui->main->ctrl_x[controller])->value(
									(double) event[2]);
						else
							// footswitches
							((Fl_Button*) ui->main->ctrl_x[controller])->value(
									event[2] > 63 ? 1 : 0);
					}
					else if (event[1] == 1) // modwhl
						ui->modwheel->value((double) event[2]);
					else if (event[1] == 7) // channel volume
						pwid[131][0]->set_value(event[2]);
					else if (event[1] == 10) // channel pan
						pwid[132][0]->set_value(event[2]);
				}
					break;
				case 0xe: // pitchwheel
				{
					int v = event[2];
					v <<= 7;
					v |= event[1];
					ui->pitchwheel->value((double) v);
				}
					break;
				default:
					pd->display_status("Received unused MIDI event.");
					pmesg(10,
							"process_device_event: unused event: %X, %X, %X\n",
							event[0], event[1], event[2]);
				}
			}
			else // controller event
			{
				switch (event[0] >> 4)
				{
				case 0x8: // note off
					ui->piano->activate_key(-3, event[1]);
					ui->main->minipiano->activate_key(-3, event[1]);
					ui->global_minipiano->activate_key(-3, event[1]);
					break;
				case 0x9: // note-on
					if (event[2] == 0)
					{
						ui->piano->activate_key(-2, event[1]);
						ui->main->minipiano->activate_key(-2, event[1]);
						ui->global_minipiano->activate_key(-2, event[1]);
					}
					else
					{
						ui->piano->activate_key(2, event[1]);
						ui->main->minipiano->activate_key(2, event[1]);
						ui->global_minipiano->activate_key(2, event[1]);
					}
					break;
				case 0xb: // controller event
				{
					if (pd->cc_to_ctrl.find(event[1]) != pd->cc_to_ctrl.end())
					{
						int controller = pd->cc_to_ctrl[event[1]];
						if (controller <= 12)
							// sliders
							((Fl_Slider*) ui->main->ctrl_x[controller])->value(
									(double) event[2]);
						else
							// footswitches
							((Fl_Button*) ui->main->ctrl_x[controller])->value(
									event[2] > 63 ? 1 : 0);
					}
					else if (event[1] == 1) // modwhl
						ui->modwheel->value((double) event[2]);
					else if (event[1] == 7) // channel volume
						pwid[131][0]->set_value(event[2]);
					else if (event[1] == 10) // channel pan
						pwid[132][0]->set_value(event[2]);
				}
					break;
				case 0xe: // pitchwheel
				{
					int v = event[2];
					v <<= 7;
					v |= event[1];
					ui->pitchwheel->value((double) v);
				}
					break;
				default:
					pd->display_status("Received unused MIDI event.");
					pmesg(
							10,
							"process_controller_event: unused event: %X, %X, %X\n",
							event[0], event[1], event[2]);
				}
			}
			// log midi events
			++count_events;
			if (ui->log_events_in->value())
			{
				char buf[28];
				snprintf(buf, 28, "\nIE.%lu %02X%02X%02X", count_events,
						event[0], event[1], event[2]);
				ui->logbuf->append(buf);
				ui->log->insert_position(ui->logbuf->length());
				if (!ui->scroll_lock->value())
					ui->log->show_insert_position();
			}
		}
	}
#ifdef WINDOWS
	if (timer_running)
	Fl::repeat_timeout(.01, process_midi_in);
#endif
}

// ########################################
// midi connection class member definitions
// ########################################
MIDI::MIDI()
{
	pmesg(40, "MIDI::MIDI()\n");
	midi_device_id = cfg->get_cfg_option(CFG_DEVICE_ID);
	// initialize (global) variables and buffers
	timer_running = false;
	midi_active = false;
	thru_active = false;
	process_midi_exit_flag = false;
	port_in = 0;
	port_out = 0;
	port_thru = 0;
	selected_port_out = -1;
	selected_port_in = -1;
	selected_port_thru = -1;
	read_buffer = jack_ringbuffer_create(RING_READ_BUFFER_SIZE);
	jack_ringbuffer_mlock(read_buffer);
	write_buffer = jack_ringbuffer_create(RING_WRITE_BUFFER_SIZE);
	jack_ringbuffer_mlock(write_buffer);
#ifndef WINDOWS
	if (pipe(p) == -1)
	{
		pmesg(10, "*** Could not open pipe\n%s", strerror(errno));
		fl_alert("Could not open pipe\n%s", strerror(errno));
		throw 1;
	}
	//	int flags;
	//	flags = fcntl(p[0], F_GETFL, 0);
	//	fcntl(p[0], F_SETFL, flags | O_NONBLOCK);
	//	flags = fcntl(p[1], F_GETFL, 0);
	//	fcntl(p[1], F_SETFL, flags | O_NONBLOCK);
#endif
	populate_ports();
}

MIDI::~MIDI()
{
	pmesg(40, "MIDI::~MIDI()\n");
	ui->b_auto_detect->deactivate();
	stop_timer();
	if (selected_port_in != -1)
		Pm_Close(port_in);
	if (selected_port_out != -1)
		Pm_Close(port_out);
	if (selected_port_thru != -1)
		Pm_Close(port_thru);
	Pm_Terminate();
	// free buffers
	jack_ringbuffer_free(read_buffer);
	jack_ringbuffer_free(write_buffer);
}

void MIDI::nibble(int value, unsigned char& lsb, unsigned char& msb) const
{
	if (value < 0)
		value += 16384;
	msb = value / 128;
	lsb = value % 128;
}

// get available system midi ports
void MIDI::populate_ports()
{
	pmesg(40, "MIDI::populate_ports()\n");
	ui->midi_outs->clear();
	ui->midi_ins->clear();
	ui->midi_ctrl->clear();
	ports_out.clear();
	ports_in.clear();
	for (int i = 0; i < Pm_CountDevices(); i++)
	{
		const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
		pmesg(40, "%s %s ", info->interf, info->name);
		if (info->output)
		{
			pmesg(40, "Out\n");
			ui->midi_outs->add("foo");
			ui->midi_outs->replace(ui->midi_outs->size() - 2, info->name);
			ports_out.push_back(i);
			pd->display_status(info->name);
			Fl::wait();
		}
		else
		{
			ui->midi_ins->add("foo");
			ui->midi_ins->replace(ui->midi_ins->size() - 2, info->name);
			ui->midi_ctrl->add("foo");
			ui->midi_ctrl->replace(ui->midi_ctrl->size() - 2, info->name);
			ports_in.push_back(i);
			pd->display_status(info->name);
			Fl::wait();
			pmesg(40, "In\n");
		}
	}
}

void MIDI::set_device_id(int id)
{
	pmesg(40, "MIDI::set_device_id(%d)\n", id);
	midi_device_id = id;
}

// start realtime receiver
int MIDI::start_timer()
{
	if (timer_running)
		return 1;
	pmesg(40, "MIDI::start_timer()\n");
	// initialize timout or filedescriptors for IPC
#ifndef WINDOWS
	Fl::add_fd(p[0], process_midi_in);
#else
	Fl::add_timeout(.1, process_midi_in);
#endif
	// start timer, clean up if we couldnt
	if (Pt_Start(1, &process_midi, 0) < 0)
	{
#ifndef WINDOWS
		Fl::remove_fd(p[0]);
		close(p[0]);
		close(p[1]);
#else
		Fl::remove_timeout(process_midi_in, 0);
#endif
		pd->display_status("*** Could not start MIDI timer.", true);
		return 0;
	}
	timer_running = true;
	return 1;
}

// stop realtime receiver
void MIDI::stop_timer()
{
	if (!timer_running)
		return;
	pmesg(40, "MIDI::stop_timer()\n");
	process_midi_exit_flag = false;
	thru_active = false;
	midi_active = false;
	while (!process_midi_exit_flag)
		Fl::wait(.1);
	Pt_Stop();
#ifndef WINDOWS
	Fl::remove_fd(p[0]);
	close(p[0]);
	close(p[1]);
#else
	Fl::remove_timeout(process_midi_in, 0);
#endif
	timer_running = false;
}

int MIDI::connect_out(int port)
{
	pmesg(40, "MIDI::connect_out(port: %d)\n", port);
	if (port < 0 || port >= (int) ports_out.size())
		return 0;
	if (midi_active)
	{
		ui->b_auto_detect->deactivate();
		process_midi_exit_flag = false;
		thru_active = false;
		midi_active = false;
		while (!process_midi_exit_flag)
			Fl::wait(.1);
	}
	if (!start_timer())
		return 0;
	pmerror = Pm_Initialize(); // start portmidi
	if (pmerror < 0)
	{
		show_error();
		return 0;
	}
	if (selected_port_out != -1)
	{
		pmerror = Pm_Close(port_out);
		if (pmerror < 0)
		{
			show_error();
			return 0;
		}
	}
	if (selected_port_out == port)
	{
		selected_port_out = -1;
		return 0;
	}
	// device ID validation
	try
	{
		ports_out.at(port);
	} catch (...)
	{
		return 0;
	}
	pmerror = Pm_OpenOutput(&port_out, ports_out.at(port), NULL, 0, NULL, NULL,
			0); // open the port
	if (pmerror < 0)
	{
		show_error();
		return 0;
	}
	selected_port_out = port;
	if (selected_port_in != -1)
	{
		midi_active = true;
		ui->b_auto_detect->activate();
		request_device_inquiry();
	}
	if (selected_port_thru != -1)
		thru_active = true;
	return 1;
}

int MIDI::connect_in(int port)
{
	pmesg(40, "MIDI::connect_in(port: %d)\n", port);
	if (port < 0 || port >= (int) ports_in.size())
		return 0;
	if (port == selected_port_thru)
	{
		fl_message("In-port must be different from Ctrl-port.");
		return 0;
	}
	if (midi_active)
	{
		ui->b_auto_detect->deactivate();
		process_midi_exit_flag = false;
		midi_active = false;
		while (!process_midi_exit_flag)
			Fl::wait(.1);
	}
	if (!start_timer())
		return 0;
	pmerror = Pm_Initialize(); // start portmidi
	if (pmerror < 0)
	{
		show_error();
		return 0;
	}
	if (selected_port_in != -1)
	{
		pmerror = Pm_Close(port_in);
		if (pmerror < 0)
		{
			show_error();
			return 0;
		}
	}
	if (selected_port_in == port)
	{
		selected_port_in = -1;
		return 0;
	}
	// device ID validation
	try
	{
		ports_in.at(port);
	} catch (...)
	{
		return 0;
	}
	pmerror = Pm_OpenInput(&port_in, ports_in.at(port), NULL, 512, NULL, NULL);
	if (pmerror < 0)
	{
		show_error();
		return 0;
	}
	// filter messages we dont process
	pmerror = Pm_SetFilter(
			port_in,
			PM_FILT_SYSTEMCOMMON | PM_FILT_ACTIVE | PM_FILT_CLOCK
					| PM_FILT_PLAY | PM_FILT_TICK | PM_FILT_FD | PM_FILT_RESET
					| PM_FILT_AFTERTOUCH | PM_FILT_PROGRAM | PM_FILT_PITCHBEND);
	if (pmerror < 0)
	{
		show_error();
		return 0;
	}
	selected_port_in = port;
	if (selected_port_out != -1)
	{
		midi_active = true;
		ui->b_auto_detect->activate();
		request_device_inquiry();
	}
	return 1;
}

// connect midi thru
int MIDI::connect_thru(int port)
{
	pmesg(40, "MIDI::connect_thru(port: %d)\n", port);
	if (port < 0 || port >= (int) ports_in.size())
		return 0;
	if (port == selected_port_in)
	{
		fl_message("Ctrl-port must be different from In-port");
		return 0;
	}
	if (thru_active)
	{
		thru_active = false;
		mysleep(20);
	}
	if (!start_timer())
		return 0;
	pmerror = Pm_Initialize(); // start portmidi
	if (pmerror < 0)
	{
		show_error();
		return 0;
	}
	if (selected_port_thru != -1)
	{
		pmerror = Pm_Close(port_thru);
		if (pmerror < 0)
		{
			show_error();
			return 0;
		}
	}
	if (selected_port_thru == port)
	{
		selected_port_thru = -1;
		return 0;
	}
	// device ID validation
	try
	{
		ports_in.at(port);
	} catch (...)
	{
		return 0;
	}
	pmerror
			= Pm_OpenInput(&port_thru, ports_in.at(port), NULL, 512, NULL, NULL);
	if (pmerror < 0)
	{
		show_error();
		return 0;
	}
	// filter messages we dont process
	pmerror = Pm_SetFilter(port_thru, PM_FILT_REALTIME | PM_FILT_SYSTEMCOMMON);
	if (pmerror < 0)
	{
		show_error();
		return 0;
	}
	selected_port_thru = port;
	if (selected_port_out != -1)
	{
		cfg->get_cfg_option(CFG_AUTOMAP) ? automap = true : automap = false;
		thru_active = true;
	}
	set_control_channel_filter(cfg->get_cfg_option(CFG_CONTROL_CHANNEL));
	return 1;
}

void MIDI::set_control_channel_filter(int channel)
{
	pmesg(40, "MIDI::set_control_channel_filter(%d)\n", channel);
	cfg->set_cfg_option(CFG_CONTROL_CHANNEL, channel);
	if (channel == 16)
		pmerror = Pm_SetChannelMask(
				port_thru,
				Pm_Channel(0) | Pm_Channel(1) | Pm_Channel(2) | Pm_Channel(3)
						| Pm_Channel(4) | Pm_Channel(5) | Pm_Channel(6)
						| Pm_Channel(7) | Pm_Channel(8) | Pm_Channel(9)
						| Pm_Channel(10) | Pm_Channel(11) | Pm_Channel(12)
						| Pm_Channel(13) | Pm_Channel(14) | Pm_Channel(15));
	else
		pmerror = Pm_SetChannelMask(port_thru, Pm_Channel(channel));
	if (pmerror < 0)
		show_error();
}

void MIDI::set_channel_filter(int channel)
{
	pmesg(40, "MIDI::set_channel_filter(%d)\n", channel);
	pmerror = Pm_SetChannelMask(port_in, Pm_Channel(channel));
	if (pmerror < 0)
		show_error();
}

void MIDI::write_sysex(const unsigned char* sysex, size_t len) const
{
	pmesg(40, "MIDI::write_sysex(data, len: %d)\n", len);
	// dont use if (!midi_active) here because we wanna be able to send
	// everything important (eg the setup_init) before the midi thread goes void
	static unsigned long count = 0;
	if (len > SYSEX_MESSAGE_BUFFER)
	{
		pmesg(1, "*** SYSEX_MESSAGE_BUFFER (%d) too small (%d) for writer\n",
				SYSEX_MESSAGE_BUFFER, len);
		overflows |= 2;
#ifndef WINDOWS
		show_error();
#endif
		return;
	}
	while (jack_ringbuffer_write_space(write_buffer) < len)
		Fl::wait(.1);
	jack_ringbuffer_write(write_buffer, sysex, len);
	// log sysex messages
	++count;
	if (ui->log_sysex_out->value())
	{
		char* buf = new char[2 * len + 17];
		int n = snprintf(buf, 15, "\nOS.%lu ", count);
		for (int i = 0; i < len; i++)
			sprintf(n + buf + 2 * i, "%02X", sysex[i]);
		ui->logbuf->append(buf);
		delete[] buf;
		ui->log->insert_position(ui->logbuf->length());
		if (!ui->scroll_lock->value())
			ui->log->show_insert_position();
	}
}

void MIDI::write_event(int status, int value1, int value2, int channel) const
{
	pmesg(40, "MIDI::write_event(%X, %X, %X, %d)\n", status, value1, value2,
			channel);
	static unsigned long count = 0;
	if (!midi_active)
		return;
	if (channel == -1)
		channel = pd->selected_channel;
	unsigned char stat = ((status & ~0xf) | channel) & 0xff;
	const unsigned char msg[] =
	{ stat, value1 & 0xff, value2 & 0xff };
	while (jack_ringbuffer_write_space(write_buffer) < 3)
	{
		pd->display_status("Throttling upload...", true);
		Fl::wait(.1);
	}
	jack_ringbuffer_write(write_buffer, msg, 3);
	// log midi events
	++count;
	if (ui->log_events_out->value())
	{
		char buf[29];
		snprintf(buf, 29, "\nOE.%lu %02X%02X%02X", count, stat, value1, value2);
		ui->logbuf->append(buf);
		ui->log->insert_position(ui->logbuf->length());
		if (!ui->scroll_lock->value())
			ui->log->show_insert_position();
	}
}

void MIDI::ack(int packet) const
{
	pmesg(40, "MIDI::ack(packet: %d) \n", packet);
	uchar ack[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x7f, packet % 128, packet / 128,
			0xf7 };
	write_sysex(ack, 9);
}
void MIDI::nak(int packet) const
{
	pmesg(40, "MIDI::nak(packet: %d) \n", packet);
	uchar nak[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x7e, packet % 128, packet / 128,
			0xf7 };
	write_sysex(nak, 9);
}
void MIDI::cancel() const
{
	pmesg(40, "MIDI::cancel() \n");
	uchar cancel[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x7d, 0xf7 };
	write_sysex(cancel, 7);
}
void MIDI::wait() const
{
	pmesg(40, "MIDI::wait() \n");
	uchar wait[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x7c, 0xf7 };
	write_sysex(wait, 7);
}
void MIDI::eof() const
{
	pmesg(40, "MIDI::eof() \n");
	uchar endof[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x7b, 0xf7 };
	write_sysex(endof, 7);
}

void MIDI::request_device_inquiry(int id) const
{
	pmesg(40, "MIDI::request_device_inquiry(%d) \n", id);
	if (!midi_active)
		return;
	int check_id = id;
	if (check_id == -1)
		check_id = midi_device_id;
	uchar request[] =
	{ 0xf0, 0x7e, check_id, 0x06, 0x01, 0xf7 };
	write_sysex(request, 6);
	requested = true;
}

void MIDI::request_hardware_config() const
{
	pmesg(40, "MIDI::request_hardware_config() \n");
	if (requested)
		return;
	uchar request[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x0a, 0xf7 };
	write_sysex(request, 7);
	requested = true;
}

void MIDI::request_preset_dump(int preset, int rom_id) const
{
	pmesg(40, "MIDI::request_preset_dump(preset: %d, rom: %d) \n", preset,
			rom_id);
	if (requested)
		return;
	if (preset < 0)
		preset += 16384;
	else
		seed_randomizer = true;
	uchar request[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x11, cfg->get_cfg_option(
			CFG_CLOSED_LOOP_DOWNLOAD) ? 0x02 : 0x04, preset % 128,
			preset / 128, rom_id % 128, rom_id / 128, 0xf7 };
	pd->loading();
	write_sysex(request, 12);
	requested = true;
}

void MIDI::request_setup_dump() const
{
	pmesg(40, "MIDI::request_setup_dump() \n");
	if (requested || !midi_active)
		return;
	// now we know that we have initialzed we don't accept any more name data
	requested_names = 0;
	uchar request[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x1d, 0xf7 };
	write_sysex(request, 7);
	requested = true;
}

void MIDI::request_arp_dump(int number, int rom_id) const
{
	if (!midi_active)
		return;
	pmesg(40, "MIDI::request_arp_dump(#: %d, rom: %d)\n", number, rom_id);
	if (number < 0)
		number += 16384;
	uchar request[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x19, number % 128, number / 128,
			rom_id % 128, rom_id / 128, 0xf7 };
	write_sysex(request, 11);
}

void MIDI::request_name(int type, int number, int rom_id) const
{
	if (!midi_active)
		return;
	pmesg(40, "MIDI::request_name(type: %d, #: %d, rom_id: %d)\n", type,
			number, rom_id);
	uchar request[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x0c, type, number % 128, number
			/ 128, rom_id % 128, rom_id / 128, 0xf7 };
	write_sysex(request, 12);
	requested_names++;
}

void MIDI::edit_parameter_value(int id, int value) const
{
	if (!midi_active)
		return;
	pmesg(40, "MIDI::edit_parameter_value(id: %d, value: %d) \n", id, value);
	static unsigned char plsb, pmsb, vlsb, vmsb;
	nibble(id, plsb, pmsb);
	nibble(value, vlsb, vmsb);
	uchar request[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x01, 0x02, plsb, pmsb, vlsb,
			vmsb, 0xf7 };
	write_sysex(request, 12);
	// display request message in menu
	static char buf[25];
	for (size_t i = 0; i < 12; i++)
		sprintf(buf + 2 * i, "%02X", request[i]);
	buf[24] = '\0';
	pd->display_status(buf);
}

void MIDI::master_volume(int volume) const
{
	if (!midi_active)
		return;
	pmesg(40, "MIDI::master_volume() \n");
	uchar master_vol[] =
			{ 0xf0, 0x7f, midi_device_id, 0x04, 0x01, volume % 128, volume
					/ 128, 0xf7 };
	write_sysex(master_vol, 8);
}

void MIDI::copy(int cmd, int src, int dst, int src_l, int dst_l, int rom_id) const
{
	if (!midi_active)
		return;
	pmesg(
			40,
			"MIDI::copy(cmd: %X, src: %d, src_l: %d, dst: %d, dst_l: %d, rom: %d)\n",
			cmd, src, src_l, dst, dst_l, rom_id);
	if (dst < 0)
		dst += 16384;
	if (src < 0)
		src += 16384;
	// layer dependent
	if (cmd > 0x24 && cmd < 0x2b)
	{
		uchar copy[] =
		{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, cmd, src % 128, src / 128,
				src_l, 0, dst % 128, dst / 128, dst_l, 0, rom_id % 128, rom_id
						/ 128, 0xf7 };
		write_sysex(copy, 17);
	}
	// layer independent
	else
	{
		if (cmd == 0x2c) // copy setup
		{
			uchar copy[] =
			{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, cmd, src % 128,
					src / 128, dst % 128, dst / 128, 0xf7 };
			write_sysex(copy, 11);
			// set device id to our chosen device id
			// so it will respond to our requests
			edit_parameter_value(388, midi_device_id);
			// set sysex delay to our chosen setting
			edit_parameter_value(405, request_delay);
		}
		else
		{
			uchar copy[] =
			{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, cmd, src % 128,
					src / 128, dst % 128, dst / 128, rom_id % 128,
					rom_id / 128, 0xf7 };
			write_sysex(copy, 13);
		}
	}
}

void MIDI::audit() const
{
	if (!midi_active)
		return;
	pmesg(40, "MIDI::audit()\n");
	// open session
	uchar os[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x40, 0x10, 0xf7 };
	write_sysex(os, 8);
	uchar press[] =
			{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x40, 0x20, 0x04, 0x0,
					0x01, 0xf7 };
	write_sysex(press, 11);
	uchar cs[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x40, 0x11, 0xf7 };
	write_sysex(cs, 8);
}

void MIDI::randomize() const
{
	pmesg(40, "MIDI::randomize()\n");
	if (seed_randomizer)
	{
#ifdef WINDOWS
		srand(time(0));
		int byte1 = rand() % 16384;
		int byte2 = rand() % 16384;
#else
		srandom(time(0));
		int byte1 = random() % 16384;
		int byte2 = random() % 16384;
#endif
		uchar seedr[] =
		{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x72, 0x7f, 0x7f, 0, 0, byte1
				% 128, byte1 / 128, byte2 % 128, byte2 / 128, 0xf7 };
		write_sysex(seedr, 15);
		seed_randomizer = false;
	}
	uchar randomize[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x71, 0x7f, 0x7f, 0, 0, 0xf7 };
	write_sysex(randomize, 11);

	request_preset_dump(-1, 0);
}
