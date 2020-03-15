/**
 *  @filename   :   main.cpp
 *  @brief      :   4.2inch e-paper display (B) demo
 *  @author     :   Yehui from Waveshare
 *
 *  Copyright (C) Waveshare     August 15 2017
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <string>
#include <math.h>
#include <ctime>
#include <unistd.h>
#include <pango/pangocairo.h>
#include "screen.h"
#include "gcal.h"
#include "secrets.h"
#include "../lib/json.hpp"

using json = nlohmann::json;

const char *GOOGLE_TIME_FORMAT = "%Y-%m-%dT%H:%M:%S%z";
const char *GOOGLE_DATE_FORMAT = "%Y-%m-%d";
const char *HEADPHONES_PNG = "/home/pi/upNext/code/headphones.png";
const char *PARTY_PNG = "/home/pi/upNext/code/party.png";

const char *TITLE_FONT = "Proxima Nova Regular 40";
const char *SUBTITLE_FONT = "Proxima Nova Regular 24";
const char *SMALL_TEXT_BOLD_FONT = "Proxima Nova Bold 16";
const char *SMALL_TEXT_REGULAR_FONT = "Proxima Nova Regular 16";

long int datediff(struct tm * a, struct tm * b);
void convert_event_time_to_time(json eventTime, tm* time);
int get_event_status_code(json event);
bool is_more_important_event(json eventA, json eventB);
json get_events(GoogleCalendar* gcal);

void draw_clock(cairo_t *cr);
void draw_events(cairo_t *cr, json events);
void draw_message_with_headphones(cairo_t *cr, string message);
void draw_no_more_meetings(cairo_t *cr);

void draw_time_tagline(cairo_t *cr, string c_str);
string time_remaining_tagline(tm* endTime);
string time_till_tagline(int delta_min);

void draw_while_until_next_event(cairo_t *cr, int delta_min);
void draw_secondary_event_line(cairo_t *cr, json event);
void draw_main_event(cairo_t *cr, json event);

void print_event(json event);

/* Returns time in seconds between two datetimes. Long can handle +/-60 years
* Does not use mktime, because of issues with daylight savings
* See https://stackoverflow.com/questions/12122084/confusing-behaviour-of-mktime-function-increasing-tm-hour-count-by-one
*/
long int datediff(struct tm * from, struct tm * to) {
  return to->tm_sec - from->tm_sec + 
    60l * (to->tm_min - from->tm_min +
        60l * (to->tm_hour - from->tm_hour +
          24l * (to->tm_yday - from->tm_yday + 
            365l * (to->tm_year - from->tm_year)
          )
        )
      );
}

void convert_event_time_to_time(json eventTime, tm* time) {
  if (eventTime["dateTime"].is_null()) {
    // All day events
    strptime(eventTime["date"].get<string>().c_str(), GOOGLE_DATE_FORMAT, time);
  } else {
    strptime(eventTime["dateTime"].get<string>().c_str(), GOOGLE_TIME_FORMAT, time);
  }
}

int get_event_status_code(json event) {
  // 0 == "accepted"
  // 1 == own event, no status
  // 2 == "tentative"
  // 3 == "needsAction"
  // 4 == "declined"
  if (event["attendees"].is_null()){
    return 1;
  }
  string status = event["attendees"][0]["responseStatus"];
  if (status == "accepted") {
    return 0;
  } else if (status == "tentative") {
    return 2;
  } else if (status == "needsAction") {
    return 3;
  } else if (status == "declined") {
    return 4;
  } 
  return 99;
}

json get_events(GoogleCalendar* gcal) {
    char buffer [80];

    time_t t = time(0);   // get time now
    struct tm * today = localtime( & t );
    today->tm_hour = 0;
    today->tm_min = 0;
    today->tm_sec = 0;
    mktime(today);
    strftime(buffer, 80, GOOGLE_TIME_FORMAT, today);
    string today_str(buffer);

    // We go until tomorrow morning to pick up tomorrow's all day events
    struct tm * tomorrow = localtime( & t );
    tomorrow->tm_hour = 0;
    tomorrow->tm_min = 0;
    tomorrow->tm_sec = 1;
    tomorrow->tm_mday += 1;
    mktime(tomorrow);
    strftime(buffer, 80, GOOGLE_TIME_FORMAT, tomorrow);
    string tomorrow_str(buffer);

    json events = gcal->GetEventsBetween("primary", today_str, tomorrow_str);
    //cout << events.dump(4) << endl;
    return events;
}

void print_event(json event) {
    if (event.is_null()) {
      cout << "Null" << endl;
      return;
    }

    cout << "Event: " << event["summary"] << " | " << event["status"] << " | ";
    if (event["start"]["dateTime"].is_null()) {
      cout << "All day: " << event["start"]["date"] << endl;
    } else {
      cout << event["start"]["dateTime"] << endl;
    }
}

bool is_more_important_event(json eventA, json eventB) {
  // Returns true if eventA is "better" than eventB. In a tie, returns false.
  //
  // We prioritize accepted events of other people over own events,
  // and priortize these over tentative over unanswered over declined.
  // We prioritize events that started last, then events that end first.
  // Therefore:
  // If A is 1-5pm, B is 2-6pm, and C is 3-4pm, then:
  // -At 1, we'll only show A
  // -At 2, we'll show B as primary, A as secondary
  // -At 3, we'll show C as primary, B as secondary
  // -At 4, we'll show B as primary, A as secondary (B started later)
  // -At 5, we'll only show B
  // If A is 1-3pm, B is 1-2pm, and C is 2-3pm, then:
  // -At 1, we'll show B as primary, A as secondary (B ends first)
  // -At 2, we'll show C as primary, A as secondary (C started later)
  // Finally, we prioritize non-recurring events over recurring events.

  // Something is always better than nothing! We check A first, because tie returns false
  if (eventA.is_null()) {
    return false;
  }
  if (eventB.is_null()) {
    return true;
  }

  // Checking response status - lower is better
  int eventAStatus = get_event_status_code(eventA);
  int eventBStatus = get_event_status_code(eventB);
  if (eventAStatus < eventBStatus) {
    return true;
  } else if (eventAStatus > eventBStatus) {
    return false;
  }

  struct tm eventAStart = {};
  struct tm eventAEnd = {};
  struct tm eventBStart = {};
  struct tm eventBEnd = {};
  convert_event_time_to_time(eventA["start"], &eventAStart);
  convert_event_time_to_time(eventA["end"], &eventAEnd);
  convert_event_time_to_time(eventB["start"], &eventBStart);
  convert_event_time_to_time(eventB["end"], &eventBEnd);

  // dateDiff(X,Y) is Y - X
  long int deltaStart = datediff(&eventAStart, &eventBStart);
  if (deltaStart < 0) {
    // Y - X < 0 , so X > Y, so A started later, and is better
    return true;
  } else if (deltaStart > 0) {
    return false;
  }

  long int deltaEnd = datediff(&eventAEnd, &eventBEnd);
  if (deltaEnd < 0) {
    // Y - X < 0 , so X > Y, so A ends later, and is worse
    return false;
  } else if (deltaEnd > 0) {
    return true;
  }

  if (eventA["recurringEventId"].is_null() && !eventB["recurringEventId"].is_null()) {
    return true;
  } else if (!eventA["recurringEventId"].is_null() && eventB["recurringEventId"].is_null()) {
    return false;
  }

  // They're basically the same, so in a tie return false
  return false;
}

void draw_events(cairo_t *cr, json events) {
  int num_events = events.size();
  json best_current_event;
  json second_current_event;
  json best_next_event;
  json second_next_event;
  json today_all_day_event;
  json tomorrow_all_day_event;
  json first_event_of_day;

  time_t currentTime = time(0);
  struct tm * now = localtime( &currentTime );
  struct tm event_start_time = {};
  struct tm event_end_time = {};
  struct tm best_next_start_time = {};
  struct tm second_next_start_time = {};

  cout << "Processing events: " << endl;
  json event;
  for (int i = 0; i < num_events; i++) {
    event = events[i];
    print_event(event);
    convert_event_time_to_time(event["start"], &event_start_time);
    convert_event_time_to_time(event["end"], &event_end_time);

    if (event["start"]["dateTime"].is_null()) {
      // All day event
      if (datediff(now, &event_start_time) < 0) {
        today_all_day_event = event;
      } else {
        tomorrow_all_day_event = event;
      }
      continue;
    }

    if (first_event_of_day.is_null()) {
      // First real event!
      first_event_of_day = event;
    }

    if (datediff(now, &event_end_time) < 0) {
      continue;
    }

    if (datediff(now, &event_start_time) < 0) {
      // Pick the best two current events
      if (is_more_important_event(event, best_current_event)) {
        // By definition, we know best_current_event is always better than second_current_event
        second_current_event = best_current_event;
        best_current_event = event;
      } else if (is_more_important_event(event, second_current_event)) {
        second_current_event = event;
      }
    } else {
      // For upcoming events, only 4 reasons why we'd care about this event:
      // we don't have a first or a second, or this starts at the same time
      // as one of those and is more important
      if (best_next_event.is_null()) {
        best_next_event = event;
        best_next_start_time = event_start_time;
      } else if (datediff(&best_next_start_time, &event_start_time) == 0 &&
          is_more_important_event(event, best_next_event)) {
        second_next_event = best_next_event;
        second_next_start_time = best_next_start_time;

        best_next_event = event;
        best_next_start_time = event_start_time;
      } else if (second_next_event.is_null()) {
        second_next_event = event;
        second_next_start_time = event_start_time;
      } else if (datediff(&second_next_start_time, &event_start_time) == 0 &&
          is_more_important_event(event, second_next_event)) {
        second_next_event = event;
        second_next_start_time = event_start_time;
      }
    }
  }

  cout << "Best curr: ";
  print_event(best_current_event);
  cout << "Second curr: ";
  print_event(second_current_event);
  cout << "Best next: ";
  print_event(best_next_event);
  cout << "Second next: ";
  print_event(second_next_event);

  int delta_min = 0;
  if (!best_next_event.is_null()) {
    convert_event_time_to_time(best_next_event["start"], &event_start_time);
    long int delta_seconds = datediff(now, &event_start_time);
    delta_min = (int) round(delta_seconds / 60.0);
    cout << "Time until next event: " << delta_min << " minutes" << endl;
  }

  // We now have all of our event information and can decide
  // what to show on the screen.
  // The screen is designed with the following components:
  //  ___________________________________
  // |[Time tagline]             [clock]|
  // |                                  |
  // |[Primary event details]           |
  // |                                  |
  // |                                  |
  // |                                  |
  // |[Secondary event tagline]         |
  //  ⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻⎻
  // [Time tagline] is things like "Now:", "In 5 minutes", or "Until 10am", and describes
  // the  primary event.
  // [clock] is a clock, and shows things like "9:41pm"
  //
  // The logic for what event to show when/where is somewhat complex, and is tuned based
  // on what felt right to me as a frequent user:
  //
  // [Primary event]
  // * If we're supposed to be in a meeting right now, show that as the primary event.
  // * If we're in a meeting, but there's another one in < 5 minutes, show that as the primary event
  //   instead of the meeting we're currently in, so we can prepare to go to that one.
  // * If we're not in a meeting and there's an event in < 30 minutes, show that as the primary event.
  // * If there isn't a meeting for > 30 minutes, don't show a primary event and instead say "heads down time"
  // * If there's an all-day event and we're > 30 minutes before the first meeting of the day, show the all-day event
  //   as the primary event
  // * If we're done with meetings for the day, yay! Don't show a primary event and instead say
  //   "Done with meetings for the day"
  //
  // [Time tagline]
  // * If the primary event is our current meeting, show "Until <end time>:"
  // * If the primary event is an upcoming event, show "In <delta> minutes:"
  // * If the primary event is an all day meeting, show "All day:"
  // * If we don't have a primary event (no more meetings or far away), don't show a tagline
  //
  // [Secondary event tagline]
  // * If we're in a meeting, show the next one in the form of "Then - <time>: <summary>"
  // * If we've got a next event coming up (in the "primary event" slot), show the one after that (the "following" event)
  //   if there is one.
  // * If we've got a next event coming up but it's a ways out (not in the "primary event" slot), show _that_ event's
  //   details (i.e. *not* the "following" event)
  // * If we're at the last event(s) of the day and so don't anything to put in the secondary event slot, see
  //   if we have an all day event tomorrow. If we do, show that, otherwise leave it blank
  json primary_event;
  json secondary_event;
  bool in_meeting = !best_current_event.is_null();
  bool have_next_event = !best_next_event.is_null();
  cout << "In meeting: " << in_meeting << endl;
  cout << "Have next: " << have_next_event << endl;

  if (in_meeting) {
    if (have_next_event && delta_min <= 5) {
      primary_event = best_next_event;
    } else {
      primary_event = best_current_event;
    }
  } else if (have_next_event && delta_min <= 30) {
    primary_event = best_next_event;
  } else if (!today_all_day_event.is_null() && best_next_event == first_event_of_day) {
    primary_event = today_all_day_event;
  } else {
    // making this explicit that we _don't_ want a primary event in this case
  }

  if (primary_event == best_next_event) {
    secondary_event = second_next_event;
  } else if (!second_current_event.is_null()) {
    secondary_event = second_current_event;
  } else {
    secondary_event = best_next_event;
  }

  if (secondary_event.is_null() && !tomorrow_all_day_event.is_null()) {
    secondary_event = tomorrow_all_day_event;
  }

  cout << "Primary event: ";
  print_event(primary_event);
  cout << "Secondary event: ";
  print_event(secondary_event);

  // Now, we draw it
  if (primary_event.is_null()) {
    if (have_next_event) {
      draw_while_until_next_event(cr, delta_min);
    } else {
      draw_no_more_meetings(cr);
    }
    draw_secondary_event_line(cr, secondary_event);
  } else {
    draw_main_event(cr, primary_event);
    draw_secondary_event_line(cr, secondary_event);

    // Drawing tagline
    string tagline;
    if (primary_event == best_current_event) {
      convert_event_time_to_time(primary_event["end"], &event_end_time);
      tagline = time_remaining_tagline(&event_end_time);
    } else if (primary_event == best_next_event) {
      tagline = time_till_tagline(delta_min);
    } else if (primary_event == today_all_day_event) {
      tagline = "All day:";
    }
    draw_time_tagline(cr, tagline);
  }
}

void draw_clock(cairo_t *cr) {
  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);

  PangoLayout *layout = pango_cairo_create_layout (cr);
  PangoFontDescription *font_description = pango_font_description_from_string (SMALL_TEXT_BOLD_FONT);
  pango_layout_set_font_description (layout, font_description);
  // Top right alignment
  int width = 100;
  int margin = 10;
  cairo_move_to (cr, 400 - (width + margin), margin);
  pango_layout_set_width (layout, width * PANGO_SCALE);
  pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);

  static char outstr[8];
  time_t t = time(0);   // get time now
  struct tm * now = localtime( & t );
  //sprintf(outstr, "%d:%02d", now->tm_hour, now->tm_min);
  strftime(outstr, 8 * sizeof(char), "%-I:%M%P", now);

  pango_layout_set_text (layout, outstr, -1);
  pango_cairo_show_layout(cr, layout);

  pango_font_description_free (font_description);
  g_object_unref (layout);
}

void draw_message_with_image(cairo_t *cr, string message, const char* filepath) {
  int margin = 10;
  PangoLayout *layout = pango_cairo_create_layout (cr);
  PangoFontDescription *font_description = pango_font_description_from_string (SUBTITLE_FONT);
  pango_layout_set_font_description (layout, font_description);
  // Center, slightly below center
  cairo_move_to (cr, margin, 200);
  pango_layout_set_width (layout, (400 - 2 * margin) * PANGO_SCALE);
  // Only draw one line
  pango_layout_set_height (layout, -1);
  pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);

  pango_layout_set_text (layout, message.c_str(), -1);
  pango_cairo_show_layout(cr, layout);

  // Draw image, restore source
  cairo_surface_t *image = cairo_image_surface_create_from_png(filepath);
  if (cairo_surface_status(image) == CAIRO_STATUS_SUCCESS) {
    cairo_set_source_surface (cr, image, 125, 50);
    cairo_paint (cr);
  }

  cairo_surface_destroy (image);
  pango_font_description_free (font_description);
  g_object_unref (layout);
}

void draw_no_more_meetings(cairo_t *cr) {
  time_t t = time(0);   // get time now
  struct tm * today = localtime( & t );
  // Weekend (or heading into it)
  if (today->tm_wday == 5) {
    draw_message_with_image(cr, "No more meetings - have a great weekend!", PARTY_PNG);
  } else if (today->tm_wday == 6 || today->tm_wday == 0) {
    draw_message_with_image(cr, "Hope you're enjoying your weekend", PARTY_PNG);
  } else {
    draw_message_with_image(cr, "No more meetings today", HEADPHONES_PNG);
  }
}

void draw_time_tagline(cairo_t *cr, string tagline) {
  if (tagline.empty()) {
    return;
  }

  PangoLayout *layout = pango_cairo_create_layout (cr);
  PangoFontDescription *font_description = pango_font_description_from_string (SMALL_TEXT_BOLD_FONT);
  pango_layout_set_font_description (layout, font_description);
  // Top left alignment
  int margin = 10;
  cairo_move_to (cr, margin, margin);
  pango_layout_set_alignment (layout, PANGO_ALIGN_LEFT);

  pango_layout_set_text (layout, tagline.c_str(), -1);
  pango_cairo_show_layout(cr, layout);

  pango_font_description_free (font_description);
  g_object_unref (layout);
}

string time_remaining_tagline(tm* endTime) {
  static char timestr[20];
  strftime(timestr, 20 * sizeof(char), "Until %-I:%M%P:", endTime);
  return string(timestr);
}

string time_till_tagline(int delta_min) {
  ostringstream os;
  if (delta_min <= 0) {
    os << "Now:";
  } else if (delta_min == 1) {
    os << "In 1 minute:";
  } else {
    os << "In " << delta_min << " minutes:";
  }

  return os.str();
}

void draw_while_until_next_event(cairo_t *cr, int delta_min) {
  ostringstream os;
  // e.g. "1.5 hours meeting free", "50 minutes meeting free"
  if (delta_min > 75) {
    // Nearest half hour
    os.precision(2);
    os << round(delta_min / 30.0) / 2 << " hours";
  } else if (delta_min >= 57) {
    os << "1 hour";
  } else {
    os << round(delta_min / 5.0) * 5 << " minutes";
  }

  os << " meeting free";

  draw_message_with_image(cr, os.str(), HEADPHONES_PNG);
}

void draw_main_event(cairo_t *cr, json event) {
  int margin = 10;
  int startY = 50;
  int title_width;
  int title_height;

  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);

  PangoLayout *layout = pango_cairo_create_layout (cr);
  PangoFontDescription *title_font = pango_font_description_from_string (TITLE_FONT);
  pango_layout_set_font_description (layout, title_font);

  string summary = event["summary"];
  pango_layout_set_text (layout, summary.c_str(), -1);
  // 2 lines, ellipsize after that
  pango_layout_set_width (layout, (400 - 2 * margin) * PANGO_SCALE);
  int max_lines = event["location"].is_null() ? 3 : 2;
  // Handy, but strange: if height is negative, it will be the (negative of) maximum number of lines per paragraph.
  pango_layout_set_height (layout, -max_lines);
  pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);
  pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);

  pango_layout_get_pixel_size(layout, &title_width, &title_height);

  cairo_move_to (cr, margin, startY);
  pango_cairo_show_layout(cr, layout);

  if (!event["location"].is_null()) {
    PangoFontDescription *subtitle_font = pango_font_description_from_string (SUBTITLE_FONT);
    pango_layout_set_font_description (layout, subtitle_font);

    string location = event["location"];
    pango_layout_set_text (layout, location.c_str(), -1);
    pango_layout_set_width (layout, (400 - 2 * margin) * PANGO_SCALE);
    pango_layout_set_height (layout, -1);
    pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);

    cairo_move_to (cr, margin, startY + title_height + margin);
    pango_cairo_show_layout(cr, layout);

    pango_font_description_free (subtitle_font);
  }

  pango_font_description_free (title_font);
  g_object_unref (layout);
}



void draw_secondary_event_line(cairo_t *cr, json event) {
  if (event.is_null()) {
    return;
  }

  string time_str;
  if (event["start"]["dateTime"].is_null()) {
    // All day event -- in current logic, this is always tomorrow
    time_str = "Tomorrow:";
  } else {
    ostringstream os;
    struct tm startTime = {};
    convert_event_time_to_time(event["start"], &startTime);

    static char timestr[10];
    time_t currentTime = time(0);
    struct tm * now = localtime( &currentTime );

    if (datediff(now, &startTime) < 0) {
      // Another concurrent event
      struct tm endTime = {};
      convert_event_time_to_time(event["end"], &endTime);
      strftime(timestr, 10 * sizeof(char), "%-I:%M%P", &endTime);
      os << "Also - until " << timestr << ":";
      time_str = os.str();
    } else {
      strftime(timestr, 10 * sizeof(char), "%-I:%M%P", &startTime);
      os << "Then - " << timestr << ":";
      time_str = os.str();
    }
  }

  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);

  PangoLayout *layout = pango_cairo_create_layout (cr);
  PangoFontDescription *bold_font = pango_font_description_from_string (SMALL_TEXT_BOLD_FONT);
  pango_layout_set_font_description (layout, bold_font);
  pango_layout_set_text (layout, time_str.c_str(), -1);

  int text_width;
  int text_height;

  pango_layout_get_pixel_size(layout, &text_width, &text_height);
  cairo_move_to (cr, 10, 300 - text_height - 10);
  pango_cairo_show_layout(cr, layout);

  PangoFontDescription *regular_font = pango_font_description_from_string (SMALL_TEXT_REGULAR_FONT);
  pango_layout_set_font_description (layout, regular_font);

  string summary = event["summary"];
  pango_layout_set_text (layout, summary.c_str(), -1);
  pango_layout_set_width (layout, (400 - text_width - 25) * PANGO_SCALE);
  pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
  cairo_move_to (cr, text_width + 15, 300 - text_height - 10);
  pango_cairo_show_layout(cr, layout);

  pango_font_description_free (bold_font);
  pango_font_description_free (regular_font);
  g_object_unref (layout);
}

int main(void)
{
    Screen screen;
    if (screen.Init() != 0) {
        printf("Screen initialization failed\n");
        return -1;
    }

    //screen.Clear();
    screen.HardWipe();

    GoogleCalendar* gcal = new GoogleCalendar();
    gcal->SetCredentials(GCAL_CLIENT_ID, GCAL_CLIENT_SECRET);
    //gcal->RequestInstalledAppToken();

    gcal->SetAuthToken(GCAL_AUTH_TOKEN, GCAL_REFRESH_TOKEN);

    cairo_surface_t *surface = screen.GetCairoSurface();
    cairo_t *cr = cairo_create (surface);

    // Every 10 seconds, fetch events and re-render
    while(true) {
      json events = get_events(gcal);

      draw_clock(cr);
      draw_events(cr, events);

      screen.Render();

      // clear cairo context
      cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
      cairo_paint (cr);
      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

      sleep(10);
    }

    cairo_destroy (cr);
    screen.Cleanup();
    return 0;
}
