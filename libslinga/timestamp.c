/** @file timestamp.c
 *
 *  @author Slinga
 *  @brief Date and timestamp
 *  @bug No known bugs.
 */
#include "timestamp.h"

// Function prototypes
int is_leap_year(int year);
int days_in_month(int month, int year);

/**
 * @brief Convert backup date to backup timestamp (seconds since 1980)
 *
 * @param[in] date - filled out backup_date structure containing date to convert
 * @param[out] timestamp - seconds since 1980 on success. This is the value used by the BUP header and saves
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR Slinga_ConvertDateToTimestamp(const PBACKUP_DATE date, unsigned int* timestamp)
{
    if(!date)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // sanity check date
    if(date->month > MAX_MONTH || date->day > MAX_DAY ||
       date->hour > MAX_HOUR || date->minute > MAX_MINUTE)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    int seconds = 0;

    // Add seconds for years since 1980
    // account for leap year
    // year is stored as year-1980
    for (int y = 0; y < date->year; y++)
    {
        // year is stored as year-1980
        seconds += (is_leap_year(y + 1980) ? DAYS_IN_LEAP_YEAR : DAYS_IN_YEAR) * SECONDS_IN_DAY;
    }

    // Add seconds for months in the current year
    // months is 1-12, not 0-11
    for (int m = 1; m < date->month; m++)
    {
        seconds += days_in_month(m, date->year) * SECONDS_IN_DAY;
    }

    // Add seconds for days in the current month
    seconds += (date->day - 1) * SECONDS_IN_DAY;

    // Add seconds for hours and minutes
    seconds += date->hour * MINUTES_IN_HOUR + date->minute * SECONDS_IN_MINUTE;

    *timestamp = seconds;

    return SLINGA_SUCCESS;
}

/**
 * @brief Convert backup timestamp to backup date 
 *
 * @param[in] timestamp - seconds since 1980. This is the value used by the BUP header and saves
 * @param[out] date - name of backup device on success
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR Slinga_ConvertTimestampToDate(unsigned int timestamp, PBACKUP_DATE date)
{
    if(!date)
    {
        return SLINGA_INVALID_PARAMETER;
    }
        
    const int days_since_1980 = timestamp / SECONDS_IN_DAY;
    int day_of_week = (days_since_1980 + 2) % DAYS_IN_WEEK;

    int year = 1980;
    while (timestamp >= (is_leap_year(year) ? 366 : 365) * SECONDS_IN_DAY) {
        timestamp -= (is_leap_year(year) ? 366 : 365) * SECONDS_IN_DAY;
        year++;
    }

    int month = 1;
    while (timestamp >= days_in_month(month, year) * SECONDS_IN_DAY) {
        timestamp -= days_in_month(month, year) * SECONDS_IN_DAY;
        month++;
    }

    int day = timestamp / SECONDS_IN_DAY;
    timestamp %= SECONDS_IN_DAY;

    int hour = timestamp / SECONDS_IN_HOUR;
    timestamp %= SECONDS_IN_HOUR;

    int minute = timestamp / SECONDS_IN_MINUTE;
    timestamp %= SECONDS_IN_MINUTE;

    date->year = year - 1980;
    date->month = month;
    date->day = day;
    date->hour = hour;
    date->minute = minute;
    date->day_of_week = day_of_week;    

    return SLINGA_SUCCESS;
}

// returns true if year is a leap year
// A year is a leap year if:
//    It is divisible by 4, and
//    It is not divisible by 100, unless it is also divisible by 400
int is_leap_year(int year)
{
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

// returns number of days in the month, handling leap years
// month is 1-12, not 0-11
int days_in_month(int month, int year)
{
    const int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == 2 && is_leap_year(year))
    {
        return 29;
    }
    return days_in_month[month];
}

