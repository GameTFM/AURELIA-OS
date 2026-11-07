/* AURELIA OS - CMOS real-time clock */
#pragma once

#include <aurelia/types.h>

typedef struct {
    u16 year;
    u8 month;
    u8 day;
    u8 hour;
    u8 minute;
    u8 second;
    u8 weekday;
} rtc_time_t;

void rtc_init(void);
void rtc_read(rtc_time_t *out);
const char *rtc_month_name(u8 month);
const char *rtc_weekday_name(u8 weekday);
void rtc_format_time(char *out, size_t size);
void rtc_format_date(char *out, size_t size);
