/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Initializes, cleans up, and manipulates gameplay elements */

#include "g_common.h"

/* Ships and ship classes */
g_ship_t g_ships[G_SHIPS_MAX];
g_ship_class_t g_ship_classes[G_SHIP_NAMES];

/* Array of game nations */
g_nation_t g_nations[G_NATION_NAMES];

/* Array of building types */
g_building_type_t g_building_types[G_BUILDING_NAMES];

/* Array of cargo names */
const char *g_cargo_names[G_CARGO_TYPES];

/******************************************************************************\
 Update function for a national color.
\******************************************************************************/
static int nation_color_update(c_var_t *var, c_var_value_t value)
{
        C_color_update(var, value);
        I_update_colors();
        return TRUE;
}

/******************************************************************************\
 Returns the space one unit of a given cargo takes up.
\******************************************************************************/
static float cargo_space(g_cargo_type_t cargo)
{
        if (cargo == G_CT_GOLD)
                return 0.01f;
        return 1.f;
}

/******************************************************************************\
 Returns the amount of space the given cargo manifest contains.
\******************************************************************************/
int G_store_space(const g_store_t *store)
{
        int i, space;

        for (space = i = 0; i < G_CARGO_TYPES; i++)
                space += (int)ceilf(cargo_space(i) * store->cargo[i].amount);
        return space;
}

/******************************************************************************\
 Returns the amount of a cargo that will fit in a given store.
\******************************************************************************/
int G_store_fits(const g_store_t *store, g_cargo_type_t cargo)
{
        int space;

        space = store->capacity - G_store_space(store);
        if (space < 0)
                return 0;
        return (int)floorf(space / cargo_space(cargo));
}

/******************************************************************************\
 Convert a nation to a color index.
\******************************************************************************/
i_color_t G_nation_to_color(g_nation_name_t nation)
{
        if (nation == G_NN_RED)
                return I_COLOR_RED;
        else if (nation == G_NN_GREEN)
                return I_COLOR_GREEN;
        else if (nation == G_NN_BLUE)
                return I_COLOR_BLUE;
        else if (nation == G_NN_PIRATE)
                return I_COLOR_PIRATE;
        return I_COLOR_ALT;
}

/******************************************************************************\
 Initialize game structures before play.
\******************************************************************************/
void G_init_elements(void)
{
        g_ship_class_t *pc;
        int i;

        C_status("Initializing game elements");

        /* Check our constants */
        C_assert(G_CLIENT_MESSAGES < 256);
        C_assert(G_SERVER_MESSAGES < 256);

        /* Nation constants */
        g_nations[G_NN_RED].short_name = "red";
        g_nations[G_NN_RED].long_name = C_str("g-nation-red", "Ruby");
        g_nations[G_NN_GREEN].short_name = "green";
        g_nations[G_NN_GREEN].long_name = C_str("g-nation-green", "Emerald");
        g_nations[G_NN_BLUE].short_name = "blue";
        g_nations[G_NN_BLUE].long_name = C_str("g-nation-blue", "Sapphire");
        g_nations[G_NN_PIRATE].short_name = "pirate";
        g_nations[G_NN_PIRATE].long_name = C_str("g-nation-pirate", "Pirate");

        /* Initialize nation window and color variables */
        for (i = 0; i < G_NATION_NAMES; i++)
                C_var_update_data(g_nation_colors + i, nation_color_update,
                                  &g_nations[i].color);

        /* Special cargo */
        g_cargo_names[G_CT_GOLD] = C_str("g-cargo-gold", "Gold");
        g_cargo_names[G_CT_CREW] = C_str("g-cargo-crew", "Crew");

        /* Food cargo */
        g_cargo_names[G_CT_RATIONS] = C_str("g-cargo-rations", "Rations");
        g_cargo_names[G_CT_BREAD] = C_str("g-cargo-bread", "Bread");
        g_cargo_names[G_CT_FISH] = C_str("g-cargo-fish", "Fish");
        g_cargo_names[G_CT_FRUIT] = C_str("g-cargo-fruit", "Fruit");

        /* Raw material cargo */
        g_cargo_names[G_CT_GRAIN] = C_str("g-cargo-grain", "Grain");
        g_cargo_names[G_CT_COTTON] = C_str("g-cargo-cotton", "Cotton");
        g_cargo_names[G_CT_LUMBER] = C_str("g-cargo-lumber", "Lumber");
        g_cargo_names[G_CT_IRON] = C_str("g-cargo-iron", "Iron");

        /* Luxury cargo */
        g_cargo_names[G_CT_RUM] = C_str("g-cargo-rum", "Rum");
        g_cargo_names[G_CT_FURNITURE] = C_str("g-cargo-furniture", "Furniture");
        g_cargo_names[G_CT_CLOTH] = C_str("g-cargo-cloth", "Cloth");
        g_cargo_names[G_CT_SUGAR] = C_str("g-cargo-sugar", "Sugar");

        /* Equipment cargo */
        g_cargo_names[G_CT_CANNON] = C_str("g-cargo-cannon", "Cannon");
        g_cargo_names[G_CT_ROUNDSHOT] = C_str("g-cargo-roundshot", "Ammo");
        g_cargo_names[G_CT_PLATING] = C_str("g-cargo-plating", "Plating");
        g_cargo_names[G_CT_GILLNET] = C_str("g-cargo-gillner", "Gillnet");

        /* Setup buildings */
        g_building_types[G_BN_NONE].name = "None";
        g_building_types[G_BN_NONE].model_path = "";
        g_building_types[G_BN_TREE].name = "Trees";
        g_building_types[G_BN_TREE].model_path = "models/tree/deciduous.plum";

        /* Sloop */
        pc = g_ship_classes + G_SN_SLOOP;
        pc->name = C_str("g-ship-sloop", "Sloop");
        pc->model_path = "models/ship/sloop.plum";
        pc->speed = 1.f;
        pc->health = 40;
        pc->cargo = 100;

        /* Spider */
        pc = g_ship_classes + G_SN_SPIDER;
        pc->name = C_str("g-ship-spider", "Spider");
        pc->model_path = "models/ship/spider.plum";
        pc->speed = 0.75f;
        pc->health = 80;
        pc->cargo = 150;

        /* Galleon */
        pc = g_ship_classes + G_SN_GALLEON;
        pc->name = C_str("g-ship-galleon", "Galleon");
        pc->model_path = "models/ship/galleon.plum";
        pc->speed = 0.5f;
        pc->health = 100;
        pc->cargo = 200;
}

/******************************************************************************\
 Set a tile's building.
\******************************************************************************/
void G_build(int tile, g_building_name_t name, float progress)
{
        /* Range checks */
        C_assert(tile >= 0 && tile < r_tiles);
        C_assert(name >= 0 && name < G_BUILDING_NAMES);
        if (progress < 0.f)
                progress = 0.f;
        if (progress > 1.f)
                progress = 1.f;

        g_tiles[tile].building = name;
        g_tiles[tile].progress = progress;
        G_set_tile_model(tile, g_building_types[name].model_path);
}

/******************************************************************************\
 Reset game structures.
\******************************************************************************/
void G_reset_elements(void)
{
        int i;

        /* Reset ships */
        memset(g_ships, 0, sizeof (g_ships));

        /* Reset clients, keeping names */
        for (i = 0; i < N_CLIENTS_MAX; i++)
                g_clients[i].nation = G_NN_NONE;

        /* Reset tiles */
        for (i = 0; i < r_tiles; i++)
                G_build(i, G_BN_NONE, 0.f);

        /* The server "client" has fixed information */
        g_clients[N_SERVER_ID].nation = G_NN_PIRATE;

        /* We can reuse names now */
        G_reset_name_counts();

        /* Start out without a selected ship */
        G_ship_select(-1);
}

