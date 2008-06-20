/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Implements texture loading, manipulation, and management. Textures are
   stored in a linked list and should only be accessed during loading time so
   efficiency here is not important. */

#include "r_common.h"

/* Font configuration variables */
extern c_var_t r_font_console, r_font_console_pt, r_font_gui, r_font_gui_pt,
               r_font_title, r_font_title_pt;

/* Terrain tile sheet */
r_texture_t *r_terrain_tex;

/* White generated texture */
r_texture_t *r_white_tex;

/* SDL format used for textures */
SDL_PixelFormat r_sdl_format;

/* Font asset array */
static struct {
        TTF_Font *ttf_font;
        int line_skip, width, height;
} fonts[R_FONTS];

static c_ref_t *root, *root_alloc;
static int ttf_inited;

/******************************************************************************\
 Frees memory associated with a texture.
\******************************************************************************/
static void texture_cleanup(r_texture_t *pt)
{
        if (pt->surface) {

                /* Keep track of free'd memory */
                r_sdl_mem -= pt->surface->w * pt->surface->h *
                             pt->surface->format->BytesPerPixel;

                SDL_FreeSurface(pt->surface);
        }
        glDeleteTextures(1, &pt->gl_name);
        R_check_errors();
}

/*****************************************************************************\
 Just allocate a texture object and cleared [width] by [height] SDL surface.
 Does not load anything or add the texture to the texture linked list. The
 texture needs to be uploaded after calling this function.
\******************************************************************************/
r_texture_t *R_texture_alloc_full(const char *file, int line, const char *func,
                                  int width, int height, int alpha)
{
        static int count;
        SDL_Rect rect;
        r_texture_t *pt;

        if (width < 1 || height < 1)
                C_error_full(file, line, func,
                             "Invalid texture dimensions: %dx%d",
                             width, height);
        if (!C_is_pow2(width) || !C_is_pow2(height))
                C_warning_full(file, line, func,
                               "Texture dimensions not power-of-two: %dx%d",
                               width, height);
        pt = C_recalloc_full(file, line, func, NULL, sizeof (*pt));
        pt->ref.refs = 1;
        pt->ref.cleanup_func = (c_ref_cleanup_f)texture_cleanup;
        if (root_alloc) {
                pt->ref.next = root_alloc;
                root_alloc->prev = &pt->ref;
        }
        pt->ref.root = &root_alloc;
        root_alloc = &pt->ref;
        if (c_mem_check.value.n)
                C_strncpy_buf(pt->ref.name,
                              C_va("Texture #%d allocated by %s()",
                                   ++count, func));
        pt->alpha = alpha;
        pt->surface = R_surface_alloc(width, height, alpha);
        rect.x = 0;
        rect.y = 0;
        rect.w = width;
        rect.h = height;
        SDL_FillRect(pt->surface, &rect, 0);
        glGenTextures(1, &pt->gl_name);
        R_check_errors();
        if (c_mem_check.value.n)
                C_trace_full(file, line, func, "Allocated texture #%d", count);
        return pt;
}

/******************************************************************************\
 Reads in an area of the screen (current buffer) from ([x], [y]) the size of
 the texture. You must manually upload the texture after calling this function.
\******************************************************************************/
void R_texture_screenshot(r_texture_t *texture, int x, int y)
{
        SDL_Surface *video;

        video = SDL_GetVideoSurface();
        if (SDL_LockSurface(texture->surface) < 0) {
                C_warning("Failed to lock texture: %s", SDL_GetError());
                return;
        }
        glReadPixels(x, r_height.value.n - texture->surface->h - y,
                     texture->surface->w, texture->surface->h,
                     GL_RGBA, GL_UNSIGNED_BYTE, texture->surface->pixels);
        R_surface_flip_v(texture->surface);
        R_check_errors();
        SDL_UnlockSurface(texture->surface);
}

/******************************************************************************\
 Allocates a new texture and copies the contents of another texture to it.
 The texture must be uploaded after calling this function.
\******************************************************************************/
r_texture_t *R_texture_clone_full(const char *file, int line, const char *func,
                                  const r_texture_t *src)
{
        r_texture_t *dest;

        if (!src)
                return NULL;
        dest = R_texture_alloc_full(file, line, func, src->surface->w,
                                    src->surface->h, src->alpha);
        if (!dest)
                return NULL;
        if (SDL_BlitSurface(src->surface, NULL, dest->surface, NULL) < 0)
                C_warning("Failed to clone texture '%s': %s",
                          src->ref.name, SDL_GetError());
        dest->mipmaps = src->mipmaps;
        return dest;
}

/******************************************************************************\
 If the texture's SDL surface has changed, the image must be reloaded into
 OpenGL. This function will do this. It is assumed that the texture surface
 format has not changed since the texture was created. Note that mipmaps will
 make UI textures look blurry so do not use them for anything that will be
 rendered in 2D mode.
\******************************************************************************/
void R_texture_upload(const r_texture_t *pt)
{
        int gl_internal;

        if (pt->alpha) {
                gl_internal = GL_RGBA;
                if (r_color_bits.value.n == 16)
                        gl_internal = GL_RGBA4;
                else if (r_color_bits.value.n == 32)
                        gl_internal = GL_RGBA8;
        } else {
                gl_internal = GL_RGB;
                if (r_color_bits.value.n == 16)
                        gl_internal = GL_RGB5;
                else if (r_color_bits.value.n == 32)
                        gl_internal = GL_RGB8;
        }
        glBindTexture(GL_TEXTURE_2D, pt->gl_name);
        if (pt->mipmaps)
                gluBuild2DMipmaps(GL_TEXTURE_2D, gl_internal,
                                  pt->surface->w, pt->surface->h,
                                  GL_RGBA, GL_UNSIGNED_BYTE,
                                  pt->surface->pixels);
        else
                glTexImage2D(GL_TEXTURE_2D, 0, gl_internal,
                             pt->surface->w, pt->surface->h, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, pt->surface->pixels);
        R_check_errors();
}

/******************************************************************************\
 Unload all textures from OpenGL.
\******************************************************************************/
void R_dealloc_textures(void)
{
        r_texture_t *tex;

        C_debug("Deallocting loaded textures");
        tex = (r_texture_t *)root;
        while (tex) {
                glDeleteTextures(1, &tex->gl_name);
                tex = (r_texture_t *)tex->ref.next;
        }

        C_debug("Deallocating allocated textures");
        tex = (r_texture_t *)root_alloc;
        while (tex) {
                glDeleteTextures(1, &tex->gl_name);
                tex = (r_texture_t *)tex->ref.next;
        }
}

/******************************************************************************\
 Reuploads all textures in the linked list to OpenGL.
\******************************************************************************/
void R_realloc_textures(void)
{
        r_texture_t *tex;

        C_debug("Uploading loaded textures");
        tex = (r_texture_t *)root;
        while (tex) {
                glGenTextures(1, &tex->gl_name);
                R_texture_upload(tex);
                tex = (r_texture_t *)tex->ref.next;
        }

        C_debug("Uploading allocated textures");
        tex = (r_texture_t *)root_alloc;
        while (tex) {
                glGenTextures(1, &tex->gl_name);
                R_texture_upload(tex);
                tex = (r_texture_t *)tex->ref.next;
        }
}

/******************************************************************************\
 Loads a texture from file. If the texture has already been loaded, just ups
 the reference count and returns the loaded surface. If the texture failed to
 load, returns NULL.
\******************************************************************************/
r_texture_t *R_texture_load(const char *filename, int mipmaps)
{
        r_texture_t *pt;
        int found;

        if (!filename || !filename[0])
                return NULL;

        /* Find a place for the texture */
        pt = C_ref_alloc(sizeof (*pt), &root, (c_ref_cleanup_f)texture_cleanup,
                         filename, &found);
        if (found)
                return pt;

        /* Load the image file */
        pt->mipmaps = mipmaps;
        pt->surface = R_surface_load_png(filename, &pt->alpha);
        if (!pt->surface) {
                C_ref_down(&pt->ref);
                return NULL;
        }

        /* Load the texture into OpenGL */
        glGenTextures(1, &pt->gl_name);
        R_texture_upload(pt);
        R_check_errors();

        return pt;
}

/******************************************************************************\
 Selects (binds) a texture for rendering in OpenGL. Also sets whatever options
 are necessary to get the texture to show up properly.
\******************************************************************************/
void R_texture_select(r_texture_t *texture)
{
        if (!texture) {
                glDisable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, 0);
                glDisable(GL_BLEND);
                glDisable(GL_ALPHA_TEST);
                return;
        }
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture->gl_name);

        /* Texture repeat */
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        /* Mipmaps */
        if (texture->mipmaps) {
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR_MIPMAP_LINEAR);
                if (texture->mipmaps > 1)
                        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD,
                                        (GLfloat)texture->mipmaps);
        } else
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        /* Anisotropic filtering */
        if (r_ext.anisotropy > 1.f) {
                GLfloat aniso;

                aniso = texture->anisotropy;
                if (aniso > r_ext.anisotropy)
                        aniso = r_ext.anisotropy;
                if (aniso < 1.f)
                        aniso = 1.f;
                glTexParameterf(GL_TEXTURE_2D,
                                GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
        }

        /* Additive blending */
        if (texture->additive) {
                glEnable(GL_BLEND);
                glDisable(GL_ALPHA_TEST);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        } else {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                /* Alpha blending */
                if (texture->alpha) {
                        glEnable(GL_BLEND);
                        glEnable(GL_ALPHA_TEST);
                } else {
                        glDisable(GL_BLEND);
                        glDisable(GL_ALPHA_TEST);
                }
        }

        R_check_errors();
}

/******************************************************************************\
 Renders a plain quad without any sprite effects. Coordinates refer to the
 upper left-hand corner.
\******************************************************************************/
void R_texture_render(r_texture_t *tex, int x, int y)
{
        r_vertex2_t verts[4];
        unsigned short indices[] = {0, 1, 2, 3};

        verts[0].co = C_vec3(0.f, 0.f, 0.f);
        verts[0].uv = C_vec2(0.f, 0.f);
        verts[1].co = C_vec3(0.f, (float)tex->surface->h, 0.f);
        verts[1].uv = C_vec2(0.f, 1.f);
        verts[2].co = C_vec3((float)tex->surface->w,
                             (float)tex->surface->h, 0.f);
        verts[2].uv = C_vec2(1.f, 1.f);
        verts[3].co = C_vec3((float)tex->surface->w, 0.f, 0.f);
        verts[3].uv = C_vec2(1.f, 0.f);
        R_push_mode(R_MODE_2D);
        R_texture_select(tex);
        glTranslatef((GLfloat)x, (GLfloat)y, 0.f);
        glInterleavedArrays(R_VERTEX2_FORMAT, 0, verts);
        glDrawElements(GL_QUADS, 4, GL_UNSIGNED_SHORT, indices);
        R_check_errors();
        R_pop_mode();
}

/******************************************************************************\
 Returns the height of the tallest glyph in the font.
\******************************************************************************/
int R_font_height(r_font_t font)
{
        if (!fonts[font].ttf_font)
                C_error("Forgot to load fonts");
        return fonts[font].height;
}

/******************************************************************************\
 Returns the width of the widest glyph in the font.
\******************************************************************************/
int R_font_width(r_font_t font)
{
        if (!fonts[font].ttf_font)
                C_error("Forgot to load fonts");
        return fonts[font].width;
}

/******************************************************************************\
 Returns distance from start of one line to the start of the next.
\******************************************************************************/
int R_font_line_skip(r_font_t font)
{
        return fonts[font].line_skip;
}

/******************************************************************************\
 Returns distance from start of one line to the start of the next.
\******************************************************************************/
c_vec2_t R_font_size(r_font_t font, const char *text)
{
        int w, h;

        TTF_SizeUTF8(fonts[font].ttf_font, text, &w, &h);
        return C_vec2((float)w, (float)h);
}

/******************************************************************************\
 Renders a line of text onto a newly-allocated SDL surface. This surface must
 be freed by the caller.
\******************************************************************************/
SDL_Surface *R_font_render(r_font_t font, const char *text)
{
        SDL_Surface *surf;
        SDL_Color white = { 255, 255, 255, 255 };

        surf = TTF_RenderUTF8_Blended(fonts[font].ttf_font, text, white);
        if (!surf) {
                C_warning("TTF_RenderUTF8_Blended() failed: %s",
                          TTF_GetError());
                return surf;
        }
        return surf;
}

/******************************************************************************\
 Loads the font, properly scaled and generates an error message if something
 goes wrong. For some reason, SDL_ttf returns a line skip that is one pixel shy
 of the image height returned by TTF_RenderUTF8_Blended() so we account for
 that here.
\******************************************************************************/
static void load_font(r_font_t font, const char *path, int size)
{
        int points;

        points = (int)ceilf(size * r_pixel_scale.value.f);
        if (points < R_FONT_SIZE_MIN)
                points = R_FONT_SIZE_MIN;
        C_zero(fonts + font);
        fonts[font].ttf_font = TTF_OpenFont(path, points);
        if (!fonts[font].ttf_font) {
                C_warning("Failed to load font '%s' (%d -> %d pt)",
                          path, size, points);
                return;
        }
        fonts[font].height = TTF_FontHeight(fonts[font].ttf_font);
        fonts[font].line_skip = TTF_FontLineSkip(fonts[font].ttf_font) + 1;

        /* SDL_ttf won't tell us the width of the widest glyph directly so we
           assume it is the width of 'W' */
        fonts[font].width = (int)R_font_size(font, "W").x;
}

/******************************************************************************\
 Initialize the SDL TTF library and generate font structures.
\******************************************************************************/
void R_load_fonts(void)
{
        const char *override;
        int i;

        if (!ttf_inited)
                return;

        /* Get the locale font override, if any */
        override = C_str("r-font-override", "");
        if (override[0]) {
                C_var_set(&r_font_console, override);
                C_var_set(&r_font_gui, override);
                C_var_set(&r_font_title, override);
        }

        /* Console font */
        C_var_unlatch(&r_font_console);
        C_var_unlatch(&r_font_console_pt);
        load_font(R_FONT_CONSOLE, r_font_console.value.s,
                  r_font_console_pt.value.n);

        /* GUI font */
        C_var_unlatch(&r_font_gui);
        C_var_unlatch(&r_font_gui_pt);
        load_font(R_FONT_GUI, r_font_gui.value.s, r_font_gui_pt.value.n);

        /* Title font */
        C_var_unlatch(&r_font_title);
        C_var_unlatch(&r_font_title_pt);
        load_font(R_FONT_TITLE, r_font_title.value.s, r_font_title_pt.value.n);

        for (i = 0; i < R_FONTS; i++)
                if (!fonts[i].ttf_font)
                        C_error("Forgot to load font %d", i + 1);
}

/******************************************************************************\
 Loads render assets.
\******************************************************************************/
void R_load_assets(void)
{
        SDL_version compiled;
        const SDL_version *linked;

        C_status("Loading render assets");

        /* Setup the texture pixel format, RGBA in 32 bits */
        r_sdl_format.BitsPerPixel = 32;
        r_sdl_format.BytesPerPixel = 4;
        r_sdl_format.Amask = 0xff000000;
        r_sdl_format.Bmask = 0x00ff0000;
        r_sdl_format.Gmask = 0x0000ff00;
        r_sdl_format.Rmask = 0x000000ff;
        r_sdl_format.Ashift = 24;
        r_sdl_format.Bshift = 16;
        r_sdl_format.Gshift = 8;
        r_sdl_format.alpha = 255;

        /* Initialize SDL_ttf library */
        TTF_VERSION(&compiled);
        C_debug("Compiled with SDL_ttf %d.%d.%d",
                compiled.major, compiled.minor, compiled.patch);
        linked = TTF_Linked_Version();
        if (!linked)
                C_error("Failed to get SDL_ttf linked version");
        C_debug("Linked with SDL_ttf %d.%d.%d",
                linked->major, linked->minor, linked->patch);
        if (TTF_Init() == -1)
                C_error("Failed to initialize SDL TTF library: %s",
                        TTF_GetError());
        ttf_inited = TRUE;
        R_load_fonts();

        /* Generate procedural content */
        r_terrain_tex = R_texture_load("models/globe/terrain.png", TRUE);
        R_prerender();

        /* Create a fake white texture */
        r_white_tex = R_texture_alloc(1, 1, FALSE);
        if (SDL_LockSurface(r_white_tex->surface) >= 0) {
                R_surface_put(r_white_tex->surface, 0, 0,
                              C_color(1.f, 1.f, 1.f, 1.f));
                SDL_UnlockSurface(r_white_tex->surface);
        } else
                C_warning("Failed to lock white texture");
        R_texture_upload(r_white_tex);
}

/******************************************************************************\
 Cleans up fonts.
\******************************************************************************/
void R_free_fonts(void)
{
        int i;

        if (!ttf_inited)
                return;
        for (i = 0; i < R_FONTS; i++)
                TTF_CloseFont(fonts[i].ttf_font);
}

/******************************************************************************\
 Cleans up the asset resources. Shuts down the SDL_ttf library.
\******************************************************************************/
void R_free_assets(void)
{
        /* Print out estimated memory usage */
        if (c_mem_check.value.n)
                C_debug("SDL surface memory high mark %.1fmb",
                        r_sdl_mem_high / (1024.f * 1024.f));

        R_texture_free(r_terrain_tex);
        R_texture_free(r_white_tex);
        R_free_fonts();
        TTF_Quit();
}

