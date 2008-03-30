/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "i_common.h"

void I_console_init(i_window_t *);
void I_game_init(i_window_t *);

i_widget_t i_root;

static c_vec2_t root_scroll;
static i_window_t left_toolbar, console_window, game_window, *open_window;
static i_button_t console_button, game_button;

/******************************************************************************\
 Root window event function.
 TODO: Stuff like rotating the globe and other interactions will happen here
       because this is the catch all for mouse/keyboard events that don't get
       handled.
\******************************************************************************/
static int root_event(i_widget_t *root, i_event_t event)
{
        switch (event) {
        case I_EV_KEY_DOWN:
                if (i_key == SDLK_RIGHT && root_scroll.x > -1.f)
                        root_scroll.x = -1.f;
                if (i_key == SDLK_LEFT && root_scroll.x < 1.f)
                        root_scroll.x = 1.f;
                if (i_key == SDLK_DOWN && root_scroll.y > -1.f)
                        root_scroll.y = -1.f;
                if (i_key == SDLK_UP && root_scroll.y < 1.f)
                        root_scroll.y = 1.f;
                break;
        case I_EV_KEY_UP:
                if ((i_key == SDLK_RIGHT && root_scroll.x < 0.f) ||
                    (i_key == SDLK_LEFT && root_scroll.x > 0.f))
                        root_scroll.x = 0.f;
                if ((i_key == SDLK_DOWN && root_scroll.y < 0.f) ||
                    (i_key == SDLK_UP && root_scroll.y > 0.f))
                        root_scroll.y = 0.f;
                break;
        case I_EV_CONFIGURE:
                i_colors[I_COLOR] = C_color_string(i_color.value.s);
                i_colors[I_COLOR_ALT] = C_color_string(i_color2.value.s);
                I_widget_propagate(root, event);

                /* Position window hangers */
                I_window_hanger(&game_window, &game_button.widget, TRUE);
                I_window_hanger(&console_window, &console_button.widget, TRUE);

                return FALSE;
        case I_EV_RENDER:
                r_camera.x += root_scroll.y * c_frame_sec *
                              i_scroll_speed.value.f;
                r_camera.y += root_scroll.x * c_frame_sec *
                              i_scroll_speed.value.f;
                break;
        default:
                break;
        }
        return TRUE;
}

/******************************************************************************\
 Parses the theme configuration file. This is called before the initialization
 functions in case there are font changes in the theme.
\******************************************************************************/
void I_parse_config(void)
{
        C_var_unlatch(&i_theme);
        C_parse_config_file(i_theme.value.s);
}

/******************************************************************************\
 Updates after a theme change.
\******************************************************************************/
static void theme_configure(void)
{
        C_var_unlatch(&i_border);
        C_var_unlatch(&i_button);
        C_var_unlatch(&i_button_active);
        C_var_unlatch(&i_button_hover);
        C_var_unlatch(&i_button_light);
        C_var_unlatch(&i_button_prelight);
        C_var_unlatch(&i_color);
        C_var_unlatch(&i_color2);
        C_var_unlatch(&i_hanger);
        C_var_unlatch(&i_shadow);
        C_var_unlatch(&i_window);
        C_var_unlatch(&i_work_area);

        /* Root window */
        i_root.size = C_vec2((float)r_width_2d, (float)r_height_2d);

        /* Left toolbar */
        left_toolbar.widget.size = C_vec2(0.f, 32.f + i_border.value.n * 2);
        left_toolbar.widget.origin = C_vec2(i_border.value.n, r_height_2d -
                                            left_toolbar.widget.size.y -
                                            i_border.value.n);

        /* Game window */
        game_window.widget.size = C_vec2(240.f, 0.f);
        game_window.widget.origin = C_vec2(i_border.value.n, r_height_2d -
                                           game_window.widget.size.y -
                                           left_toolbar.widget.size.y -
                                           i_border.value.n * 2);

        /* Console window */
        console_window.widget.size = C_vec2(480.f, 240.f);
        console_window.widget.origin = C_vec2(i_border.value.n, r_height_2d -
                                              console_window.widget.size.y -
                                              left_toolbar.widget.size.y -
                                              i_border.value.n * 2);
}

/******************************************************************************\
 Update function for i_theme. Will also reload fonts.
\******************************************************************************/
static int theme_update(c_var_t *var, c_var_value_t value)
{
        if (!C_parse_config_file(value.s))
                return FALSE;
        theme_configure();
        R_free_fonts();
        R_load_fonts();
        I_widget_event(&i_root, I_EV_CONFIGURE);
        return TRUE;
}

/******************************************************************************\
 Left toolbar button handlers.
\******************************************************************************/
static void left_toolbar_open(i_window_t *window)
{
        if (open_window == window) {
                I_widget_show(&window->widget, FALSE);
                open_window = NULL;
                return;
        }
        if (open_window)
                I_widget_show(&open_window->widget, FALSE);
        I_widget_show(&window->widget, TRUE);
        open_window = window;
}

static void game_button_click(i_button_t *button)
{
        left_toolbar_open(&game_window);
}

static void console_button_click(i_button_t *button)
{
        left_toolbar_open(&console_window);
}

/******************************************************************************\
 Loads interface assets and initializes windows.
\******************************************************************************/
void I_init(void)
{
        C_status("Initializing interface");

        /* Enable key repeats so held keys generate key-down events */
        SDL_EnableKeyRepeat(300, 30);

        /* Enable Unicode so we get Unicode key strokes AND capitals */
        SDL_EnableUNICODE(TRUE);

        /* Root window */
        C_zero(&i_root);
        I_widget_set_name(&i_root, "Root Window");
        i_root.state = I_WS_READY;
        i_root.event_func = (i_event_f)root_event;
        i_root.configured = 1;
        i_root.entry = TRUE;
        i_root.shown = TRUE;

        /* Left toolbar */
        I_window_init(&left_toolbar);
        left_toolbar.fit = I_FIT_BOTTOM;
        left_toolbar.pack_children = I_PACK_H;
        I_widget_add(&i_root, &left_toolbar.widget);

        /* Game button */
        I_button_init(&game_button, "gui/icons/game.png", NULL, FALSE);
        game_button.on_click = (i_callback_f)game_button_click;
        game_button.widget.padding = 0.5f;
        I_widget_add(&left_toolbar.widget, &game_button.widget);

        /* Console button */
        I_button_init(&console_button, "gui/icons/console.png", NULL, FALSE);
        console_button.on_click = (i_callback_f)console_button_click;
        I_widget_add(&left_toolbar.widget, &console_button.widget);

        /* Game window */
        I_window_init(&game_window);
        game_window.fit = I_FIT_TOP;
        I_game_init(&game_window);
        I_widget_add(&i_root, &game_window.widget);

        /* Console window */
        I_window_init(&console_window);
        I_console_init(&console_window);
        I_widget_add(&i_root, &console_window.widget);

        /* Theme can now be modified dynamically */
        theme_configure();
        i_theme.update = (c_var_update_f)theme_update;
        i_theme.edit = C_VE_FUNCTION;

        I_widget_event(&i_root, I_EV_CONFIGURE);
        I_widget_show(&left_toolbar.widget, TRUE);
}

/******************************************************************************\
 Free interface assets.
\******************************************************************************/
void I_cleanup(void)
{
        I_widget_event(&i_root, I_EV_CLEANUP);
}

/******************************************************************************\
 Render all visible windows.
\******************************************************************************/
void I_render(void)
{
        I_widget_event(&i_root, I_EV_RENDER);
}

