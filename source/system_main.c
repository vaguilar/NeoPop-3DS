/* $NiH: system_main.c,v 1.50 2004/07/25 10:34:57 dillo Exp $ */
/*
  system_main.c -- main program
  Copyright (C) 2002-2004 Thomas Klausner and Dieter Baron

  This file is part of NeoPop-SDL, a NeoGeo Pocket emulator
  The author can be contacted at <wiz@danbala.tuwien.ac.at>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <sys/time.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include "config.h"
#include "NeoPop-SDL.h"

char *prg;
struct timeval throttle_last;
_u8 system_frameskip_key;
_u32 throttle_rate;
int do_exit = 0;
int paused = 0;
int have_sound;
void readrc(void);

struct PrintConsole print_console;
#define CONSOLE_WIDTH 37
#define CONSOLE_HEIGHT 30
#define TICKS_PER_SEC 268123480

static void
usage(int exitcode)
{
    printf("NeoGeo Pocket emulator\n\n"
	   "Usage: %s [-cefghjMmSsv] [-C mode] [-P port] [-R remove] [game]\n"
	   "\t-C mode\t\tspecify comms mode (none, server, client; default: none)\n"
	   "\t-c\t\tstart in colour mode (default: automatic)\n"
	   "\t-e\t\temulate English language NeoGeo Pocket (default)\n"
	   "\t-f count\tframeskip: show one in `count' frames (default: 1)\n"
	   "\t-g\t\tstart in greyscale mode (default: automatic)\n"
	   "\t-h\t\tshow this short help\n"
	   "\t-j\t\temulate Japanese language NeoGeo Pocket\n"
	   "\t-l state\tload start state from file `state'\n"
	   "\t-M\t\tdo not use smoothed magnification modes\n"
	   "\t-m\t\tuse smoothed magnification modes (default)\n"
	   "\t-P port\t\tspecify port number to use for comms (default: 7846)\n"
	   "\t-R host\t\tspecify host to connect to as comms client\n"
	   "\t-S\t\tsilent mode\n"
	   "\t-s\t\twith sound (default)\n"
	   "\t-v\t\tshow version number\n"
	   "\t-y mode\t\tspecify for which modes to use YUV\n", prg);

    exit(exitcode);
}

void
system_message(char *vaMessage, ...)
{
    va_list vl;

    va_start(vl, vaMessage);
    vprintf(vaMessage, vl);
	system_debug_print(vaMessage);
    va_end(vl);
}

void
system_VBL(void)
{
    static int frameskip_counter = 0;
    static int frame_counter = 0;
    static _u64 total_frame_counter = 0;

	static _u64 previous_ticks = 0;
	static _u64 current_ticks = 0;
	static float fps = 0;

	current_ticks = svcGetSystemTick();
	_u64 delta = current_ticks - previous_ticks;
	if (delta > TICKS_PER_SEC) {
		fps = total_frame_counter / (1.0 * delta / TICKS_PER_SEC);

		consoleClear();
		printf("%.2f fps\n", fps);
		printf("frame skip: %d\n", system_frameskip_key - 1);
		//printf("pattern table writes: %d\n", ptable_writes);

		previous_ticks = current_ticks;
		total_frame_counter = -1;
	}

    system_input_update();
    if (++frameskip_counter >= system_frameskip_key) {
		system_graphics_update();
		frameskip_counter = 0;
    }

	total_frame_counter++;

	//system_sound_update(time_spent/throttle_rate + 1);

    return;
}

#ifdef _3DS
void rom_menu(char *dir, char *rom_filename)
{
	_u32 i				= 0;
	_u32 keys 			= 0;
	_u32 oldKeys 		= 0;
	_u16 current_rom 	= 0;
	_u16 num_files		= 0;
	FS_dirent dir_entry;

	current_display = NPDS_ROM_MENU;
	read_dir_open(dir);

	/* read files into array */
	while (read_dir_next(&dir_entry) && num_files < MAX_ROMS) {
		/* the name string in FS_dirent uses 2 bytes per char, can't use normal strlen */
		_u32 length = strlen2(dir_entry.name);

		/* make into c string */
		rom_list[num_files] = calloc(length + 1, 1);
		for(i = 0; dir_entry.name[i]; i++) {
			rom_list[num_files][i] = dir_entry.name[i];
		}

		num_files++;
	}

	read_dir_close();

	/* TODO move input stuff to system_input.c */
	while (1) {
		hidScanInput();
		keys = keysHeld();

		if(keys != oldKeys && keys & KEY_UP) {
			current_rom = current_rom > 0 ? (current_rom - 1) % num_files : num_files - 1;
		}

		if(keys != oldKeys && keys & KEY_DOWN) {
			current_rom = (current_rom + 1) % num_files;
		}

		if(keys != oldKeys && keys & KEY_X) break;

		if(keys != oldKeys && (keys & KEY_B || keys & KEY_START)) break;

		consoleClear();
		printf("ROM LIST\n");

		for(i = 0; i < num_files; i++) {
			if (current_rom == i)
				printf(" > ");
			else
				printf("   ");

			if (strlen(rom_list[i]) == CONSOLE_WIDTH - 3)
				printf("%s", rom_list[i]);
			else
				printf("%s\n", rom_list[i]);
		}

		system_graphics_update();
		oldKeys = keysHeld();
	}

	strcpy(rom_filename, rom_list[current_rom]);

	consoleClear();
	printf("Loading ROM %s...\n", rom_list[current_rom]);

	/* free calloc'd memory */
	for(i = 0; i < num_files; i++) {
		free(rom_list[i]);
	}
}
#endif

#ifndef _3DS
int aptMainLoop() {
	return 1;
}
#endif

int
main(int argc, char *argv[])
{
    char *start_state;

#ifdef _3DS
	srvInit(); // Needed
	aptInit(); // Needed
	//gfxInitDefault(); // Init graphic stuff
	gfxInit(GSP_RGBA4_OES, GSP_RGBA4_OES, false); // RGBA4 closer to Neo Geo's ABGR4 format
	fsInit();  // Filesystem
	hidInit(NULL);

	consoleInit(GFX_BOTTOM, &print_console);
	consoleSetWindow(&print_console, 1, 1, CONSOLE_WIDTH, CONSOLE_HEIGHT);
#endif

    prg = argv[0];
    start_state = NULL;

    /* some defaults, to be changed by getopt args */
    /* auto-select colour mode */
    system_colour = COLOURMODE_AUTO;
    /* default to English as language for now */
    language_english = TRUE;
    /* default to smooth graphics magnification */
    graphics_mag_smooth = 1;
    /* default to sound on */
    mute = FALSE;
    /* show every frame */
    system_frameskip_key = 1;
    /* interlink defaults (port taken from System_Win32) */
    comms_mode = COMMS_NONE;
    comms_port = 7846;
    comms_host = NULL;
    /* output sample rate */
    samplerate = DEFAULT_SAMPLERATE;
    /* use YUV overlay (hardware scaling) */
    use_yuv = DEFAULT_YUV;
    use_software_yuv = 0;
    /* directories for save files */
    flash_dir = strdup("~/.neopop/flash");
    screenshot_dir = strdup(".");
    state_dir = strdup("~/.neopop/state");
    /* use rom name rather than file name for save files */
    use_rom_name = FALSE;
    /* state save slot */
    state_slot = 0;
    /* colour to display OSD in */
    osd_colour = 0xfff;

#ifndef _3DS
    /* initialize SDL */
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK) < 0) {
       fprintf(stderr, "cannot initialize SDL: %s\n", SDL_GetError());
       exit(1);
    }
    atexit(SDL_Quit);

    system_bindings_init();
    system_rc_read();

    while ((ch=getopt(argc, argv, "C:cef:ghjl:MmP:R:SsVy:")) != -1) {
	switch (ch) {
	case 'C':
	    i = system_rc_parse_comms_mode(optarg);
	    if (i == -1) {
		fprintf(stderr, "%s: unknown comms mode `%s'\n",
			prg, optarg);
		exit(1);
	    }
	    else
		comms_mode = i;
	    break;
	case 'c':
	    system_colour = COLOURMODE_COLOUR;
	    break;
	case 'e':
	    language_english = TRUE;
	    break;
	case 'f':
	    i = atoi(optarg);
	    if (i <1 || i > 7) {
		fprintf(stderr, "%s: illegal frame skip `%s'\n",
			prg, optarg);
		exit(1);
	    }		
	    system_frameskip_key = i;
	    break;
	case 'g':
	    system_colour = COLOURMODE_GREYSCALE;
	    break;
	case 'h':
	    usage(1);
	    break;
	case 'j':
	    language_english = FALSE;
	    break;
	case 'l':
	    start_state = optarg;
	    break;
	case 'M':
	    graphics_mag_smooth = 0;
	    break;
	case 'm':
	    graphics_mag_smooth = 1;
	    break;
	case 'P':
	    i = atoi(optarg);
	    if (i == 0) {
		fprintf(stderr, "%s: unknown YUV mode `%s'\n",
			prg, optarg);
		exit(1);
	    }
	    else
		comms_port = i;
	    break;
	case 'R':
	    if (comms_host)
		free(comms_host);
	    comms_host = strdup(optarg);
	    break;
	case 'S':
	    mute = TRUE;
	    break;
	case 's':
	    mute = FALSE;
	    break;
	case 'y':
	    i = system_rc_parse_yuv(optarg);
	    if (i == -1) {
		/* XXX: error message */
	    }
	    else
		use_yuv = i;
	    break;
	default:
	    usage(1);
	}
    }

    argc -= optind;
    argv += optind;
#endif

    /* Fill BIOS buffer */
    if (bios_install() == FALSE) {
	fprintf(stderr, "cannot install BIOS\n");
	exit(1);
    }

#ifndef _3DS
    if (SDL_NumJoysticks() > 0) {
	SDL_JoystickOpen(0);
	SDL_JoystickEventState(SDL_ENABLE);
    }

    if (system_graphics_init() == FALSE) {
	//fprintf(stderr, "cannot create window: %s\n", SDL_GetError());
	exit(1);
    }

    if (mute == FALSE && system_sound_init() == FALSE) {
	//fprintf(stderr, "cannot turn on sound: %s\n", SDL_GetError());
	mute = TRUE;
    }
    have_sound = !mute;

    if (system_osd_init() == FALSE)
	fprintf(stderr, "OSD font unusable, OSD disabled\n");

    if (comms_mode != COMMS_NONE)
	system_comms_connect();

#endif
    /*
     * Throttle rate is number_of_ticks_per_second divided by number
     * of complete frames that should be shown per second.
     * In this case, both are constants;
     */
    throttle_rate = 1000000/NGP_FPS;

	char rom_filename[256] = "/neogeo/";
	rom_menu("/neogeo", &rom_filename[8]);

	if (strlen(rom_filename) == 0) {
		exit(0);
	}

	if (system_rom_load(rom_filename) == FALSE) {
		// Deinit everything before the app process get's terminated
#ifdef _3DS
		gfxExit();
		aptExit();
		srvExit();
		fsExit();
		hidExit();
#endif
		return 0;
	}

    reset();
    //SDL_PauseAudio(0);
    if (start_state != NULL)
	state_restore(start_state);

    gettimeofday(&throttle_last, NULL);
	current_display = NPDS_GAME;

    do {
	if (paused == 0)
	    emulate();
	else
	    system_VBL();
    } while (aptMainLoop() && do_exit == 0);

    system_rom_unload();
    system_sound_shutdown();

#ifdef _3DS
		gfxExit();
		aptExit();
		srvExit();
		fsExit();
		hidExit();
#endif

    return 0;
}
