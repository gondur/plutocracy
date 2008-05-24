/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Implements 2D sprites and text rendering with the SDL TTF library. This
   should be the only file that calls functions from the library. */

#include "r_common.h"

/******************************************************************************\
 Initialize a sprite structure. 2D mode textures are expected to be at the
 maximum pixel scale (2x).
\******************************************************************************/
void R_sprite_init(r_sprite_t *sprite, const char *filename)
{
        if (!sprite)
                return;
        C_zero(sprite);
        sprite->modulate = C_color(1., 1., 1., 1.);
        if (!filename || !filename[0])
                return;
        sprite->texture = R_texture_load(filename, FALSE);
        if (sprite->texture) {
                sprite->size.x = sprite->texture->surface->w /
                                 R_PIXEL_SCALE_MAX;
                sprite->size.y = sprite->texture->surface->h /
                                 R_PIXEL_SCALE_MAX;
        }
}

/******************************************************************************\
 Initialize a sprite structure.
\******************************************************************************/
void R_sprite_cleanup(r_sprite_t *sprite)
{
        if (!sprite)
                return;
        R_texture_free(sprite->texture);
        C_zero(sprite);
}

/******************************************************************************\
 Sets up for a render of a 2D sprite. Returns FALSE if the sprite cannot
 be rendered.
\******************************************************************************/
static int sprite_render_start(const r_sprite_t *sprite)
{
        if (!sprite || !sprite->texture || sprite->z > 0.f)
                return FALSE;
        R_push_mode(R_MODE_2D);
        R_texture_select(sprite->texture);

        /* Modulate color */
        glColor4f(sprite->modulate.r, sprite->modulate.g,
                  sprite->modulate.b, sprite->modulate.a);
        if (sprite->modulate.a < 1.f)
                glEnable(GL_BLEND);

        /* If z-offset is enabled (non-zero), depth test the sprite */
        if (sprite->z < 0.f)
                glEnable(GL_DEPTH_TEST);

        /* Setup transformation matrix */
        glPushMatrix();
        glTranslatef(sprite->origin.x + sprite->size.x / 2,
                     sprite->origin.y + sprite->size.y / 2, sprite->z);
        glRotatef(C_rad_to_deg(sprite->angle), 0.0, 0.0, 1.0);

        return TRUE;
}

/******************************************************************************\
 Cleans up after rendering a 2D sprite.
\******************************************************************************/
static void sprite_render_finish(void)
{
        glDisable(GL_DEPTH_TEST);
        glColor4f(1.f, 1.f, 1.f, 1.f);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
        glPopMatrix();
        R_check_errors();
        R_pop_mode();
}

/******************************************************************************\
 Renders a 2D textured quad on screen. If we are rendering a rotated sprite
 that doesn't use alpha, this function will draw an anti-aliased border for it.
 The coordinates work here like you would expect for 2D, the origin is in the
 upper left of the sprite and y decreases down the screen.

 The vertices are arranged in the following order:

   0---3
   |   |
   1---2

 TODO: Disable anti-aliasing here if FSAA is on.
 FIXME: There is a slight overlap between the antialiased edge line and the
        polygon which should probably be fixed if possible.
\******************************************************************************/
void R_sprite_render(const r_sprite_t *sprite)
{
        r_vertex2_t verts[4];
        c_vec2_t half;
        unsigned short indices[] = { 0, 1, 2, 3, 0 };

        if (!sprite_render_start(sprite))
                return;

        /* Render textured quad */
        half = C_vec2_divf(sprite->size, 2.f);
        verts[0].co = C_vec3(-half.x, half.y, 0.f);
        verts[0].uv = C_vec2(0.f, 1.f);
        verts[1].co = C_vec3(-half.x, -half.y, 0.f);
        verts[1].uv = C_vec2(0.f, 0.f);
        verts[2].co = C_vec3(half.x, -half.y, 0.f);
        verts[2].uv = C_vec2(1.f, 0.f);
        verts[3].co = C_vec3(half.x, half.y, 0.f);
        verts[3].uv = C_vec2(1.f, 1.f);
        C_count_add(&r_count_faces, 2);
        glInterleavedArrays(R_VERTEX2_FORMAT, 0, verts);
        glDrawElements(GL_QUADS, 4, GL_UNSIGNED_SHORT, indices);

        /* Draw the edge lines to anti-alias non-alpha quads */
        if (!sprite->texture->alpha && sprite->angle != 0.f &&
            sprite->modulate.a == 1.f) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glEnable(GL_BLEND);
                glDrawElements(GL_LINE_STRIP, 5, GL_UNSIGNED_SHORT, indices);
        }

        sprite_render_finish();
}

/******************************************************************************\
 Blits the entire [src] surface to dest at [x], [y] and applies a one-pixel
 shadow of [alpha] opacity. This function could be made more efficient by
 unrolling the loops a bit, but it is not worth the loss in readablitiy.
\******************************************************************************/
static void blit_shadowed(SDL_Surface *dest, SDL_Surface *src,
                          int to_x, int to_y, float alpha)
{
        float w1, w2;
        int y, x;

        if (SDL_LockSurface(dest) < 0) {
                C_warning("Failed to lock destination surface");
                return;
        }
        if (SDL_LockSurface(src) < 0) {
                C_warning("Failed to lock source surface");
                return;
        }
        if (r_pixel_scale.value.f < 1.f) {
                alpha *= r_pixel_scale.value.f;
                w1 = alpha;
        } else
                w1 = (2.f - r_pixel_scale.value.f) * alpha;
        w2 = alpha - w1;
        for (y = 0; y < src->h + 2; y++)
                for (x = 0; x < src->w + 2; x++) {
                        c_color_t dc, sc;

                        dc = R_surface_get(dest, to_x + x, to_y + y);

                        /* Blit the source image pixel */
                        if (x < src->w && y < src->h) {
                                sc = R_surface_get(src, x, y);
                                dc = C_color_blend(dc, sc);
                                if (dc.a >= 1.f) {
                                        R_surface_put(dest, to_x + x,
                                                      to_y + y, dc);
                                        continue;
                                }
                        }

                        /* Blit the shadow from (x - 1, y - 1) */
                        if (x && y && w1 && x <= src->w && y <= src->h) {
                                sc = R_surface_get(src, x - 1, y - 1);
                                sc = C_color(0, 0, 0, sc.a * w1);
                                dc = C_color_blend(sc, dc);
                        }

                        /* Blit the shadow from (x - 2, y - 2) */
                        if (x > 1 && y > 1 && w2) {
                                sc = R_surface_get(src, x - 2, y - 2);
                                sc = C_color(0, 0, 0, sc.a * w2);
                                dc = C_color_blend(sc, dc);
                        }

                        R_surface_put(dest, to_x + x, to_y + y, dc);
                }
        SDL_UnlockSurface(dest);
        SDL_UnlockSurface(src);
}

/******************************************************************************\
 Setup the sprite for rendering the string. Note that sprite size will be
 reset if it is regenerated. Text can be wrapped (or not, set wrap to 0) and a
 shadow (of variable opacity) can be applied. [string] does need to persist
 after the function call.
\******************************************************************************/
void R_sprite_init_text(r_sprite_t *sprite, r_font_t font, float wrap,
                        float shadow, int invert, const char *string)
{
        r_texture_t *tex;
        int i, j, y, last_break, last_line, width, height, line_skip, char_len;
        char buf[1024], *line;

        if (font < 0 || font >= R_FONTS)
                C_error("Invalid font index %d", font);
        if (!sprite)
                return;
        C_zero(sprite);
        if (!string || !string[0])
                return;

        /* Wrap the text and figure out how large the surface needs to be. The
           width and height also need to be expanded by a pixel to leave room
           for the shadow (up to 2 pixels). */
        wrap *= r_pixel_scale.value.f;
        last_line = 0;
        last_break = 0;
        width = 0;
        height = (line_skip = R_font_line_skip(font)) + 2;
        line = buf;
        for (i = 0, j = 0; ; i += char_len) {
                c_vec2_t size;

                char_len = C_utf8_append(buf, &j, sizeof (buf) - 1, string + i);
                buf[j] = NUL;
                if (!char_len)
                        break;
                if (C_is_space(string[i]))
                        last_break = i;
                size = R_font_size(font, line);
                if ((wrap > 0.f && size.x > wrap) || string[i] == '\n') {
                        if (last_line == last_break)
                                last_break = last_line >= i - 1 ? i : i - 1;
                        j -= i - last_break + 1;
                        buf[j++] = '\n';
                        line = buf + j;
                        while (C_is_space(string[last_break]))
                                last_break++;
                        i = last_break - 1;
                        last_line = last_break;
                        height += line_skip;
                        continue;
                }
                if (size.x > width)
                        width = (int)size.x;
        }
        if (++width < 2 || height < 2)
                return;
        width += 2;

        /* SDL_ttf will generate a 32-bit RGBA surface with the text printed
           on it in white (we can modulate the color later) but it won't
           recognize newlines so we need to print each line to a temporary
           surface and blit to the final surface (in our format). */
        tex = R_texture_alloc(width, height, TRUE);
        for (i = 0, last_break = 0, y = 0; ; i++) {
                SDL_Surface *surf;
                char swap;

                if (buf[i] != '\n' && buf[i])
                        continue;
                swap = buf[i];
                if (last_break < i) {
                        buf[i] = NUL;
                        surf = R_font_render(font, buf + last_break);
                        if (!surf)
                                break;
                        if (invert)
                                R_surface_invert(surf, FALSE, TRUE);
                        blit_shadowed(tex->surface, surf, 0, y, shadow);
                        SDL_FreeSurface(surf);
                }
                if (!swap)
                        break;
                last_break = i + 1;
                y += line_skip;
        }
        R_texture_upload(tex);

        /* Text is actually just a sprite and after this function has finished
           the sprite itself can be manipulated as expected */
        R_sprite_init(sprite, NULL);
        sprite->texture = tex;
        sprite->size.x = width / r_pixel_scale.value.f;
        sprite->size.y = height / r_pixel_scale.value.f;
}

/******************************************************************************\
 Wrapper around the sprite text initializer. Avoids re-rendering the texture
 if the parameters have not changed.
\******************************************************************************/
void R_text_configure(r_text_t *text, r_font_t font, float wrap, float shadow,
                      int invert, const char *string)
{
        if (text->font == font && text->wrap == wrap &&
            text->shadow == shadow && text->invert == invert &&
            !strcasecmp(string, text->buffer))
                return;
        R_sprite_cleanup(&text->sprite);
        R_sprite_init_text(&text->sprite, font, wrap, shadow, invert, string);
        text->frame = c_frame;
        text->font = font;
        text->wrap = wrap;
        text->shadow = shadow;
        text->invert = invert;
        C_strncpy_buf(text->buffer, string);
}

/******************************************************************************\
 Renders a text sprite. Will re-configure the text sprite when necessary.
\******************************************************************************/
void R_text_render(r_text_t *text)
{
        /* Pixel scale changes require re-initialization */
        if (r_pixel_scale.changed > text->frame) {
                c_vec2_t origin;

                text->frame = c_frame;
                origin = text->sprite.origin;
                R_sprite_cleanup(&text->sprite);
                R_sprite_init_text(&text->sprite, text->font, text->wrap,
                                   text->shadow, text->invert, text->buffer);
                text->sprite.origin = origin;
        }

        R_sprite_render(&text->sprite);
}

/******************************************************************************\
 Initializes a window sprite.
\******************************************************************************/
void R_window_init(r_window_t *window, const char *path)
{
        if (!window)
                return;
        R_sprite_init(&window->sprite, path);
        window->corner = C_vec2_divf(window->sprite.size, 4.f);
}

/******************************************************************************\
 Renders a window sprite. A window sprite is composed of a grid of nine quads
 where the corner quads have a fixed size and the connecting quads stretch to
 fill the rest of the sprite size.

 The array is indexed this way:

   0---2------4---6
   |   |      |   |
   1---3------5---7
   |   |      |   |
   |   |      |   |
   11--10-----9---8
   |   |      |   |
   12--13-----14--15

 Note that unlike normal sprites, no edge anti-aliasing is done as it is
 assumed that all windows will make use of alpha transparency.
\******************************************************************************/
void R_window_render(r_window_t *window)
{
        r_vertex2_t verts[16];
        c_vec2_t mid_half, mid_uv, corner;
        unsigned short indices[] = {0, 1, 3, 2,      2, 3, 5, 4,
                                    4, 5, 7, 6,      1, 11, 10, 3,
                                    3, 10, 9, 5,     5, 9, 8, 7,
                                    11, 12, 13, 10,  10, 13, 14, 9,
                                    9, 14, 15, 8};

        if (!window || !sprite_render_start(&window->sprite))
                return;

        /* If the window dimensions are too small to fit the corners in,
           we need to trim the corner size a little */
        corner = window->corner;
        mid_uv = C_vec2(0.25f, 0.25f);
        if (window->sprite.size.x <= corner.x * 2) {
                corner.x = window->sprite.size.x / 2;
                mid_uv.x *= corner.x / window->corner.x;
        }
        if (window->sprite.size.y <= corner.y * 2) {
                corner.y = window->sprite.size.y / 2;
                mid_uv.y *= corner.y / window->corner.y;
        }

        mid_half = C_vec2_divf(window->sprite.size, 2.f);
        mid_half = C_vec2_sub(mid_half, corner);
        verts[0].co = C_vec3(-corner.x - mid_half.x,
                             -corner.y - mid_half.y, 0.f);
        verts[0].uv = C_vec2(0.00f, 0.00f);
        verts[1].co = C_vec3(-corner.x - mid_half.x, -mid_half.y, 0.f);
        verts[1].uv = C_vec2(0.00f, mid_uv.y);
        verts[2].co = C_vec3(-mid_half.x, -corner.y - mid_half.y, 0.f);
        verts[2].uv = C_vec2(mid_uv.x, 0.00f);
        verts[3].co = C_vec3(-mid_half.x, -mid_half.y, 0.f);
        verts[3].uv = C_vec2(mid_uv.x, mid_uv.y);
        verts[4].co = C_vec3(mid_half.x, -corner.y - mid_half.y, 0.f);
        verts[4].uv = C_vec2(1.f - mid_uv.x, 0.00f);
        verts[5].co = C_vec3(mid_half.x, -mid_half.y, 0.f);
        verts[5].uv = C_vec2(1.f - mid_uv.x, mid_uv.y);
        verts[6].co = C_vec3(corner.x + mid_half.x,
                             -corner.y - mid_half.y, 0.f);
        verts[6].uv = C_vec2(1.00f, 0.00f);
        verts[7].co = C_vec3(corner.x + mid_half.x, -mid_half.y, 0.f);
        verts[7].uv = C_vec2(1.00f, mid_uv.y);
        verts[8].co = C_vec3(corner.x + mid_half.x, mid_half.y, 0.f);
        verts[8].uv = C_vec2(1.00f, 1.f - mid_uv.y);
        verts[9].co = C_vec3(mid_half.x, mid_half.y, 0.f);
        verts[9].uv = C_vec2(1.f - mid_uv.x, 1.f - mid_uv.y);
        verts[10].co = C_vec3(-mid_half.x, mid_half.y, 0.f);
        verts[10].uv = C_vec2(mid_uv.x, 1.f - mid_uv.y);
        verts[11].co = C_vec3(-corner.x - mid_half.x, mid_half.y, 0.f);
        verts[11].uv = C_vec2(0.00f, 1.f - mid_uv.y);
        verts[12].co = C_vec3(-corner.x - mid_half.x,
                              corner.y + mid_half.y, 0.f);
        verts[12].uv = C_vec2(0.00f, 1.00f);
        verts[13].co = C_vec3(-mid_half.x, corner.y + mid_half.y, 0.f);
        verts[13].uv = C_vec2(mid_uv.x, 1.00f);
        verts[14].co = C_vec3(mid_half.x, corner.y + mid_half.y, 0.f);
        verts[14].uv = C_vec2(1.f - mid_uv.x, 1.00f);
        verts[15].co = C_vec3(corner.x + mid_half.x,
                              corner.y + mid_half.y, 0.f);
        verts[15].uv = C_vec2(1.00f, 1.00f);

        C_count_add(&r_count_faces, 18);
        glInterleavedArrays(R_VERTEX2_FORMAT, 0, verts);
        glDrawElements(GL_QUADS, 36, GL_UNSIGNED_SHORT, indices);

        sprite_render_finish();
}

/******************************************************************************\
 Initializes a billboard point sprite.
\******************************************************************************/
void R_billboard_init(r_billboard_t *bb, const char *filename)
{
        R_sprite_init(&bb->sprite, filename);
        bb->size = (bb->sprite.size.x + bb->sprite.size.y) / 2;
        bb->world_origin = C_vec3(0.f, 0.f, 0.f);
}

/******************************************************************************\
 Render a billboard point sprite.
\******************************************************************************/
void R_billboard_render(r_billboard_t *bb)
{
        GLfloat model_view[16], projection[16];
        c_vec3_t co;

        /* If the point sprite extension is available we can use it instead
           of doing all of the math on the CPU */
        if (r_extensions[R_EXT_POINT_SPRITE]) {
                R_push_mode(R_MODE_3D);
                R_texture_select(bb->sprite.texture);
                glPointSize(bb->size);
                glBegin(GL_POINTS);
                glVertex3f(bb->world_origin.x, bb->world_origin.y,
                           bb->world_origin.z);
                glEnd();
                R_pop_mode();
                return;
        }

        /* Get the transformation matrices from 3D mode */
        R_push_mode(R_MODE_3D);
        glGetFloatv(GL_MODELVIEW_MATRIX, model_view);
        glGetFloatv(GL_PROJECTION_MATRIX, projection);
        R_pop_mode();

        /* Push a point through the transformation matrices */
        co = bb->world_origin;
        co = C_vec3_tfm(co, model_view);
        co = C_vec3_tfm(co, projection);
        co.x = (co.x + 1.f) * r_width_2d / 2.f + bb->sprite.size.x;
        co.y = (1.f - co.y) * r_height_2d / 2.f + bb->sprite.size.y;
        co.z = (co.z + 1.f) / -2.f;
        if (co.z >= 0.f)
                return;

        /* Render the billboard as a regular sprite but pushed back in the z
           direction and with depth testing.
           FIXME: Why do we have to translate by the size times 1.5? */
        bb->sprite.origin.x = co.x - bb->sprite.size.x * 1.5f;
        bb->sprite.origin.y = co.y - bb->sprite.size.y * 1.5f;
        bb->sprite.size.x = bb->size / r_pixel_scale.value.f;
        bb->sprite.size.y = bb->sprite.size.x;
        bb->sprite.z = co.z;
        R_sprite_render(&bb->sprite);
}

