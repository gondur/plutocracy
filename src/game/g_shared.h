/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Maximum number of ships in a game */
#define G_SHIPS_MAX 128

/* Ship class names */
typedef enum {
        G_SN_SLOOP,
        G_SHIP_NAMES,
        G_SN_NONE,
} g_ship_name_t;

/* There is a fixed number of nations */
typedef enum {
        G_NN_RED,
        G_NN_GREEN,
        G_NN_BLUE,
        G_NN_PIRATE,
        G_NATION_NAMES,
        G_NN_NONE,
} g_nation_name_t;

/* Cargo that a ship can carry */
typedef enum {
        G_CT_GOLD,
        G_CT_CREW,

        /* Foods */
        G_CT_RATIONS,
        G_CT_BREAD,
        G_CT_FISH,
        G_CT_FRUIT,

        /* Raw materials */
        G_CT_GRAIN,
        G_CT_COTTON,
        G_CT_LUMBER,
        G_CT_IRON,
        G_CT_STEEL,
        G_CT_GEMSTONE,
        G_CT_SULFUR,
        G_CT_FERTILIZER,

        /* Luxuries */
        G_CT_JEWELRY,
        G_CT_RUM,
        G_CT_TEXTILES,
        G_CT_FURNITURE,
        G_CT_SUGAR,

        /* Equipment */
        G_CT_ARMOR,
        G_CT_CANNON,
        G_CT_CHAINSHOT,
        G_CT_GRAPESHOT,
        G_CT_ROUNDSHOT,
        G_CT_SAILS,

        G_CARGO_TYPES,
        G_CT_FOODS = G_CT_RATIONS,
        G_CT_MATERIALS = G_CT_GRAIN,
        G_CT_LUXURIES = G_CT_JEWELRY,
        G_CT_EQUIPMENT = G_CT_ARMOR,
} g_cargo_type_t;

/* Structure for each nation */
typedef struct g_nation {
        c_color_t color;
        const char *short_name, *long_name;
} g_nation_t;

/* Structure containing ship class information */
typedef struct g_ship_class {
        const char *model_path, *name;
        float speed;
        int health, cargo;
} g_ship_class_t;

/* Cargo and trading settings structure */
typedef struct g_cargo {
        int amounts[G_CARGO_TYPES], prices[G_CARGO_TYPES], maxs[G_CARGO_TYPES];
} g_cargo_t;

/* Structure containing ship information */
typedef struct g_ship {
        g_ship_name_t class_name;
        g_cargo_t cargo;
        float progress;
        int tile, rear_tile, target, client, health, armor;
        char path[R_PATH_MAX];
        bool in_use;
} g_ship_t;

/* g_client.c */
int G_cargo_space(const g_cargo_t *);
void G_init(void);
void G_leave_game(void);
void G_process_click(int button);
void G_update_client(void);

/* g_globe.c */
void G_mouse_ray(c_vec3_t origin, c_vec3_t forward);
void G_mouse_ray_miss(void);
void G_render_globe(void);

/* g_host.c */
void G_change_nation(int index);
void G_cleanup(void);
void G_host_game(void);
void G_update_host(void);

extern g_nation_t g_nations[G_NATION_NAMES];
extern const char *g_cargo_names[G_CARGO_TYPES];

/* g_ship.c */
extern g_ship_class_t g_ship_classes[G_SHIP_NAMES];
extern g_ship_t g_ships[G_SHIPS_MAX];

/* g_variables.c */
void G_register_variables(void);

