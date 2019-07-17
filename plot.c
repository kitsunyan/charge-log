#include "common.h"

#include <gtk/gtk.h>
#include <math.h>

struct plot_value_t {
	gint time;
	gdouble energy;
	gdouble power;
};

struct plot_data_t {
	guint source_id;
	GtkWidget * energy_area;
	GtkWidget * power_area;
	struct plot_value_t * values;
	gint count;
};

struct color_t {
	gdouble red;
	gdouble green;
	gdouble blue;
	gdouble alpha;
};

static void plot_reload(struct plot_data_t * plot_data) {
	struct {
		gint time;
		gint full;
		gint now;
	} * m = NULL;
	gint count = 0;
	gint capacity = 0;
	FILE * file = fopen(LOG_FILE, "r");
	if (file) {
		gint last_time = -1;
		gint time, full, now;
		while (fscanf(file, "%d %d %d\n", &time, &full, &now) == 3) {
			if (last_time < 0 || last_time + 10 < time) {
				if (capacity == 0) {
					capacity = 2;
					m = malloc(capacity * sizeof(m[0]));
					if (!m) {
						break;
					}
				} else if (count == capacity) {
					gpointer last_m = m;
					capacity *= 2;
					m = realloc(m, capacity * sizeof(m[0]));
					if (!m) {
						free(last_m);
						break;
					}
				}
				m[count].time = time;
				m[count].full = full;
				m[count].now = now;
				count++;
			}
		}
		fclose(file);
	}
	if (count >= plot_data->count) {
		if (plot_data->count > 0) {
			free(plot_data->values);
			plot_data->count = 0;
		}
		if (count > 0) {
			plot_data->values = malloc(count * sizeof(struct plot_value_t));
			if (plot_data->values) {
				plot_data->count = count;
			}
		}
	} else {
		if (count == 0 && plot_data->count > 0) {
			free(plot_data->values);
		}
		plot_data->count = count;
	}
	if (plot_data->count > 0) {
		gint i;
		gint step = count / 200;
		if (step < 1) {
			step = 1;
		}
		for (i = 0; i < count; i++) {
			gint first, last, td, pd;
			gdouble energy = (gdouble) m[i].now / m[i].full;
			plot_data->values[i].time = m[i].time;
			plot_data->values[i].energy = energy > 1 ? 1 : energy < 0 ? 0 : energy;
			first = i - step;
			last = i + step;
			if (first < 0) {
				first = 0;
			}
			if (last >= count) {
				last = count - 1;
			}
			td = m[last].time - m[first].time;
			pd = m[last].now - m[first].now;
			plot_data->values[i].power = 0.0036 * pd / td;
		}
	}
	free(m);
}

static gint string_array_size(const gchar * const * array) {
	gint i = 0;
	while (array && array[i]) {
		i++;
	}
	return i;
}

#if GTK_CHECK_VERSION(3, 0, 0)
static void get_color(GtkWidget * widget, gboolean fg, struct color_t * color) {
	GtkStyleContext * context = gtk_widget_get_style_context(widget);
	GdkRGBA * rgba;

	gtk_style_context_save(context);
	gtk_style_context_add_class(context, "gtkstyle-fallback");
	if (fg) {
		gtk_style_context_get(context, GTK_STATE_FLAG_NORMAL,
			GTK_STYLE_PROPERTY_COLOR, &rgba, NULL);
	} else {
		gtk_style_context_get(context, GTK_STATE_FLAG_SELECTED,
			GTK_STYLE_PROPERTY_BACKGROUND_COLOR, &rgba, NULL);
	}
	gtk_style_context_restore(context);

	if (rgba) {
		color->red = rgba->red;
		color->green = rgba->green;
		color->blue = rgba->blue;
		color->alpha = rgba->alpha;
		gdk_rgba_free(rgba);
	}
}
#else
static void get_color(GtkWidget * widget, gboolean fg, struct color_t * color) {
	GtkStyle * style = gtk_widget_get_style(widget);
	GdkColor * gcolor = fg ? &style->fg[GTK_STATE_NORMAL] : &style->bg[GTK_STATE_SELECTED];
	color->red = gcolor->red / 65535.;
	color->green = gcolor->green / 65535.;
	color->blue = gcolor->blue / 65535.;
}
#endif

#define PLOT_DRAW_LINE_ARGS struct plot_data_t * plot_data, cairo_t * cr, \
	gint wshift, gint hshift, gint gw, gint gh, \
	gint min_hours, gint max_hours, gint time_correction, gpointer user_data

static void plot_draw_grid(struct plot_data_t * plot_data, cairo_t * cr, PangoLayout * layout,
	struct color_t * fg_color, struct color_t * line_color,
	gint width, gint height, gint padding,
	gint min_hours, gint max_hours, gint time_correction,
	const gchar * const * rows, gint row_width, gint row_height, gint rows_minor,
	const gchar * const * columns, gint column_width, gint column_height, gint columns_minor,
	void (* plot_draw_line)(PLOT_DRAW_LINE_ARGS), gpointer user_data) {
	static const gdouble dashes[] = { 1 };
	gint wshift = padding + row_width + padding;
	gint hshift = padding;
	gint gw = width - wshift - padding - column_width / 2;
	gint gh = height - hshift - padding - row_height - padding;
	gint rows_count = string_array_size(rows);
	gint columns_count = string_array_size(columns);
	gint i, j;

	cairo_set_line_width(cr, 1);
	for (i = 0; i < rows_count; i++) {
		gint jc = i == rows_count - 1 ? 1 : rows_minor;
		for (j = 0; j < jc; j++) {
			gint y = hshift + gh * (i * rows_minor + j)
				/ rows_minor / (rows_count - 1);

			cairo_set_dash(cr, dashes, ARRAY_SIZE(dashes), 0);
			cairo_set_source_rgba(cr, fg_color->red,
				fg_color->green, fg_color->blue, 0.2);
			cairo_move_to(cr, wshift, y + 0.5);
			cairo_rel_line_to(cr, gw, 0);
			cairo_stroke(cr);

			cairo_set_dash(cr, NULL, 0, 0);
			cairo_set_source_rgba(cr, fg_color->red,
				fg_color->green, fg_color->blue, 1);
			cairo_move_to(cr, wshift, y + 0.5);
			cairo_rel_line_to(cr, j == 0 ? 6 : 4, 0);
			cairo_stroke(cr);

			if (j == 0) {
				gint w, h;
				pango_layout_set_text(layout, rows[rows_count - i - 1], -1);
				pango_layout_get_pixel_size(layout, &w, &h);
				cairo_move_to(cr, wshift - padding - w, y - row_height / 2);
				pango_cairo_show_layout(cr, layout);
			}
		}
	}

	cairo_set_line_width(cr, 1);
	for (i = 0; i < columns_count; i++) {
		gint jc = i == columns_count - 1 ? 1 : columns_minor;
		for (j = 0; j < jc; j++) {
			gint x = wshift + gw * (i * columns_minor + j)
				/ columns_minor / (columns_count - 1);

			cairo_set_dash(cr, dashes, ARRAY_SIZE(dashes), 0);
			cairo_set_source_rgba(cr, fg_color->red,
				fg_color->green, fg_color->blue, 0.2);
			cairo_move_to(cr, x - 0.5, hshift);
			cairo_rel_line_to(cr, 0, gh);
			cairo_stroke(cr);

			cairo_set_dash(cr, NULL, 0, 0);
			cairo_set_source_rgba(cr, fg_color->red,
				fg_color->green, fg_color->blue, 1);
			cairo_move_to(cr, x - 0.5, hshift + gh);
			cairo_rel_line_to(cr, 0, j == 0 ? -6 : -4);
			cairo_stroke(cr);

			if (j == 0) {
				gint w, h;
				pango_layout_set_text(layout, columns[i], -1);
				pango_layout_get_pixel_size(layout, &w, &h);
				cairo_move_to(cr, x - column_width / 2, hshift + gh + padding);
				pango_cairo_show_layout(cr, layout);
			}
		}
	}

	cairo_set_line_width(cr, 2);
	cairo_set_source_rgba(cr, line_color->red, line_color->green, line_color->blue, 1);
	plot_draw_line(plot_data, cr, wshift, hshift, gw, gh,
		min_hours, max_hours, time_correction, user_data);

	cairo_set_line_width(cr, 2);
	cairo_set_source_rgba(cr, fg_color->red, fg_color->green, fg_color->blue, 1);
	cairo_move_to(cr, wshift, hshift);
	cairo_rel_line_to(cr, 0, gh);
	cairo_rel_line_to(cr, gw, 0);
	cairo_stroke(cr);
}

#define PLOT_DRAW_NEXT_ARGS struct plot_data_t * plot_data, cairo_t * cr, \
	PangoLayout * layout, struct color_t * fg_color, struct color_t * line_color, \
	gint width, gint height, gint padding, \
	gint row_width, gint row_height, gint columns_minor, \
	const gchar * const * columns, gint column_width, gint column_height, \
	gint min_hours, gint max_hours, gint time_correction

static void plot_draw(struct plot_data_t * plot_data, cairo_t * cr,
	const PangoFontDescription * desc, struct color_t * fg_color, struct color_t * line_color,
	gint width, gint height, void (* plot_draw_next)(PLOT_DRAW_NEXT_ARGS)) {
	PangoLayout * layout = pango_cairo_create_layout(cr);
	gint padding = 8;
	gint row_width, row_height;
	gint columns_count = 0;
	const gchar ** columns;
	gchar ** columns_free;
	gint columns_minor;
	gint column_width, column_height;
	gint min_hours, max_hours;
	gint time_correction;
	struct timespec timespec;
	gint i;

	clock_gettime(CLOCK_REALTIME, &timespec);
	time_correction = timespec.tv_sec - get_time();
	min_hours = plot_data->count > 0
		? plot_data->values[0].time + time_correction
		: timespec.tv_sec;
	min_hours = min_hours / 60 / 60;
	max_hours = plot_data->count > 0
		? plot_data->values[plot_data->count - 1].time + time_correction
		: timespec.tv_sec;
	max_hours = (max_hours + 59 * 60 + 59) / 60 / 60;
	if (max_hours - min_hours < 1) {
		min_hours = max_hours - 1;
	}

	pango_layout_set_font_description(layout, desc);
	pango_layout_set_text(layout, "100", -1);
	pango_layout_get_pixel_size(layout, &row_width, &row_height);
	pango_layout_set_text(layout, "00:00", -1);
	pango_layout_get_pixel_size(layout, &column_width, &column_height);
	if (row_height / 2 > padding) {
		padding = row_height / 2;
	}

	columns_count = (width - 3 * padding - row_width - column_width / 2)
		/ (int) (1.5 * column_width);
	for (columns_minor = 1;; columns_minor *= 2) {
		gint hours_diff = max_hours - min_hours;
		gint full_hours_diff = (hours_diff + columns_minor - 1)
			/ columns_minor * columns_minor;
		gint count = full_hours_diff / columns_minor + 1;
		if (count <= columns_count) {
			gpointer ptr;
			gint add_hours = full_hours_diff - hours_diff;
			gint prepend = add_hours / 2;
			max_hours += prepend;
			min_hours -= add_hours - prepend;
			ptr = malloc((count + 1) * sizeof(gchar *));
			columns_free = ptr;
			columns = ptr;
			for (i = 0; i < count; i++) {
				gint time = (min_hours + i * ((max_hours - min_hours)
					/ (count - 1))) * 60 * 60;
				GDateTime * date_time = g_date_time_new_from_unix_local(time);
				columns[i] = g_date_time_format(date_time, "%H:%M");
				g_date_time_unref(date_time);
			}
			columns[count] = NULL;
			if (columns_minor >= 4) {
				columns_minor = 4;
			}
			break;
		}
	}

	plot_draw_next(plot_data, cr, layout, fg_color, line_color, width, height, padding,
		row_width, row_height, columns_minor, columns, column_width, column_height,
		min_hours, max_hours, time_correction);

	if (columns_free) {
		for (i = 0; columns_free[i]; i++) {
			g_free(columns_free[i]);
		}
		free(columns_free);
	}
	g_object_unref(layout);
}

static gdouble plot_time_x(gint time, gint time_correction, gint min_hours, gint max_hours) {
	return (gdouble) (time + time_correction - min_hours * 60 * 60)
		/ (max_hours - min_hours) / 60 / 60;
}

static void plot_draw_energy_line(PLOT_DRAW_LINE_ARGS) {
	gint i;
	for (i = 0; i < plot_data->count; i++) {
		gdouble energy = plot_data->values[i].energy;
		gdouble xd = plot_time_x(plot_data->values[i].time, time_correction,
			min_hours, max_hours);
		gdouble x = wshift + gw * xd;
		gdouble y = hshift + gh * (1 - energy);
		if (i == 0) {
			cairo_move_to(cr, x, y);
		} else {
			cairo_line_to(cr, x, y);
		}
		if (i == plot_data->count - 1) {
			cairo_stroke(cr);
		}
	}
}

static void plot_draw_power_line(PLOT_DRAW_LINE_ARGS) {
	gint * max_power = user_data;
	gboolean in_line = FALSE;
	gint i;
	for (i = 0; i < plot_data->count; i++) {
		gdouble power = plot_data->values[i].power;
		if (power >= 0) {
			if (in_line) {
				cairo_stroke(cr);
				in_line = FALSE;
			}
		} else {
			gdouble xd = plot_time_x(plot_data->values[i].time, time_correction,
				min_hours, max_hours);
			gdouble x = wshift + gw * xd;
			gdouble y = hshift + gh * (*max_power + power) / *max_power;
			if (in_line) {
				cairo_line_to(cr, x, y);
			} else {
				in_line = TRUE;
				cairo_move_to(cr, x, y);
			}
		}
	}
	if (in_line) {
		cairo_stroke(cr);
	}
}

static void plot_draw_energy(PLOT_DRAW_NEXT_ARGS) {
	static const gchar * rows[] = { "0", "20", "40", "60", "80", "100", NULL };
	gint rows_minor;

	rows_minor = (height - 3 * padding - column_height) >= 1.5 *
		pow(string_array_size(rows), 2) ? 2 : 1;

	plot_draw_grid(plot_data, cr, layout,
		fg_color, line_color, width, height, padding,
		min_hours, max_hours, time_correction,
		rows, row_width, row_height, rows_minor,
		columns, column_width, column_height, columns_minor,
		plot_draw_energy_line, NULL);
}

static void plot_draw_power(PLOT_DRAW_NEXT_ARGS) {
	gpointer ptr;
	const gchar ** rows;
	gchar ** rows_free;
	gint rows_count, max_rows_count;
	gint rows_step, rows_minor;
	gint max_power = 1;
	gint i;

	for (i = 0; i < plot_data->count; i++) {
		gdouble dpower = plot_data->values[i].power;
		if (dpower < 0) {
			gint power = abs(floor(dpower));
			if (power > max_power) {
				max_power = power;
			}
		}
	}

	max_rows_count = (height - 3 * padding - column_height) / (int) (1.5 * row_height);
	rows_step = ((max_power + max_rows_count - 2) / (max_rows_count - 1) + 2) / 3 * 3;
	max_power = (max_power + rows_step - 1) / rows_step * rows_step;
	rows_count = max_power / rows_step + 1;
	rows_minor = 3;

	ptr = malloc((rows_count + 1) * sizeof(gchar *));
	rows_free = ptr;
	rows = ptr;
	for (i = 0; i < rows_count; i++) {
		rows[i] = g_strdup_printf("%d", i * rows_step);
	}
	rows[rows_count] = NULL;

	plot_draw_grid(plot_data, cr, layout,
		fg_color, line_color, width, height, padding,
		min_hours, max_hours, time_correction,
		rows, row_width, row_height, rows_minor,
		columns, column_width, column_height, columns_minor,
		plot_draw_power_line, &max_power);

	for (i = 0; rows_free[i]; i++) {
		g_free(rows_free[i]);
	}
	free(rows_free);
}

#if GTK_CHECK_VERSION(3, 0, 0)
static gboolean energy_draw(GtkWidget * widget, cairo_t * cr, gpointer user_data) {
#else
static gboolean energy_expose_event(GtkWidget * widget, GdkEvent * event, gpointer user_data) {
	cairo_t * cr = gdk_cairo_create(widget->window);
#endif
	const PangoFontDescription * desc;
	GtkAllocation allocation;
	struct color_t fg_color = { 0, 0, 0, 1 };
	struct color_t line_color = { 0, 0, 0, 1 };

	desc = pango_context_get_font_description(gtk_widget_get_pango_context(widget));
	gtk_widget_get_allocation(widget, &allocation);
	get_color(widget, TRUE, &fg_color);
	get_color(widget, FALSE, &line_color);

	plot_draw(user_data, cr, desc, &fg_color, &line_color,
		allocation.width, allocation.height, plot_draw_energy);

#if !GTK_CHECK_VERSION(3, 0, 0)
	cairo_destroy(cr);
#endif
	return FALSE;
}

#if GTK_CHECK_VERSION(3, 0, 0)
static gboolean power_draw(GtkWidget * widget, cairo_t * cr, gpointer user_data) {
#else
static gboolean power_expose_event(GtkWidget * widget, GdkEvent * event, gpointer user_data) {
	cairo_t * cr = gdk_cairo_create(widget->window);
#endif
	const PangoFontDescription * desc;
	GtkAllocation allocation;
	struct color_t fg_color = { 0, 0, 0, 1 };
	struct color_t line_color = { 0, 0, 0, 1 };

	desc = pango_context_get_font_description(gtk_widget_get_pango_context(widget));
	gtk_widget_get_allocation(widget, &allocation);
	get_color(widget, TRUE, &fg_color);
	get_color(widget, FALSE, &line_color);

	plot_draw(user_data, cr, desc, &fg_color, &line_color,
		allocation.width, allocation.height, plot_draw_power);

#if !GTK_CHECK_VERSION(3, 0, 0)
	cairo_destroy(cr);
#endif
	return FALSE;
}

static gboolean window_key_press_event(GtkWidget * widget, GdkEvent * event) {
	if (event->key.state & GDK_CONTROL_MASK) {
		if (event->key.keyval == 113 || event->key.keyval == 119) {
			gtk_widget_destroy(widget);
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean plot_reload_timer(gpointer user_data) {
	struct plot_data_t * plot_data = user_data;

	plot_reload(plot_data);
	gtk_widget_queue_draw(plot_data->energy_area);
	gtk_widget_queue_draw(plot_data->power_area);

	if (plot_data->count > 0) {
		gint last = plot_data->values[plot_data->count - 1].time;
		plot_data->source_id = g_timeout_add((60 + 1 + last - get_time()) * 1000,
			plot_reload_timer, user_data);
	} else {
		plot_data->source_id = g_timeout_add(UPDATE_INTERVAL * 1000,
			plot_reload_timer, user_data);
	}

	return G_SOURCE_REMOVE;
}

gint main(gint argc, gchar ** argv) {
	struct plot_data_t plot_data = {
		.source_id = 0,
		.values = NULL,
		.count = 0
	};
	GdkPixbuf * icon;
	GtkWidget * window, * box;
#if !GTK_CHECK_VERSION(3, 14, 0)
	GtkWidget * energy_alignment, * power_alignment;
#endif
	GtkWidget * energy_label, * power_label;
	static const gint margin = 12;

	gtk_init(&argc, &argv);

	icon = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "battery",
		16, 0, NULL);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "Battery Charge History");
	if (icon) {
		gtk_window_set_icon(GTK_WINDOW(window), icon);
		g_object_unref(icon);
	}
	gtk_widget_set_size_request(window, 600, 400);
	gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_widget_set_events(window, gtk_widget_get_events(window) |
		GDK_KEY_PRESS_MASK);
	gtk_widget_show(window);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(window, "key-press-event",
		G_CALLBACK(window_key_press_event), NULL);

#if GTK_CHECK_VERSION(3, 2, 0)
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
	box = gtk_vbox_new(FALSE, 0);
#endif
	gtk_container_add(GTK_CONTAINER(window), box);
	gtk_widget_show(box);

	energy_label = gtk_label_new("Energy");
#if GTK_CHECK_VERSION(3, 14, 0)
	gtk_widget_set_margin_top(energy_label, margin);
	gtk_box_pack_start(GTK_BOX(box), energy_label, FALSE, FALSE, 0);
#else
	energy_alignment = gtk_alignment_new(0.5, 0.5, 1, 1);
	gtk_alignment_set_padding(GTK_ALIGNMENT(energy_alignment), margin, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(box), energy_alignment, FALSE, FALSE, 0);
	gtk_widget_show(energy_alignment);
	gtk_container_add(GTK_CONTAINER(energy_alignment), energy_label);
#endif
	gtk_widget_show(energy_label);

	plot_data.energy_area = gtk_drawing_area_new();
#if GTK_CHECK_VERSION(3, 0, 0)
	g_signal_connect(plot_data.energy_area, "draw", G_CALLBACK(energy_draw), &plot_data);
#else
	g_signal_connect(plot_data.energy_area, "expose-event",
		G_CALLBACK(energy_expose_event), &plot_data);
#endif
	gtk_box_pack_start(GTK_BOX(box), plot_data.energy_area, TRUE, TRUE, 0);
	gtk_widget_show(plot_data.energy_area);

	power_label = gtk_label_new("Power");
#if GTK_CHECK_VERSION(3, 14, 0)
	gtk_widget_set_margin_top(power_label, margin);
	gtk_box_pack_start(GTK_BOX(box), power_label, FALSE, FALSE, 0);
#else
	power_alignment = gtk_alignment_new(0.5, 0.5, 1, 1);
	gtk_alignment_set_padding(GTK_ALIGNMENT(power_alignment), margin, 0, 0, 0);
	gtk_box_pack_start(GTK_BOX(box), power_alignment, FALSE, FALSE, 0);
	gtk_widget_show(power_alignment);
	gtk_container_add(GTK_CONTAINER(power_alignment), power_label);
#endif
	gtk_widget_show(power_label);

	plot_data.power_area = gtk_drawing_area_new();
#if GTK_CHECK_VERSION(3, 0, 0)
	g_signal_connect(plot_data.power_area, "draw", G_CALLBACK(power_draw), &plot_data);
#else
	g_signal_connect(plot_data.power_area, "expose-event",
		G_CALLBACK(power_expose_event), &plot_data);
#endif
	gtk_box_pack_start(GTK_BOX(box), plot_data.power_area, TRUE, TRUE, 0);
	gtk_widget_show(plot_data.power_area);

	plot_reload_timer(&plot_data);
	gtk_main();
	g_source_remove(plot_data.source_id);
	if (plot_data.count > 0) {
		free(plot_data.values);
	}
	return 0;
}
