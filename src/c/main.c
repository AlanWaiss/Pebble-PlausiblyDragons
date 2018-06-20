#include <pebble.h>
#define BATTERY_HEIGHT 3

// keys for AppMessage
#define KEY_MIN 0
#define KEY_MAX 1
#define KEY_CON 2
#define KEY_PREC 3
#define KEY_TOM 4
#define KEY_TOM_MIN 5
#define KEY_TOM_MAX 6
#define KEY_TOM_CON 7
#define KEY_TOM_PREC 8
#define KEY_MESSAGE 9

#define KEY_COLOR_BG 10
#define KEY_COLOR_TIME 11
#define KEY_COLOR_TEMP 12
#define KEY_COLOR_DAY 13
#define KEY_COLOR_DATE 14
#define KEY_COLOR_AMPM 15
#define KEY_COLOR_WEATHER 16
#define KEY_COLOR_BATTERY 17

#define KEY_GET_LOCATION 50

#define COL_W 30
#define COL_H 16
#define COL_1 6
#define COL_2 53
#define COL_3 100

#define VALUE_OFFSET 9

#define ROW_1 -5
#define ROW_2 10
#define ROW_3 25
#define ROW_4 40

#define NUM_TOP 62
#define NUM_W 30
#define NUM_H 40

struct party_member {
	char name[5];
	char labels[3][3];	// 3 arrays, 3 chars (2 + null) each
	int values[3];
	char texts[3][4];	// 3 arrays, 4 chars (3 + null) each
	TextLayer *layers[7];
};

static struct party_member members[3];
static struct party_member *s_col1 = &(members[0]);
static struct party_member *s_col2 = &(members[1]);
static struct party_member *s_col3 = &(members[2]);
	
static GBitmap *s_images[10];

static BitmapLayer *s_numbers[4];

static int s_top = 34;

static Window *s_main_window;

static Layer *s_border_layer;

static TextLayer *s_time_layer;

static TextLayer *s_ampm_layer;

static TextLayer *s_date_layer;

static TextLayer *s_temp_layer;

static TextLayer *s_weather_layer;

//static Layer *s_battery_layer;

static GFont s_font_small;

static GFont s_font_small;

static GFont s_font_tiny;

static int s_battery_level;

static GColor8 s_battery_color;

// Copies the first N characters from src to target
static void substr(char target[], char src[], int len) {
	int srcLen = strlen(src);
	if(srcLen < len)
		len = srcLen;
	for(int i = 0; i < len; i++)
		target[i] = src[i];
	target[len] = '\0';
}

static void layer_set_buffer_text(TextLayer *layer, char buffer[], char text[]) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "buffer_text %s %s", buffer, text);
	if(strcmp(buffer, text) != 0) {
		strcpy(buffer, text);
		APP_LOG(APP_LOG_LEVEL_DEBUG, "copied %s", buffer);
		text_layer_set_text(layer, buffer);
	}
}

static void party_member_set_name(struct party_member *member, char name[]) {
	layer_set_buffer_text(member->layers[0], member->name, name);
}

static void party_member_set_label(struct party_member *member, int labelIndex, char label[]) {
	if(strcmp(member->labels[labelIndex], label) != 0) {
		substr(member->labels[labelIndex], label, 2);
		text_layer_set_text(member->layers[labelIndex + 1], member->labels[labelIndex]);
	}
}

static void party_member_set_value(struct party_member *member, int valueIndex, int value) {
	if(member->values[valueIndex] != value) {
		member->values[valueIndex] = value;
		snprintf(member->texts[valueIndex], sizeof(member->texts[valueIndex]), "%d", value);
		text_layer_set_text(member->layers[valueIndex + 4], member->texts[valueIndex]);
	}
}

static void init_member(Layer *window_layer, struct party_member *member, int left, char label1[2], char label2[2], char label3[3]) {
	member->layers[0] = text_layer_create(GRect(left, ROW_1, COL_W, COL_H));
	member->layers[1] = text_layer_create(GRect(left, ROW_2, COL_W, COL_H));
	member->layers[2] = text_layer_create(GRect(left, ROW_3, COL_W, COL_H));
	member->layers[3] = text_layer_create(GRect(left, ROW_4, COL_W, COL_H));
	member->layers[4] = text_layer_create(GRect(left + VALUE_OFFSET, ROW_2, COL_W, COL_H));
	member->layers[5] = text_layer_create(GRect(left + VALUE_OFFSET, ROW_3, COL_W, COL_H));
	member->layers[6] = text_layer_create(GRect(left + VALUE_OFFSET, ROW_4, COL_W, COL_H));
	member->values[0] = member->values[1] = member->values[2] = -9999;
	for(int i = 0; i < 7; i++) {
		text_layer_set_background_color(member->layers[i], GColorClear);
		text_layer_set_text_color(member->layers[i], GColorWhite);
		text_layer_set_font(member->layers[i], s_font_small);
		text_layer_set_text_alignment(member->layers[i], i < 4 ? GTextAlignmentLeft : GTextAlignmentRight);
		layer_add_child(window_layer, text_layer_get_layer(member->layers[i]));
	}
	text_layer_set_background_color(member->layers[0], GColorBlack);
	party_member_set_label(member, 0, label1);
	party_member_set_label(member, 1, label2);
	party_member_set_label(member, 2, label3);
}

static void deinit_member(struct party_member *member) {
	for(int i = 0; i < 7; i++)
		text_layer_destroy(member->layers[i]);
}

// Store incoming information
static char s_temperature_buffer[8] = "";
static char s_weather_layer_buffer[32] = "";

int toupper(int c) {
	return c >= 97 && c <= 122 ? c - 32 : c;
}

void str_upper(char str[]) {
	int len = strlen(str);
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "strlen %d", len);
	for(int i = 0; i < len; i++) {
		if(str[i] == '\0')
			break;
		str[i] = toupper(str[i]);
	}
}

static int s_digits[4] = {0, 0, 0, 0};

static void set_digit(int index, int digit) {
	if(s_digits[index] != digit) {
		s_digits[index] = digit;
		
		bitmap_layer_set_bitmap(s_numbers[index], s_images[digit]);
	}
}

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
	int hour = tick_time->tm_hour;
	if((0 == hour || hour > 12) && !clock_is_24h_style()) {
		if(0 == hour)
			hour = 12;
		else
			hour -= 12;
	}
		
	int digit = hour / 10;
	if(digit != s_digits[0]) {
		s_digits[0] = digit;

		bitmap_layer_set_bitmap(s_numbers[0], digit == 0 ? NULL : s_images[digit]);
	}
	set_digit(1, hour % 10);
		
	set_digit(2, tick_time->tm_min / 10);
	set_digit(3, tick_time->tm_min % 10);
}

static void update_date() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
	
	char day_buffer[4];
	strftime(day_buffer, sizeof(day_buffer), "%a", tick_time);	// Tue
	//str_upper(day_buffer);
		
	APP_LOG(APP_LOG_LEVEL_DEBUG, "day %s", day_buffer);
	party_member_set_name(s_col1, day_buffer);
	
	strftime(day_buffer, sizeof(day_buffer), "%b", tick_time);	// Jan
	party_member_set_name(s_col3, day_buffer);
	party_member_set_value(s_col3, 0, tick_time->tm_mday);
	party_member_set_value(s_col3, 1, tick_time->tm_year % 100);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
	if((units_changed & DAY_UNIT) == DAY_UNIT) {
		update_date();
	}
	if(tick_time->tm_min % 30 == 0) {
		// Begin dictionary
		DictionaryIterator *iter;
		app_message_outbox_begin(&iter);

		// Add a key-value pair
		// Get weather every 30 minutes, update location every 4 hours
		dict_write_uint8(iter, KEY_GET_LOCATION, tick_time->tm_min == 0 && tick_time->tm_hour % 4 == 0 ? 1 : 0);

		// Send the message!
		app_message_outbox_send();
	}
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
	// Read tuples for data
	Tuple *tuple = dict_find(iterator, KEY_MIN);
	if(tuple)
		party_member_set_value(s_col1, 1, tuple->value->int32);
	if((tuple = dict_find(iterator, KEY_MAX)))
		party_member_set_value(s_col1, 0, tuple->value->int32);
	if((tuple = dict_find(iterator, KEY_CON)))
		party_member_set_label(s_col1, 2, tuple->value->cstring);
	if((tuple = dict_find(iterator, KEY_PREC)))
		party_member_set_value(s_col1, 2, tuple->value->int32);
	if((tuple = dict_find(iterator, KEY_TOM)))
		party_member_set_name(s_col2, tuple->value->cstring);
	if((tuple = dict_find(iterator, KEY_TOM_MIN)))
		party_member_set_value(s_col2, 1, tuple->value->int32);
	if((tuple = dict_find(iterator, KEY_TOM_MAX)))
		party_member_set_value(s_col2, 0, tuple->value->int32);
	if((tuple = dict_find(iterator, KEY_TOM_CON)))
		party_member_set_label(s_col2, 2, tuple->value->cstring);
	if((tuple = dict_find(iterator, KEY_TOM_PREC)))
		party_member_set_value(s_col2, 2, tuple->value->int32);
	if((tuple = dict_find(iterator, KEY_MESSAGE))) {
		static char s_message_buffer[300];
		strcpy(s_message_buffer, tuple->value->cstring);
		text_layer_set_text(s_weather_layer, s_message_buffer);
	}
	/*uint8_t buffer[512];
	Tuple *tuple = dict_read_begin_from_buffer(iterator, buffer, dict_size(iterator));
	
	while(tuple) {
		switch(tuple->key) {
			case KEY_MIN:
				party_member_set_value(s_col1, 1, tuple->value->int32);
				break;
			case KEY_MAX:
				party_member_set_value(s_col1, 0, tuple->value->int32);
				break;
			case KEY_CON:
				party_member_set_label(s_col1, 2, tuple->value->cstring);
				break;
			case KEY_PREC:
				party_member_set_value(s_col1, 2, tuple->value->int32);
				break;
		}
	}*/
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
	APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

/*static void battery_update_proc(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	
	// Find the width of the bar
	int width = (int)(float)(((float)s_battery_level / 100.0F) * (float)bounds.size.w);

	// Draw the background
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_rect(ctx, bounds, 0, GCornerNone);

	// Draw the bar
	graphics_context_set_fill_color(ctx, s_battery_level <= 25 ? GColorRed : s_battery_color);
	graphics_fill_rect(ctx, GRect(0, 0, width, BATTERY_HEIGHT), 0, GCornerNone);
}*/

static void battery_callback(BatteryChargeState state) {
	// Get the current state of the battery
	s_battery_level = state.charge_percent;
	// Update meter
	//layer_mark_dirty(s_battery_layer);
	
	party_member_set_value(s_col3, 2, state.charge_percent);
}

static void border_draw(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	
	graphics_context_set_stroke_color(ctx, GColorWhite);
	graphics_context_set_stroke_width(ctx, 1);
	graphics_draw_round_rect(ctx, GRect(2, 3, bounds.size.w - 4, 58), 2);
	graphics_draw_round_rect(ctx, GRect(1, bounds.size.h - 64, bounds.size.w - 2, 64), 2);
}

static void main_window_load(Window *window) {
	// Get information about the Window
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	
	// Create battery meter layer
	s_border_layer = layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
	layer_set_update_proc(s_border_layer, border_draw);
	
	int top = PBL_IF_ROUND_ELSE(s_top - 6, s_top);	//34

	// Create the TextLayer with specific bounds
	s_time_layer = text_layer_create(GRect(0, top + 12, bounds.size.w - 30, 55));
	s_ampm_layer = text_layer_create(GRect(bounds.size.w - 28, top + 41, 28, 24));
	s_date_layer = text_layer_create(GRect(4, top + 120, bounds.size.w - 8, 24));
	s_temp_layer = text_layer_create(GRect(bounds.size.w - 40, top - 2, 40, 30));
	
	s_weather_layer = text_layer_create(
		GRect(4, bounds.size.h - 62, bounds.size.w - 8, 60));
  
	// Create GFont
	s_font_small = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TEXT_15));
	s_font_tiny = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TEXT_12));

	// Improve the layout to be more like a watchface
	text_layer_set_background_color(s_time_layer, GColorClear);
	text_layer_set_text_color(s_time_layer, GColorWhite);
	text_layer_set_font(s_time_layer, s_font_small);
	text_layer_set_text_alignment(s_time_layer, GTextAlignmentRight);

	text_layer_set_background_color(s_ampm_layer, GColorClear);
	text_layer_set_text_color(s_ampm_layer, GColorChromeYellow);
	text_layer_set_font(s_ampm_layer, s_font_small);
	text_layer_set_text_alignment(s_ampm_layer, GTextAlignmentLeft);

	text_layer_set_background_color(s_date_layer, GColorClear);
	text_layer_set_text_color(s_date_layer, GColorWhite);
	text_layer_set_font(s_date_layer, s_font_tiny);
	text_layer_set_text_alignment(s_date_layer, GTextAlignmentRight);

	text_layer_set_background_color(s_temp_layer, GColorClear);
	text_layer_set_text_color(s_temp_layer, GColorCyan);
	text_layer_set_font(s_temp_layer, s_font_small);
	text_layer_set_text_alignment(s_temp_layer, GTextAlignmentRight);

	text_layer_set_background_color(s_weather_layer, GColorClear);
	text_layer_set_text_color(s_weather_layer, GColorWhite);
	text_layer_set_font(s_weather_layer, s_font_tiny);
	text_layer_set_text_alignment(s_weather_layer, GTextAlignmentLeft);
	
	// Create battery meter layer
	//s_battery_layer = layer_create(GRect(0, bounds.size.h - BATTERY_HEIGHT, bounds.size.w, BATTERY_HEIGHT));
	//layer_set_update_proc(s_battery_layer, battery_update_proc);

	// Add it as a child layer to the Window's root layer
	layer_add_child(window_layer, s_border_layer);
	layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_ampm_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_temp_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_weather_layer));
	//layer_add_child(window_layer, s_battery_layer);
	
	init_member(window_layer, s_col1, COL_1, "H", "L", "Hr");
	init_member(window_layer, s_col2, COL_2, "H", "L", "Mi");
	init_member(window_layer, s_col3, COL_3, "D", "Y", "B");
	
	// Images for time
	s_images[0] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_0);
	s_images[1] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_1);
	s_images[2] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_2);
	s_images[3] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_3);
	s_images[4] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_4);
	s_images[5] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_5);
	s_images[6] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_6);
	s_images[7] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_7);
	s_images[8] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_8);
	s_images[9] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_9);
	
	int lefts[4] = {2, 34, 80, 112};
	for(int i = 0; i < 4; i++) {
		s_numbers[i] = bitmap_layer_create(GRect(lefts[i], NUM_TOP, NUM_W, NUM_H));
		bitmap_layer_set_bitmap(s_numbers[i], s_images[0]);
		
		layer_add_child(window_layer, bitmap_layer_get_layer(s_numbers[i]));
	}
	bitmap_layer_set_bitmap(s_numbers[0], NULL);
	
	layer_mark_dirty(s_border_layer);
	
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Set Name");
	
	party_member_set_name(s_col1, "Fri");
	party_member_set_name(s_col2, "Sat");
	
	APP_LOG(APP_LOG_LEVEL_DEBUG, "window loaded");
}

static void main_window_unload(Window *window) {
	deinit_member(s_col1);
	deinit_member(s_col2);
	deinit_member(s_col3);
	
	// Destroy battery layer
	//layer_destroy(s_battery_layer);
	// Destroy TextLayer
	text_layer_destroy(s_weather_layer);
	text_layer_destroy(s_temp_layer);
	text_layer_destroy(s_date_layer);
	text_layer_destroy(s_ampm_layer);
	text_layer_destroy(s_time_layer);
	// Destroy border layer
	layer_destroy(s_border_layer);
	
	for(int i = 0; i < 4; i++) {
		bitmap_layer_destroy(s_numbers[i]);
	}
	for(int i = 0; i < 10; i++) {
		gbitmap_destroy(s_images[i]);
	}
	// Destroy Font
	fonts_unload_custom_font(s_font_tiny);
	fonts_unload_custom_font(s_font_small);
}

static void init() {
	s_battery_color = GColorVividCerulean;

	// Create main Window element and assign to pointer
	s_main_window = window_create();
	window_set_background_color(s_main_window, GColorBlack);

	// Set handlers to manage the elements inside the Window
	window_set_window_handlers(s_main_window, (WindowHandlers) {
		.load = main_window_load,
		.unload = main_window_unload
	});

	// Show the Window on the watch, with animated=true
	window_stack_push(s_main_window, true);
	update_time();
	update_date();

	// Register with TickTimerService
	tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

	// Register callbacks
	app_message_register_inbox_received(inbox_received_callback);
	app_message_register_inbox_dropped(inbox_dropped_callback);
	app_message_register_outbox_failed(outbox_failed_callback);
	app_message_register_outbox_sent(outbox_sent_callback);

	// Open AppMessage
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

	// Register for battery level updates
	battery_state_service_subscribe(battery_callback);

	battery_callback(battery_state_service_peek());
}

static void deinit() {
	// Destroy Window
	window_destroy(s_main_window);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}