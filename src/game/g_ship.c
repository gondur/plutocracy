/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "g_common.h"

/* Maximum health value */
#define HEALTH_MAX 100

/* Maximum breadth of a path search */
#define SEARCH_BREADTH (R_PATH_MAX * 3)

/* Structure for searched tile nodes */
typedef struct search_node {
        float dist;
        int tile;
} search_node_t;

/* Ships and ship classes */
g_ship_t g_ships[G_SHIPS_MAX];
g_ship_class_t g_ship_classes[G_SHIP_NAMES];

/******************************************************************************\
 Initialize ship assets.
\******************************************************************************/
void G_init_ships(void)
{
        g_ship_class_t *pc;
        int i;

        /* Setup ships array */
        for (i = 0; i < G_SHIPS_MAX; i++)
                g_ships[i].class_name = G_SN_NONE;

        /* Sloop */
        pc = g_ship_classes + G_SN_SLOOP;
        pc->name = C_str("g-ship-sloop", "Sloop");
        pc->model_path = "models/ship/sloop.plum";
        pc->speed = 1.f;
        pc->health = 100;
}

/******************************************************************************\
 Clean up ship assets.
\******************************************************************************/
void G_cleanup_ships(void)
{
}

/******************************************************************************\
 Returns TRUE if a ship can sail into the given tile.
\******************************************************************************/
bool G_open_tile(int tile)
{
        return g_tiles[tile].ship < 0 &&
               (g_tiles[tile].render->terrain == R_T_SHALLOW ||
                g_tiles[tile].render->terrain == R_T_WATER);
}

/******************************************************************************\
 Find an available tile around [tile] (including [tile]) and spawn a new ship
 of the given class there. If [tile] is negative, the ship will be placed on
 a random open tile. If [index] is negative, the first available slot will be
 used.
\******************************************************************************/
int G_spawn_ship(int client, int tile, g_ship_name_t name, int index)
{
        if (index >= G_SHIPS_MAX) {
                C_warning("Failed to spawn ship at tile %d, "
                          "index out of range (%d)", tile, index);
                return -1;
        }

        /* Find an available ship slot if [index] is not given */
        if (index < 0)
                for (index = 0; g_ships[index].in_use; index++)
                        if (index >= G_SHIPS_MAX) {
                                C_warning("Failed to spawn ship at tile %d, "
                                          "array full", tile);
                                return -1;
                        }

        /* If we are to spawn the ship anywhere, pick a random tile */
        if (tile < 0) {
                int i;

                /* FIXME: This isn't a great way to pick a random open tile and
                          it doesn't guarantee success */
                tile = C_rand() % r_tiles;
                for (i = 0; !G_open_tile(tile); i++) {
                        if (i >= 100)
                                return -1;
                        tile = C_rand() % r_tiles;
                }
        }

        /* Find an available tile to start the ship on */
        else if (!G_open_tile(tile)) {
                int i, neighbors[12], len;

                /* Not being able to fit a ship in is common, so don't complain
                   if this happens */
                len = R_get_tile_region(tile, neighbors);
                for (i = 0; !G_open_tile(neighbors[i]); i++)
                        if (i >= len)
                                return -1;
                tile = neighbors[i];
        }

        /* Initialize ship structure */
        C_zero(g_ships + index);
        g_ships[index].in_use = TRUE;
        g_ships[index].class_name = name;
        g_ships[index].tile = tile;
        g_ships[index].path[0] = 0;
        g_ships[index].client = client;
        g_ships[index].health = g_ship_classes[name].health;
        g_ships[index].armor = 0;

        /* Place the ship on the tile */
        g_tiles[tile].ship = index;
        G_set_tile_model(tile, g_ship_classes[name].model_path);

        /* TODO: If we are the server, tell other clients */

        return index;
}

/******************************************************************************\
 Render ship 2D effects.
\******************************************************************************/
void G_render_ships(void)
{
        const g_ship_class_t *ship_class;
        const g_ship_t *ship;
        const g_tile_t *tile;
        c_color_t color;
        float armor, health, health_max;
        int i;

        for (i = 0; i < G_SHIPS_MAX; i++) {
                ship = g_ships + i;
                if (!ship->in_use)
                        continue;
                tile = g_tiles + ship->tile;
                if (!tile->visible)
                        return;
                ship_class = g_ship_classes + ship->class_name;
                armor = (float)ship->armor / HEALTH_MAX;
                health = (float)ship->health / HEALTH_MAX;
                health_max = (float)ship_class->health / HEALTH_MAX;
                color = g_nations[g_clients[ship->client].nation].color;
                R_render_ship_status(&tile->model, health, health_max,
                                     armor, health_max, color,
                                     g_selected_ship == i);
        }
}

/******************************************************************************\
 Returns the linear distance from one tile to another. This is the search
 function heuristic.
\******************************************************************************/
static float tile_dist(int a, int b)
{
        return C_vec3_len(C_vec3_sub(g_tiles[b].origin, g_tiles[a].origin));
}

/******************************************************************************\
 Find a path from where the [ship] is to the target [tile] and sets that as
 the ship's new path.
\******************************************************************************/
void G_ship_path(int ship, int target)
{
        static int search_stamp;
        search_node_t nodes[SEARCH_BREADTH];
        int i, nodes_len, closest, path_len, neighbors[3];

        search_stamp++;

        /* Clear the ship's old path */
        g_ships[ship].path[0] = 0;
        if (g_ships[ship].tile == target || !G_open_tile(target))
                return;

        /* Start with just the initial tile open */
        nodes[0].tile = g_ships[ship].tile;
        nodes[0].dist = tile_dist(nodes[0].tile, target);
        nodes_len = 1;
        g_tiles[nodes[0].tile].search_parent = -1;
        g_tiles[nodes[0].tile].search_stamp = search_stamp;
        closest = 0;

        for (;;) {
                search_node_t node;

                node = nodes[closest];

                /* Ran out of search nodes -- no path to target */
                if (nodes_len < 1)
                        return;

                /* Remove the node from the list */
                nodes_len--;
                memmove(nodes + closest, nodes + closest + 1,
                        (nodes_len - closest) * sizeof (*nodes));

                /* Add its children */
                R_get_tile_neighbors(node.tile, neighbors);
                for (i = 0; i < 3; i++) {
                        int stamp;

                        /* Already opened? */
                        stamp = g_tiles[neighbors[i]].search_stamp;
                        C_assert(stamp <= search_stamp);
                        if (stamp == search_stamp || !G_open_tile(neighbors[i]))
                                continue;
                        g_tiles[neighbors[i]].search_stamp = search_stamp;

                        /* Out of space? */
                        if (nodes_len >= SEARCH_BREADTH) {
                                C_warning("Out of search space");
                                return;
                        }

                        /* Add to array */
                        nodes[nodes_len].tile = neighbors[i];
                        g_tiles[neighbors[i]].search_parent = node.tile;

                        /* Did we make it? */
                        if (neighbors[i] == target)
                                goto rewind;

                        /* Distance to destination */
                        nodes[nodes_len].dist = tile_dist(neighbors[i], target);

                        nodes_len++;
                }

                /* Find the new closest node */
                for (closest = 0, i = 1; i < nodes_len; i++)
                        if (nodes[i].dist < nodes[closest].dist)
                                closest = i;
        }

rewind: /* Count length of the path */
        path_len = -1;
        for (i = nodes[nodes_len].tile; i >= 0; i = g_tiles[i].search_parent)
                path_len++;

        /* The path is too long */
        if (path_len > R_PATH_MAX) {
                C_warning("Path is too long (%d tiles)", path_len);
                return;
        }

        /* Write the path backwards */
        g_ships[ship].path[path_len] = 0;
        for (i = nodes[nodes_len].tile; i >= 0; ) {
                int j, parent;

                parent = g_tiles[i].search_parent;
                if (parent < 0)
                        break;
                R_get_tile_neighbors(parent, neighbors);
                for (j = 0; neighbors[j] != i; j++);
                g_ships[ship].path[--path_len] = j + 1;
                i = parent;
        }
}

