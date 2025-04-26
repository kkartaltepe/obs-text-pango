#pragma once

#include <obs-module.h>

enum {
	ALIGN_LEFT = 0,
	ALIGN_RIGHT = 1,
	ALIGN_CENTER = 2,
	ALIGN_TOP = 3,
	ALIGN_BOTTOM = 4, // Align with gdi/freetype if possible.
};


struct pango_source {
	/* Config */
	bool from_file;
	char *text;
	char *text_file;

	bool font_from_file;
	const char *font_file; // direct from QT

	char *font_name;
	uint16_t font_size;
	uint32_t font_flags;

	bool vertical;

	int align;
	int v_align;

	bool gradient;
	uint32_t color[2];

	bool outline;
	uint32_t outline_width;
	uint32_t outline_color;

	bool drop_shadow;
	uint32_t drop_shadow_offset;
	uint32_t drop_shadow_color;

	bool log_mode;
	uint32_t log_lines;
	uint32_t custom_width;
	bool word_wrap;

	char *encoding;

	char *lang;

	/* State */
	gs_texture_t *tex;
	uint32_t width, height;

	bool file_load_failed;
	time_t file_timestamp;
	float file_last_checked; // Sec since last checked
};
