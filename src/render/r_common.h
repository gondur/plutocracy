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
#include "r_shared.h"

/* Supported extensions */
typedef enum {
        R_EXT_MULTITEXTURE,
        R_EXTENSIONS,
} r_extension_t;

/* Vertex type for meshes */
#pragma pack(push, 4)
typedef struct r_vertex3 {
        c_vec2_t uv;
        c_vec3_t no;
        c_vec3_t co;
} r_vertex3_t;
#pragma pack(pop)
#define R_VERTEX3_FORMAT GL_T2F_N3F_V3F

/* Vertex type for sprites */
#pragma pack(push, 4)
typedef struct r_vertex2 {
        c_vec2_t uv;
        c_vec3_t co;
} r_vertex2_t;
#pragma pack(pop)
#define R_VERTEX2_FORMAT GL_T2F_V3F

/* Texture class */
struct r_texture {
        c_ref_t ref;
        SDL_Surface *surface;
        GLuint gl_name;
        int alpha;
};

/* Non-animated mesh */
typedef struct r_static_mesh {
        r_vertex3_t *verts;
        unsigned short *indices;
        int verts_len, indices_len;
} r_static_mesh_t;

/* Model animation type */
typedef struct r_model_anim {
        int from, to, delay;
        char name[64], end_anim[64];
} r_model_anim_t;

/* Model object type */
typedef struct r_model_object {
        r_texture_t *texture;
        char name[64];
} r_model_object_t;

/* Animated, textured, multi-mesh model. The matrix contains enough room to
   store every object's static mesh for every frame, it is indexed by frame
   then by object. */
typedef struct r_model_data {
        c_ref_t ref;
        r_static_mesh_t *matrix;
        r_model_anim_t *anims;
        r_model_object_t *objects;
        int anims_len, objects_len, frames;
} r_model_data_t;

/* Render modes */
typedef enum {
        R_MODE_NONE,
        R_MODE_HOLD,
        R_MODE_2D,
        R_MODE_3D,
} r_mode_t;

/* r_assets.c */
SDL_Surface *R_font_render(r_font_t, const char *);
void R_free_assets(void);
void R_load_assets(void);
void R_surface_flip_v(SDL_Surface *);
c_color_t R_surface_get(const SDL_Surface *, int x, int y);
void R_surface_invert(SDL_Surface *, int rgb, int alpha);
void R_surface_mask(SDL_Surface *dest, SDL_Surface *src);
void R_surface_put(SDL_Surface *, int x, int y, c_color_t);
#define R_texture_alloc(w, h, a) R_texture_alloc_full(__FILE__, __LINE__, \
                                                      __func__, w, h, a)
r_texture_t *R_texture_alloc_full(const char *file, int line, const char *func,
                                  int width, int height, int alpha);
#define R_texture_clone(t) R_texture_clone_full(__FILE__, __LINE__, \
                                                __func__, t)
r_texture_t *R_texture_clone_full(const char *file, int line, const char *func,
                                  const r_texture_t *);
#define R_texture_free(t) C_ref_down((c_ref_t *)(t))
r_texture_t *R_texture_load(const char *filename, int mipmaps);
#define R_texture_ref(t) C_ref_up((c_ref_t *)(t))
void R_texture_render(r_texture_t *, int x, int y);
void R_texture_select(r_texture_t *);
void R_texture_upload(const r_texture_t *, int mipmaps);

extern r_texture_t *r_terrain_tex;

/* r_globe.c */
void R_render_globe(void);

extern float r_globe_radius;

/* r_mode.c */
extern GLfloat r_cam_matrix[16], r_proj3_matrix[16];

/* r_prerender.c */
void R_prerender(void);

/* r_render.c */
#define R_check_errors() R_check_errors_full(__FILE__, __LINE__, __func__);
void R_check_errors_full(const char *file, int line, const char *func);
void R_set_mode(r_mode_t);

extern r_mode_t r_mode;
extern float r_cam_dist, r_cam_zoom;
extern int r_extensions[R_EXTENSIONS];

/* r_solar.c */
void R_cleanup_solar(void);
void R_disable_light(void);
void R_enable_light(void);
void R_init_solar(void);
void R_render_solar(void);

/* r_static_mesh.c */
void R_static_mesh_cleanup(r_static_mesh_t *);
unsigned short R_static_mesh_find_vert(const r_static_mesh_t *mesh,
                                       const r_vertex3_t *vert);
void R_static_mesh_render(r_static_mesh_t *, r_texture_t *);
void R_static_mesh_free(r_static_mesh_t *);

/* r_test.c */
void R_render_tests(void);

/* r_variables.c */
extern c_var_t r_clear, r_color_bits, r_depth_bits, r_gamma, r_globe_colors[4],
               r_globe_shininess, r_globe_smooth, r_gl_errors, r_light,
               r_moon_atten, r_moon_colors[3], r_moon_height, r_solar,
               r_sun_colors[3], r_test_sprite_num, r_test_sprite, r_test_model,
               r_test_prerender, r_test_text, r_vsync, r_windowed;

