/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Implements model, mesh, and texture loading and management. Textures are
   stored in a linked list and should only be accessed during loading time so
   efficiency here is not important. */

#include "r_common.h"

/* Texture linked list */
static c_ref_t *root;

/* SDL/OpenGL variables */
static SDL_PixelFormat sdl_rgb_fmt, sdl_rgba_fmt;
static int gl_rgb_type, gl_rgba_type;

/******************************************************************************\
 Frees memory associated with a texture.
\******************************************************************************/
static void texture_cleanup(r_texture_t *pt)
{
        SDL_FreeSurface(pt->surface);
        glDeleteTextures(1, &pt->gl_name);
}

/******************************************************************************\
 Gets a pixel from an SDL surface. Lock the surface before calling this!
\******************************************************************************/
c_color_t R_SDL_get_pixel(const SDL_Surface *surf, int x, int y)
{
        Uint8 *p, r, g, b, a;
        Uint32 pixel;
        int bpp;

        bpp = surf->format->BytesPerPixel;
        p = (Uint8 *)surf->pixels + y * surf->pitch + x * bpp;
        switch(bpp) {
        case 1: pixel = *p;
                break;
        case 2: pixel = *(Uint16 *)p;
                break;
        case 3: pixel = SDL_BYTEORDER == SDL_BIG_ENDIAN ?
                        p[0] << 16 | p[1] << 8 | p[2] :
                        p[0] | p[1] << 8 | p[2] << 16;
                break;
        case 4: pixel = *(Uint32 *)p;
                break;
        default:
                C_error("Invalid surface format");
        }
        SDL_GetRGBA(pixel, surf->format, &r, &g, &b, &a);
        return C_color32(r, g, b, a);
}

/******************************************************************************\
 Sets the color of a pixel on an SDL surface. Lock the surface before
 calling this!
\******************************************************************************/
void R_SDL_put_pixel(SDL_Surface *surf, int x, int y, c_color_t color)
{
        Uint8 *p;
        Uint32 pixel;
        int bpp;

        bpp = surf->format->BytesPerPixel;
        p = (Uint8 *)surf->pixels + y * surf->pitch + x * bpp;
        pixel = SDL_MapRGBA(surf->format, 255.f * color.r, 255.f * color.g,
                            255.f * color.b, 255.f * color.a);
        switch(bpp) {
        case 1: *p = pixel;
                break;
        case 2: *(Uint16 *)p = pixel;
                break;
        case 3: if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                        p[0] = (pixel >> 16) & 0xff;
                        p[1] = (pixel >> 8) & 0xff;
                        p[2] = pixel & 0xff;
                } else {
                        p[0] = pixel & 0xff;
                        p[1] = (pixel >> 8) & 0xff;
                        p[2] = (pixel >> 16) & 0xff;
                }
                break;
        case 4: *(Uint32 *)p = pixel;
                break;
        default:
                C_error("Invalid surface format");
        }
}

/******************************************************************************\
 Iterates over an SDL surface and checks if any pixels actually use alpha
 blending. Returns TRUE if at least one pixel has alpha less than full.
\******************************************************************************/
static int surface_uses_alpha(SDL_Surface *surf)
{
        int x, y, bpp;

        if (SDL_LockSurface(surf) < 0) {
                C_warning("Failed to lock surface");
                return TRUE;
        }
        bpp = surf->format->BytesPerPixel;
        for (y = 0; y < surf->h; y++)
                for (x = 0; x < surf->w; x++)
                        if (R_SDL_get_pixel(surf, x, y).a < 1.f) {
                                SDL_UnlockSurface(surf);
                                return TRUE;
                        }
        SDL_UnlockSurface(surf);
        return FALSE;
}

/******************************************************************************\
 Just allocate a texture object and cleared w by h SDL surface. Does not load
 anything or add the texture to the texture linked list. The texture needs to
 be uploaded after you are done editing it. Don't forget to lock the surface!

 Mip-maps are disabled by default for allocated textures, if you are going to
 use the texture on a 3D object, you will want to enable them for that
 texture.
\******************************************************************************/
r_texture_t *R_texture_alloc_full(const char *file, int line, const char *func,
                                  int w, int h, int alpha)
{
        static int count;
        SDL_Rect rect;
        SDL_PixelFormat *format;
        r_texture_t *pt;
        int flags;

        if (w < 1 || h < 1)
                C_error("Invalid texture dimensions");
        pt = C_calloc(sizeof (*pt));
        pt->ref.refs = 1;
        pt->ref.cleanup_func = (c_ref_cleanup_f)texture_cleanup;
        if (c_mem_check.value.n)
                C_strncpy_buf(pt->ref.name,
                              C_va("Texture #%d allocated by %s()",
                                   ++count, func));
        pt->alpha = alpha;
        flags = SDL_HWSURFACE;
        if (alpha) {
                flags |= SDL_SRCALPHA;
                format = &sdl_rgba_fmt;
                pt->gl_format = GL_RGBA;
                pt->gl_type = gl_rgba_type;
        } else {
                format = &sdl_rgb_fmt;
                pt->gl_format = r_color_bits.value.n > 16 ? GL_RGBA : GL_RGB;
                pt->gl_type = gl_rgb_type;
        }
        pt->surface = SDL_CreateRGBSurface(flags, w, h, format->BitsPerPixel,
                                           format->Rmask, format->Gmask,
                                           format->Bmask, format->Amask);
        SDL_SetAlpha(pt->surface, SDL_SRCALPHA, 255);
        rect.x = 0;
        rect.y = 0;
        rect.w = w;
        rect.h = h;
        SDL_FillRect(pt->surface, &rect, 0);
        glGenTextures(1, &pt->gl_name);
        R_check_errors();
        if (c_mem_check.value.n)
                C_debug_full(file, line, func, "Allocated texture #%d", count);
        return pt;
}

/******************************************************************************\
 If the texture's SDL surface has changed, the image must be reloaded into
 OpenGL. This function will do this. It is assumed that the texture surface
 format has not changed since the texture was created. Note that mipmaps will
 make UI textures look blurry so do not use them for anything that will be
 rendered in 2D mode.
\******************************************************************************/
void R_texture_upload(const r_texture_t *pt, int mipmaps)
{
        int flags, gl_internal;

        flags = SDL_HWSURFACE;
        if (pt->alpha) {
                gl_internal = GL_RGBA;
                flags |= SDL_SRCALPHA;
        } else
                gl_internal = GL_RGB;
        glBindTexture(GL_TEXTURE_2D, pt->gl_name);
        if (mipmaps) {
                gluBuild2DMipmaps(GL_TEXTURE_2D, gl_internal,
                                  pt->surface->w, pt->surface->h,
                                  pt->gl_format, pt->gl_type,
                                  pt->surface->pixels);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR_MIPMAP_NEAREST);
        } else {
                glTexImage2D(GL_TEXTURE_2D, 0, gl_internal,
                             pt->surface->w, pt->surface->h, 0,
                             pt->gl_format, pt->gl_type, pt->surface->pixels);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR);
        }
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        R_check_errors();
}

/******************************************************************************\
 Loads a texture using SDL_image. If the texture has already been loaded,
 just ups the reference count and returns the loaded surface.
   TODO: Support loading from sources other than files.
\******************************************************************************/
r_texture_t *R_texture_load(const char *filename, int mipmaps)
{
        SDL_Surface *surface, *surface_new;
        SDL_PixelFormat *sdl_format;
        r_texture_t *pt;
        int found, flags;

        if (!filename || !filename[0])
                return NULL;

        /* Find a place for the texture */
        pt = C_ref_alloc(sizeof (*pt), &root, (c_ref_cleanup_f)texture_cleanup,
                         filename, &found);
        if (found)
                return pt;

        /* Load the image file */
        surface = IMG_Load(filename);
        if (!surface) {
                C_warning("Failed to load texture '%s'", filename);
                return NULL;
        }

        /* Convert to our texture format */
        pt->alpha = surface_uses_alpha(surface);
        flags = SDL_HWSURFACE;
        if (pt->alpha) {
                pt->gl_type = gl_rgba_type;
                pt->gl_format = GL_RGBA;
                sdl_format = &sdl_rgba_fmt;
                flags |= SDL_SRCALPHA;
        } else {
                pt->gl_format = r_color_bits.value.n > 16 ? GL_RGBA : GL_RGB;
                pt->gl_type = gl_rgb_type;
                sdl_format = &sdl_rgb_fmt;
        }
        surface_new = SDL_ConvertSurface(surface, sdl_format, flags);
        SDL_FreeSurface(surface);
        if (!surface_new) {
                C_warning("Failed to convert texture '%s'", filename);
                return NULL;
        }
        pt->surface = surface_new;

        /* Load the texture into OpenGL */
        glGenTextures(1, &pt->gl_name);
        R_texture_upload(pt, mipmaps);
        R_check_errors();

        return pt;
}

/******************************************************************************\
 Selects (binds) a texture for rendering in OpenGL. Also sets whatever options
 are necessary to get the texture to show up properly.
\******************************************************************************/
void R_texture_select(r_texture_t *texture)
{
        int alpha;

        alpha = FALSE;
        if (texture) {
                glBindTexture(GL_TEXTURE_2D, texture->gl_name);
                alpha = texture->alpha;
                if (r_mode == R_MODE_3D) {
                        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                        GL_REPEAT);
                        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                        GL_REPEAT);
                } else {
                        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                        GL_CLAMP);
                        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                        GL_CLAMP);
                }
        } else
                glBindTexture(GL_TEXTURE_2D, 0);
        if (alpha) {
                glEnable(GL_BLEND);
                glEnable(GL_ALPHA_TEST);
        } else {
                glDisable(GL_BLEND);
                glDisable(GL_ALPHA_TEST);
        }
}

/******************************************************************************\
 Loads render assets.
\******************************************************************************/
void R_load_assets(void)
{
        C_status("Loading render assets");

        /* Setup the texture pixel format, RGBA in 32 or 16 bits */
        C_zero(&sdl_rgb_fmt);
        C_zero(&sdl_rgba_fmt);
        if (r_color_bits.value.n > 16) {

                /* RGBA */
                sdl_rgba_fmt.BitsPerPixel = 32;
                sdl_rgba_fmt.BytesPerPixel = 4;
                sdl_rgba_fmt.Rmask = 0xff000000;
                sdl_rgba_fmt.Gmask = 0x00ff0000;
                sdl_rgba_fmt.Bmask = 0x0000ff00;
                sdl_rgba_fmt.Amask = 0x000000ff;
                sdl_rgba_fmt.Rshift = 24;
                sdl_rgba_fmt.Gshift = 16;
                sdl_rgba_fmt.Bshift = 8;
                gl_rgba_type = GL_UNSIGNED_INT_8_8_8_8;

                /* FIXME: Just using RGBA format, is there even a plain RGB? */
                sdl_rgb_fmt = sdl_rgba_fmt;
                gl_rgb_type = gl_rgba_type;

                C_debug("Using 32-bit textures");
        } else {

                /* RGB */
                sdl_rgb_fmt.BitsPerPixel = 16;
                sdl_rgb_fmt.BytesPerPixel = 2;
                sdl_rgb_fmt.Rmask = 0xf800;
                sdl_rgb_fmt.Gmask = 0x07e0;
                sdl_rgb_fmt.Bmask = 0x001f;
                sdl_rgb_fmt.Rshift = 11;
                sdl_rgb_fmt.Gshift = 5;
                sdl_rgb_fmt.Rloss = 3;
                sdl_rgb_fmt.Gloss = 2;
                sdl_rgb_fmt.Bloss = 3;
                gl_rgb_type = GL_UNSIGNED_SHORT_5_6_5;

                /* RGBA */
                sdl_rgba_fmt.BitsPerPixel = 16;
                sdl_rgba_fmt.BytesPerPixel = 2;
                sdl_rgba_fmt.Rmask = 0xf000;
                sdl_rgba_fmt.Gmask = 0x0f00;
                sdl_rgba_fmt.Bmask = 0x00f0;
                sdl_rgba_fmt.Amask = 0x000f;
                sdl_rgba_fmt.Rshift = 12;
                sdl_rgba_fmt.Gshift = 8;
                sdl_rgba_fmt.Bshift = 4;
                sdl_rgba_fmt.Rloss = 4;
                sdl_rgba_fmt.Gloss = 4;
                sdl_rgba_fmt.Bloss = 4;
                sdl_rgba_fmt.Aloss = 4;
                gl_rgba_type = GL_UNSIGNED_SHORT_4_4_4_4;

                C_debug("Using 16-bit textures");
        }
}

/******************************************************************************\
 Cleans up the asset resources.
\******************************************************************************/
void R_free_assets(void)
{
}

