#pragma once

#include "text-pango.h"
#include <pango/pangocairo.h>
#include <pango/pangofc-fontmap.h>
#include <util/platform.h>
#include <obs-module.h>
#include <gmodule.h>
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>

#define RGBA_CAIRO(c) \
	 (c & 0xff) / 256.0, \
	((c & 0xff00) >> 8) / 256.0, \
	((c & 0xff0000) >> 16) / 256.0, \
	((c & 0xff000000) >> 24) / 256.0

cairo_t *create_layout_context()
{
	cairo_surface_t *temp_surface;
	cairo_t *context;

	temp_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
	context = cairo_create(temp_surface);
	cairo_surface_destroy(temp_surface);

	return context;
}

static void set_font(struct pango_source *src, PangoLayout *layout) {
	PangoFontDescription *desc = NULL;

#if FC_VERSION >= 21291
	if (src->font_from_file && src->font_file && strcmp(src->font_file, "") != 0) {
		if (FcConfigAppFontAddFile(NULL, src->font_file)) {
			FcFontSet *font_set = FcFontSetCreate();
			int count = FcFreeTypeQueryAll(src->font_file, -1, NULL, &count, font_set);
			if (count > 0 ) {
				// need to explicitly notify newer Pango versions that a new font has been added to the FC map
				// more info: https://gitlab.gnome.org/GNOME/gtk/-/issues/3886
				if (PANGO_IS_FC_FONT_MAP(pango_cairo_font_map_get_default())) {
					pango_fc_font_map_config_changed(PANGO_FC_FONT_MAP(pango_cairo_font_map_get_default()));
				}
				desc = pango_fc_font_description_from_pattern(font_set->fonts[0], FALSE);
				if (count > 1) {
					blog(LOG_INFO, "[pango] Specified file(%s) had more than 1 font", src->font_file);
				}
			}
		} else {
			blog(LOG_WARNING, "[pango] Failed to load font: %s", src->font_file);
		}
	}
#endif

	if (!desc) { // If we havnt loaded a font file, go ahead and load our font name choice.
		desc = pango_font_description_new ();
		pango_font_description_set_family(desc, src->font_name);
		pango_font_description_set_weight(desc, !!(src->font_flags & OBS_FONT_BOLD) ? PANGO_WEIGHT_BOLD : 0);
		pango_font_description_set_style(desc, !!(src->font_flags & OBS_FONT_ITALIC) ? PANGO_STYLE_ITALIC : 0);
	}

	pango_font_description_set_size(desc, (src->font_size * PANGO_SCALE * 2)/3); // Scaling to approximate GDI text pts
	pango_layout_set_font_description(layout, desc);

	pango_font_description_free(desc);
}

static void set_halignment(struct pango_source *src, PangoLayout *layout) {
	PangoAlignment pangoAlignment;

	switch (src->align) {
	case ALIGN_RIGHT:
		pangoAlignment = PANGO_ALIGN_RIGHT;
		break;
	case ALIGN_CENTER:
		pangoAlignment = PANGO_ALIGN_CENTER;
		break;
	default:
		pangoAlignment = PANGO_ALIGN_LEFT;	
	}
	pango_layout_set_alignment(layout, pangoAlignment);
}

static void set_lang(struct pango_source *src, PangoLayout *layout) {
	PangoLanguage *pangoLang;

	if (src->lang) {
		pangoLang = pango_language_from_string(src->lang);
		if(pangoLang)
			pango_context_set_language(pango_layout_get_context(layout), pangoLang);
	}
}

static bool pango_source_properties_outline_changed(obs_properties_t *props,
		obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	obs_property_t *prop;

	bool enabled = obs_data_get_bool(settings, "outline");

	prop = obs_properties_get(props, "outline_width");
	obs_property_set_visible(prop, enabled);
	prop = obs_properties_get(props, "outline_color");
	obs_property_set_visible(prop, enabled);

	return true;
}

static bool pango_source_properties_drop_shadow_changed(obs_properties_t *props,
		obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	obs_property_t *prop;

	bool enabled = obs_data_get_bool(settings, "drop_shadow");

	prop = obs_properties_get(props, "drop_shadow_offset");
	obs_property_set_visible(prop, enabled);
	prop = obs_properties_get(props, "drop_shadow_color");
	obs_property_set_visible(prop, enabled);

	return true;
}

static bool pango_source_properties_gradient_changed(obs_properties_t *props,
		obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	obs_property_t *prop;

	bool enabled = obs_data_get_bool(settings, "gradient");

	prop = obs_properties_get(props, "color2");
	obs_property_set_visible(prop, enabled);

	return true;
}

static bool pango_source_properties_encoding_changed(obs_properties_t *props,
		obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	obs_property_t *prop;

	bool enabled = obs_data_get_bool(settings, "encoding.enable");

	prop = obs_properties_get(props, "encoding.name");
	obs_property_set_visible(prop, enabled);

	return true;
}

static bool pango_source_properties_lang_changed(obs_properties_t *props,
		obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	obs_property_t *prop;

	bool enabled = obs_data_get_bool(settings, "lang.enable");

	prop = obs_properties_get(props, "lang.code");
	obs_property_set_visible(prop, enabled);

	return true;
}

static bool pango_source_properties_from_file_changed(obs_properties_t *props,
		obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	obs_property_t *text_prop,*file_prop;

	bool enabled = obs_data_get_bool(settings, "from_file");

	text_prop = obs_properties_get(props, "text");
	file_prop = obs_properties_get(props, "text_file");
	obs_property_set_visible(text_prop, !enabled);
	obs_property_set_visible(file_prop, enabled);

	return true;
}

static bool pango_source_properties_font_from_file_changed(obs_properties_t *props,
		obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	obs_property_t *font_prop,*font_file_prop,*font_file_size_p;

	bool enabled = obs_data_get_bool(settings, "font_from_file");

	font_prop = obs_properties_get(props, "font");
	font_file_prop = obs_properties_get(props, "font_file");
	font_file_size_p = obs_properties_get(props, "font_file_size");
	obs_property_set_visible(font_prop, !enabled);
	obs_property_set_visible(font_file_prop, enabled);
	obs_property_set_visible(font_file_size_p, enabled);

	return true;
}

static bool pango_source_properties_log_mode_changed(obs_properties_t *props,
		obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	obs_property_t *lines_prop;

	bool enabled = obs_data_get_bool(settings, "log_mode");

	lines_prop = obs_properties_get(props, "log_lines");
	obs_property_set_visible(lines_prop, enabled);

	return true;
}

char *encoding_ln[4] = {
	"\n", // unknown just use single byte newline
	"\n", // UTF-8
	"\n\x00" // UTF-16LE
	"\x00\n", // UTF-16BE
};

static bool read_from_end(char **dst_buf, size_t *size, uint8_t *utf_encoding, const char *filename, int lines)
{
	FILE *file = NULL;
	long filesize = 0; long cur_pos = 0;
	uint16_t line_breaks = 0;
	size_t bytes_read;

	uint8_t encoding = 0;
	char bom[4] = {0, 0, 0, 0};
	long header_offset = 0;
	long alignment = 1;
	uint8_t bytes[2] = {0, 0};

	bool utf16 = false;

	file = os_fopen(filename, "rb");
	if (file == NULL) {
		blog(LOG_WARNING, "[pango] Failed to open file %s", filename);
		return false;
	}
	bytes_read = fread(bom, 1, 3, file);

	if (bytes_read == 3 && strncmp(bom, "\xEF\xBB\xBF", 3) == 0) {
		utf16 = false;
		encoding = 1; // UTF-8
		header_offset = 3;
	} else if (bytes_read >= 2 && strncmp(bom, "\xFF\xFE", 2) == 0) {
		utf16 = true;
		encoding = 2; // UTF-16LE
		header_offset = 2;
		alignment = 2;
	} else if (bytes_read >= 2 && strncmp(bom, "\xFE\xFF", 2) == 0) {
		utf16 = true;
		encoding = 3; // UTF-16BE
		header_offset = 2;
		alignment = 2;
	}

	fseek(file, 0, SEEK_END);
	filesize = ftell(file);
	cur_pos = filesize;

	bool trailing_ln_checked = false;
	while (line_breaks <= lines && cur_pos != 0) {
		cur_pos -= alignment;
		fseek(file, cur_pos, SEEK_SET);

		bytes_read = fread(bytes, 1, alignment, file);
		if(bytes_read == alignment && strncmp(bytes, encoding_ln[encoding], alignment) == 0) {
			if (trailing_ln_checked)
				line_breaks++;
			else
				filesize -= alignment;
		}
		trailing_ln_checked = true;
	}

	// If we are not at the end shift past the newline, otherwise shift off the header
	if (cur_pos != 0)
		cur_pos += alignment;
	else
		cur_pos += header_offset;

	fseek(file, cur_pos, SEEK_SET);

	*dst_buf = bzalloc(filesize - cur_pos + 1);
	*size = fread(*dst_buf, 1, filesize - cur_pos, file);
	(*dst_buf)[filesize - cur_pos] = 0;
	*utf_encoding = encoding;

	fclose(file);
	return true;
}

static bool read_whole_file(char **dst_buf, size_t *size, uint8_t *utf_encoding, const char *filename) {
	size_t len = 0;
	char *tmp_buf = NULL;
	char bom[4] = {0, 0, 0, 0};
	uint8_t encoding = 0;
	long header_offset = 0;

	FILE *file = os_fopen(filename, "rb");
	if(!file) {
		blog(LOG_WARNING, "[pango] Failed to open file %s", filename);
		return false;
	}
	len = fread(bom, 1, 3, file);

	if (len == 3 && strncmp(bom, "\xEF\xBB\xBF", 3) == 0) {
		encoding = 1; // UTF-8
		header_offset = 3;
	} else if (len >= 2 && strncmp(bom, "\xFF\xFE", 2) == 0) {
		encoding = 2; // UTF-16LE
		header_offset = 2;
	} else if (len >= 2 && strncmp(bom, "\xFE\xFF", 2) == 0) {
		encoding = 3; // UTF-16BE
		header_offset = 2;
	}

	fseek(file, 0, SEEK_END);
	len = (size_t)os_ftelli64(file);
	tmp_buf = bmalloc(len-header_offset+1);

	if (len > 0) {
		fseek(file, header_offset, SEEK_SET);
		size_t len_read = fread(tmp_buf, 1, len-header_offset, file);
		if (len_read == 0) {
			bfree(tmp_buf);
			fclose(file);
			return false;
		}
	}
	fclose(file);

	*dst_buf = tmp_buf;
	(*dst_buf)[len-header_offset] = 0;
	*size = len-header_offset;
	*utf_encoding = encoding;
	return true;
}

static bool read_textfile(struct pango_source *src)
{
	char *tmp_buf = NULL;
	size_t size = 0;
	uint8_t	encoding_header = 0;

	if(src->log_mode) {
		if (!read_from_end(&tmp_buf, &size, &encoding_header, src->text_file, src->log_lines)) {
			blog(LOG_WARNING, "[pango] Failed to read from end of file: %s", src->text_file);
			return false;
		}
	} else {
		if (!read_whole_file(&tmp_buf, &size, &encoding_header, src->text_file)) {
			blog(LOG_WARNING, "[pango] Failed to read all of file: %s", src->text_file);
			return false;
		}
	}

	// Check UTF-8
	if (g_utf8_validate(tmp_buf, size, NULL)) { // Very low false positive
		src->text = tmp_buf;
		return true;
	}

	char *encoding = NULL;
	if (encoding_header == 2) {
		encoding = "UTF-16LE";
	} else if (encoding_header == 3) {
		encoding = "UTF-16BE";
	} else if (src->encoding) { // Only use user-defined encoding if we _havnt_ read valid utf-8 or a utf-16 bom.
		encoding = src->encoding;
	} else { // Use glib narrow locale if we have no other guesses, has no invalid encodings i presume.
		g_get_charset(&encoding);
	}

	GError *err = NULL;
	gsize conv_size = 0;
	gchar *conv_str = g_convert (tmp_buf, size, "UTF-8", encoding, NULL, &conv_size, &err);
	bfree(tmp_buf);
	if (conv_str != NULL) {
		src->text = bstrdup_n(conv_str, conv_size);
		g_free(conv_str);
		return true;
	} else {
		if (err)
			blog(LOG_WARNING, "[pango] Failed to convert file from: %s, conversion ended at %lu. Msg: %s", encoding, conv_size, err->message);
		else
			blog(LOG_WARNING, "[pango] Failed to convert file from: %s. Bad g_convert call", encoding);
	}

	return false;
}
