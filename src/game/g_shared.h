/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* There is a fixed number of nations */
typedef enum {
        G_NN_NONE,
        G_NN_RED,
        G_NN_GREEN,
        G_NN_BLUE,
        G_NN_PIRATE,
        G_NATION_NAMES,
} g_nation_name_t;

/* Cargo that a ship can carry */
typedef enum {
        G_CT_GOLD,
        G_CT_CREW,

        /* Tech preview resources */
        G_CT_RATIONS,
        G_CT_WOOD,
        G_CT_IRON,

        G_CARGO_TYPES,
} g_cargo_type_t;

/* Structure for each nation */
typedef struct g_nation {
        c_color_t color;
        const char *short_name, *long_name;
        int gold;
} g_nation_t;

/* g_client.c */
void G_cleanup(void);
void G_init(void);
void G_refresh_servers(void);
void G_update_client(void);

/* g_commands.c */
void G_buy_cargo(g_cargo_type_t, int amount);
void G_drop_cargo(g_cargo_type_t, int amount);
void G_input_chat(char *message);
void G_join_game(const char *address);
void G_leave_game(void);
bool G_process_click(int button);
void G_process_key(int key, bool shift, bool ctrl, bool alt);
void G_trade_params(g_cargo_type_t, int buy_price, int sell_price,
                    int minimum, int maximum);

/* g_elements.c */
extern g_nation_t g_nations[G_NATION_NAMES];
extern const char *g_cargo_names[G_CARGO_TYPES];
extern bool g_game_over;

/* g_globe.c */
void G_mouse_ray(c_vec3_t origin, c_vec3_t forward);
void G_mouse_ray_miss(void);
void G_render_globe(void);

/* g_host.c */
void G_change_nation(int index);
void G_kick_client(int index);
void G_host_game(void);
void G_update_host(void);

extern int g_clients_max, g_time_limit_msec;

/* g_variables.c */
void G_register_variables(void);

extern c_var_t g_draw_distance;

