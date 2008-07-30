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

/******************************************************************************\
 Find the length of the longest select option. Returns the width of the
 widest option.
\******************************************************************************/
static float select_widest(i_select_t *select)
{
        i_select_option_t *option;
        c_vec2_t size;
        float width;

        /* Numeric widget maximum is assumed to be widest */
        if (!select->options) {
                const char *fmt;

                fmt = C_va("%%.0%df", select->decimals);
                size = R_font_size(select->item.font, C_va(fmt, select->max));
                return size.x / r_pixel_scale.value.f;
        }

        /* Cycle through each option */
        width = 0.f;
        select->list_len = 0;
        for (option = select->options; option; option = option->next) {
                size = R_font_size(select->item.font, option->string);
                size.x /= r_pixel_scale.value.f;
                if (size.x > width)
                        width = size.x;
                select->list_len++;
        }
        return width;
}

/******************************************************************************\
 Selection widget event function.
\******************************************************************************/
int I_select_event(i_select_t *select, i_event_t event)
{
        if (event == I_EV_CONFIGURE) {
                select->item.width = select_widest(select);
                select->widget.size.y = R_font_height(R_FONT_GUI) /
                                        r_pixel_scale.value.f;
                I_widget_pack(&select->widget, I_PACK_H, I_FIT_NONE);
                select->widget.size = I_widget_child_bounds(&select->widget);
                return FALSE;
        }
        if (event == I_EV_CLEANUP) {
                i_select_option_t *option, *next;

                option = select->options;
                while (option) {
                        next = option->next;
                        C_free(option);
                        option = next;
                }
                select->options = NULL;
        }
        return TRUE;
}

/******************************************************************************\
 Change the selection widget's item by index.
\******************************************************************************/
void I_select_change(i_select_t *select, int index)
{
        i_select_option_t numeric, *option;
        int i, max;

        /* Numeric widget */
        if (select->list_len <= 0)
                max = (select->max - select->min) / select->increment;
        else
                max = select->list_len - 1;

        /* Range checks */
        if (index <= 0) {
                index = 0;
                select->left.widget.state = I_WS_DISABLED;
        } else if (select->left.widget.state == I_WS_DISABLED) {
                select->left.widget.state = I_WS_READY;
        }
        if (index >= max) {
                index = max;
                select->right.widget.state = I_WS_DISABLED;
        } else if (select->right.widget.state == I_WS_DISABLED)
                select->right.widget.state = I_WS_READY;

        /* Already set? */
        if (select->index == index)
                return;
        select->index = index;

        /* Get the option */
        if (select->list_len > 0) {
                option = select->options;
                for (i = 0; option && i < index; i++)
                        option = option->next;
        }

        /* For numeric options, fake one */
        else {
                float value;
                const char *fmt;

                option = &numeric;
                value = select->min + select->increment * select->index;
                fmt = C_va("%%.0%df%%s", select->decimals);
                snprintf(option->string, sizeof (option->string), fmt, value,
                         select->suffix ? select->suffix : "");
                if (select->variable) {
                        if (select->variable->type == C_VT_FLOAT)
                                option->value.f = value;
                        else if (select->variable->type == C_VT_INTEGER)
                                option->value.n = (int)(value + 0.5f);
                }
        }

        if (select->widget.configured)
                I_label_configure(&select->item, option->string);
        else
                C_strncpy_buf(select->item.buffer, option->string);
        if (select->on_change)
                select->on_change(select);

        /* If a auto-set variable is configured, set it now */
        if (select->variable && option) {
                if (select->variable->type == C_VT_FLOAT)
                        C_var_set(select->variable,
                                  C_va("%g", option->value.f));
                else if (select->variable->type == C_VT_INTEGER)
                        C_var_set(select->variable,
                                  C_va("%d", option->value.n));
                else
                        C_var_set(select->variable, option->string);
        }
}

/******************************************************************************\
 Left select button clicked.
\******************************************************************************/
static void left_arrow_clicked(i_button_t *button)
{
        i_select_t *select;

        select = (i_select_t *)button->data;
        I_select_change(select, select->index - 1);
}

/******************************************************************************\
 Right select button clicked.
\******************************************************************************/
static void right_arrow_clicked(i_button_t *button)
{
        i_select_t *select;

        select = (i_select_t *)button->data;
        I_select_change(select, select->index + 1);
}

/******************************************************************************\
 Add a value to the start of the options list.
\******************************************************************************/
static i_select_option_t *select_add(i_select_t *select, const char *string)
{
        i_select_option_t *option;

        option = C_malloc(sizeof (*option));
        C_strncpy_buf(option->string, string);
        option->next = select->options;
        select->options = option;
        select->list_len++;
        return option;
}

void I_select_add_string(i_select_t *select, const char *string)
{
        select_add(select, string);
}

void I_select_add_float(i_select_t *select, float f, const char *override)
{
        i_select_option_t *option;
        const char *fmt;

        if (override)
                option = select_add(select, override);
        else if (select->suffix && select->suffix[0]) {
                fmt = C_va("%%.0%df%%s", select->decimals);
                option = select_add(select, C_va(fmt, f, select->suffix));
        } else {
                fmt = C_va("%%.0%df", select->decimals);
                option = select_add(select, C_va(fmt, f, select->suffix));
        }
        option->value.f = f;
}

void I_select_add_int(i_select_t *select, int n, const char *override)
{
        i_select_option_t *option;

        if (override)
                option = select_add(select, override);
        else if (select->suffix && select->suffix[0])
                option = select_add(select, C_va("%d%s", n, select->suffix));
        else
                option = select_add(select, C_va("%d", n, select->suffix));
        option->value.n = n;
}

/******************************************************************************\
 Update the selection widget with the nearest value to the variable we are
 trying to set.
\******************************************************************************/
void I_select_update(i_select_t *select)
{
        i_select_option_t *option;
        int i, best;

        if (!select->variable)
                return;

        /* Numeric widgets don't need to cycle */
        if (select->list_len <= 0) {
                float value;

                if (select->variable->type == C_VT_FLOAT)
                        value = select->variable->value.f;
                else if (select->variable->type == C_VT_INTEGER)
                        value = (int)(select->variable->value.n + 0.5f);
                else
                        C_error("Invalid variable type %d",
                                select->variable->type);
                if (value < select->min)
                        value = select->min;
                if (value > select->max)
                        value = select->max;
                best = (int)((value - select->min) / select->increment + 0.5f);
                I_select_change(select, best);
                return;
        }

        /* Go through the options list and find the closest value */
        best = 0;
        if (select->variable->type == C_VT_FLOAT) {
                float diff, best_diff;

                best_diff = C_FLOAT_MAX;
                for (option = select->options, i = 0; option;
                     option = option->next, i++) {
                        diff = select->variable->value.f - option->value.f;
                        if (diff < 0.f)
                                diff = -diff;
                        if (diff < best_diff) {
                                best = i;
                                if (!diff)
                                        break;
                                best_diff = diff;
                        }
                }
        } else if (select->variable->type == C_VT_INTEGER) {
                int diff, best_diff;

                best_diff = C_INT_MAX;
                for (option = select->options, i = 0; option;
                     option = option->next, i++) {
                        diff = select->variable->value.n - option->value.n;
                        if (diff < 0)
                                diff = -diff;
                        if (diff < best_diff) {
                                best = i;
                                if (!diff)
                                        break;
                                best_diff = diff;
                        }
                }
        } else
                C_error("Invalid variable type %d", select->variable->type);
        I_select_change(select, best);
}

/******************************************************************************\
 Initialize a selection widget.
\******************************************************************************/
void I_select_init(i_select_t *select, const char *label, const char *suffix)
{
        if (!select)
                return;
        C_zero(select);
        I_widget_init(&select->widget, "Select");
        select->widget.event_func = (i_event_f)I_select_event;
        select->widget.state = I_WS_READY;
        select->suffix = suffix;
        select->decimals = 2;
        select->index = -1;

        /* Description label */
        I_label_init(&select->label, label);
        select->label.widget.expand = TRUE;
        select->label.widget.margin_rear = 0.5f;
        I_widget_add(&select->widget, &select->label.widget);

        /* Left button */
        I_button_init(&select->left, "gui/icons/arrow-left.png", NULL,
                      I_BT_ROUND);
        select->left.widget.state = I_WS_DISABLED;
        select->left.widget.margin_rear = 0.5f;
        select->left.on_click = (i_callback_f)left_arrow_clicked;
        select->left.data = select;
        I_widget_add(&select->widget, &select->left.widget);

        /* Selected item label */
        I_label_init(&select->item, NULL);
        select->item.color = I_COLOR_ALT;
        I_widget_add(&select->widget, &select->item.widget);

        /* Right button */
        I_button_init(&select->right, "gui/icons/arrow-right.png", NULL,
                      I_BT_ROUND);
        select->right.widget.state = I_WS_DISABLED;
        select->right.widget.margin_front = 0.5f;
        select->right.on_click = (i_callback_f)right_arrow_clicked;
        select->right.data = select;
        I_widget_add(&select->widget, &select->right.widget);
}

