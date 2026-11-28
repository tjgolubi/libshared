////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 - 2021, 2023, Gothenburg Bit Factory.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// https://opensource.org/license/mit
//
////////////////////////////////////////////////////////////////////////////////

#include <Datetime.h>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <format>
#include <format.h>
#include <iomanip>
#include <iostream>
#include <print>
#include <shared.h>
#include <sstream>
#include <unicode.h>
#include <utf8.h>
#include <utility>

namespace chrono = std::chrono;

namespace
{
constexpr auto from_epoch (time_t value) -> chrono::sys_seconds
{
  return chrono::sys_seconds{chrono::seconds{value}};
}

constexpr auto to_epoch (chrono::sys_seconds value) -> time_t
{
  const auto seconds = value.time_since_epoch ().count ();
  return std::in_range<time_t> (seconds) ? static_cast<time_t> (seconds) : -1;
}

auto now_seconds () -> chrono::sys_seconds
{
  return chrono::floor<chrono::seconds> (chrono::system_clock::now ());
}

// Count from the first of the month to normalize dates such as January 32.
constexpr auto calendar_day (chrono::year_month month, int day)
  -> chrono::local_days
{
  return chrono::local_days{month / 1} + chrono::days{day - 1};
}

constexpr auto calendar_day (int year, int month, int day) -> chrono::local_days
{
  return calendar_day (chrono::year{year} / chrono::January +
                       chrono::months{month - 1}, day);
}

// Use the platform timezone so TZ strings and mktime's DST rules still apply.
auto local_fields (time_t value) -> std::tm
{
  std::tm fields {};
#ifdef _WIN32
  if (localtime_s (&fields, &value) != 0)
#else
  tzset ();
  if (localtime_r (&value, &fields) == nullptr)
#endif
    throw std::string ("Timestamp is outside the local time range.");
  return fields;
}

auto local_time (const std::tm& fields) -> chrono::local_seconds
{
  return calendar_day (fields.tm_year + 1900, fields.tm_mon + 1, fields.tm_mday)
       + chrono::hours{fields.tm_hour} + chrono::minutes{fields.tm_min}
       + chrono::seconds{fields.tm_sec};
}

auto local_time (time_t value) -> chrono::local_seconds
{
  return local_time (local_fields (value));
}

auto local_day (time_t value) -> chrono::local_days
{
  return chrono::floor<chrono::days> (local_time (value));
}

auto local_clock (time_t value) -> chrono::hh_mm_ss<chrono::seconds>
{
  const auto local = local_time (value);
  return chrono::hh_mm_ss{local - chrono::floor<chrono::days> (local)};
}

auto local_epoch (chrono::local_seconds value, int is_dst = -1) -> time_t
{
  const auto day = chrono::floor<chrono::days> (value);
  const auto date = chrono::year_month_day{day};
  const auto clock = chrono::hh_mm_ss{value - day};
  std::tm fields {};
  fields.tm_year  = int(date.year ()) - 1900;
  fields.tm_mon   = int(unsigned(date.month ())) - 1;
  fields.tm_mday  = int(unsigned(date.day ()));
  fields.tm_hour  = clock.hours ().count ();
  fields.tm_min   = clock.minutes ().count ();
  fields.tm_sec   = clock.seconds ().count ();
  fields.tm_isdst = is_dst;
  return std::mktime (&fields);
}

auto today () -> chrono::local_days
{
  return local_day (to_epoch (now_seconds ()));
}

auto monday_of (chrono::local_days day) -> chrono::local_days
{
  return day - (chrono::weekday{day} - chrono::Monday);
}

auto month_start (int offset) -> chrono::local_days
{
  const auto date = chrono::year_month_day{today ()};
  const auto month = date.year () / date.month () + chrono::months{offset};
  return chrono::local_days{month / 1};
}

auto quarter_start (int offset) -> chrono::local_days
{
  const auto date = chrono::year_month_day{today ()};
  const auto first = chrono::January +
    chrono::months{(int(unsigned(date.month ())) - 1) / 3 * 3};
  const auto month = date.year () / first + chrono::months{offset * 3};
  return chrono::local_days{month / 1};
}

auto year_start (int offset) -> chrono::local_days
{
  const auto year = chrono::year_month_day{today ()}.year () +
                    chrono::years{offset};
  return chrono::local_days{year / chrono::January / 1};
}
} // namespace

static std::vector <std::string> dayNames {
  "sunday",
  "monday",
  "tuesday",
  "wednesday",
  "thursday",
  "friday",
  "saturday"};

static std::vector <std::string> monthNames {
  "january",
  "february",
  "march",
  "april",
  "may",
  "june",
  "july",
  "august",
  "september",
  "october",
  "november",
  "december"};

int Datetime::weekstart = 1; // Monday, per ISO-8601.
int Datetime::minimumMatchLength = 3;
bool Datetime::isoEnabled            = true;
bool Datetime::standaloneDateEnabled = true;
bool Datetime::standaloneTimeEnabled = true;

// When true,
// - HH:MM:SS is assumed to be today if the time > now, otherwise it is assumed to be tomorrow.
// - day name is converted to date relative to now
// - 1st, 2nd, 3rd, ... is converted to date relative to now
// When false,
// - HH:MM:SS is always today.
// - day name is always converted to date before now
// - 1st, 2nd, 3rd, ... is always converted to date before now
bool Datetime::timeRelative = true;

////////////////////////////////////////////////////////////////////////////////
Datetime::Datetime ()
{
  clear ();
  _date = to_epoch (now_seconds ());
}

////////////////////////////////////////////////////////////////////////////////
Datetime::Datetime (const std::string& input, const std::string& format)
{
  clear ();
  std::string::size_type start = 0;
  if (! parse (input, start, format))
    throw std::format ("'{}' is not a valid date in the '{}' format.", input, format);
}

////////////////////////////////////////////////////////////////////////////////
Datetime::Datetime (const time_t t)
{
  clear ();
  _date = t;
}

////////////////////////////////////////////////////////////////////////////////
Datetime::Datetime (const int y, const int m, const int d)
{
  // Protect against arguments being passed in the wrong order.
  assert (y >= 1969 && y <= 9999);
  assert (m >= 1 && m <= 12);
  assert (d >= 1 && d <= 31);

  clear ();

  _date = local_epoch (calendar_day (y, m, d));
}

////////////////////////////////////////////////////////////////////////////////
Datetime::Datetime (const int y,  const int m,  const int d,
                    const int hr, const int mi, const int se)
{
  // Protect against arguments being passed in the wrong order.
  assert (y >= 1969 && y <= 9999);
  assert (m >= 1 && m <= 12);
  assert (d >= 1 && d <= 31);
  assert (hr >= 0 && hr <= 24);
  assert (mi >= 0 && mi < 60);
  assert (se >= 0 && se < 60);

  clear ();

  _date = local_epoch (calendar_day (y, m, d)
                     + chrono::hours{hr} + chrono::minutes{mi}
                     + chrono::seconds{se});
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::parse (
  const std::string& input,
  std::string::size_type& start,
  const std::string& format)
{
  auto i = start;
  Pig pig (input);
  if (i)
    pig.skipN (static_cast <int> (i));

  auto checkpoint = pig.cursor ();

  // Parse epoch first, as it's the most common scenario.
  if (parse_epoch (pig))
  {
    // ::validate and ::resolve are not needed in this case.
    start = pig.cursor ();
    return true;
  }

  bool ambiguous {false};
  if (parse_formatted (pig, format, ambiguous))
  {
    // Check the values and determine time_t.
    if (validate ())
    {
      start = pig.cursor ();
      resolve ();
      return true;
    }
  }
  else if (ambiguous)
  {
    return false;
  }

  // Allow parse_date_time and parse_date_time_ext regardless of
  // Datetime::isoEnabled setting, because these formats are relied upon by
  // the 'import' command, JSON parser and hook system.
  if (parse_date_time_ext   (pig) || // Strictest first.
      parse_date_time       (pig) ||
      (Datetime::isoEnabled &&
       (                                    parse_date_ext      (pig)  ||
        (Datetime::standaloneDateEnabled && parse_date          (pig)) ||
                                            parse_time_utc_ext  (pig)  ||
                                            parse_time_utc      (pig)  ||
                                            parse_time_off_ext  (pig)  ||
                                            parse_time_ext      (pig)  ||
        (Datetime::standaloneTimeEnabled && parse_time          (pig)) || // Time last, as it is the most permissive.
        (Datetime::standaloneTimeEnabled && parse_time_off      (pig))
       )
      )
     )
  {
    // Check the values and determine time_t.
    if (validate ())
    {
      start = pig.cursor ();
      resolve ();
      return true;
    }
  }

  pig.restoreTo (checkpoint);

  if (parse_named (pig))
  {
    // ::validate and ::resolve are not needed in this case.
    start = pig.cursor ();
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
void Datetime::clear ()
{
  _year    = 0;
  _month   = 0;
  _week    = 0;
  _weekday = Datetime::weekstart;
  _julian  = 0;
  _day     = 0;
  _seconds = 0;
  _offset  = 0;
  _utc     = false;
  _date    = 0;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::parse_formatted (Pig& pig, const std::string& format, bool& ambiguous)
{
  // Short-circuit on missing format.
  if (format.empty ())
    return false;

  auto checkpoint = pig.cursor ();

  int month  {-1};   // So we can check later.
  int day    {-1};
  int year   {-1};
  int hour   {-1};
  int minute {-1};
  int second {-1};
  int meridiem {-1};
  bool twelveHourTime {false};

  // For parsing, unused.
  int wday   {-1};
  int week   {-1};

  for (unsigned int f = 0; f < format.length (); ++f)
  {
    switch (format[f])
    {
    case 'm':
      if (pig.getDigit (month))
      {
        if (month == 0)
          pig.getDigit (month);

        if (month == 1)
          if (pig.getDigit (month))
            month += 10;
      }
      else
      {
        pig.restoreTo (checkpoint);
        return false;
      }
      break;

    case 'M':
      if (! pig.getDigit2 (month))
      {
        pig.restoreTo (checkpoint);
        return false;
      }
      break;

    case 'd':
      if (pig.getDigit (day))
      {
        if (day == 0)
          pig.getDigit (day);

        if (day == 1 || day == 2 || day == 3)
        {
          int tens = day;
          if (pig.getDigit (day))
            day += 10 * tens;
        }
      }
      else
      {
        pig.restoreTo (checkpoint);
        return false;
      }
      break;

    case 'D':
      if (! pig.getDigit2 (day))
      {
        pig.restoreTo (checkpoint);
        return false;
      }
      break;

    case 'y':
      if (! pig.getDigit2 (year))
      {
        pig.restoreTo (checkpoint);
        return false;
      }
      year += 2000;
      break;

    case 'Y':
      if (! pig.getDigit4 (year))
      {
        pig.restoreTo (checkpoint);
        return false;
      }
      break;

    case 'h':
      if (pig.getDigit (hour))
      {
        if (hour == 0)
          pig.getDigit (hour);

        if (hour == 1 || hour == 2)
        {
          int tens = hour;
          if (pig.getDigit (hour))
            hour += 10 * tens;
        }
      }
      else
      {
        pig.restoreTo (checkpoint);
        return false;
      }
      break;

    case 'H':
      if (! pig.getDigit2 (hour))
      {
        pig.restoreTo (checkpoint);
        return false;
      }
      break;

    case 'i':
      twelveHourTime = true;
      if (pig.getDigit (hour))
      {
        if (hour == 0)
          pig.getDigit (hour);

        if (hour == 1)
        {
          int tens = hour;
          if (pig.getDigit (hour))
            hour += 10 * tens;
        }

        if (hour < 1 || hour > 12)
        {
          pig.restoreTo (checkpoint);
          return false;
        }
      }
      else
      {
        pig.restoreTo (checkpoint);
        return false;
      }
      break;

    case 'I':
      twelveHourTime = true;
      if (! pig.getDigit2 (hour) || hour < 1 || hour > 12)
      {
        pig.restoreTo (checkpoint);
        return false;
      }
      break;

    case 'p':
      if (compare (pig.peek (2), "AM", false))
      {
        meridiem = 0;
        pig.skipN (2);
      }
      else if (compare (pig.peek (2), "PM", false))
      {
        meridiem = 1;
        pig.skipN (2);
      }
      else
      {
        pig.restoreTo (checkpoint);
        return false;
      }
      break;

    case 'n':
      if (pig.getDigit (minute))
      {
        if (minute == 0)
          pig.getDigit (minute);

        if (minute < 6)
        {
          int tens = minute;
          if (pig.getDigit (minute))
            minute += 10 * tens;
        }
      }
      else
      {
        pig.restoreTo (checkpoint);
        return false;
      }
      break;

    case 'N':
      if (! pig.getDigit2 (minute))
      {
        pig.restoreTo (checkpoint);
        return false;
      }
      break;

    case 's':
      if (pig.getDigit (second))
      {
        if (second == 0)
          pig.getDigit (second);

        if (second < 6)
        {
          int tens = second;
          if (pig.getDigit (second))
            second += 10 * tens;
        }
      }
      else
      {
        pig.restoreTo (checkpoint);
        return false;
      }
      break;

    case 'S':
      if (! pig.getDigit2 (second))
      {
        pig.restoreTo (checkpoint);
        return false;
      }
      break;

    case 'v':
      if (pig.getDigit (week))
      {
        if (week == 0)
          pig.getDigit (week);

        if (week < 6)
        {
          int tens = week;
          if (pig.getDigit (week))
            week += 10 * tens;
        }
      }
      else
      {
        pig.restoreTo (checkpoint);
        return false;
      }
      break;

    case 'V':
      if (! pig.getDigit2 (week))
      {
        pig.restoreTo (checkpoint);
        return false;
      }
      break;

    case 'a':
      wday = Datetime::dayOfWeek (pig.str ().substr (0, 3));
      if (wday == -1)
      {
        pig.restoreTo (checkpoint);
        return false;
      }

      pig.skipN (3);
      break;

    case 'A':
      {
        std::string dayName;
        if (pig.getUntil (format[f + 1], dayName))
        {
          wday = Datetime::dayOfWeek (dayName);
          if (wday == -1)
          {
            pig.restoreTo (checkpoint);
            return false;
          }
        }
      }
      break;

    case 'b':
      month = Datetime::monthOfYear (pig.str ().substr (0, 3));
      if (month == -1)
      {
        pig.restoreTo (checkpoint);
        return false;
      }

      pig.skipN (3);
      break;

    case 'B':
      {
        std::string monthName;
        if (pig.getUntil (format[f + 1], monthName))
        {
          month = Datetime::monthOfYear (monthName);
          if (month == -1)
          {
            pig.restoreTo (checkpoint);
            return false;
          }
        }
      }
      break;

    default:
      if (! pig.skip (format[f]))
      {
        pig.restoreTo (checkpoint);
        return false;
      }
      break;
    }
  }

  // It is possible that the format='Y-M-D', and the input is Y-M-DTH:N:SZ, and
  // this should not be considered a match.
  if (! pig.eos () && ! unicodeWhitespace (pig.peek ()))
  {
    pig.restoreTo (checkpoint);
    return false;
  }

  // Missing values are filled in from the current date.
  if (year == -1)
  {
    Datetime now;
    year = now.year ();
    if (month == -1)
    {
      month = now.month ();
      if (day == -1)
      {
        day = now.day ();
        if (hour == -1)
        {
          hour = now.hour ();
          if (minute == -1)
          {
            minute = now.minute ();
            if (second == -1)
              second = now.second ();
          }
        }
      }
    }
  }

  // Any remaining undefined values are assigned defaults.
  if (month  == -1) month  = 1;
  if (day    == -1) day    = 1;
  if (hour   == -1) hour   = 0;
  if (minute == -1) minute = 0;
  if (second == -1) second = 0;

  if (twelveHourTime && meridiem == -1)
  {
    ambiguous = true;
    pig.restoreTo (checkpoint);
    return false;
  }

  if (meridiem != -1)
  {
    if (hour < 1 || hour > 12)
    {
      pig.restoreTo (checkpoint);
      return false;
    }

    if (meridiem == 0 && hour == 12)
      hour = 0;
    else if (meridiem == 1 && hour != 12)
      hour += 12;
  }

  _year    = year;
  _month   = month;
  _day     = day;
  _seconds = (hour * 3600) + (minute * 60) + second;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// Note how these are all single words.
//
// Examples and descriptions, assuming now == 2017-03-05T12:34:56.
//
//                  Example              Notes
//                  -------------------  ------------------
//   now            2017-03-05T12:34:56  Unaffected
//   yesterday      2017-03-04T00:00:00  Unaffected
//   today          2017-03-05T00:00:00  Unaffected
//   tomorrow       2017-03-06T00:00:00  Unaffected
//   <ordinal> 12th 2017-03-12T00:00:00
//   <day> monday   2017-03-06T00:00:00
//   <month> april  2017-04-01T00:00:00
//   later          9999-12-30T00:00:00  Unaffected
//   someday        9999-12-30T00:00:00  Unaffected
//   sopd           2017-03-04T00:00:00  Unaffected
//   sod            2017-03-05T00:00:00  Unaffected
//   sond           2017-03-06T00:00:00  Unaffected
//   eopd           2017-03-04T23:59:59  Unaffected
//   eod            2017-03-05T23:59:59  Unaffected
//   eond           2017-03-06T23:59:59  Unaffected
//   sopw           2017-02-26T00:00:00  Unaffected
//   sow            2017-03-05T00:00:00  Unaffected
//   sonw           2017-03-12T00:00:00  Unaffected
//   eopw           2017-02-26T23:59:59  Unaffected
//   eow            2017-03-05T23:59:59  Unaffected
//   eonw           2017-03-12T23:59:59  Unaffected
//   sopww          2017-02-27T00:00:00  Unaffected
//   soww           2017-03-06T00:00:00
//   sonww          2017-03-06T00:00:00  Unaffected
//   eopww          2017-03-03T23:59:59  Unaffected
//   eoww           2017-03-10T23:59:59
//   eonww          2017-03-17T23:59:59  Unaffected
//   sopm           2017-02-01T00:00:00  Unaffected
//   som            2017-03-01T00:00:00  Unaffected
//   sonm           2017-04-01T00:00:00  Unaffected
//   eopm           2017-02-28T23:59:59  Unaffected
//   eom            2017-03-31T23:59:59  Unaffected
//   eonm           2017-04-30T23:59:59  Unaffected
//   sopq           2017-10-01T00:00:00  Unaffected
//   soq            2017-01-01T00:00:00  Unaffected
//   sonq           2017-04-01T00:00:00  Unaffected
//   eopq           2016-12-31T23:59:59  Unaffected
//   eoq            2017-03-31T23:59:59  Unaffected
//   eonq           2017-06-30T23:59:59  Unaffected
//   sopy           2016-01-01T00:00:00  Unaffected
//   soy            2017-01-01T00:00:00  Unaffected
//   sony           2018-01-01T00:00:00  Unaffected
//   eopy           2016-12-31T23:59:59  Unaffected
//   eoy            2017-12-31T23:59:59  Unaffected
//   eony           2018-12-31T23:59:59  Unaffected
//   easter         2017-04-16T00:00:00
//   eastermonday   2017-04-16T00:00:00
//   ascension      2017-05-25T00:00:00
//   pentecost      2017-06-04T00:00:00
//   goodfriday     2017-04-14T00:00:00
//   midsommar      2017-06-24T00:00:00  midnight, 1st Saturday after 20th June
//   midsommarafton 2017-06-23T00:00:00  midnight, 1st Friday after 19th June
//   juhannus       2017-06-23T00:00:00  midnight, 1st Friday after 19th June
//
bool Datetime::parse_named (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  // Experimental handling of date phrases, such as "first monday in march".
  // Note that this requires that phrases are delimited by EOS or WS.
  std::string token;
  std::vector <std::string> tokens;
  while (pig.getUntilWS (token))
  {
    tokens.push_back (token);
    if (! pig.skipWS ())
      break;
  }

/*
  // This group contains "1st monday ..." which must be processed before
  // initializeOrdinal below.
  if (initializeNthDayInMonth (tokens))
  {
    return true;
  }
*/

  // Restoration necessary because of the tokenization.
  pig.restoreTo (checkpoint);

  if (initializeNow            (pig) ||
      initializeYesterday      (pig) ||
      initializeToday          (pig) ||
      initializeTomorrow       (pig) ||
      initializeOrdinal        (pig) ||
      initializeDayName        (pig) ||
      initializeMonthName      (pig) ||
      initializeLater          (pig) ||
      initializeSopd           (pig) ||
      initializeSod            (pig) ||
      initializeSond           (pig) ||
      initializeEopd           (pig) ||
      initializeEod            (pig) ||
      initializeEond           (pig) ||
      initializeSopw           (pig) ||
      initializeSow            (pig) ||
      initializeSonw           (pig) ||
      initializeEopw           (pig) ||
      initializeEow            (pig) ||
      initializeEonw           (pig) ||
      initializeSopww          (pig) ||  // Must appear after sopw
      initializeSonww          (pig) ||  // Must appear after sonw
      initializeSoww           (pig) ||  // Must appear after sow
      initializeEopww          (pig) ||  // Must appear after eopw
      initializeEonww          (pig) ||  // Must appear after eonw
      initializeEoww           (pig) ||  // Must appear after eow
      initializeSopm           (pig) ||
      initializeSom            (pig) ||
      initializeSonm           (pig) ||
      initializeEopm           (pig) ||
      initializeEom            (pig) ||
      initializeEonm           (pig) ||
      initializeSopq           (pig) ||
      initializeSoq            (pig) ||
      initializeSonq           (pig) ||
      initializeEopq           (pig) ||
      initializeEoq            (pig) ||
      initializeEonq           (pig) ||
      initializeSopy           (pig) ||
      initializeSoy            (pig) ||
      initializeSony           (pig) ||
      initializeEopy           (pig) ||
      initializeEoy            (pig) ||
      initializeEony           (pig) ||
      initializeEaster         (pig) ||
      initializeMidsommar      (pig) ||
      initializeMidsommarafton (pig) ||
      initializeInformalTime   (pig))
  {
    return true;
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Valid epoch values are unsigned integers after 1980-01-01T00:00:00Z. This
// restriction means that '12' will not be identified as an epoch date.
bool Datetime::parse_epoch (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  long long epoch {};
  if (pig.getDigits (epoch)             &&
      ! unicodeLatinAlpha (pig.peek ()) &&
      epoch >= EPOCH_MIN_VALUE          &&
      epoch < EPOCH_MAX_VALUE)
  {
    _date = static_cast <time_t> (epoch);
    return true;
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// date_ext 'T' time_utc_ext 'Z'
// date_ext 'T' time_off_ext
// date_ext 'T' time_ext
bool Datetime::parse_date_time_ext (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (parse_date_ext (pig) &&
      pig.skip ('T')       &&
      (parse_time_utc_ext (pig) ||
       parse_time_off_ext (pig) ||
       parse_time_ext     (pig)))
  {
    return true;
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// YYYY-MM-DD
// YYYY-MM
// YYYY-DDD
// YYYY-Www-D
// YYYY-Www
bool Datetime::parse_date_ext (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  int year {};
  if (parse_year (pig, year) &&
      pig.skip ('-'))
  {
    auto checkpointYear = pig.cursor ();

    int month {};
    int day {};
    int julian {};
    int week {};
    int weekday {};

    if (pig.skip ('W') &&
        parse_week (pig, week))
    {
      if (! (pig.skip ('-') &&
             parse_weekday (pig, weekday)))
      {
        weekday = Datetime::weekstart;
      }

      if (! unicodeLatinDigit (pig.peek ()))
      {
        _weekday = weekday;
        _week = week;
        _year = year;
        return true;
      }
    }

    pig.restoreTo (checkpointYear);

    if (parse_month (pig, month) &&
        pig.skip ('-')           &&
        parse_day (pig, day)     &&
        ! unicodeLatinDigit (pig.peek ()))
    {
      _year = year;
      _month = month;
      _day = day;
      return true;
    }

    pig.restoreTo (checkpointYear);

    if (parse_julian (pig, julian) &&
        ! unicodeLatinDigit (pig.peek ()))
    {
      _year = year;
      _julian = julian;
      return true;
    }

    pig.restoreTo (checkpointYear);

    if (parse_month (pig, month) &&
        pig.peek () != '-'       &&
        ! unicodeLatinDigit (pig.peek ()))
    {
      _year = year;
      _month = month;
      _day = 1;
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// ±hh[:mm]
bool Datetime::parse_off_ext (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  int sign = pig.peek ();
  if (sign == '+' || sign == '-')
  {
    pig.skipN (1);

    int hour {0};
    int minute {0};

    if (parse_off_hour (pig, hour))
    {
      if (pig.skip (':'))
      {
        if (! parse_off_minute (pig, minute))
        {
          pig.restoreTo (checkpoint);
          return false;
        }
      }

      _offset = (hour * 3600) + (minute * 60);
      if (sign == '-')
        _offset = - _offset;

      if (! unicodeLatinDigit (pig.peek ()))
        return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// hh:mm[:ss]
bool Datetime::parse_time_ext (Pig& pig, bool terminated)
{
  auto checkpoint = pig.cursor ();

  int hour {};
  int minute {};
  if (parse_hour (pig, hour) &&
      pig.skip (':')         &&
      parse_minute (pig, minute))
  {
    if (pig.skip (':'))
    {
      int second {};
      if (parse_second (pig, second) &&
          ! unicodeLatinDigit (pig.peek ()) &&
          (! terminated || (pig.peek () != '-' && pig.peek () != '+')))
      {
        _seconds = (hour * 3600) + (minute * 60) + second;
        return true;
      }

      pig.restoreTo (checkpoint);
      return false;
    }

    auto following = pig.peek ();
    if (! unicodeLatinDigit (following)    &&
        (! terminated || (following != '+' && following != '-')) &&
        following != 'A'                   &&
        following != 'a'                   &&
        following != 'P'                   &&
        following != 'p')
    {
      _seconds = (hour * 3600) + (minute * 60);
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// time-ext 'Z'
bool Datetime::parse_time_utc_ext (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (parse_time_ext (pig, false) &&
      pig.skip ('Z'))
  {
    if (! unicodeLatinDigit (pig.peek ()))
    {
      _utc = true;
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// time-ext off-ext
bool Datetime::parse_time_off_ext (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (parse_time_ext (pig, false) &&
      parse_off_ext (pig))
  {
    return true;
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// YYYYMMDDTHHMMSSZ
// YYYYMMDDTHHMMSS
bool Datetime::parse_date_time (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (parse_date (pig) &&
      pig.skip ('T')   &&
      (parse_time_utc (pig) ||
       parse_time_off (pig) ||
       parse_time     (pig)))
  {
    return true;
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// YYYYWww
// YYYYDDD
// YYYYMMDD
// YYYYMM
bool Datetime::parse_date (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  int year {};
  int month {};
  int julian {};
  int week {};
  int weekday {};
  int day {};
  if (parse_year (pig, year))
  {
    auto checkpointYear = pig.cursor ();

    if (pig.skip ('W') &&
        parse_week (pig, week))
    {
      if (! (parse_weekday (pig, weekday)))
        weekday = Datetime::weekstart;

      if (! unicodeLatinDigit (pig.peek ()))
      {
        _year = year;
        _week = week;
        _weekday = weekday;
        return true;
      }
    }

    pig.restoreTo (checkpointYear);

    if (parse_julian (pig, julian) &&
        ! unicodeLatinDigit (pig.peek ()))
    {
      _year = year;
      _julian = julian;
      return true;
    }

    pig.restoreTo (checkpointYear);

    if (parse_month (pig, month))
    {
      if (parse_day (pig, day))
      {
        if (! unicodeLatinDigit (pig.peek ()))
        {
          _year = year;
          _month = month;
          _day = day;
          return true;
        }
      }
      else
      {
        if (! unicodeLatinDigit (pig.peek ()))
        {
          _year = year;
          _month = month;
          _day = 1;
          return true;
        }
      }
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// <time> Z
bool Datetime::parse_time_utc (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (parse_time (pig, false) &&
      pig.skip ('Z'))
  {
    _utc = true;
    if (! unicodeLatinDigit (pig.peek ()))
      return true;
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// <time><off>
bool Datetime::parse_time_off (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (parse_time (pig, false) &&
      parse_off (pig))
  {
    auto terminator = pig.peek ();
    if (terminator != '-' && ! unicodeLatinDigit (terminator))
    {
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// hhmmss
// hhmm
bool Datetime::parse_time (Pig& pig, bool terminated)
{
  auto checkpoint = pig.cursor ();

  int hour {};
  int minute {};
  if (parse_hour (pig, hour) &&
      parse_minute (pig, minute))
  {
    int second {};
    parse_second (pig, second);

    auto terminator = pig.peek ();
    if (! terminated ||
        (! unicodeLatinDigit (terminator) && terminator != '-' && terminator != '+'))
    {
      _seconds = (hour * 3600) + (minute * 60) + second;
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// ±hhmm
// ±hh
bool Datetime::parse_off (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  int sign = pig.peek ();
  if (sign == '+' || sign == '-')
  {
    pig.skipN (1);

    int hour {};
    if (parse_off_hour (pig, hour))
    {
      int minute {};
      parse_off_minute (pig, minute);

      if (! unicodeLatinDigit (pig.peek ()))
      {
        _offset = (hour * 3600) + (minute * 60);
        if (sign == '-')
          _offset = - _offset;

        return true;
      }
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::parse_year (Pig& pig, int& value)
{
  auto checkpoint = pig.cursor ();

  int year;
  if (pig.getDigit4 (year) &&
      year > 1969)
  {
    value = year;
    return true;
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::parse_month (Pig& pig, int& value)
{
  auto checkpoint = pig.cursor ();

  int month;
  if (pig.getDigit2 (month) &&
      month > 0             &&
      month <= 12)
  {
    value = month;
    return true;
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::parse_week (Pig& pig, int& value)
{
  auto checkpoint = pig.cursor ();

  int week;
  if (pig.getDigit2 (week) &&
      week > 0             &&
      week <= 53)
  {
    value = week;
    return true;
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::parse_julian (Pig& pig, int& value)
{
  auto checkpoint = pig.cursor ();

  int julian;
  if (pig.getDigit3 (julian) &&
      julian > 0             &&
      julian <= 366)
  {
    value = julian;
    return true;
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::parse_day (Pig& pig, int& value)
{
  auto checkpoint = pig.cursor ();

  int day;
  if (pig.getDigit2 (day) &&
      day > 0             &&
      day <= 31)
  {
    value = day;
    return true;
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::parse_weekday (Pig& pig, int& value)
{
  auto checkpoint = pig.cursor ();

  int weekday;
  if (pig.getDigit (weekday) &&
      weekday >= Datetime::weekstart &&
      weekday <= (Datetime::weekstart == 1 ? 7 : 6))
  {
    value = weekday;
    return true;
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::parse_hour (Pig& pig, int& value)
{
  auto checkpoint = pig.cursor ();

  int hour;
  if (pig.getDigit2 (hour) &&
      hour >= 0            &&
      hour < 24)
  {
    value = hour;
    return true;
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::parse_minute (Pig& pig, int& value)
{
  auto checkpoint = pig.cursor ();

  int minute;
  if (pig.getDigit2 (minute) &&
      minute >= 0            &&
      minute < 60)
  {
    value = minute;
    return true;
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::parse_second (Pig& pig, int& value)
{
  auto checkpoint = pig.cursor ();

  int second;
  if (pig.getDigit2 (second) &&
      second >= 0            &&
      second < 60)
  {
    value = second;
    return true;
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::parse_off_hour (Pig& pig, int& value)
{
  auto checkpoint = pig.cursor ();

  int hour;
  if (pig.getDigit2 (hour) &&
      hour >= 0            &&
      hour <= 12)
  {
    value = hour;
    return true;
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::parse_off_minute (Pig& pig, int& value)
{
  auto checkpoint = pig.cursor ();

  int minute;
  if (pig.getDigit2 (minute) &&
      minute >= 0            &&
      minute < 60)
  {
    value = minute;
    return true;
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// now [ !<alpha> && !<digit> ]
bool Datetime::initializeNow (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("now"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = to_epoch (now_seconds ());
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// yesterday/abbrev  [ !<alpha> && !<digit> ]
bool Datetime::initializeYesterday (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  std::string token;
  if (pig.skipPartial ("yesterday", token) &&
      token.length () >= static_cast <std::string::size_type> (Datetime::minimumMatchLength))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (today () - chrono::days{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// today/abbrev  [ !<alpha> && !<digit> ]
bool Datetime::initializeToday (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  std::string token;
  if (pig.skipPartial ("today", token) &&
      token.length () >= static_cast <std::string::size_type> (Datetime::minimumMatchLength))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (today ());

      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// tomcorrow/abbrev  [ !<alpha> && !<digit> ]
bool Datetime::initializeTomorrow (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  std::string token;
  if (pig.skipPartial ("tomorrow", token) &&
      token.length () >= static_cast <std::string::size_type> (Datetime::minimumMatchLength))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (today () + chrono::days{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// <digit>+ [ "st" | "nd" | "rd" | "th" ] [ !<alpha> && !<digit> ]
bool Datetime::initializeOrdinal (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  long long number = 0;
  if (pig.getDigits (number) &&
      number > 0             &&
      number <= 31)
  {
    int character1;
    int character2;
    if (pig.getCharacter (character1)     &&
        pig.getCharacter (character2)     &&
        ! unicodeLatinAlpha (pig.peek ()) &&
        ! unicodeLatinDigit (pig.peek ()))
    {
      int remainder1 = number % 10;
      int remainder2 = number % 100;
      if ((remainder2 != 11 && remainder1 == 1 && character1 == 's' && character2 == 't') ||
          (remainder2 != 12 && remainder1 == 2 && character1 == 'n' && character2 == 'd') ||
          (remainder2 != 13 && remainder1 == 3 && character1 == 'r' && character2 == 'd') ||
          ((remainder2 == 11 ||
            remainder2 == 12 ||
            remainder2 == 13 ||
            remainder1 == 0 ||
            remainder1 > 3) && character1 == 't' && character2 == 'h'))
      {
        const auto date = chrono::year_month_day{today ()};
        auto month = date.year () / date.month ();
        const int day = unsigned(date.day ());
        if (timeRelative && number <= day)
          month += chrono::months{1};
        else if (!timeRelative && day < number &&
                 number <= unsigned((month / chrono::last).day ()))
          month -= chrono::months{1};

        _date = local_epoch (calendar_day (month, static_cast<int> (number)));

        return true;
      }
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// sunday/abbrev [ !<alpha> && !<digit> && !: && != ]
bool Datetime::initializeDayName (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  std::string token;
  for (int day = 0; day <= 7; ++day)   // Deliberate <= so that 'sunday' is either 0 or 7.
  {
    if (pig.skipPartial (dayNames[day % 7], token, true) &&
        token.length () >= static_cast <std::string::size_type> (Datetime::minimumMatchLength))
    {
      auto following = pig.peek ();
      if (! unicodeLatinAlpha (following) &&
          ! unicodeLatinDigit (following) &&
          following != ':' &&
          following != '=')
      {
        const auto current = today ();
        const int weekday = chrono::weekday{current}.c_encoding ();
        auto offset = chrono::days{day - weekday};
        if (weekday >= day && timeRelative)
          offset += chrono::weeks{1};
        else if (weekday < day && !timeRelative)
          offset -= chrono::weeks{1};

        _date = local_epoch (current + offset);
        return true;
      }
    }

    pig.restoreTo (checkpoint);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// january/abbrev [ !<alpha> && !<digit> && !: && != ]
bool Datetime::initializeMonthName (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  std::string token;
  for (int month = 0; month < 12; ++month)
  {
    if (pig.skipPartial (monthNames[month], token, true) &&
        token.length () >= static_cast <std::string::size_type> (Datetime::minimumMatchLength))
    {
      auto following = pig.peek ();
      if (! unicodeLatinAlpha (following) &&
          ! unicodeLatinDigit (following) &&
          following != ':' &&
          following != '=')
      {
        const auto date = chrono::year_month_day{today ()};
        auto year = date.year ();
        const auto target = chrono::January + chrono::months{month};
        if (date.month () >= target && timeRelative)
          year += chrono::years{1};
        _date = local_epoch (chrono::local_days{year / target / 1});
        return true;
      }
    }

    pig.restoreTo (checkpoint);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// later/abbrev  [ !<alpha> && !<digit> ]
// someday/abbrev  [ !<alpha> && !<digit> ]
bool Datetime::initializeLater (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  std::string token;
  if ((pig.skipPartial ("later", token) &&
      token.length () >= static_cast <std::string::size_type> (Datetime::minimumMatchLength))

      ||

     (pig.skipPartial ("someday", token) &&
      token.length () >= static_cast <std::string::size_type> (std::max (Datetime::minimumMatchLength, 4))))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (calendar_day (9999, 12, 30));
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// sopd [ !<alpha> && !<digit> ]
bool Datetime::initializeSopd (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("sopd"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (today () - chrono::days{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// sod [ !<alpha> && !<digit> ]
bool Datetime::initializeSod (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("sod"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (today ());
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// sond [ !<alpha> && !<digit> ]
bool Datetime::initializeSond (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("sond"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (today () + chrono::days{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// eopd [ !<alpha> && !<digit> ]
bool Datetime::initializeEopd (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("eopd"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (today () - chrono::seconds{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// eod [ !<alpha> && !<digit> ]
bool Datetime::initializeEod (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("eod"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (today () + chrono::days{1} - chrono::seconds{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// eond [ !<alpha> && !<digit> ]
bool Datetime::initializeEond (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("eond"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (today () + chrono::days{2} - chrono::seconds{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// sopw [ !<alpha> && !<digit> ]
bool Datetime::initializeSopw (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("sopw"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (monday_of (today ()) - chrono::weeks{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// sow [ !<alpha> && !<digit> ]
bool Datetime::initializeSow (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("sow"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (monday_of (today ()));
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// sonw [ !<alpha> && !<digit> ]
bool Datetime::initializeSonw (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("sonw"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (monday_of (today ()) + chrono::weeks{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// eopw [ !<alpha> && !<digit> ]
bool Datetime::initializeEopw (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("eopw"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (monday_of (today ()) - chrono::seconds{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// eow [ !<alpha> && !<digit> ]
bool Datetime::initializeEow (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("eow"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (monday_of (today ()) + chrono::weeks{1}
                           - chrono::seconds{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// eonw [ !<alpha> && !<digit> ]
bool Datetime::initializeEonw (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("eonw"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (monday_of (today ()) + chrono::weeks{2}
                           - chrono::seconds{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// sopww [ !<alpha> && !<digit> ]
bool Datetime::initializeSopww (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("sopww"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      const auto day = today ();
      const int weekday = chrono::weekday{day}.c_encoding ();
      _date = local_epoch (day + chrono::days{-6 - weekday});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// soww [ !<alpha> && !<digit> ]
bool Datetime::initializeSoww (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("soww"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      const auto day = today ();
      const int weekday = chrono::weekday{day}.c_encoding ();
      _date = local_epoch (day + chrono::days{1 - weekday});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// sonww [ !<alpha> && !<digit> ]
bool Datetime::initializeSonww (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("sonww"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      const auto day = today ();
      const int weekday = chrono::weekday{day}.c_encoding ();
      _date = local_epoch (day + chrono::days{8 - weekday});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// eopww [ !<alpha> && !<digit> ]
bool Datetime::initializeEopww (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("eopww"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      const auto day = today ();
      const int weekday = chrono::weekday{day}.c_encoding ();
      _date = local_epoch (day + chrono::days{-1 - weekday}
                           - chrono::seconds{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// eoww [ !<alpha> && !<digit> ]
bool Datetime::initializeEoww (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("eoww"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      const auto day = today ();
      const int weekday = chrono::weekday{day}.c_encoding ();
      _date = local_epoch (day + chrono::days{6 - weekday}
                               - chrono::seconds{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// eonww [ !<alpha> && !<digit> ]
bool Datetime::initializeEonww (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("eonww"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      const auto day = today ();
      const int weekday = chrono::weekday{day}.c_encoding ();
      _date = local_epoch (day + chrono::days{13 - weekday}
                               - chrono::seconds{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// sopm [ !<alpha> && !<digit> ]
bool Datetime::initializeSopm (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("sopm"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (month_start (-1));
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// som [ !<alpha> && !<digit> ]
bool Datetime::initializeSom (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("som"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (month_start (0));
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// sonm [ !<alpha> && !<digit> ]
bool Datetime::initializeSonm (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("sonm"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (month_start (1));
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// eopm [ !<alpha> && !<digit> ]
bool Datetime::initializeEopm (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("eopm"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (month_start (0) - chrono::seconds{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// eom [ !<alpha> && !<digit> ]
bool Datetime::initializeEom (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("eom"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (month_start (1) - chrono::seconds{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// eonm [ !<alpha> && !<digit> ]
bool Datetime::initializeEonm (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("eonm"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (month_start (2) - chrono::seconds{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// sopq [ !<alpha> && !<digit> ]
bool Datetime::initializeSopq (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("sopq"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (quarter_start (-1));
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// soq [ !<alpha> && !<digit> ]
bool Datetime::initializeSoq (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("soq"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (quarter_start (0));
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// sonq [ !<alpha> && !<digit> ]
bool Datetime::initializeSonq (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("sonq"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (quarter_start (1));
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// eopq [ !<alpha> && !<digit> ]
bool Datetime::initializeEopq (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("eopq"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (quarter_start (0) - chrono::seconds{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// eoq [ !<alpha> && !<digit> ]
bool Datetime::initializeEoq (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("eoq"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (quarter_start (1) - chrono::seconds{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// eonq [ !<alpha> && !<digit> ]
bool Datetime::initializeEonq (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("eonq"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (quarter_start (2) - chrono::seconds{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// sopy [ !<alpha> && !<digit> ]
bool Datetime::initializeSopy (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("sopy"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (year_start (-1));
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// soy [ !<alpha> && !<digit> ]
bool Datetime::initializeSoy (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("soy"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (year_start (0));
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// sony [ !<alpha> && !<digit> ]
bool Datetime::initializeSony (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("sony"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (year_start (1));
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// eopy [ !<alpha> && !<digit> ]
bool Datetime::initializeEopy (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("eopy"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (year_start (0) - chrono::seconds{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// eoy [ !<alpha> && !<digit> ]
bool Datetime::initializeEoy (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("eoy"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (year_start (1) - chrono::seconds{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// eony [ !<alpha> && !<digit> ]
bool Datetime::initializeEony (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("eony"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      _date = local_epoch (year_start (2) - chrono::seconds{1});
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

namespace
{
auto easter_day (chrono::year year) -> chrono::local_days
{
  const int Y = int(year);
  int a = Y % 19;
  int b = Y / 100;
  int c = Y % 100;
  int d = b / 4;
  int e = b % 4;
  int f = (b + 8) / 25;
  int g = (b - f + 1) / 3;
  int h = (19 * a + b - d - g + 15) % 30;
  int i = c / 4;
  int k = c % 4;
  int L = (32 + 2 * e + 2 * i - h - k) % 7;
  int m = (a + 11 * h + 22 * L) / 451;
  int month = (h + L - 7 * m + 114) / 31;
  int day = ((h + L - 7 * m + 114) % 31) + 1;

  return calendar_day (Y, month, day);
}

auto midsummer_epoch (chrono::year year, int day, chrono::weekday weekday)
  -> time_t
{
  const auto date = chrono::local_days{year / chrono::June / day};
  const auto anchor = local_epoch (date);
  const auto fields = local_fields (anchor);
  const auto local = local_time (fields);
  const auto current = chrono::weekday{chrono::floor<chrono::days> (local)};
  const int offset = int(weekday.c_encoding ()) - int(current.c_encoding ());
  return local_epoch (local + chrono::days{offset}, fields.tm_isdst);
}
} // namespace

////////////////////////////////////////////////////////////////////////////////
// easter       [ !<alpha> && !<digit> ]
// eastermonday [ !<alpha> && !<digit> ]
// ascension    [ !<alpha> && !<digit> ]
// pentecost    [ !<alpha> && !<digit> ]
// goodfriday   [ !<alpha> && !<digit> ]
bool Datetime::initializeEaster (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  std::vector <std::string> holidays = {"eastermonday", "easter", "ascension", "pentecost", "goodfriday"};
  std::vector <int>         offsets  = {             1,        0,          39,          49,           -2};

  std::string token;
  for (int holiday = 0; holiday < 5; ++holiday)
  {
   if (pig.skipLiteral (holidays[holiday]) &&
       ! unicodeLatinAlpha (pig.peek ()) &&
       ! unicodeLatinDigit (pig.peek ()))
    {
      const auto now = to_epoch (now_seconds ());
      const auto year = chrono::year_month_day{
        chrono::floor<chrono::days> (local_time (now))}.year ();
      const auto offset = chrono::days{offsets[holiday]};
      const auto easter = local_epoch (easter_day (year));

      if (easter < now)
        _date = local_epoch (easter_day (year + chrono::years{1}) + offset);
      else
      {
        const auto fields = local_fields (easter);
        _date = local_epoch (local_time (fields) + offset, fields.tm_isdst);
      }
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// midsommar [ !<alpha> && !<digit> ]
bool Datetime::initializeMidsommar (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("midsommar"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      const auto now = to_epoch (now_seconds ());
      const auto year = chrono::year_month_day{
        chrono::floor<chrono::days> (local_time (now))}.year ();
      _date = midsummer_epoch (year, 20, chrono::Saturday);
      if (_date < now)
        _date = midsummer_epoch (year + chrono::years{1}, 20, chrono::Saturday);
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// midsommarafton [ !<alpha> && !<digit> ]
// juhannus [ !<alpha> && !<digit> ]
bool Datetime::initializeMidsommarafton (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  if (pig.skipLiteral ("midsommarafton") ||
      pig.skipLiteral ("juhannus"))
  {
    auto following = pig.peek ();
    if (! unicodeLatinAlpha (following) &&
        ! unicodeLatinDigit (following))
    {
      const auto now = to_epoch (now_seconds ());
      const auto year = chrono::year_month_day{
        chrono::floor<chrono::days> (local_time (now))}.year ();
      _date = midsummer_epoch (year, 19, chrono::Friday);
      if (_date < now)
        _date = midsummer_epoch (year + chrono::years{1}, 19, chrono::Friday);
      return true;
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// 8am
// 8a
// 8:30am
// 8:30a
// 8:30
// 8:30:00
//
// \d+ [ : \d{2} ] [ am | a | pm | p ] [ !<alpha> && !<digit> && !: && !+ && !- ]
//
bool Datetime::initializeInformalTime (Pig& pig)
{
  auto checkpoint = pig.cursor ();

  int digit = 0;
  bool needDesignator = true;   // Require am/pm.
  bool haveDesignator = false;  // Provided am/pm.
  if (pig.getDigit (digit))
  {
    int hours = digit;
    if (pig.getDigit (digit))
      hours = 10 * hours + digit;

    int minutes = 0;
    long long seconds = 0;
    if (pig.skip (':'))
    {
      if (! pig.getDigit2 (minutes))
      {
        pig.restoreTo (checkpoint);
        return false;
      }

      if (pig.skip (':'))
      {
        if (! pig.getDigits (seconds))
        {
          pig.restoreTo (checkpoint);
          return false;
        }
      }

      needDesignator = false;
    }

    if (pig.skipLiteral ("am") ||
        pig.skipLiteral ("a"))
    {
      haveDesignator = true;
      if (hours == 12)
        hours = 0;
    }

    else if (pig.skipLiteral ("pm") ||
             pig.skipLiteral ("p"))
    {
      // Note: '12pm is an exception:
      //  12am = 0h
      //  11am = 11h + 12h
      //  12pm = 12h
      //  1pm  = 1h + 12h
      if (hours != 12)
        hours += 12;

      haveDesignator = true;
    }

    // Informal time needs to be terminated.
    auto following = pig.peek ();
    if (unicodeLatinAlpha (following) ||
        unicodeLatinDigit (following) ||
        following == ':'              ||
        following == '-'              ||
        following == '+')
    {
      pig.restoreTo (checkpoint);
      return false;
    }

    if (haveDesignator || ! needDesignator)
    {
      // Midnight today + hours:minutes:seconds.
      const auto now = local_time (to_epoch (now_seconds ()));
      auto day = chrono::floor<chrono::days> (now);
      const auto clock = chrono::hours{hours} + chrono::minutes{minutes}
                       + chrono::seconds{seconds};
      if (Datetime::timeRelative && day + clock < now)
        day += chrono::days{1};

      // Basic validation.
      if (hours   >= 0 && hours   < 24 &&
          minutes >= 0 && minutes < 60 &&
          seconds >= 0 && seconds < 60)
      {
        _date = local_epoch (day + clock);

        return true;
      }
    }
  }

  pig.restoreTo (checkpoint);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Suggested date expressions:
//   {ordinal} {day} in|of {month}
//   last|past|next|this {day}
//   last|past|next|this {month}
//   last|past|next|this week
//   last|past|next|this month
//   last|past|next|this weekend
//   last|past|next|this year
//   {day} last|past|next|this week
//   {day} [at] {time}
//   {time} {day}
//
// Candidates:
//   <dayname> <time>
//   <time>
//   tue 9am
//   Friday before easter
//   3 days before eom
//   in the morning
//   am|pm
//   4pm
//   noon
//   midnight
//   tomorrow in one year
//   in two weeks
//   2 weeks from now
//   2 weeks ago tuesday
//   thursday in 2 weeks
//   last day next month
//   10 days from today
//   thursday before last weekend in may
//   friday last full week in may
//   3rd wednesday this month
//   3 weeks after 2nd tuesday next month
//   100 days from the beginning of the month
//   10 days after last monday
//   sunday in the evening
//   in 6 hours
//   6 in the morning
//   kl 18
//   feb 11
//   11 feb
//   2011-02-08
//   11/19/2011
//   next business day
//   new moon
//   full moon
//   in 28 days
//   3rd quarter
//   week 23
//   {number} {unit}
//   - {number} {unit}
//   {ordinal} {unit} in {larger-unit}
//   end of day tomorrow
//   end of {day}
//   by {day}
//   first thing {day}
//

////////////////////////////////////////////////////////////////////////////////
// <ordinal> <weekday> in|of <month>
bool Datetime::initializeNthDayInMonth (const std::vector <std::string>& tokens)
{
  if (tokens.size () == 4)
  {
    int ordinal {0};
    if (isOrdinal (tokens[0], ordinal))
    {
      auto day = Datetime::dayOfWeek (tokens[1]);
      if (day != -1)
      {
        if (tokens[2] == "in" ||
            tokens[2] == "of")
        {
          auto month = Datetime::monthOfYear (tokens[3]);
          if (month != -1)
          {
            std::println (std::cout, "# ordinal={} day={} in month={}", ordinal, day, month);

            // TODO Assume 1st of the month
            // TODO Assume current year
            // TODO Determine the day
            // TODO Project forwards/backwards, to the desired day
            // TODO Add ((ordinal - 1) * 7) days

            return true;
          }
        }
      }
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::isOrdinal (const std::string& token, int& ordinal)
{
  Pig p (token);
  long long number;
  std::string suffix;
  if (p.getDigits (number) &&
      p.getRemainder (suffix))
  {
    if ((number >= 11 && number <= 13 && suffix == "th") ||
        (number % 10 == 1             && suffix == "st") ||
        (number % 10 == 2             && suffix == "nd") ||
        (number % 10 == 3             && suffix == "rd") ||
        (                                suffix == "th"))
    {
      ordinal = number;
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Validation via simple range checking.
bool Datetime::validate ()
{
  // for ISO 8601 week specification: YYYY-Www-D where D is 1-7 (Mon-Sun)
  int wks = (Datetime::weekstart == 1 ? 1: 0);
  int wke = (Datetime::weekstart == 1 ? 7: 6);

  // _year;
  if ((_year    && (_year    <   1900 || _year    >                                  9999)) ||
      (_month   && (_month   <      1 || _month   >                                    12)) ||
      (_week    && (_week    <      1 || _week    >                                    53)) ||
      (true     && (_weekday <    wks || _weekday >                                   wke)) ||
      (_julian  && (_julian  <      1 || _julian  >          Datetime::daysInYear (_year))) ||
      (_day     && (_day     <      1 || _day     > Datetime::daysInMonth (_year, _month))) ||
      (_seconds && (_seconds <      1 || _seconds >                                 86400)) ||
      (_offset  && (_offset  < -86400 || _offset  >                                 86400)))
    return false;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
void Datetime::resolve ()
{
  // A UTC offset shifts the specified wall time and the reference day.
  auto seconds = chrono::seconds{_seconds} - chrono::seconds{_offset};
  const auto now = now_seconds () - chrono::seconds{_offset};
  const bool utc = _utc || _offset != 0;
  const auto reference = utc ? chrono::local_seconds{now.time_since_epoch ()}
                             : local_time (to_epoch (now));
  const auto reference_day = chrono::floor<chrono::days> (reference);

  if (Datetime::timeRelative &&
      _year == 0 && _month == 0 && _day == 0 && _week == 0 &&
      _weekday == Datetime::weekstart && seconds < reference - reference_day)
    seconds += chrono::days{1};

  auto day = reference_day;
  if (_week)
  {
    const auto jan4 = calendar_day (_year, 1, 4);
    if (Datetime::weekstart == 1)
      day = monday_of (jan4) + chrono::weeks{_week - 1}
                            + chrono::days{_weekday - 1};
    else
    {
      // Preserve Taskwarrior's Sunday-based week-date convention.
      const int jan4_weekday = chrono::weekday{jan4}.c_encoding ();
      const int ordinal = _week * 7 + _weekday - jan4_weekday - 3;
      day = ordinal ? calendar_day (_year, 1, ordinal)
                    : calendar_day (_year, _month, _day);
    }
  }
  else if (_julian)
  {
    const auto reference_date = chrono::year_month_day{reference_day};
    const int year = _year ? _year : int(reference_date.year ());
    day = calendar_day (year, 1, _julian);
  }
  else if (_year)
  {
    const int month = _month ? _month : 1;
    day = calendar_day (_year, month, _month && _day ? _day : 1);
  }

  const auto timestamp = day + seconds;
  _date = utc ? to_epoch (chrono::sys_seconds{timestamp.time_since_epoch ()})
              : local_epoch (timestamp);
}

////////////////////////////////////////////////////////////////////////////////
time_t Datetime::toEpoch () const
{
  return _date;
}

////////////////////////////////////////////////////////////////////////////////
std::string Datetime::toEpochString () const
{
  return std::format ("{}", _date);
}

////////////////////////////////////////////////////////////////////////////////
// 19980119T070000Z =  YYYYMMDDThhmmssZ
std::string Datetime::toISO () const
{
  return std::format ("{:%Y%m%dT%H%M%SZ}", from_epoch (_date));
}

////////////////////////////////////////////////////////////////////////////////
// 1998-01-19T07:00:00 =  YYYY-MM-DDThh:mm:ss
std::string Datetime::toISOLocalExtended () const
{
  return std::format ("{:%FT%T}", local_time (_date));
}

////////////////////////////////////////////////////////////////////////////////
double Datetime::toJulian () const
{
  using fractional_days = chrono::duration<double, chrono::days::period>;
  return fractional_days{from_epoch (_date).time_since_epoch ()}.count ()
       + 2440587.5;
}

////////////////////////////////////////////////////////////////////////////////
void Datetime::toYMD (int& y, int& m, int& d) const
{
  const auto date = chrono::year_month_day{local_day (_date)};
  y = int(date.year ());
  m = unsigned(date.month ());
  d = unsigned(date.day ());
}

////////////////////////////////////////////////////////////////////////////////
std::string Datetime::toString (const std::string& format) const
{
  std::stringstream formatted;
  for (unsigned int i = 0; i < format.length (); ++i)
  {
    int c = format[i];
    switch (c)
    {
    case 'm': formatted                                        << month ();               break;
    case 'M': formatted << std::setw (2) << std::setfill ('0') << month ();               break;
    case 'd': formatted                                        << day ();                 break;
    case 'D': formatted << std::setw (2) << std::setfill ('0') << day ();                 break;
    case 'y': formatted << std::setw (2) << std::setfill ('0') << (year () % 100);        break;
    case 'Y': formatted                                        << year ();                break;
    case 'a': formatted                                        << Datetime::dayNameShort (dayOfWeek ()); break;
    case 'A': formatted                                        << Datetime::dayName (dayOfWeek ());      break;
    case 'b': formatted                                        << Datetime::monthNameShort (month ());   break;
    case 'B': formatted                                        << Datetime::monthName (month ());        break;
    case 'v': formatted                                        << week ();                break;
    case 'V': formatted << std::setw (2) << std::setfill ('0') << week ();                break;
    case 'h': formatted                                        << hour ();                break;
    case 'H': formatted << std::setw (2) << std::setfill ('0') << hour ();                break;
    case 'i': formatted << ((hour () % 12) == 0 ? 12 : (hour () % 12));                break;
    case 'I': formatted << std::setw (2) << std::setfill ('0') << ((hour () % 12) == 0 ? 12 : (hour () % 12));                break;
    case 'p': formatted << (hour () < 12 ? "AM" : "PM");                break;
    case 'n': formatted                                        << minute ();              break;
    case 'N': formatted << std::setw (2) << std::setfill ('0') << minute ();              break;
    case 's': formatted                                        << second ();              break;
    case 'S': formatted << std::setw (2) << std::setfill ('0') << second ();              break;
    case 'j': formatted                                        << dayOfYear ();           break;
    case 'J': formatted << std::setw (3) << std::setfill ('0') << dayOfYear ();           break;
    case 'w': formatted                                        << dayOfWeek ();           break;
    default:  formatted                                        << static_cast <char> (c); break;
    }
  }

  return formatted.str ();
}

////////////////////////////////////////////////////////////////////////////////
Datetime Datetime::startOfDay () const
{
  return { local_epoch (local_day (_date)) };
}

////////////////////////////////////////////////////////////////////////////////
Datetime Datetime::startOfWeek () const
{
  // The existing week boundary is based on elapsed days before local midnight.
  const auto previous = from_epoch (_date) - chrono::days{dayOfWeek ()};
  return { local_epoch (local_day (to_epoch (previous))) };
}

////////////////////////////////////////////////////////////////////////////////
Datetime Datetime::startOfMonth () const
{
  const auto date = chrono::year_month_day{local_day (_date)};
  return { local_epoch (chrono::local_days{date.year () / date.month () / 1}) };
}

////////////////////////////////////////////////////////////////////////////////
Datetime Datetime::startOfYear () const
{
  const auto year = chrono::year_month_day{local_day (_date)}.year ();
  return { local_epoch (chrono::local_days{year / chrono::January / 1}) };
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::valid (const std::string& input, const std::string& format)
{
  try
  {
    Datetime test (input, format);
  }

  catch (...)
  {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::valid (
  const int y, const int m, const int d,
  const int hr, const int mi, const int se)
{
  if (hr < 0 || hr > 24)
    return false;

  if (mi < 0 || mi > 59)
    return false;

  if (se < 0 || se > 59)
    return false;

  if (hr == 24 &&
      (mi != 0 ||
       se != 0))
    return false;

  return Datetime::valid (y, m, d);
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::valid (const int y, const int m, const int d)
{
  if (y < 0 || m < 1 || m > 12 || d < 1 || d > 31)
    return false;

  // Gregorian leap years repeat every 400 years.
  return (chrono::year{y % 400} / m / d).ok ();
}

////////////////////////////////////////////////////////////////////////////////
// Julian
bool Datetime::valid (const int y, const int d)
{
  // Check that the year is valid.
  if (y < 0)
    return false;

  if (d < 1 || d > Datetime::daysInYear (y))
    return false;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// Static
bool Datetime::leapYear (int year)
{
  return chrono::year{year % 400}.is_leap ();
}

////////////////////////////////////////////////////////////////////////////////
// Static
int Datetime::daysInMonth (int year, int month)
{
  assert (month >= 1 && month <= 12);
  return unsigned((chrono::year{year % 400} / month / chrono::last).day ());
}

////////////////////////////////////////////////////////////////////////////////
// Static
int Datetime::daysInYear (int year)
{
  return Datetime::leapYear (year) ? 366 : 365;
}

////////////////////////////////////////////////////////////////////////////////
// Static
std::string Datetime::monthName (int month)
{
  assert (month > 0);
  assert (month <= 12);
  return upperCaseFirst (monthNames[month - 1]);
}

////////////////////////////////////////////////////////////////////////////////
// Static
std::string Datetime::monthNameShort (int month)
{
  assert (month > 0);
  assert (month <= 12);
  return upperCaseFirst (monthNames[month - 1]).substr (0, 3);
}

////////////////////////////////////////////////////////////////////////////////
// Static
std::string Datetime::dayName (int dow)
{
  assert (dow >= 0);
  assert (dow <= 6);
  return upperCaseFirst (dayNames[dow]);
}

////////////////////////////////////////////////////////////////////////////////
// Static
std::string Datetime::dayNameShort (int dow)
{
  assert (dow >= 0);
  assert (dow <= 6);
  return upperCaseFirst (dayNames[dow]).substr (0, 3);
}

////////////////////////////////////////////////////////////////////////////////
// Static
int Datetime::dayOfWeek (const std::string& input)
{
  if (Datetime::minimumMatchLength== 0)
    Datetime::minimumMatchLength = 3;

  for (unsigned int i = 0; i < dayNames.size (); ++i)
    if (closeEnough (dayNames[i], input, Datetime::minimumMatchLength))
       return i;

  return -1;
}

////////////////////////////////////////////////////////////////////////////////
// Static
int Datetime::dayOfWeek (int year, int month, int day)
{
  const auto date = calendar_day (year % 400, month, day);
  return chrono::weekday{date}.c_encoding ();
}

////////////////////////////////////////////////////////////////////////////////
// Static
int Datetime::monthOfYear (const std::string& input)
{
  if (Datetime::minimumMatchLength== 0)
    Datetime::minimumMatchLength = 3;

  for (unsigned int i = 0; i < monthNames.size (); ++i)
    if (closeEnough (monthNames[i], input, Datetime::minimumMatchLength))
       return i + 1;

  return -1;
}

////////////////////////////////////////////////////////////////////////////////
// Static
int Datetime::length (const std::string& format)
{
  int len = 0;
  for (auto& i : format)
  {
    switch (i)
    {
    case 'm':
    case 'M':
    case 'd':
    case 'D':
    case 'y':
    case 'v':
    case 'V':
    case 'h':
    case 'H':
    case 'i':
    case 'I':
    case 'p':
    case 'n':
    case 'N':
    case 's':
    case 'S': len += 2;  break;
    case 'b':
    case 'j':
    case 'J':
    case 'a': len += 3;  break;
    case 'Y': len += 4;  break;
    case 'A':
    case 'B': len += 10; break;

    // Calculate the width, don't assume a single character width.
    default:  len += mk_wcwidth (i); break;
    }
  }

  return len;
}

////////////////////////////////////////////////////////////////////////////////
int Datetime::month () const
{
  const auto date = chrono::year_month_day{local_day (_date)};
  return unsigned(date.month ());
}

////////////////////////////////////////////////////////////////////////////////
int Datetime::week () const
{
  const auto day = local_day (_date);
  if (Datetime::weekstart == 0)
    return std::stoi (std::format ("{:%U}", day));
  if (Datetime::weekstart == 1)
    return std::stoi (std::format ("{:%V}", day));
  throw std::string ("The week may only start on a Sunday or Monday.");
}

////////////////////////////////////////////////////////////////////////////////
int Datetime::day () const
{
  const auto date = chrono::year_month_day{local_day (_date)};
  return unsigned(date.day ());
}

////////////////////////////////////////////////////////////////////////////////
int Datetime::year () const
{
  const auto date = chrono::year_month_day{local_day (_date)};
  return int(date.year ());
}

////////////////////////////////////////////////////////////////////////////////
int Datetime::dayOfWeek () const
{
  return chrono::weekday{local_day (_date)}.c_encoding ();
}

////////////////////////////////////////////////////////////////////////////////
int Datetime::dayOfYear () const
{
  const auto day = local_day (_date);
  const auto year = chrono::year_month_day{day}.year ();
  return (day - chrono::local_days{year / chrono::January / 1}).count () + 1;
}

////////////////////////////////////////////////////////////////////////////////
int Datetime::hour () const
{
  return local_clock (_date).hours ().count ();
}

////////////////////////////////////////////////////////////////////////////////
int Datetime::minute () const
{
  return local_clock (_date).minutes ().count ();
}

////////////////////////////////////////////////////////////////////////////////
int Datetime::second () const
{
  return local_clock (_date).seconds ().count ();
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::operator== (const Datetime& rhs) const
{
  return rhs._date == _date;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::operator!= (const Datetime& rhs) const
{
  return rhs._date != _date;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::operator< (const Datetime& rhs) const
{
  return _date < rhs._date;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::operator> (const Datetime& rhs) const
{
  return _date > rhs._date;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::operator<= (const Datetime& rhs) const
{
  return _date <= rhs._date;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::operator>= (const Datetime& rhs) const
{
  return _date >= rhs._date;
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::sameHour (const Datetime& rhs) const
{
  return year ()  == rhs.year ()  &&
         month () == rhs.month () &&
         day ()   == rhs.day ()   &&
         hour ()  == rhs.hour ();
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::sameDay (const Datetime& rhs) const
{
  return year ()  == rhs.year ()  &&
         month () == rhs.month () &&
         day ()   == rhs.day ();
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::sameWeek (const Datetime& rhs) const
{
  return year () == rhs.year () &&
         week () == rhs.week ();
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::sameMonth (const Datetime& rhs) const
{
  return year ()  == rhs.year () &&
         month () == rhs.month ();
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::sameQuarter (const Datetime& rhs) const
{
  return year () == rhs.year () &&
         ((month () - 1) / 3) == ((rhs.month () - 1) / 3);
}

////////////////////////////////////////////////////////////////////////////////
bool Datetime::sameYear (const Datetime& rhs) const
{
  return year () == rhs.year ();
}

////////////////////////////////////////////////////////////////////////////////
Datetime Datetime::operator+ (const int64_t delta)
{
  return { to_epoch (from_epoch (_date) + chrono::seconds{delta}) };
}

////////////////////////////////////////////////////////////////////////////////
Datetime Datetime::operator- (const int64_t delta)
{
  return { to_epoch (from_epoch (_date) - chrono::seconds{delta}) };
}

////////////////////////////////////////////////////////////////////////////////
Datetime& Datetime::operator+= (const int64_t delta)
{
  _date = to_epoch (from_epoch (_date) + chrono::seconds{delta});
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
Datetime& Datetime::operator-= (const int64_t delta)
{
  _date = to_epoch (from_epoch (_date) - chrono::seconds{delta});
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
time_t Datetime::operator- (const Datetime& rhs)
{
  return (from_epoch (_date) - from_epoch (rhs._date)).count ();
}

////////////////////////////////////////////////////////////////////////////////
// Prefix decrement by one day.
void Datetime::operator-- ()
{
  _date = local_epoch (local_time (_date) - chrono::days{1});
}

////////////////////////////////////////////////////////////////////////////////
// Postfix decrement by one day.
void Datetime::operator-- (int)
{
  --(*this);
}

////////////////////////////////////////////////////////////////////////////////
// Prefix increment by one day.
void Datetime::operator++ ()
{
  _date = local_epoch (local_time (_date) + chrono::days{1});
}

////////////////////////////////////////////////////////////////////////////////
// Postfix increment by one day.
void Datetime::operator++ (int)
{
  ++(*this);
}

////////////////////////////////////////////////////////////////////////////////
