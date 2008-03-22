/******************************************************************************\
 Plutocracy GenDoc - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* Boolean defines */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NUL
#define NUL '\0'
#endif

/* Copies string to a fixed-size buffer */
#define D_strncpy_buf(b, s) strncpy(b, s, sizeof (b))

/* List entry */
typedef struct entry {
        char name[80], def[1024], comment[4096];
        struct entry *next;
} entry_t;

/* gendoc_entry.c */
void D_entry_add(const entry_t *, entry_t **root);
entry_t *D_entry_find(entry_t *root, const char *name);
void D_output_entries(FILE *, entry_t *root);

/* gendoc_parse.c */
const char *D_comment(void);
const char *D_def(void);
void D_parse_close(void);
int D_parse_def(void);
int D_parse_open(const char *filename);
const char *D_token(int n);

extern int d_num_tokens;

