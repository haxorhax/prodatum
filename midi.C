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

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <FL/fl_ask.H>

#include "ringbuffer.H"
#include "ui.H"

extern PD_UI* ui;
extern Cfg* cfg;
extern PXK* pxk;

volatile bool got_answer;
extern volatile bool join_bro;

static bool timer_running = false;
static bool midi_active = false;
static bool thru_active = false;
static bool process_midi_exit_flag = false;
static bool automap = true;

// check buffer spaces
#ifdef SYNCLOG
unsigned int write_space = RINGBUFFER_WRITE;
unsigned int read_space = RINGBUFFER_READ;
unsigned int max_write = 0;
unsigned int max_read = 0;
#endif

/**
 * midi core implementation (sender/receiver/decoder).
 * this is where all MIDI bytes (outgoing and incoming) pass through.
 * this thread should never lock
 * in the linux version a pipe is used to notify \c process_midi_in
 * for new messages.
 */
static void process_midi(PtTimestamp, void*);
/*! \fn process_midi_in
 * connects the MIDI receiver with the main thread.
 * all incoming MIDI messages are passed to this function via the
 * read buffer. runs in the main thread.
 * in the linux version this gets notified by \c process_midi if a new message
 * is available. on windows and mac this is a timeout function, repeated
 * until we close our MIDI ports
 */
#ifdef __linux
static void process_midi_in(int fd, void*);
static int p[2];
#else
static void process_midi_in(void*);
#endif

static PmError pmerror = pmNoError;
static PortMidiStream *port_in;
static PortMidiStream *port_out;
static PortMidiStream *port_thru; // controller port (eg keyboard)

static jack_ringbuffer_t *read_buffer;
static jack_ringbuffer_t *write_buffer;
volatile static unsigned char midi_device_id = 127;
static bool requested = false;

static void show_error(void)
{
	char* __buffer = (char*) malloc(256 * sizeof(char));
	if (pmerror == -10000)
		Pm_GetHostErrorText(__buffer, 256);
	else if (pmerror < 0)
		snprintf(__buffer, 256, "%s", Pm_GetErrorText(pmerror));
	pmerror = pmNoError;
	fprintf(stderr, "%s", __buffer);
#ifdef WIN32
	fflush(stderr);
#endif
	free(__buffer);
}

static bool __midi_wait = false;
static void process_midi(PtTimestamp, void*)
{
	PmEvent ev;
	static unsigned char event[4]; // 3 midi bytes, one byte to distinguish device (0) and controller (1) events
	static unsigned char data, shift;
	// we only want to put full sysex messages in our ringbuffer
	// these are the buffers to store sysex chunks locally
	static unsigned char local_read_buffer[SYSEX_MAX_SIZE];
	static unsigned char local_write_buffer[SYSEX_MAX_SIZE];
	static unsigned int position = 3;
	static bool receiving_sysex = false;
	static bool result_out = false;
	static unsigned char poll = 0;
	if (!midi_active)
	{
		if (!jack_ringbuffer_read_space(write_buffer))
		{
			process_midi_exit_flag = true;
			__midi_wait = false;
			receiving_sysex = false;
			position = 3;
			result_out = false;
			poll = 0;
			return;
		}
	}
	do
	{
		// check if theres something from the device and write it to the read_buffer
		while (midi_active && Pm_Poll(port_in))
		{
			pmerror = (PmError) Pm_Read(port_in, &ev, 1);
			if (pmerror < 0)
			{
				show_error();
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
					if (receiving_sysex) //  Overlapping sysex messages!
						receiving_sysex = false;
					// filter sysex
					// e-mu proteus
					if (((ev.message >> 8) & 0xFF) == 0x18 && ((ev.message >> 16) & 0xFF) == 0x0F
							&& ((ev.message >> 24) & 0xFF) == midi_device_id)
						receiving_sysex = true;
					// universal sysex
					else if (((ev.message >> 8) & 0xFF) == 0x7E)
						receiving_sysex = true;
					if (receiving_sysex)
					{
						position = 3;
						goto Copy;
					}
					else
						break;
				}
				// check for truncated sysex
				if (receiving_sysex && (((data & 0x80) == 0 || data == 0xF7) || position == 3))
				{
					// copy data
					if (position < SYSEX_MAX_SIZE)
					{
						Copy: local_read_buffer[position++] = data;
						// hand over a complete sysex message to the main thread
						if (data == MIDI_EOX)
						{
							// check if its a WAIT command
							if (local_read_buffer[4] == 0x55 && local_read_buffer[5] == 0x7c)
							{
								__midi_wait = true;
								receiving_sysex = false;
								break;
							}
							// check if it's an ack command
							else if (__midi_wait == true && local_read_buffer[4] == 0x55 && local_read_buffer[5] == 0x7f)
								__midi_wait = false;
#ifdef SYNCLOG
							if (read_space > (jack_ringbuffer_write_space(read_buffer) - position))
								read_space = jack_ringbuffer_write_space(read_buffer) - position;
							if (max_read < position + 3)
								max_read = position + 3;
#endif
							// write a header with length info
							local_read_buffer[0] = MIDI_SYSEX;
							local_read_buffer[1] = (position - 3) / 128;
							local_read_buffer[2] = (position - 3) % 128;
							jack_ringbuffer_write(read_buffer, local_read_buffer, position);
#ifdef __linux
							write(p[1], " ", 1);
#endif
							receiving_sysex = false;
							break;
						}
					} // (position < SYSEX_MAX_SIZE)
					else
					{
						receiving_sysex = false;
						break;
					}
				}
				// voice message
				else if (shift == 0 && data > 0x7F && data < 0xF0)
				{
					event[0] = Pm_MessageStatus(ev.message);
					event[1] = Pm_MessageData1(ev.message);
					event[2] = Pm_MessageData2(ev.message);
					event[3] = 0;
					jack_ringbuffer_write(read_buffer, event, 4);
#ifdef __linux
					write(p[1], " ", 1);
#endif
					break;
				}
				else
					break;
			}
		}
		// check if theres something from the controller
		if (thru_active && Pm_Poll(port_thru))
		{
			pmerror = (PmError) Pm_Read(port_thru, &ev, 1);
			if (!(pmerror < 0)) // no error
			{
				event[0] = Pm_MessageStatus(ev.message);
				// voice messages
				if (event[0] >= 0x80 && event[0] <= 0xEF)
				{
					// automap
					if (automap && pxk->midi_mode != OMNI)
						event[0] = (event[0] & ~0xf) | (pxk->selected_channel & 0xff);
					event[1] = Pm_MessageData1(ev.message);
					event[2] = Pm_MessageData2(ev.message);
					event[3] = 1;
					// write to ringbuffer for internal processing
					jack_ringbuffer_write(read_buffer, event, 4);
#ifdef __linux
					write(p[1], " ", 1);
#endif
					ev.message = Pm_Message(event[0], event[1], event[2]);
				}
				// forward message
				pmerror = (PmError) Pm_Write(port_out, &ev, 1);
				if (pmerror < 0)
					show_error();
			}
			else
				// pmerror = (PmError) Pm_Read(port_thru, &ev, 1);
				show_error();
		}

		// check if theres some MIDI to write on the bus
		result_out = false;
		if (jack_ringbuffer_peek(write_buffer, &poll, 1) == 1)
		{
			result_out = true;
			if (poll == MIDI_SYSEX)
			{
				if (!__midi_wait)
				{
					jack_ringbuffer_read(write_buffer, local_write_buffer, 3); // read header
					unsigned int len = local_write_buffer[1] * 128 + local_write_buffer[2];
					jack_ringbuffer_read(write_buffer, local_write_buffer, len);
					pmerror = Pm_WriteSysEx(port_out, 0, local_write_buffer);
					if (pmerror < 0)
						show_error();
				}
			}
			else if (jack_ringbuffer_read(write_buffer, event, 3) == 3)
			{
				ev.message = Pm_Message(event[0], event[1], event[2]);
				pmerror = Pm_Write(port_out, &ev, 1);
				if (pmerror < 0)
					show_error();
			}
		}
	} while (result_out);
}

#ifdef __linux
static void process_midi_in(int fd, void*)
#else
static void process_midi_in(void*)
#endif
{
	static unsigned long count_events = 0;
	static unsigned char sysex[SYSEX_MAX_SIZE];
	static unsigned int len;
	unsigned char poll = 0;
#ifdef __linux
	static char buf;
#endif
	while (midi_active && jack_ringbuffer_peek(read_buffer, &poll, 1) == 1)
	{
#ifdef __linux
		read(fd, &buf, 1);
#endif
		if (poll == MIDI_SYSEX)
		{
			jack_ringbuffer_read(read_buffer, sysex, 3); // read header
			len = sysex[1] * 128 + sysex[2];
			jack_ringbuffer_read(read_buffer, sysex, len);
			if (join_bro)
				break;
			// e-mu sysex
			if (sysex[1] == 0x18)
			{
				switch (sysex[5])
				{
					case 0x0b: // generic name
						if (!pxk->Synchronized())
						{
							got_answer = true;
							pxk->incoming_generic_name(sysex);
						}
						break;
					case 0x10: // preset dumps
						if (requested)
							switch (sysex[6])
							{
								case 0x01: // dump header (closed)
								case 0x03: // dump header (open)
									pxk->incoming_preset_dump(sysex, len);
									break;
								case 0x02: // dump data (closed)
									pxk->incoming_preset_dump(sysex, len);
									break;
								case 0x04: // dump data (open)
									pxk->incoming_preset_dump(sysex, len);
									if (len < 253) // last packet
									{
										got_answer = true;
										requested = false;
									}
									break;
							}
						break;

					case 0x7f: // ACK
						pxk->incoming_ACK(sysex[7] * 128 + sysex[6]);
						break;

					case 0x7e: // NAK
						pxk->incoming_NAK(sysex[7] * 128 + sysex[6]);
						break;

					case 0x7b: // EOF
						got_answer = true;
						requested = false;
						break;

					case 0x1c: // setup dumps
						if (requested)
						{
							got_answer = true;
							requested = false;
							pxk->incoming_setup_dump(sysex, len);
						}
						break;

					case 0x18: // arp pattern dump
						if (requested)
						{
							got_answer = true;
							requested = false;
							pxk->incoming_arp_dump(sysex, len);
						}
						break;

					case 0x09: // hardware configuration
						if (!pxk->Synchronized() && requested)
						{
							requested = false;
							pxk->incoming_hardware_config(sysex);
						}
						break;

					case 0x7d: // CANCEL
						got_answer = true;
						requested = false;
						pxk->display_status("Device sent CANCEL.");
						ui->supergroup->clear_output();
						break;
					case 0x70: // ERROR
					case 0x40: // remote front panel control command
						break;

					default:
#ifdef SYNCLOG
						ui->init_log->append("\nprocess_midi_in: Received unrecognized e-mu sysex:\n");
						char* __buffer = (char*) malloc(len * sizeof(char));
						for (unsigned int i = 0; i < len; i++)
							snprintf(__buffer + i, 1, "%02hhX", *(sysex + i));
						ui->init_log->append(__buffer);
						ui->init_log->append("\n");
						free(__buffer);
#endif
						break;
				}
				pxk->log_add(sysex, len, 1);
			}
			// universal sysex
			else if (sysex[1] == 0x7e)
			{
				//pmesg("process_midi_in: received MIDI standard universal message: ");
				// device inquiry
				if (sysex[3] == 0x06 && sysex[4] == 0x02 && sysex[5] == 0x18)
				{
					//pmesg("device inquiry response\n");
					if (!pxk->Synchronized())
						pxk->incoming_inquiry_data(sysex);
				}
				pxk->log_add(sysex, len, 1);
			}
#ifdef SYNCLOG
			else
			{
				ui->init_log->append("\nprocess_midi_in: Received unknown sysex:\n");
				char* __buffer = (char*) malloc(len * sizeof(char));
				for (unsigned int i = 0; i < len; i++)
					snprintf(__buffer + i, 1, "%02hhX", *(sysex + i));
				ui->init_log->append(__buffer);
				ui->init_log->append("\n");
				free(__buffer);
			}
#endif
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
						ui->arp_mp->activate_key(-1, event[1]);
						break;
					case 0x9: // note-on
						if (event[2] == 0)
						{
							ui->piano->activate_key(-1, event[1]);
							ui->main->minipiano->activate_key(-1, event[1]);
							ui->global_minipiano->activate_key(-1, event[1]);
							ui->arp_mp->activate_key(-1, event[1]);
						}
						else
						{
							ui->piano->activate_key(1, event[1]);
							ui->main->minipiano->activate_key(1, event[1]);
							ui->global_minipiano->activate_key(1, event[1]);
							ui->arp_mp->activate_key(1, event[1]);
						}
						break;
					case 0xb: // controller event
					{
						if (pxk->cc_to_ctrl.find(event[1]) != pxk->cc_to_ctrl.end())
						{
							int controller = pxk->cc_to_ctrl[event[1]];
							if (controller <= 12)
								// sliders
								((Fl_Slider*) ui->main->ctrl_x[controller])->value((double) event[2]);
							else
								// footswitches
								((Fl_Button*) ui->main->ctrl_x[controller])->value(event[2] > 63 ? 1 : 0);
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
						ui->arp_mp->activate_key(-3, event[1]);
						break;
					case 0x9: // note-on
						if (event[2] == 0)
						{
							ui->piano->activate_key(-2, event[1]);
							ui->main->minipiano->activate_key(-2, event[1]);
							ui->global_minipiano->activate_key(-2, event[1]);
							ui->arp_mp->activate_key(-2, event[1]);
						}
						else
						{
							ui->piano->activate_key(2, event[1]);
							ui->main->minipiano->activate_key(2, event[1]);
							ui->global_minipiano->activate_key(2, event[1]);
							ui->arp_mp->activate_key(2, event[1]);
						}
						break;
					case 0xb: // controller event
					{
						if (pxk->cc_to_ctrl.find(event[1]) != pxk->cc_to_ctrl.end())
						{
							int controller = pxk->cc_to_ctrl[event[1]];
							if (controller <= 12)
								// sliders
								((Fl_Slider*) ui->main->ctrl_x[controller])->value((double) event[2]);
							else
								// footswitches
								((Fl_Button*) ui->main->ctrl_x[controller])->value(event[2] > 63 ? 1 : 0);
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
				}
			}
			// log midi events
			if (cfg->get_cfg_option(CFG_LOG_EVENTS_IN))
			{
				char _b[30];
				snprintf(_b, 30, "\nIE.%lu::%02X%02X%02X", ++count_events, event[0], event[1], event[2]);
				ui->logbuf->append(_b);
			}
		}
	}
#ifndef __linux
	if (timer_running)
	Fl::repeat_timeout(.01, process_midi_in);
#endif
}

// ########################################
// midi connection class member definitions
// ########################################
MIDI::MIDI()
{
	pmesg("MIDI::MIDI()\n");
	// initialize (global) variables and buffers
	selected_port_out = -1;
	port_out = 0;
	selected_port_in = -1;
	port_in = 0;
	selected_port_thru = -1;
	port_thru = 0;
#ifdef __linux
	if (pipe(p) == -1)
		fprintf(stderr, "*** Could not open pipe\n%s", strerror(errno));
#endif
	read_buffer = jack_ringbuffer_create(RINGBUFFER_READ);
	write_buffer = jack_ringbuffer_create(RINGBUFFER_WRITE);
#ifdef USE_MLOCK
	jack_ringbuffer_mlock(write_buffer);
	jack_ringbuffer_mlock(read_buffer);
#endif
	// populate ports
	pxk->display_status("Populating MIDI ports...");
	Fl::flush();
	for (unsigned char i = 0; i < Pm_CountDevices(); i++)
	{
		const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
		if (info->output)
		{
			ui->midi_outs->add("foo");
			ui->midi_outs->replace(ui->midi_outs->size() - 2, info->name);
			ports_out.push_back(i);
		}
		else
		{
			ui->midi_ins->add("foo");
			ui->midi_ins->replace(ui->midi_ins->size() - 2, info->name);
			ui->midi_ctrl->add("foo");
			ui->midi_ctrl->replace(ui->midi_ctrl->size() - 2, info->name);
			ports_in.push_back(i);
		}
	}
	pxk->display_status(0);
	Fl::flush();
}

MIDI::~MIDI()
{
	pmesg("MIDI::~MIDI()\n");
	stop_timer();
	jack_ringbuffer_free(read_buffer);
	jack_ringbuffer_free(write_buffer);
}

bool MIDI::in()
{
	if (selected_port_in != -1)
		return true;
	return false;
}

bool MIDI::out()
{
	if (selected_port_out != -1)
		return true;
	return false;
}

bool MIDI::Wait()
{
	return __midi_wait;
}

void MIDI::Wait(bool wait)
{
	__midi_wait = wait;
}

void MIDI::set_device_id(unsigned char id)
{
	pmesg("MIDI::set_device_id(%d)\n", id);
	midi_device_id = id;
	// sysex packet delay
	edit_parameter_value(405, cfg->get_cfg_option(CFG_SPEED));
}

// start realtime receiver
int MIDI::start_timer()
{
	if (timer_running)
		return 1;
	pmesg("MIDI::start_timer()\n");
	// initialize timout or filedescriptors for IPC
#ifdef __linux
	Fl::add_fd(p[0], process_midi_in);
#else
	Fl::add_timeout(0, process_midi_in);
#endif
	// start timer, clean up if we couldnt
	if (Pt_Start(1, &process_midi, 0) < 0)
	{
#ifdef __linux
		Fl::remove_fd(p[0]);
		close(p[0]);
		close(p[1]);
#else
		Fl::remove_timeout(process_midi_in);
#endif
		pxk->display_status("*** Could not start MIDI timer.");
		fprintf(stderr, "*** Could not start MIDI timer.\n");
#ifdef WIN32
		fflush(stderr);
#endif
		Pt_Stop();
		return 0;
	}
	timer_running = true;
	Pm_Initialize(); // start portmidi
	return 1;
}

// stop realtime receiver
void MIDI::stop_timer()
{
	if (!timer_running)
		return;
	pmesg("MIDI::stop_timer()\n");
	process_midi_exit_flag = false;
	thru_active = false;
	timer_running = false;
	midi_active = false;
	while (!process_midi_exit_flag)
		mysleep(10);
#ifdef __linux
	Fl::remove_fd(p[0]);
	close(p[0]);
	close(p[1]);
#else
	Fl::remove_timeout(process_midi_in);
#endif
	Pm_Terminate();
	Pt_Stop();
	if (selected_port_in != -1)
		Pm_Close(port_in);
	if (selected_port_out != -1)
		Pm_Close(port_out);
	if (selected_port_thru != -1)
		Pm_Close(port_thru);
}

int MIDI::connect_out(int port)
{
	pmesg("MIDI::connect_out(port: %d)\n", port);
	if (port < 0 || port >= (int) ports_out.size())
		return 0;
	if (selected_port_out == port)
		return 1;
	if (midi_active)
	{
		process_midi_exit_flag = false;
		thru_active = false;
		midi_active = false;
		while (!process_midi_exit_flag)
			mysleep(10);
	}
	if (!start_timer())
		return 0;
	if (selected_port_out != -1)
	{
		pmerror = Pm_Close(port_out);
		if (pmerror < 0)
		{
			show_error();
			return 0;
		}
	}
	// device ID validation
	try
	{
		ports_out.at(port);
	} catch (...)
	{
		fprintf(stderr, "*** Selected MIDI port (%d, out) does not exist.\n", port);
#ifdef WIN32
		fflush(stderr);
#endif
		return 0;
	}
	pmerror = Pm_OpenOutput(&port_out, ports_out.at(port), NULL, 0, NULL, NULL, 0); // open the port
	if (pmerror < 0)
	{
		show_error();
		return 0;
	}
	selected_port_out = port;
	if (selected_port_in != -1)
		midi_active = true;
	if (selected_port_thru != -1)
		thru_active = true;
	return 1;
}

int MIDI::connect_in(int port)
{
	pmesg("MIDI::connect_in(port: %d)\n", port);
	if (port < 0 || port >= (int) ports_in.size())
		return 0;
	if (selected_port_in == port)
		return 1;
	if (port == selected_port_thru)
	{
		fl_message("In-port must be different from Ctrl-port.");
		return 0;
	}
	if (midi_active)
	{
		process_midi_exit_flag = false;
		midi_active = false;
		while (!process_midi_exit_flag)
			mysleep(10);
	}
	if (!start_timer())
		return 0;
	if (selected_port_in != -1)
	{
		pmerror = Pm_Close(port_in);
		if (pmerror < 0)
		{
			show_error();
			return 0;
		}
	}
	// device ID validation
	try
	{
		ports_in.at(port);
	} catch (...)
	{
		fprintf(stderr, "*** Selected MIDI port (%d, in) does not exist.\n", port);
#ifdef WIN32
		fflush(stderr);
#endif
		return 0;
	}
	pmerror = Pm_OpenInput(&port_in, ports_in.at(port), NULL, 512, NULL, NULL);
	if (pmerror < 0)
	{
		show_error();
		return 0;
	}
	// only allow sysex for now
	pmerror = Pm_SetFilter(port_in, ~1);
	if (pmerror < 0)
	{
		show_error();
		return 0;
	}
	selected_port_in = port;
	if (selected_port_out != -1)
		midi_active = true;
	return 1;
}

// connect midi thru
int MIDI::connect_thru(int port)
{
	pmesg("MIDI::connect_thru(port: %d)\n", port);
	if (port < 0 || port >= (int) ports_in.size())
		return 0;
	if (port == selected_port_in)
	{
		fl_message("Ctrl-port must be different from In-port");
		return 0;
	}
	if (thru_active)
		thru_active = false;
	if (!start_timer())
		return 0;
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
		fprintf(stderr, "*** Selected MIDI port (%d, thru) does not exist.\n", port);
#ifdef WIN32
		fflush(stderr);
#endif
		return 0;
	}
	pmerror = Pm_OpenInput(&port_thru, ports_in.at(port), NULL, 512, NULL, NULL);
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

void MIDI::set_control_channel_filter(int channel) const
{
	pmesg("MIDI::set_control_channel_filter(%d)\n", channel);
	cfg->set_cfg_option(CFG_CONTROL_CHANNEL, channel);
	if (channel == 16) // 0-15, single channels
		pmerror = Pm_SetChannelMask(port_thru, ~0); // all channels
	else
		pmerror = Pm_SetChannelMask(port_thru, Pm_Channel(channel));
	if (pmerror < 0)
		show_error();
}

void MIDI::set_channel_filter(int channel) const
{
	pmesg("MIDI::set_channel_filter(%d)\n", channel);
	if (port_in)
	{
		pmerror = Pm_SetChannelMask(port_in, Pm_Channel(channel));
		if (pmerror < 0)
			show_error();
	}
}

void MIDI::filter_loose() const
{
	pmesg("MIDI::filter_loose()\n");
	int filter = ~0; // filter everything
	filter ^= (PM_FILT_SYSEX + PM_FILT_NOTE + PM_FILT_CONTROL + PM_FILT_PITCHBEND);
	if (port_in)
	{
		pmerror = Pm_SetFilter(port_in, filter);
		if (pmerror < 0)
			show_error();
	}
	if (port_thru)
	{
		pmerror = Pm_SetFilter(port_thru, PM_FILT_REALTIME | PM_FILT_SYSTEMCOMMON);
		if (pmerror < 0)
			show_error();
	}
}

void MIDI::filter_strict() const
{
	pmesg("MIDI::filter_strict()\n");
	if (port_in)
	{
		pmerror = Pm_SetFilter(port_in, ~1); // only sysex on input
		if (pmerror < 0)
			show_error();
	}
	if (port_thru)
	{
		pmerror = Pm_SetFilter(port_thru, ~0); // everything on thru
		if (pmerror < 0)
			show_error();
	}
}

void MIDI::write_sysex(const unsigned char* sysex, unsigned int len) const
{
	//pmesg("MIDI::write_sysex(data, len: %d)\n", len);
	static unsigned char data[SYSEX_MAX_SIZE];
	if (!midi_active || len > SYSEX_MAX_SIZE)
		return;
	data[0] = MIDI_SYSEX;
	data[1] = len / 128;
	data[2] = len % 128;
	memcpy(data + 3, sysex, len);
#ifdef SYNCLOG
	if (write_space > jack_ringbuffer_write_space(write_buffer) - len - 3)
		write_space = jack_ringbuffer_write_space(write_buffer) - len - 3;
	if (max_write < len + 3)
		max_write = len + 3;
#endif
//	while (jack_ringbuffer_write_space(write_buffer) < len + 3)
//		Fl::wait(.1);
	jack_ringbuffer_write(write_buffer, data, len + 3);
	pxk->log_add(sysex, len, 0);
}

void MIDI::write_event(int status, int value1, int value2, int channel) const
{
	//pmesg("MIDI::write_event(%X, %X, %X, %d)\n", status, value1, value2, channel);
	if (!midi_active)
		return;
	if (channel == -1)
		channel = pxk->selected_channel;
	unsigned char stat = ((status & ~0xf) | channel) & 0xff;
	unsigned char v1 = value1 & 0xff;
	unsigned char v2 = value2 & 0xff;
	const unsigned char msg[] =
	{ stat, v1, v2 };
//	while (jack_ringbuffer_write_space(write_buffer) < 3)
//		Fl::wait(.1);
	jack_ringbuffer_write(write_buffer, msg, 3);
	// log midi events
	if (cfg->get_cfg_option(CFG_LOG_EVENTS_OUT))
	{
		static unsigned long count = 0;
		char buf[30];
		snprintf(buf, 30, "\nOE.%lu::%02x%02x%02x", ++count, stat, v1, v2);
		ui->logbuf->append(buf);
	}
}

void MIDI::ack(int packet) const
{
	pmesg("MIDI::ack(packet: %d) \n", packet);
	unsigned char l = packet % 128;
	unsigned char m = packet / 128;
	unsigned char a[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x7f, l, m, 0xf7 };
	write_sysex(a, 9);
}
void MIDI::nak(int packet) const
{
	pmesg("MIDI::nak(packet: %d) \n", packet);
	unsigned char l = packet % 128;
	unsigned char m = packet / 128;
	unsigned char n[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x7e, l, m, 0xf7 };
	write_sysex(n, 9);
}

void MIDI::eof() const
{
	pmesg("MIDI::eof() \n");
	got_answer = true;
	unsigned char endof[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x7b, 0xf7 };
	write_sysex(endof, 7);
}

void MIDI::request_hardware_config() const
{
	pmesg("MIDI::request_hardware_config() \n");
	if (requested)
		return;
	unsigned char request[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x0a, 0xf7 };
	write_sysex(request, 7);
	requested = true;
}

void request_preset_dump_timeout(void* m)
{
	((MIDI*) m)->request_preset_dump();
}

void MIDI::request_preset_dump(int timeout) const
{
	if (timeout > 0)
	{
		Fl::add_timeout((double) timeout / 1000, request_preset_dump_timeout, (void*) this);
		return;
	}
	if (requested)
		return;
	unsigned char loop = cfg->get_cfg_option(CFG_CLOSED_LOOP_DOWNLOAD) ? 0x02 : 0x04;
	unsigned char request[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x11, loop, 0x7f, 0x7f, 0, 0, 0xf7 };
	write_sysex(request, 12);
	pxk->Loading();
	requested = true;
}

void MIDI::request_setup_dump() const
{
	pmesg("MIDI::request_setup_dump() \n");
	pxk->display_status("Loading multisetup...");
	if (requested)
		return;
	unsigned char request[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x1d, 0xf7 };
	write_sysex(request, 7);
	requested = true;
}

void MIDI::request_arp_dump(int number, int rom_id) const
{
	//pmesg("MIDI::request_arp_dump(#: %d, rom: %d)\n", number, rom_id);
	if (join_bro)
		return;
	if (number < 0)
		number += 16384;
	unsigned char nl = number % 128;
	unsigned char nm = number / 128;
	unsigned char rl = rom_id % 128;
	unsigned char rm = rom_id / 128;
	unsigned char request[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x19, nl, nm, rl, rm, 0xf7 };
	write_sysex(request, 11);
	requested = true;
}

void MIDI::request_name(int type, int number, int rom_id) const
{
	//pmesg("MIDI::request_name(type: %d, #: %d, rom_id: %d)\n", type, number, rom_id);
	if (join_bro)
		return;
	unsigned char nl = number % 128;
	unsigned char nm = number / 128;
	unsigned char rl = rom_id % 128;
	unsigned char rm = rom_id / 128;
	unsigned char t = type & 0xff;
	unsigned char request[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x0c, t, nl, nm, rl, rm, 0xf7 };
	write_sysex(request, 12);
}

void MIDI::edit_parameter_value(int id, int value) const
{
	//pmesg("MIDI::edit_parameter_value(id: %d, value: %d) \n", id, value);
	if (id < 0)
		id += 16384;
	if (value < 0)
		value += 16384;
	unsigned char il = id % 128;
	unsigned char im = id / 128;
	unsigned char vl = value % 128;
	unsigned char vm = value / 128;
	unsigned char request[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x01, 0x02, il, im, vl, vm, 0xf7 };
	write_sysex(request, 12);
}

void MIDI::master_volume(int volume) const
{
	//pmesg("MIDI::master_volume(%d) \n", volume);
	unsigned char vl = volume % 128;
	unsigned char vm = volume / 128;
	unsigned char master_vol[] =
	{ 0xf0, 0x7f, midi_device_id, 0x04, 0x01, vl, vm, 0xf7 };
	write_sysex(master_vol, 8);
}

void MIDI::copy(int cmd, int src, int dst, int src_l, int dst_l, int rom_id) const
{
	//pmesg("MIDI::copy(cmd: %X, src: %d, src_l: %d, dst: %d, dst_l: %d, rom: %d)\n", cmd, src, src_l, dst, dst_l, rom_id);
	if (dst < 0)
		dst += 16384;
	if (src < 0)
		src += 16384;
	unsigned char c = cmd & 0xff;
	unsigned char rl = rom_id % 128;
	unsigned char rm = rom_id / 128;
	unsigned char srcl = src % 128;
	unsigned char srcm = src / 128;
	unsigned char dstl = dst % 128;
	unsigned char dstm = dst / 128;
	// layer dependent
	if (cmd > 0x24 && cmd < 0x2b)
	{
		unsigned char s_l = src_l & 0xff;
		unsigned char d_l = dst_l & 0xff;
		unsigned char cm[] =
		{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, c, srcl, srcm, s_l, 0, dstl, dstm, d_l, 0, rl, rm, 0xf7 };
		write_sysex(cm, 17);
	}
	// layer independent
	else
	{
		if (cmd == 0x2c) // copy setup
		{
			unsigned char cm[] =
			{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, c, srcl, srcm, dstl, dstm, 0xf7 };
			write_sysex(cm, 11);
			// set device id to our chosen device id
			// so it will respond to our requests
			edit_parameter_value(388, midi_device_id);
			// set sysex delay to our chosen setting
			edit_parameter_value(405, cfg->get_cfg_option(CFG_SPEED));
		}
		else
		{
			unsigned char cm[] =
			{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, c, srcl, srcm, dstl, dstm, rl, rm, 0xf7 };
			write_sysex(cm, 13);
		}
	}
}

void MIDI::audit() const
{
	pmesg("MIDI::audit()\n");
	// open session
	unsigned char os[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x40, 0x10, 0xf7 };
	write_sysex(os, 8);
	unsigned char press[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x40, 0x20, 0x04, 0x0, 0x01, 0xf7 };
	write_sysex(press, 11);
	unsigned char cs[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x40, 0x11, 0xf7 };
	write_sysex(cs, 8);
}

void MIDI::randomize() const
{
	pmesg("MIDI::randomize()\n");
	static bool seed = true;
	if (seed)
	{
		seed = false;
#ifdef WIN32
		srand(time(0));
		int byte1 = rand() % 16384;
		int byte2 = rand() % 16384;
#else
		srandom(time(0));
		int byte1 = random() % 16384;
		int byte2 = random() % 16384;
#endif
		unsigned char b1l = byte1 % 128;
		unsigned char b1m = byte1 / 128;
		unsigned char b2l = byte2 % 128;
		unsigned char b2m = byte2 / 128;
		unsigned char seedr[] =
		{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x72, 0x7f, 0x7f, 0, 0, b1l, b1m, b2l, b2m, 0xf7 };
		write_sysex(seedr, 15);
	}
	unsigned char r[] =
	{ 0xf0, 0x18, 0x0f, midi_device_id, 0x55, 0x71, 0x7f, 0x7f, 0, 0, 0xf7 };
	write_sysex(r, 11);
	request_preset_dump(300);
}
