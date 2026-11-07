/* AURELIA OS - CMOS RTC driver (MC146818 compatible) */
#include <aurelia/rtc.h>
#include <aurelia/arch.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

static bool g_ready;

static u8 cmos_read(u8 reg)
{
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

static i32 bcd_to_bin(u8 value)
{
    return (value & 0x0F) + ((value >> 4) * 10);
}

static bool update_in_progress(void)
{
    return (cmos_read(0x0A) & 0x80) != 0;
}

void rtc_init(void)
{
    u8 seconds = cmos_read(0x00);
    g_ready = (seconds != 0xFF);
    kprintf("[%s] CMOS RTC %s\n", g_ready ? "ok" : "warn",
            g_ready ? "responding" : "not present");
}

void rtc_read(rtc_time_t *out)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    u32 guard = 100000;
    while (update_in_progress() && guard--) {
    }
    u8 sec = cmos_read(0x00);
    u8 min = cmos_read(0x02);
    u8 hour = cmos_read(0x04);
    u8 day = cmos_read(0x07);
    u8 month = cmos_read(0x08);
    u8 year = cmos_read(0x09);
    u8 weekday = cmos_read(0x06);
    u8 century = cmos_read(0x32);
    u8 status_b = cmos_read(0x0B);

    bool bcd = (status_b & 0x04) == 0;
    bool hour12 = (status_b & 0x02) == 0;
    bool pm = (hour & 0x80) != 0;
    hour &= 0x7F;

    if (bcd) {
        sec = (u8)bcd_to_bin(sec);
        min = (u8)bcd_to_bin(min);
        hour = (u8)bcd_to_bin(hour);
        day = (u8)bcd_to_bin(day);
        month = (u8)bcd_to_bin(month);
        year = (u8)bcd_to_bin(year);
        weekday = (u8)bcd_to_bin(weekday);
        century = (u8)bcd_to_bin(century);
    }
    if (hour12) {
        hour = (u8)((hour % 12) + (pm ? 12 : 0));
    }

    out->second = sec;
    out->minute = min;
    out->hour = hour;
    out->day = day;
    out->month = month;
    out->weekday = weekday;
    u16 full_year = (u16)(year + (century ? (u16)century * 100 : 2000));
    if (full_year < 1970 || full_year > 2200) {
        full_year = 2026;
    }
    out->year = full_year;
}

const char *rtc_month_name(u8 month)
{
    static const char *names[] = {"", "January", "February", "March", "April", "May", "June",
                                  "July", "August", "September", "October", "November", "December"};
    return (month >= 1 && month <= 12) ? names[month] : "Unknown";
}

const char *rtc_weekday_name(u8 weekday)
{
    /* CMOS weekday is 1=Sunday..7=Saturday */
    static const char *names[] = {"", "Sunday", "Monday", "Tuesday", "Wednesday",
                                  "Thursday", "Friday", "Saturday"};
    return (weekday >= 1 && weekday <= 7) ? names[weekday] : "Unknown";
}

void rtc_format_time(char *out, size_t size)
{
    rtc_time_t now;
    rtc_read(&now);
    ksnprintf(out, size, "%02u:%02u", now.hour, now.minute);
}

void rtc_format_date(char *out, size_t size)
{
    rtc_time_t now;
    rtc_read(&now);
    ksnprintf(out, size, "%s, %u %s %u", rtc_weekday_name(now.weekday), now.day,
              rtc_month_name(now.month), now.year);
}
