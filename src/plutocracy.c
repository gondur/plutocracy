/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* This file forms the starting point for the client program. The client may
   also function as a server. */

#include "common/c_shared.h"
#include "render/r_shared.h"
#include "interface/i_shared.h"
#include "game/g_shared.h"

#define CORRUPT_CHECK_VALUE 1776

extern c_var_t c_max_fps, c_show_fps;

static c_count_t throttled;
static r_text_t status_text;
static int desired_msec;

/******************************************************************************\
 Renders the status text on the screen. This function uses the throttled
 counter for interval timing. Counters are reset periodically.
\******************************************************************************/
static void render_status(void)
{
        if (c_show_fps.value.n < 0)
                return;
        if (C_count_poll(&throttled, 5000)) {
                char *str;

                str = C_va("%.0f fps, (%.0f%% throttled), %.0f faces/frame",
                           C_count_fps(&throttled),
                           100.f * C_count_per_frame(&throttled) / desired_msec,
                           C_count_per_frame(&r_count_faces));
                R_text_configure(&status_text, R_FONT_CONSOLE,
                                 0, 1.f, FALSE, str);
                status_text.sprite.origin = C_vec2(4, 4);
                C_count_reset(&throttled);
                C_count_reset(&r_count_faces);
        }
        R_text_render(&status_text);
}

/******************************************************************************\
 Throttle framerate if vsync is off or broken so we don't burn the CPU for no
 reason. SDL_Delay() will only work in 10ms increments on some systems so it
 is necessary to break down our delays into bite-sized chunks ([wait_msec]).
\******************************************************************************/
static void throttle_fps(void)
{
        static int wait_msec;
        int msec;

        if (c_max_fps.value.n < 1 || c_frame_msec > desired_msec)
                return;
        wait_msec += desired_msec - c_frame_msec;
        msec = (wait_msec / 10) * 10;
        if (msec > 0) {
                SDL_Delay(msec);
                wait_msec -= msec;
                C_count_add(&throttled, msec);
        }
}

/******************************************************************************\
 This is the client's graphical main loop.
\******************************************************************************/
static void main_loop(void)
{
        static int corrupt_check = CORRUPT_CHECK_VALUE;
        SDL_Event ev;

        C_status("Main loop");
        C_time_init();
        C_count_reset(&throttled);
        C_rand_seed((unsigned int)time(NULL));
        R_text_init(&status_text);

        /* Calculate the desired frame msec.
           TODO: Why do we need to wait twice as long as the correct rate? */
        if (c_max_fps.value.n >= 1)
                desired_msec = 2000 / c_max_fps.value.n;

        while (!c_exit) {
                R_start_frame();
                while (SDL_PollEvent(&ev)) {
                        switch(ev.type) {
                        case SDL_QUIT:
                                return;
                        case SDL_KEYDOWN:
                                if (ev.key.keysym.sym == SDLK_ESCAPE) {
                                        C_debug("Escape key pressed");
                                        return;
                                }
                        default:
                                I_dispatch(&ev);
                                break;
                        }
                }
                G_render();
                I_render();
                render_status();
                R_finish_frame();
                C_time_update();
                throttle_fps();

                /* This check is a long-shot, but if there was rampant memory
                   corruption this variable's value may have been changed */
                if (corrupt_check != CORRUPT_CHECK_VALUE)
                        C_error("Static memory corruption detected");
        }

        /* Do NOT do your cleanup here! This code is never reached. */
}

/******************************************************************************\
 Concatenates an argument array and runs it through the config parser.
\******************************************************************************/
static void parse_config_args(int argc, char *argv[])
{
        int i, len;
        char buffer[4096], *pos;

        if (argc < 2)
                return;
        C_status("Parsing command line");
        buffer[0] = NUL;
        pos = buffer;
        for (i = 1; i < argc; i++) {
                len = C_strlen(argv[i]);
                if (pos + len >= buffer + sizeof (buffer)) {
                        C_warning("Command-line config overflowed");
                        return;
                }
                memcpy(pos, argv[i], len);
                pos += len;
                *(pos++) = ' ';
        }
        *pos = NUL;
        C_parse_config_string(buffer);
}

/******************************************************************************\
 Called when the program quits normally or is killed by a signal and should
 perform an orderly cleanup.
\******************************************************************************/
static void cleanup(void)
{
        static int ran_once;

        /* Disable the log event handler */
        c_log_mode = C_LM_CLEANUP;

        /* It is possible that this function will get called multiple times
           for certain kinds of exits, do not clean-up twice! */
        if (ran_once) {
                C_warning("Cleanup already called");
                return;
        }
        ran_once = TRUE;

        C_status("Cleaning up");
        I_cleanup();
        R_text_cleanup(&status_text);
        R_cleanup();
        SDL_Quit();
        C_check_leaks();
        C_debug("Done");
}

/******************************************************************************\
 Initialize SDL.
\******************************************************************************/
static void init_sdl(void)
{
        SDL_version compiled;
        const SDL_version *linked;

        SDL_VERSION(&compiled);
        C_debug("Compiled with SDL %d.%d.%d",
                compiled.major, compiled.minor, compiled.patch);
        linked = SDL_Linked_Version();
        if (!linked)
                C_error("Failed to get SDL linked version");
        C_debug("Linked with SDL %d.%d.%d",
                linked->major, linked->minor, linked->patch);
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
                C_error("Failed to initialize SDL: %s", SDL_GetError());
        SDL_WM_SetCaption(PACKAGE_STRING, PACKAGE);
}

/******************************************************************************\
 Start up the client program from here.
\******************************************************************************/
int main(int argc, char *argv[])
{
        /* Use the cleanup function instead of lots of atexit() calls to
           control the order of cleanup */
        atexit(cleanup);

        /* gettext initialization */
        setlocale(LC_ALL, "");
        bindtextdomain(PACKAGE, LOCALEDIR);
        textdomain(PACKAGE);

        /* Each namespace must register its configurable variables */
        C_register_variables();
        R_register_variables();
        I_register_variables();
        G_register_variables();

        /* Parse configuration scripts and open the log file */
        C_parse_config_file(C_va("%s/autogen.cfg", C_user_dir()));
        C_parse_config_file(C_va("%s/autoexec.cfg", C_user_dir()));
        parse_config_args(argc, argv);
        C_open_log_file();

        /* Run tests if they are enabled */
        C_test_mem_check();

        /* Initialize */
        C_status("Initializing " PACKAGE_STRING " client");
        C_debug(_("Untranslated version"));
        I_parse_config();
        init_sdl();
        R_init();
        I_init();
        G_init();

        /* Run the main loop */
        main_loop();

        /* This will only be reached at proper exits */
        C_write_autogen();

        return 0;
}

