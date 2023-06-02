/** @file timestamp.h
 *
 *  @author Slinga
 *  @brief Date and timestamp
 *  @bug No known bugs.
 */
#pragma once

#include "../libslinga.h"

#define SECONDS_IN_MINUTE 60
#define MINUTES_IN_HOUR   60
#define HOURS_IN_DAY      24
#define DAYS_IN_WEEK      7
#define SECONDS_IN_HOUR   (SECONDS_IN_MINUTE * MINUTES_IN_HOUR)
#define SECONDS_IN_DAY    (SECONDS_IN_HOUR * HOURS_IN_DAY)
#define DAYS_IN_YEAR      365
#define DAYS_IN_LEAP_YEAR (DAYS_IN_YEAR + 1)

#define MAX_MONTH         12
#define MAX_DAY           31
#define MAX_HOUR          23
#define MAX_MINUTE        59

/** @brief Date struct */
typedef struct _BACKUP_DATE
{
    unsigned char year;           ///< @brief Year subtracted by 1980
    unsigned char month;          ///< @brief Month (1-12)
    unsigned char day;            ///< @brief Day of the month (1-31)
    unsigned char hour;           ///< @brief Hour (0-23)
    unsigned char minute;         ///< @brief Minute (0-59)
    unsigned char day_of_week;    ///< @brief Day of the week (Sunday 0 - Saturday 6)
} BACKUP_DATE, *PBACKUP_DATE;

SLINGA_ERROR Slinga_ConvertDateToTimestamp(const PBACKUP_DATE backup_date, unsigned int* seconds_since_1980);
SLINGA_ERROR Slinga_ConvertTimestampToDate(unsigned int timestamp, PBACKUP_DATE date);
