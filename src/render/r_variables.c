/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "r_common.h"

/* Window parameters */
c_var_t r_width, r_height, r_colordepth, r_depth, r_windowed, r_mesh;

/* The one mesh that we currently look at. */
r_static_mesh_t* r_mesh_data = NULL;

/******************************************************************************\
 Registers the render variables.
\******************************************************************************/
void R_register_variables(void)
{
        /* Window parameters */
        C_register_integer(&r_width, "r_width", 800);
        C_register_integer(&r_height, "r_height", 600);
        C_register_integer(&r_depth, "r_colordepth", 24);
        C_register_integer(&r_depth, "r_depth", 16);
        C_register_integer(&r_windowed, "r_windowed", 1);
        C_register_string(&r_mesh, "r_mesh", "");
}

