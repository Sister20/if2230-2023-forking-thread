#ifndef _CMOSRTC_H
#define _CMOSRTC_H

#include "stdtype.h"

/* Using reference: https://wiki.osdev.org/CMOS 
    Register  Contents            Range
    0x00      Seconds             0–59
    0x02      Minutes             0–59
    0x04      Hours               0–23 in 24-hour mode, 
                                  1–12 in 12-hour mode, highest bit set if pm
    0x06      Weekday             1–7, Sunday = 1
    0x07      Day of Month        1–31
    0x08      Month               1–12
    0x09      Year                0–99
    0x32      Century (maybe)     19–20?
    0x0A      Status Register A
    0x0B      Status Register B
*/

#define CURRENT_YEAR        2023                            // Change this each year!
 
int32_t century_register = 0x00;                                // Set by ACPI table parsing code if possible

/**
 * @param second    Range: 0-59
 * @param minute    Range: 0-59
 * @param hour      Range: 0-23; uses 24 hour mode
 *
 * @param day       Day of month, Range: 1-31
 * @param month     Range: 1-12
 * @param year      Range: 0-99
 */
struct CMOSTRTC
{
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint32_t year;
} __attribute__((packed));

ReadFromCMOS (uint8_t array []);
WriteTOCMOS(uint8_t array[]);

void out_byte(int32_t port, int32_t value);
int32_t in_byte(int32_t port);
 
enum {
      cmos_address = 0x70,
      cmos_data    = 0x71
};
 
int32_t get_update_in_progress_flag();
 
uint8_t get_RTC_register(int32_t reg);
 
void read_rtc();

#endif