/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "../common/c_shared.h"
#include "../render/r_shared.h"
#include "../interface/i_shared.h"
#include "../network/n_shared.h"
#include "g_shared.h"

/* Nation indices */
#define G_NATIONS 4
#define G_NATION_PIRATE (G_NATIONS - 1)

/* Message tokens sent by clients */
typedef enum {
        G_CM_NONE,
        G_CM_AFFILIATE,
        G_CLIENT_MESSAGES,
} g_client_msg_t;

/* Message tokens sent by the server */
typedef enum {
        G_SM_NONE,
        G_SM_POPUP,
        G_SM_AFFILIATE,
        G_SERVER_MESSAGES,
} g_server_msg_t;

/* A tile on the globe */
typedef struct g_tile {
        c_vec3_t origin, normal, forward;
        r_model_t model;
        r_tile_t *render;
        struct g_tile *neighbors[3];
        int island;
        bool visible;
} g_tile_t;

/* Structure for each player */
typedef struct g_client {
        c_count_t commands;
        int nation;
        char name[16];
} g_client_t;

/* Structure for each nation */
typedef struct g_nation {
        const char *short_name, *long_name;
} g_nation_t;

/* g_client.c */
void G_client_callback(int client, n_event_t);

/* g_host.c */
extern g_client_t g_clients[N_CLIENTS_MAX];
extern g_nation_t g_nations[G_NATIONS];

/* g_globe.c */
extern g_tile_t g_tiles[R_TILES_MAX];
extern c_var_t g_globe_islands, g_globe_island_size, g_globe_seed,
               g_globe_subdiv4;

/* g_variables.c */
extern c_var_t g_test_globe, g_test_tile;

