/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Devin Papineau

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "r_common.h"
#include "../game/g_shared.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include "SDL.h"

/******************************************************************************\
 Render the test mesh.
\******************************************************************************/
static void render_test_mesh(void)
{
        static float x_rot, y_rot;
        float white[] = { 1.0, 1.0, 1.0, 1.0 };
        float left[] = { -1.0, 0.0, 0.0, 0.0 };

        if (!r_test_mesh_data)
                return;

        glLoadIdentity();

        /* Setup a white light to the left */
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glLightfv(GL_LIGHT0, GL_POSITION, left);
        glLightfv(GL_LIGHT0, GL_COLOR, white);

        /* Use a white testing material */
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        glColor3f(1.0, 1.0, 1.0);

        /* FIXME: Disabling culling */
        glDisable(GL_CULL_FACE);

        /* Render the test mesh */
        glPushMatrix();
        glTranslatef(0.0, 0.0, -20.0);
        glRotatef(x_rot, 1.0, 0.0, 0.0);
        glRotatef(y_rot, 0.0, 1.0, 0.0);
        glTranslatef(0.0, -10.0, 0.0);
        R_static_mesh_render(r_test_mesh_data);
        glPopMatrix();

        x_rot += 8 * c_frame_sec;
        y_rot += 20 * c_frame_sec;
}

/******************************************************************************\
 Render the test sphere.
\******************************************************************************/
void render_test_sphere()
{
        static float x_rot, y_rot;
        static g_sphere_t *sphere = NULL;
        r_static_mesh_t fake_mesh;

        if (!sphere)
                sphere = G_sphere_alloc(5); /* subdivide thrice */

        /* No lighting or culling */
        glDisable(GL_LIGHTING);
        glDisable(GL_CULL_FACE);

        /* Fake a static mesh */
        fake_mesh.nverts = sphere->nverts;
        fake_mesh.ninds = sphere->ninds;
        fake_mesh.verts = sphere->verts;
        fake_mesh.inds = sphere->inds;
        fake_mesh.norms = NULL;
        fake_mesh.sts = NULL;

        glPushMatrix();

        /* And render */
        glTranslatef(0.0, 0.0, -20.0);
        glRotatef(x_rot, 1.0, 0.0, 0.0);
        glRotatef(y_rot, 0.0, 1.0, 0.0);
        glScalef(5, 5, 5);
        glColor3f(0.0, 0.0, 0.0);
        R_static_mesh_render(&fake_mesh); /* render for depth */
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glColor3f(1.0, 1.0, 1.0);
        R_static_mesh_render(&fake_mesh); /* render lines over */

        /* See if neighbors works */
        c_vec3_t v = sphere->verts[8];
        int i;

        glPointSize(5.0);
        glBegin(GL_POINTS);
        glColor3f(1.0, 0.0, 0.0);
        glVertex3f(v.x, v.y, v.z);
        glColor3f(0.0, 1.0, 1.0);
        for (i = 0; i < sphere->neighbors_lists[8].count; i++) {
                v = sphere->verts[sphere->neighbors_lists[8].indices[i]];
                glVertex3f(v.x, v.y, v.z);
        }
        glEnd();

        glPopMatrix();

        /* Reset polygon mode */
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        x_rot += 8 * c_frame_sec;
        y_rot += 20 * c_frame_sec;
}

/****************************************************************************** \
 Renders the scene.
\******************************************************************************/
void R_render(void)
{
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (0)
                render_test_mesh();

        render_test_sphere();

        SDL_GL_SwapBuffers();
}

