/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Spawning, updating, and rendering ships */

#include "g_common.h"

/* Maximum health value */
#define HEALTH_MAX 100

static int focus_stamp;

/******************************************************************************\
 Find an available tile around [tile] (including [tile]) and spawn a new ship
 of the given class there. If [tile] is negative, the ship will be placed on
 a random open tile. If [index] is negative, the first available slot will be
 used.
\******************************************************************************/
int G_ship_spawn(int index, n_client_id_t client, int tile, g_ship_type_t type)
{
        g_ship_t *ship;

        if (!N_client_valid(client) || tile >= r_tiles ||
            type < 0 || type >= G_SHIP_TYPES) {
                C_warning("Invalid parameters (%d, %d, %d, %d)",
                          index, client, tile, type);
                return -1;
        }
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
                int start;

                start = C_rand() % r_tiles;
                for (tile = start + 1; tile < r_tiles; tile++)
                        if (G_open_tile(tile, -1))
                                goto init;
                for (tile = 0; tile <= start; tile++)
                        if (G_open_tile(tile, -1))
                                goto init;

                /* If this happens, the globe is completely full! */
                return -1;
        }

        /* Find an available tile to start the ship on */
        else if (!G_open_tile(tile, -1)) {
                int i, neighbors[12], len;

                /* Not being able to fit a ship in is common, so don't complain
                   if this happens */
                len = R_get_tile_region(tile, neighbors);
                for (i = 0; !G_open_tile(neighbors[i], -1); i++)
                        if (i >= len)
                                return -1;
                tile = neighbors[i];
        }

init:   /* Initialize ship structure */
        ship = g_ships + index;
        C_zero(ship);
        ship->in_use = TRUE;
        ship->type = type;
        ship->tile = ship->target = tile;
        ship->rear_tile = -1;
        ship->progress = 1.f;
        ship->client = client;
        ship->health = g_ship_classes[type].health;
        ship->forward = g_tiles[tile].forward;
        ship->trade_tile = -1;
        ship->focus_stamp = -1;

        /* Start out unnamed */
        C_strncpy_buf(ship->name, C_va("Unnamed #%d", index));

        /* Place the ship on the tile */
        G_set_tile_model(tile, g_ship_classes[type].model_path);
        g_tiles[tile].ship = index;
        g_tiles[tile].fade = 0.f;

        /* Initialize store */
        G_store_init(&ship->store, g_ship_classes[ship->type].cargo);

        /* If we are the server, tell other clients */
        if (n_client_id == N_HOST_CLIENT_ID)
                N_broadcast_except(N_HOST_CLIENT_ID, "11121", G_SM_SHIP_SPAWN,
                                   index, client, tile, type);

        /* If this is one of ours, name it */
        if (client == n_client_id) {
                G_get_name_buf(G_NT_SHIP, ship->name);
                N_send(N_SERVER_ID, "11s", G_CM_SHIP_NAME, index, ship->name);
        }

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

        if (i_limbo)
                return;
        for (i = 0; i < G_SHIPS_MAX; i++) {
                ship = g_ships + i;
                if (!ship->in_use)
                        continue;
                C_assert(ship->tile >= 0 && ship->tile < r_tiles);
                C_assert(g_tiles[ship->tile].ship == i);
                tile = g_tiles + ship->tile;

                /* If globe testing is on, draw a line from ship tile origins */
                if (g_test_globe.value.n) {
                        c_vec3_t b;

                        b = C_vec3_add(tile->origin, C_vec3_norm(tile->origin));
                        R_render_test_line(tile->origin, b,
                                           C_color(0.f, 1.f, 1.f, 1.f));
                }

                /* Don't bother rendering if the ship isn't visible */
                if (!tile->visible)
                        continue;

                /* Draw the status display */
                ship_class = g_ship_classes + ship->type;
                armor = (float)ship->armor / HEALTH_MAX;
                health = (float)ship->health / HEALTH_MAX;
                health_max = (float)ship_class->health / HEALTH_MAX;
                color = g_nations[g_clients[ship->client].nation].color;
                R_render_ship_status(&tile->model, health, health_max,
                                     armor, health_max, color,
                                     g_selected_ship == i,
                                     ship->client == n_client_id);
        }
}

/******************************************************************************\
 Configure the interface to reflect the ship's cargo and trade status.
\******************************************************************************/
static void ship_configure_trade(int index)
{
        g_ship_t *ship, *partner;
        int i;

        /* Our client can't actually see this cargo -- we probably don't have
           the right data for it anyway! */
        ship = g_ships + index;
        if (index < 0 || !ship->store.visible[n_client_id]) {
                I_disable_trade();
                return;
        }

        /* Do we have a trading partner? */
        G_store_space(&ship->store);
        if (ship->trade_tile > 0) {
                partner = g_ships + g_tiles[ship->trade_tile].ship;
                I_enable_trade(ship->client == n_client_id, partner->name,
                               ship->store.space_used, ship->store.capacity);
        } else {
                partner = NULL;
                I_enable_trade(ship->client == n_client_id, NULL,
                               ship->store.space_used, ship->store.capacity);
        }

        /* Configure cargo items */
        for (i = 0; i < G_CARGO_TYPES; i++) {
                i_cargo_info_t info;
                g_cargo_t *cargo;

                /* Our side */
                cargo = ship->store.cargo + i;
                info.amount = cargo->amount;
                info.auto_buy = cargo->auto_buy;
                info.auto_sell = cargo->auto_sell;
                info.buy_price = cargo->buy_price;
                info.sell_price = cargo->sell_price;
                info.minimum = cargo->minimum;
                info.maximum = cargo->maximum;

                /* No trade partner */
                info.p_buy_limit = 0;
                info.p_buy_price = -1;
                info.p_sell_limit = 0;
                info.p_sell_price = -1;
                if (!partner) {
                        info.p_amount = -1;
                        I_configure_cargo(i, &info);
                        continue;
                }

                /* Trade partner's side */
                cargo = partner->store.cargo + i;
                info.p_amount = cargo->amount;
                if (cargo->auto_sell) {
                        info.p_buy_price = partner->store.cargo[i].sell_price;
                        info.p_buy_limit = G_limit_purchase(&ship->store,
                                                            &partner->store,
                                                            i, 999);
                }
                if (cargo->auto_buy) {
                        info.p_sell_price = partner->store.cargo[i].buy_price;
                        info.p_sell_limit = -G_limit_purchase(&ship->store,
                                                              &partner->store,
                                                              i, -999);
                }
                I_configure_cargo(i, &info);
        }
}

/******************************************************************************\
 Returns TRUE if a ship can trade with another ship.
\******************************************************************************/
static bool ship_can_trade(int index)
{
        return index >= 0 && index < G_SHIPS_MAX &&
               g_ships[index].in_use && g_ships[index].rear_tile < 0 &&
               g_ships[index].health > 0;
}

/******************************************************************************\
 Returns TRUE if a ship can trade with the given tile.
\******************************************************************************/
bool G_ship_can_trade_with(int index, int tile)
{
        int i, neighbors[3];

        R_get_tile_neighbors(g_ships[index].tile, neighbors);
        for (i = 0; i < 3; i++)
                if (neighbors[i] == tile)
                        return ship_can_trade(g_tiles[tile].ship);
        return FALSE;
}

/******************************************************************************\
 Check if the ship needs a new trading partner.
\******************************************************************************/
static void ship_update_trade(int index)
{
        g_ship_t *ship;
        int i, trade_tile, neighbors[3];

        /* Only need to do this for our own ships */
        ship = g_ships + index;
        if (ship->client != n_client_id)
                return;

        /* Find a trading partner */
        trade_tile = -1;
        if (ship->rear_tile < 0) {
                R_get_tile_neighbors(ship->tile, neighbors);
                for (i = 0; i < 3; i++) {
                        if (!ship_can_trade(g_tiles[neighbors[i]].ship))
                                continue;
                        trade_tile = neighbors[i];

                        /* Found our old trading partner */
                        if (trade_tile == g_ships[index].trade_tile)
                                return;
                }
        }

        /* Still no partner */
        if (trade_tile < 0 && ship->trade_tile < 0)
                return;

        /* Update to reflect our new trading partner */
        ship->trade_tile = trade_tile;
        if (g_selected_ship == index)
                ship_configure_trade(index);
}

/******************************************************************************\
 Send updated cargo information to clients. If [client] is negative, all
 clients that can see the ship's cargo are updated.
\******************************************************************************/
void G_ship_send_cargo(int index, n_client_id_t client)
{
        int i;

        /* Host already knows everything */
        if (client == N_HOST_CLIENT_ID || n_client_id != N_HOST_CLIENT_ID)
                return;

        /* Pack all the cargo information */
        N_send_start();
        N_send_char(G_SM_SHIP_CARGO);
        N_send_char(index);
        for (i = 0; i < G_CARGO_TYPES; i++) {
                g_cargo_t *cargo;

                cargo = g_ships[index].store.cargo + i;
                N_send_short(cargo->amount);
                N_send_short(cargo->auto_buy ? cargo->buy_price : -1);
                N_send_short(cargo->auto_sell ? cargo->sell_price : -1);
                N_send_short(cargo->minimum);
                N_send_short(cargo->maximum);
        }

        /* Already selected the target clients */
        if (client == N_SELECTED_ID) {
                N_send_selected(NULL);
                return;
        }

        /* Send to selected clients */
        if (client < 0 || client == N_BROADCAST_ID) {
                for (i = 0; i < N_CLIENTS_MAX; i++)
                        n_clients[i].selected = g_ships[index].store.visible[i];
                N_send_selected(NULL);
                return;
        }

        /* Send to a single client */
        N_send(client, NULL);
}

/******************************************************************************\
 Update which clients can see [ship]'s cargo. Also updates the ship's
 neighbors' visibility due to the [ship] having moved into its tile.
\******************************************************************************/
static void ship_update_visible(int ship)
{
        int i, client, neighbors[3];
        bool old_visible[N_CLIENTS_MAX];

        memcpy(old_visible, g_ships[ship].store.visible, sizeof (old_visible));
        C_zero_buf(g_ships[ship].store.visible);

        /* We can always see the stores of our own ships */
        client = g_ships[ship].client;
        if (client >= 0 && client < N_CLIENTS_MAX)
                g_ships[ship].store.visible[client] = TRUE;

        /* Neighboring ships' clients can see our store */
        R_get_tile_neighbors(g_ships[ship].tile, neighbors);
        for (i = 0; i < 3; i++) {
                if (g_tiles[neighbors[i]].ship < 0)
                        continue;
                client = g_ships[g_tiles[neighbors[i]].ship].client;
                if (client >= 0 && client < N_CLIENTS_MAX)
                        g_ships[ship].store.visible[client] = TRUE;
        }

        /* Setup trade if our client's visibility toward this ship changed and
           we have it selected */
        if (g_selected_ship == ship && old_visible[n_client_id] !=
                                       g_ships[ship].store.visible[n_client_id])
                ship_configure_trade(ship);

        /* Update clients' cargo info if we are the host */
        if (n_client_id != N_HOST_CLIENT_ID)
                return;

        /* Only send the update to clients that can see the store now but
           couldn't see it before */
        for (i = 0; i < N_CLIENTS_MAX; i++) {
                n_clients[i].selected = FALSE;
                if (i == N_HOST_CLIENT_ID)
                        continue;
                if (!old_visible[i] && g_ships[ship].store.visible[i])
                        n_clients[i].selected = TRUE;
        }

        G_ship_send_cargo(ship, N_SELECTED_ID);
}

/******************************************************************************\
 Updates ship positions and actions.
\******************************************************************************/
void G_update_ships(void)
{
        int i;

        for (i = 0; i < G_SHIPS_MAX; i++) {
                if (!g_ships[i].in_use)
                        continue;
                G_ship_update_move(i);
                ship_update_trade(i);
                ship_update_visible(i);
        }
}

/******************************************************************************\
 Update which ship the mouse is hovering over and setup the hover window.
 Pass -1 to deselect.
\******************************************************************************/
void G_ship_hover(int index)
{
        r_model_t *model;

        if (g_hover_ship >= 0)
                model = &g_tiles[g_ships[g_hover_ship].tile].model;
        if (g_hover_ship == index) {
                if (index < 0)
                        return;

                /* Ship can get unhighlighted for whatever reason */
                if (!model->selected)
                        model->selected = R_MS_HOVER;

                return;
        }

        /* Deselect last hovered-over ship */
        if (g_hover_ship >= 0 && model->selected == R_MS_HOVER)
                model->selected = R_MS_NONE;

        /* Highlight the new ship */
        if ((g_hover_ship = index) < 0)
                return;
        model = &g_tiles[g_ships[index].tile].model;
        if (!model->selected)
                model->selected = R_MS_HOVER;
}

/******************************************************************************\
 Setup the quick info window to reflect a ship's information.
\******************************************************************************/
static void ship_quick_info(int index)
{
        g_ship_t *ship;
        g_ship_class_t *ship_class;
        i_color_t color;
        float prop;

        if (index < 0) {
                I_quick_info_close();
                return;
        }
        ship = g_ships + index;
        ship_class = g_ship_classes + ship->type;
        I_quick_info_show(ship->name);

        /* Owner */
        color = G_nation_to_color(g_clients[ship->client].nation);
        I_quick_info_add_color("Owner:", g_clients[ship->client].name, color);

        /* Health */
        color = I_COLOR_ALT;
        prop = (float)ship->health / ship_class->health;
        if (prop >= 0.67)
                color = I_COLOR_GOOD;
        if (prop <= 0.33)
                color = I_COLOR_BAD;
        I_quick_info_add_color("Health:", C_va("%d/%d", ship->health,
                                               ship_class->health), color);

        /* Armor */
        color = I_COLOR_ALT;
        prop = (float)ship->armor / ship_class->health;
        if (prop >= 0.67)
                color = I_COLOR_GOOD;
        if (prop <= 0.33)
                color = I_COLOR_BAD;
        I_quick_info_add_color("Armor:", C_va("%d/%d", ship->armor,
                                               ship_class->health), color);
}

/******************************************************************************\
 Select a ship. Pass a negative [index] to deselect.
\******************************************************************************/
void G_ship_select(int index)
{
        r_model_t *model;

        if (g_selected_ship == index)
                return;

        /* Deselect previous ship */
        if (g_selected_ship >= 0) {
                model = &g_tiles[g_ships[g_selected_ship].tile].model;
                model->selected = R_MS_NONE;
        }

        /* Select the new ship */
        if (index >= 0) {
                model = &g_tiles[g_ships[index].tile].model;
                model->selected = R_MS_SELECTED;

                /* Only show the path of our own ships */
                if (g_ships[index].client == n_client_id)
                        R_select_path(g_ships[index].tile, g_ships[index].path);
                else
                        R_select_path(-1, NULL);
        }

        /* Deselect */
        else
                R_select_path(-1, NULL);

        ship_configure_trade(index);
        ship_quick_info(index);

        /* Selecting a ship resets the focus order */
        focus_stamp++;

        g_selected_ship = index;
}

/******************************************************************************\
 Re-selects the currently selected ship if it is [index] and if its client
 is [client]. If either [index] or [client] is negative, they are ignored.
\******************************************************************************/
void G_ship_reselect(int index, n_client_id_t client)
{
        if (g_selected_ship < 0 ||
            (index >= 0 && g_selected_ship != index) ||
            (client >= 0 && g_ships[g_selected_ship].client != client))
                return;
        index = g_selected_ship;
        g_selected_ship = -1;
        G_ship_select(index);
}

/******************************************************************************\
 Returns TRUE if a ship index is valid and can be controlled by the given
 client.
\******************************************************************************/
bool G_ship_controlled_by(int index, n_client_id_t client)
{
        return index >= 0 && index < G_SHIPS_MAX && g_ships[index].in_use &&
               g_ships[index].health > 0 && g_ships[index].client == client;
}

/******************************************************************************\
 Focus on the next suitable ship.
\******************************************************************************/
void G_focus_next_ship(void)
{
        float best_dist;
        int i, best_i, tile, available;

        /* If we have a ship selected, just center on that */
        if (g_selected_ship >= 0) {
                tile = g_ships[g_selected_ship].tile;
                R_rotate_cam_to(g_tiles[tile].model.origin);
                return;
        }

        /* Find the closest available ship */
        available = 0;
        best_i = -1;
        best_dist = C_FLOAT_MAX;
        for (i = 0; i < G_SHIPS_MAX; i++) {
                c_vec3_t origin;
                float dist;

                if (!G_ship_controlled_by(i, n_client_id) ||
                    g_ships[i].focus_stamp >= focus_stamp)
                        continue;
                available++;
                origin = g_tiles[g_ships[i].tile].model.origin;
                dist = C_vec3_len(C_vec3_sub(r_cam_origin, origin));
                if (dist < best_dist) {
                        best_dist = dist;
                        best_i = i;
                }
        }
        if (available <= 1)
                focus_stamp++;
        if (best_i < 0)
                return;
        g_ships[best_i].focus_stamp = focus_stamp;
        tile = g_ships[best_i].tile;
        R_rotate_cam_to(g_tiles[tile].model.origin);
}

