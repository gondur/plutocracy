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

/* Number of windows defined */
#define WINDOWS 3

/* Windows */
static struct property_t {
        void (*init)(i_window_t *);
        char *icon;
        c_vec2_t size;
} properties[] = {
        {I_game_init, "gui/icons/game.png", {240.f, 0.f}},
        {I_console_init, "gui/icons/console.png", {480.f, 240.f}},
        {I_video_init, "gui/icons/video.png", {240.f, 0.f}},
};

i_widget_t i_root;

static c_vec2_t root_scroll;
static i_window_t left_toolbar, *open_window, windows[WINDOWS];
static i_button_t buttons[WINDOWS];
static int grabbing, grab_x, grab_y;

/******************************************************************************\
 Root window event function.
\******************************************************************************/
static int root_event(i_widget_t *root, i_event_t event)
{
        int i;

        switch (event) {
        case I_EV_KEY_DOWN:
                if (i_key == SDLK_RIGHT && root_scroll.x > -1.f)
                        root_scroll.x = -i_scroll_speed.value.f;
                if (i_key == SDLK_LEFT && root_scroll.x < 1.f)
                        root_scroll.x = i_scroll_speed.value.f;
                if (i_key == SDLK_DOWN && root_scroll.y > -1.f)
                        root_scroll.y = -i_scroll_speed.value.f;
                if (i_key == SDLK_UP && root_scroll.y < 1.f)
                        root_scroll.y = i_scroll_speed.value.f;
                if (i_key == '-')
                        R_zoom_cam_by(i_zoom_speed.value.f);
                if (i_key == '=')
                        R_zoom_cam_by(-i_zoom_speed.value.f);
                break;
        case I_EV_KEY_UP:
                if ((i_key == SDLK_RIGHT && root_scroll.x < 0.f) ||
                    (i_key == SDLK_LEFT && root_scroll.x > 0.f))
                        root_scroll.x = 0.f;
                if ((i_key == SDLK_DOWN && root_scroll.y < 0.f) ||
                    (i_key == SDLK_UP && root_scroll.y > 0.f))
                        root_scroll.y = 0.f;
                break;
        case I_EV_MOUSE_DOWN:
                if (i_mouse == SDL_BUTTON_WHEELDOWN)
                        R_zoom_cam_by(i_zoom_speed.value.f);
                if (i_mouse == SDL_BUTTON_WHEELUP)
                        R_zoom_cam_by(-i_zoom_speed.value.f);
                if (i_mouse == SDL_BUTTON_MIDDLE) {
                        grabbing = TRUE;
                        grab_x = i_mouse_x;
                        grab_y = i_mouse_y;
                }
                break;
        case I_EV_MOUSE_UP:
                if (i_mouse == SDL_BUTTON_MIDDLE)
                        grabbing = FALSE;
                break;
        case I_EV_MOUSE_MOVE:
                if (grabbing) {
                        float dx, dy;

                        dx = R_screen_to_globe(i_mouse_x - grab_x);
                        dy = R_screen_to_globe(i_mouse_y - grab_y);
                        R_move_cam_by(C_vec2(dx, dy));
                        grab_x = i_mouse_x;
                        grab_y = i_mouse_y;
                }
                break;
        case I_EV_CONFIGURE:
                i_colors[I_COLOR] = C_color_string(i_color.value.s);
                i_colors[I_COLOR_ALT] = C_color_string(i_color2.value.s);
                I_widget_propagate(root, event);

                /* Position window hangers */
                for (i = 0; i < WINDOWS; i++)
                        I_window_hanger(windows + i, &buttons[i].widget, TRUE);

                return FALSE;
        case I_EV_RENDER:
                R_move_cam_by(C_vec2_scalef(root_scroll, c_frame_sec));
                break;
        default:
                break;
        }
        return TRUE;
}

/******************************************************************************\
 Updates after a theme change.
\******************************************************************************/
static void theme_configure(void)
{
        float offset;
        int i;

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
        left_toolbar.widget.origin = C_vec2((float)i_border.value.n,
                                            r_height_2d -
                                            left_toolbar.widget.size.y -
                                            i_border.value.n);

        /* Windows */
        offset = r_height_2d - left_toolbar.widget.size.y -
                 i_border.value.n * 2;
        for (i = 0; i < WINDOWS; i++) {
                windows[i].widget.size = properties[i].size;
                windows[i].widget.origin = C_vec2((float)i_border.value.n,
                                                  offset -
                                                  windows[i].widget.size.y);
        }
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

static void button_click(i_button_t *button)
{
        left_toolbar_open(button->data);
}

/******************************************************************************\
 Parse the theme config.
\******************************************************************************/
void I_parse_config(void)
{
        C_var_unlatch(&i_theme);
        C_parse_config_file(i_theme.value.s);
}

/******************************************************************************\
 Loads interface assets and initializes windows.
\******************************************************************************/
void I_init(void)
{
        int i;

        C_status("Initializing interface");

        /* Enable key repeats so held keys generate key-down events */
        SDL_EnableKeyRepeat(300, 30);

        /* Enable Unicode so we get Unicode key strokes AND capitals */
        SDL_EnableUNICODE(1);

        /* Root window */
        C_zero(&i_root);
        I_widget_set_name(&i_root, "Root Window");
        i_root.state = I_WS_READY;
        i_root.event_func = (i_event_f)root_event;
        i_root.configured = 1;
        i_root.entry = TRUE;
        i_root.shown = TRUE;
        i_root.clickable = TRUE;
        i_key_focus = &i_root;

        /* Left toolbar */
        I_window_init(&left_toolbar);
        left_toolbar.fit = I_FIT_BOTTOM;
        left_toolbar.pack_children = I_PACK_H;
        I_widget_add(&i_root, &left_toolbar.widget);

        /* Window and buttons */
        for (i = 0; i < WINDOWS; i++) {
                properties[i].init(windows + i);
                I_widget_add(&i_root, &windows[i].widget);
                I_button_init(buttons + i, properties[i].icon, NULL, FALSE);
                buttons[i].on_click = (i_callback_f)button_click;
                buttons[i].data = windows + i;
                if (i < WINDOWS - 1)
                        buttons[i].widget.margin_rear = 0.5f;
                I_widget_add(&left_toolbar.widget, &buttons[i].widget);
        }

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
        /* If video parameters changed last frame, we need to reconfigure */
        if (r_pixel_scale.changed == c_frame - 1 ||
            r_width.changed == c_frame - 1 ||
            r_height.changed == c_frame - 1) {
                theme_configure();
                I_widget_event(&i_root, I_EV_CONFIGURE);
        }

        I_widget_event(&i_root, I_EV_RENDER);
}

