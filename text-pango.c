#include <obs-module.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <sys/stat.h>
#include <math.h>

#include <glib.h>
// Let us choose the backends even though API compatibilty is not guarenteed
#define PANGO_ENABLE_BACKEND
#include <pango/pangocairo.h>
#include <fontconfig/fontconfig.h>


#include "text-pango.h"
#include "text-utilities.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("text-pango", "en-US")

#ifdef _WIN32
#define DEFAULT_FACE "Arial"
#elif __APPLE__
#define DEFAULT_FACE "Helvetica"
#elif __linux__
#define DEFAULT_FACE "DejaVu Sans"
#else
#define DEFAULT_FACE "Sans Serif"
#endif


#define FILE_CHECK_TIMEOUT_SEC 1.0
#define MAX_TEXTURE_SIZE 16384
#ifndef max
#define max(a,b) (a > b ? a : b)
#endif

void render_text(struct pango_source *src)
{
	cairo_t *layout_context;
	cairo_t *render_context;
	cairo_surface_t *surface;
	uint8_t *surface_data = NULL;
	PangoLayout *layout;

	//Clear any old textures
	if (src->tex) {
		obs_enter_graphics();
		gs_texture_destroy(src->tex);
		obs_leave_graphics();
		src->tex = NULL;
	}

	if (!src->text)
		return;
	if (!src->font_name)
		return;

	int outline_width = src->outline ? src->outline_width : 0;
	int drop_shadow_offset = src->drop_shadow ? src->drop_shadow_offset : 0;

	/* Set fontconfig backend to fontconfig on all platforms */
	PangoCairoFontMap *map = PANGO_CAIRO_FONT_MAP(pango_cairo_font_map_get_default()); // transfer none, no need to cleanup.
	if (pango_cairo_font_map_get_font_type(map) != CAIRO_FONT_TYPE_FT ) {
		PangoCairoFontMap *fc_fontmap = PANGO_CAIRO_FONT_MAP(pango_cairo_font_map_new_for_font_type(CAIRO_FONT_TYPE_FT));
		pango_cairo_font_map_set_default(fc_fontmap);
		g_object_unref(fc_fontmap);
	}
	/* Create a PangoLayout without manual context */
	layout_context = create_layout_context();
	layout = pango_cairo_create_layout(layout_context);
	if (src->vertical) {
		pango_context_set_base_gravity(pango_layout_get_context(layout), PANGO_GRAVITY_EAST);
	}

	set_font(src, layout);
	set_halignment(src, layout);
	set_lang(src, layout);

	pango_layout_set_text(layout, src->text, -1);

	/* Get text dimensions and create a context to render to */
	int text_height = 0; int text_width = 0;
	int line_num = 1;
	PangoRectangle log_rect; PangoRectangle ink_rect;
	PangoLayoutIter *sizing_iter = pango_layout_get_iter(layout);
	do {
		pango_layout_iter_get_line_extents(sizing_iter, &ink_rect, &log_rect);
		int new_h = text_height+PANGO_PIXELS_FLOOR(log_rect.height); // May be too conservative, but looks good in arial
		int new_w = max(text_width, PANGO_PIXELS_FLOOR(max(0, ink_rect.x) + ink_rect.width));
		if(new_h + outline_width + max(outline_width, drop_shadow_offset) > MAX_TEXTURE_SIZE
		|| new_w + outline_width + max(outline_width, drop_shadow_offset) > MAX_TEXTURE_SIZE)  {
			blog(LOG_WARNING, "[pango]: Text truncated at line: %i", line_num);
			break; // If this line would put us over max texture.
		}
		text_height = new_h;
		text_width = new_w;
		line_num += 1;
	} while (pango_layout_iter_next_line(sizing_iter));
	pango_layout_iter_free(sizing_iter);
	text_height += outline_width + max(outline_width, drop_shadow_offset);
	text_width += outline_width + max(outline_width, drop_shadow_offset);

	/* Set source dimensions */
	if (src->vertical) {
		src->height = text_width;
		src->width = text_height;
	} else {
		src->height = text_height;
		src->width = text_width;
	}

	/* Allocate and initialize Cairo surface */
	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, src->width);
	surface_data = bzalloc(stride * src->height);
	surface = cairo_image_surface_create_for_data(surface_data,
			CAIRO_FORMAT_ARGB32,
			src->width, src->height, stride);

	/* Change to render context */
	render_context = cairo_create(surface);
	pango_cairo_update_layout(render_context, layout);

	/* Transform coordinates and move origin for vertical text */
	if (src->vertical) {
		cairo_translate(render_context, src->width, 0);
		cairo_rotate(render_context, (M_PI*0.5));
	}

	int xoffset = outline_width;
	int yoffset = outline_width;
	PangoLayoutIter *iter = pango_layout_get_iter(layout);
	do {
		PangoLayoutLine *line;
		PangoRectangle rect;
		PangoRectangle paint_rect;
		int y1, y2;
		cairo_pattern_t *pattern;

		line = pango_layout_iter_get_line_readonly(iter);

		pango_layout_iter_get_line_extents(iter, &paint_rect, &rect);
		int baseline = pango_layout_iter_get_baseline(iter);
		int xpos = xoffset + PANGO_PIXELS_FLOOR(rect.x);
		int ypos = yoffset + PANGO_PIXELS_FLOOR(baseline);

		if(ypos > MAX_TEXTURE_SIZE || PANGO_PIXELS_FLOOR(rect.width) > MAX_TEXTURE_SIZE)
			break; // If this line would put us over max texture.

		cairo_push_group(render_context); // Render lines independently via groups.
		/* Draw the drop shadow */
		if (drop_shadow_offset > 0) {
			cairo_move_to(render_context,
					xpos + drop_shadow_offset,
					ypos + drop_shadow_offset);
			cairo_set_source_rgba(render_context,
					RGBA_CAIRO(src->drop_shadow_color));
			pango_cairo_show_layout_line(render_context, line);
			cairo_fill(render_context);
			cairo_set_operator(render_context, CAIRO_OPERATOR_IN);
			cairo_set_source_rgba(render_context,
					RGBA_CAIRO(src->drop_shadow_color));
			cairo_paint(render_context);
		}

		/* Draw text with outline */
		if (outline_width > 0) {
			cairo_set_operator(render_context, CAIRO_OPERATOR_SOURCE);
			cairo_move_to(render_context, xpos, ypos);
			pango_cairo_layout_line_path(render_context, line);
			cairo_set_line_join(render_context, CAIRO_LINE_JOIN_ROUND);
			cairo_set_line_width(render_context, outline_width * 2);
			cairo_set_source_rgba(render_context,
					RGBA_CAIRO(src->outline_color));
			cairo_stroke(render_context);
		}

		/* Handle Gradienting source by line */
		cairo_set_operator(render_context, CAIRO_OPERATOR_SOURCE);
		pango_layout_iter_get_line_yrange(iter, &y1, &y2);
		pattern = cairo_pattern_create_linear(
				0, PANGO_PIXELS_FLOOR(paint_rect.y) + yoffset,
				0, PANGO_PIXELS_FLOOR(paint_rect.y+paint_rect.height) + yoffset);
		cairo_pattern_set_extend(pattern, CAIRO_EXTEND_PAD);
		cairo_pattern_add_color_stop_rgba(pattern, 0.0,
				RGBA_CAIRO(src->color[0]));
		cairo_pattern_add_color_stop_rgba(pattern, 1.0,
				RGBA_CAIRO(src->color[1]));

		cairo_set_source(render_context, pattern);
		cairo_fill(render_context);
		cairo_pattern_destroy(pattern);

		cairo_move_to(render_context, xpos, ypos);
		pango_cairo_show_layout_line(render_context, line);

		cairo_pop_group_to_source(render_context); // Use isolated line as source for final texture
		cairo_paint(render_context);
	} while (pango_layout_iter_next_line(iter));
	pango_layout_iter_free(iter);

	obs_enter_graphics();
	src->tex = gs_texture_create(src->width, src->height, GS_BGRA, 1,
			(const uint8_t **) &surface_data, 0);
	obs_leave_graphics();
	if(!src->tex) {
		blog(LOG_ERROR,"[pango]: Failed creating texture of dim (%i,%i)", src->width, src->height);
	}

	/* Clean up */
	bfree(surface_data);
	g_object_unref(layout);
	cairo_destroy(layout_context);
	cairo_destroy(render_context);
	cairo_surface_destroy(surface);
}

static const char *pango_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);

	return obs_module_text("TextPango");
}

static uint32_t pango_source_get_width(void *data)
{
	struct pango_source *src = data;

	return src->width;
}

static uint32_t pango_source_get_height(void *data)
{
	struct pango_source *src = data;

	return src->height;
}

static void pango_source_get_defaults(obs_data_t *settings)
{
	obs_data_t *font;

	obs_data_set_default_bool(settings, "font_from_file", false);

	font = obs_data_create();
	obs_data_set_default_string(font, "face", DEFAULT_FACE);
	obs_data_set_default_int(font, "size", 256);
	obs_data_set_default_obj(settings, "font", font);
	obs_data_release(font);


	obs_data_set_default_bool(settings, "gradient", false);
	obs_data_set_default_int(settings, "color1", 0xFFFFFFFF);
	obs_data_set_default_int(settings, "color2", 0xFFFFFFFF);

	obs_data_set_default_int(settings, "outline_width", 2);
	obs_data_set_default_int(settings, "outline_color", 0xFF000000);

	obs_data_set_default_int(settings, "drop_shadow_offset", 4);
	obs_data_set_default_int(settings, "drop_shadow_color", 0xFF000000);

	obs_data_set_default_bool(settings, "log_mode", false);
	obs_data_set_default_int(settings, "log_lines", 6);

	obs_data_set_default_string(settings, "encoding.name", "UTF-8");

}

static obs_properties_t *pango_source_get_properties(void *unused)
{
	UNUSED_PARAMETER(unused);
	obs_properties_t *props;
	obs_property_t *prop;

	props = obs_properties_create();

	prop = obs_properties_add_bool(props, "font_from_file",
		obs_module_text("Font.FromFile"));
#if FC_VERSION < 21291
	// Disable this if FC is too old.
	obs_property_set_visible(prop, false);
#endif
	obs_property_set_modified_callback(prop,
		pango_source_properties_font_from_file_changed);
	obs_properties_add_path(props, "font_file",
		obs_module_text("Font.File"), OBS_PATH_FILE,
		NULL, NULL);
	obs_properties_add_int(props, "font_file_size",
		obs_module_text("Font.Size"), 8, 512, 1);

	obs_properties_add_font(props, "font",
		obs_module_text("Font"));
	prop = obs_properties_add_bool(props, "from_file",
		obs_module_text("ReadFromFile"));
	obs_property_set_modified_callback(prop,
		pango_source_properties_from_file_changed);

	obs_properties_add_text(props, "text",
		obs_module_text("Text"), OBS_TEXT_MULTILINE);
	obs_properties_add_path(props, "text_file",
		obs_module_text("TextFile"), OBS_PATH_FILE,
		NULL, NULL);

	obs_properties_add_bool(props, "vertical",
		obs_module_text("Vertical"));

	prop = obs_properties_add_bool(props, "gradient",
		obs_module_text("Gradient"));
	obs_property_set_modified_callback(prop,
		pango_source_properties_gradient_changed);
	obs_properties_add_color_alpha(props, "color1",
		obs_module_text("Gradient.Color"));
	obs_properties_add_color_alpha(props, "color2",
		obs_module_text("Gradient.Color2"));

	prop = obs_properties_add_list(props, "align",
		obs_module_text("Alignment"), OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(prop,
		obs_module_text("Alignment.Left"), ALIGN_LEFT);
	obs_property_list_add_int(prop,
		obs_module_text("Alignment.Right"), ALIGN_RIGHT);
	obs_property_list_add_int(prop,
		obs_module_text("Alignment.Center"), ALIGN_CENTER);

	prop = obs_properties_add_bool(props, "outline",
		obs_module_text("Outline"));
	obs_property_set_modified_callback(prop,
		pango_source_properties_outline_changed);
	prop = obs_properties_add_int(props, "outline_width",
		obs_module_text("Outline.Size"), 1, 256, 1);
	obs_property_set_visible(prop, false);
	prop = obs_properties_add_color(props, "outline_color",
		obs_module_text("Outline.Color"));
	obs_property_set_visible(prop, false);

	prop = obs_properties_add_bool(props, "drop_shadow",
		obs_module_text("DropShadow"));
	obs_property_set_modified_callback(prop,
		pango_source_properties_drop_shadow_changed);
	prop = obs_properties_add_int(props, "drop_shadow_offset",
		obs_module_text("DropShadow.Offset"), 1, 256, 1);
	obs_property_set_visible(prop, false);
	prop = obs_properties_add_color(props, "drop_shadow_color",
		obs_module_text("DropShadow.Color"));
	obs_property_set_visible(prop, false);

	prop = obs_properties_add_bool(props, "log_mode",
		obs_module_text("ChatlogMode"));
	obs_property_set_modified_callback(prop,
		pango_source_properties_log_mode_changed);
	prop = obs_properties_add_int(props, "log_lines",
		obs_module_text("ChatlogMode.Lines"), 1, 1000, 1);
	obs_property_set_visible(prop, false);
	// obs_properties_add_int(props, "custom_width",
	// 	obs_module_text("CustomWidth"), 0, 4096, 1);
	// obs_properties_add_bool(props, "word_wrap",
	// 	obs_module_text("WordWrap"));

	prop = obs_properties_add_bool(props, "encoding.enable",
		obs_module_text("Encoding.Enable"));
	obs_property_set_modified_callback(prop,
		pango_source_properties_encoding_changed);
	prop = obs_properties_add_text(props, "encoding.name",
		obs_module_text("Encoding.Name"), OBS_TEXT_DEFAULT);
	obs_property_set_visible(prop, false);

	prop = obs_properties_add_bool(props, "lang.enable",
		obs_module_text("Lang.Enable"));
	obs_property_set_modified_callback(prop,
		pango_source_properties_lang_changed);
	prop = obs_properties_add_text(props, "lang.code",
		obs_module_text("Lang.Code"), OBS_TEXT_DEFAULT);
	obs_property_set_visible(prop, false);

	return props;
}

static void pango_source_destroy(void *data)
{
	struct pango_source *src = data;

	if (src->text != NULL)
		bfree(src->text);

	if (src->font_name != NULL)
		bfree(src->font_name);

	if (src->text_file != NULL)
		bfree(src->text_file);

	obs_enter_graphics();

	if (src->tex != NULL) {
		gs_texture_destroy(src->tex);
		src->tex = NULL;
	}

	obs_leave_graphics();

	bfree(src);
}

static void pango_source_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct pango_source *src = data;

	if (src->tex == NULL)
		return;

	const bool prev = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"),
			src->tex);
	gs_draw_sprite(src->tex, 0,
			src->width, src->height);

	gs_blend_state_pop();
	gs_enable_framebuffer_srgb(prev);
}

static void pango_video_tick(void *data, float seconds)
{
	struct pango_source *src = data;

	if (src->from_file) { // Questionable check
		src->file_last_checked += seconds;
		if (src->file_last_checked > FILE_CHECK_TIMEOUT_SEC) {
			src->file_last_checked = 0.0;
			struct stat stat_s = {0};
			os_stat(src->text_file, &stat_s);
			if (src->file_timestamp != stat_s.st_mtime) {
				if (read_textfile(src)) {
					src->file_timestamp = stat_s.st_mtime;
					render_text(src);
				}
			}
		}
	}
}

static void pango_source_update(void *data, obs_data_t *settings)
{
	struct pango_source *src = data;
	obs_data_t *font;
	char *font_file =  NULL;
	
	if (src->text) {
		bfree(src->text);
		src->text = NULL;
	}

	src->font_from_file = obs_data_get_bool(settings, "font_from_file");
	src->font_file = obs_data_get_string(settings, "font_file"); // Copy not needed, do not free.

	font = obs_data_get_obj(settings, "font");
	if (src->font_name)
		bfree(src->font_name);
	src->font_name  = bstrdup(obs_data_get_string(font, "face"));
	if(src->font_from_file) { // Keep sizes synced.
		src->font_size = (uint16_t)obs_data_get_int(settings, "font_file_size");
		obs_data_set_int(font, "size", src->font_size);
	} else {
		src->font_size   = (uint16_t)obs_data_get_int(font, "size");
		obs_data_set_int(settings, "font_file_size", src->font_size);
	}
	src->font_flags  = (uint32_t)obs_data_get_int(font, "flags");
	obs_data_release(font);


	src->vertical = obs_data_get_bool(settings, "vertical");
	src->align = (int)obs_data_get_int(settings, "align");

	if(src->encoding) {
		bfree(src->encoding);
		src->encoding = NULL;
	}
	if(obs_data_get_bool(settings, "encoding.enable"))
		src->encoding = bstrdup(obs_data_get_string(settings, "encoding.name"));

	if(src->lang) {
		bfree(src->lang);
		src->lang = NULL;
	}
	if(obs_data_get_bool(settings, "lang.enable"))
		src->lang = bstrdup(obs_data_get_string(settings, "lang.code"));

	src->gradient = obs_data_get_bool(settings, "gradient");
	src->color[0] = (uint32_t)obs_data_get_int(settings, "color1");
	if (src->gradient) {
		src->color[1] = (uint32_t)obs_data_get_int(settings, "color2");
	} else {
		src->color[1] = src->color[0];
	}

	src->outline = obs_data_get_bool(settings, "outline");
	src->outline_width = (uint32_t)obs_data_get_int(settings, "outline_width");
	src->outline_color = (uint32_t)obs_data_get_int(settings, "outline_color");

	src->drop_shadow = obs_data_get_bool(settings, "drop_shadow");
	src->drop_shadow_offset = (uint32_t)obs_data_get_int(settings, "drop_shadow_offset");
	src->drop_shadow_color = (uint32_t)obs_data_get_int(settings, "drop_shadow_color");

	src->log_mode = obs_data_get_bool(settings, "log_mode");
	src->log_lines = (uint32_t)obs_data_get_int(settings, "log_lines");

	src->file_timestamp = 0;
	src->file_last_checked = 0.0;

	src->from_file = obs_data_get_bool(settings, "from_file");
	if (src->from_file) { // Questionable check
		if (src->text_file) {
			bfree(src->text_file);
			src->text_file = NULL;
		}

		src->text_file = bstrdup(obs_data_get_string(settings, "text_file"));
		if (!read_textfile(src)) {
			src->text = bstrdup(obs_data_get_string(settings, "text"));
		} else {
			struct stat stat_s = {0};
			os_stat(src->text_file, &stat_s);
			src->file_timestamp = stat_s.st_mtime;
		}
	} else {
		src->text = bstrdup(obs_data_get_string(settings, "text"));
	}

	//todo: Add a single queued "latest" change to catch when many fast changes are made to slow rendering text
	render_text(src);
}

static void *pango_source_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(source);
	struct pango_source *src = bzalloc(sizeof(struct pango_source));

	pango_source_update(src, settings);

	return src;
}

static struct obs_source_info pango_source_info = {
	.id = "text_pango_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = pango_source_get_name,
	.create = pango_source_create,
	.destroy = pango_source_destroy,
	.update = pango_source_update,
	.get_width = pango_source_get_width,
	.get_height = pango_source_get_height,
	.video_render = pango_source_render,
	.video_tick = pango_video_tick,
	.get_defaults = pango_source_get_defaults,
	.get_properties = pango_source_get_properties,
};

bool obs_module_load()
{
	obs_register_source(&pango_source_info);

	FcConfig *config = FcConfigCreate();
	FcBool complain = true;
	if (FcConfigParseAndLoad(config, NULL, complain) == FcTrue) {
		blog(LOG_INFO, "[pango]: Loaded system fontconfig");
	} else {
		const char *path = obs_get_module_data_path(obs_current_module());
		char *abs_path = os_get_abs_path_ptr(path);
		char *tmplt_config_path = obs_module_file("fonts.conf");
		char *tmplt_config = os_quick_read_utf8_file(tmplt_config_path);
		struct dstr config_buf = {0};
		dstr_copy(&config_buf, tmplt_config);
		dstr_replace(&config_buf, "${plugin_path}", abs_path);

		bfree(tmplt_config);
		bfree(tmplt_config_path);
		bfree(abs_path);

		if (FcConfigParseAndLoadFromMemory(config, config_buf.array, complain) != FcTrue) {
			FcConfigDestroy(config);
			blog(LOG_ERROR, "[pango] Failed to load fontconfig");
			dstr_free(&config_buf);
			return false;
		}
		blog(LOG_INFO, "[pango] Loaded plugin fontconfig");
		dstr_free(&config_buf);
	}
	FcConfigSetCurrent(config);
	FcConfigBuildFonts(config);
	return true;
}

void obs_module_unload(void)
{
	FcConfigAppFontClear(NULL);
}
