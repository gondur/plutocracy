/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Functions for trading */

#include "g_common.h"

/******************************************************************************\
 Returns FALSE if the two cargo structures differ in a significant way.
\******************************************************************************/
bool G_cargo_equal(const g_cargo_t *a, const g_cargo_t *b)
{
        if (a->auto_buy != b->auto_buy || a->auto_sell != b->auto_sell)
                return TRUE;
        if (a->auto_buy &&
            (a->maximum != b->maximum || a->buy_price != b->buy_price))
                return TRUE;
        if (a->auto_sell &&
            (a->minimum != b->minimum || a->sell_price != b->sell_price))
                return TRUE;
        return FALSE;
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
 Returns the amount of space the given cargo manifest contains. Updates the
 store's cached space count.
\******************************************************************************/
int G_store_space(g_store_t *store)
{
        int i;

        for (store->space_used = i = 0; i < G_CARGO_TYPES; i++) {
                if (store->cargo[i].amount < 0)
                        continue;
                store->space_used += (int)ceilf(cargo_space(i) *
                                                store->cargo[i].amount);
        }
        return store->space_used;
}

/******************************************************************************\
 Returns the amount of a cargo that will fit in a given store.
\******************************************************************************/
int G_store_fits(const g_store_t *store, g_cargo_type_t cargo)
{
        int space_left;

        if ((space_left = store->capacity - store->space_used) <= 0)
                return 0;
        return (int)floorf(space_left / cargo_space(cargo));
}

/******************************************************************************\
 Add or subtract cargo from a store. Returns the amount actually added or
 subtracted.
\******************************************************************************/
int G_store_add(g_store_t *store, g_cargo_type_t cargo, int amount)
{
        int excess;

        /* Store is already overflowing */
        if (store->space_used > store->capacity)
                return 0;

        /* Don't take more than what's there */
        if (amount < -store->cargo[cargo].amount)
                amount = -store->cargo[cargo].amount;

        /* Don't put in more than what it can hold */
        store->cargo[cargo].amount += amount;
        if ((excess = G_store_space(store) - store->capacity) > 0) {
                store->cargo[cargo].amount -= (int)(excess / 
                                                    cargo_space(cargo));
                store->space_used = store->capacity;
        }
        C_assert(store->cargo[cargo].amount >= 0);

        return amount;
}

/******************************************************************************\
 Returns the amount of a cargo that a store can transfer from another store.
\******************************************************************************/
int G_limit_purchase(const g_store_t *buyer, const g_store_t *seller,
                     g_cargo_type_t cargo, int amount)
{
        int limit, price;
        bool reversed;

        price = seller->cargo[cargo].sell_price;

        /* Negative amount is a sale */
        if ((reversed = amount < 0)) {
                const g_store_t *temp;

                price = seller->cargo[cargo].buy_price;
                temp = buyer;
                buyer = seller;
                seller = temp;
                amount = -amount;
        }

        /* Buying too much */
        limit = buyer->cargo[cargo].maximum - buyer->cargo[cargo].amount;
        if (amount > limit)
                amount = limit;

        /* Selling too much */
        limit = seller->cargo[cargo].amount - seller->cargo[cargo].minimum;
        if (amount > limit)
                amount = limit;

        /* How much can you fit? */
        if (amount > (limit = G_store_fits(buyer, cargo)))
                amount = limit;

        /* How much does the seller have? */
        if (amount > (limit = seller->cargo[cargo].amount))
                amount = limit;

        if (price > 0) {

                /* How much can you afford? */
                if (amount > (limit = buyer->cargo[G_CT_GOLD].amount / price))
                        amount = limit;

                /* How much of the gold can the seller hold? */
                limit = (int)((seller->capacity - seller->space_used) /
                              (cargo_space(G_CT_GOLD) * amount * price));
                if (amount > limit)
                        amount = limit;
        }
        if (amount < 0)
                return 0;
        return reversed ? -amount : amount;
}

/******************************************************************************\
 Select the clients that can see this store.
\******************************************************************************/
void G_store_select_clients(const g_store_t *store)
{
        int i;

        if (!store)
                return;
        for (i = 0; i < N_CLIENTS_MAX; i++)
                n_clients[i].selected = store->visible[i];
}

/******************************************************************************\
 Initialize the store structure, set default prices, minimums, and maximums.
\******************************************************************************/
void G_store_init(g_store_t *store, int capacity)
{
        int i;

        C_zero(store);
        store->capacity = capacity;
        for (i = 0; i < G_CARGO_TYPES; i++) {
                store->cargo[i].maximum = (int)(capacity / cargo_space(i));
                store->cargo[i].buy_price = 50;
                store->cargo[i].sell_price = 50;
        }

        /* Buying/selling gold refers to donations so do not charge for it */
        store->cargo[G_CT_GOLD].buy_price = 0;
        store->cargo[G_CT_GOLD].sell_price = 0;
        store->cargo[G_CT_GOLD].auto_buy = TRUE;

        /* Don't want to sell all of your crew */
        store->cargo[G_CT_CREW].minimum = 1;
}

