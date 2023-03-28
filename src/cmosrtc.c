#include "lib-header/cmosrtc.h"

static struct CMOSRTC rtc;

uint8_t get_update_in_progress_flag() {
      out(cmos_address, 0x0A);
      return (in(cmos_data) & 0x80);
}
 
uint8_t get_RTC_register(uint8_t reg) {
      out(cmos_address, reg);
      return in(cmos_data);
}
 
void read_rtc() {
      uint8_t century;
      uint8_t last_second;
      uint8_t last_minute;
      uint8_t last_hour;
      uint8_t last_day;
      uint8_t last_month;
      uint8_t last_year;
      uint8_t last_century;
      uint8_t registerB;
 
      // Note: This uses the "read registers until you get the same values twice in a row" technique
      //       to avoid getting dodgy/inconsistent values due to RTC updates
 
      while (get_update_in_progress_flag());                // Make sure an update isn't in progress
      rtc.second = get_RTC_register(0x00);
      rtc.minute = get_RTC_register(0x02);
      rtc.hour = get_RTC_register(0x04);
      rtc.day = get_RTC_register(0x07);
      rtc.month = get_RTC_register(0x08);
      rtc.year = get_RTC_register(0x09);
 
      do {
            last_second = rtc.second;
            last_minute = rtc.minute;
            last_hour = rtc.hour;
            last_day = rtc.day;
            last_month = rtc.month;
            last_year = rtc.year;
            last_century = century;
 
            while (get_update_in_progress_flag());           // Make sure an update isn't in progress
            rtc.second = get_RTC_register(0x00);
            rtc.minute = get_RTC_register(0x02);
            rtc.hour = get_RTC_register(0x04);
            rtc.day = get_RTC_register(0x07);
            rtc.month = get_RTC_register(0x08);
            rtc.year = get_RTC_register(0x09);
            
      } while( (last_second != rtc.second) || (last_minute != rtc.minute) || (last_hour != rtc.hour) ||
               (last_day != rtc.day) || (last_month != rtc.month) || (last_year != rtc.year) ||
               (last_century != century) );
 
      registerB = get_RTC_register(0x0B);
 
      // Convert BCD to binary values if necessary
 
      if (!(registerB & 0x04)) {
            rtc.second = (rtc.second & 0x0F) + ((rtc.second / 16) * 10);
            rtc.minute = (rtc.minute & 0x0F) + ((rtc.minute / 16) * 10);
            rtc.hour = ( (rtc.hour & 0x0F) + (((rtc.hour & 0x70) / 16) * 10) ) | (rtc.hour & 0x80);
            rtc.day = (rtc.day & 0x0F) + ((rtc.day / 16) * 10);
            rtc.month = (rtc.month & 0x0F) + ((rtc.month / 16) * 10);
            rtc.year = (rtc.year & 0x0F) + ((rtc.year / 16) * 10);
      }
 
      // Convert 12 hour clock to 24 hour clock if necessary
 
      if (!(registerB & 0x02) && (rtc.hour & 0x80)) {
            rtc.hour = ((rtc.hour & 0x7F) + 12) % 24;
      }

}

struct CMOSRTC get_time() {
      read_rtc();
 
      // Calculate the full (4-digit) year

      rtc.year += (START_YEAR / 100) * 100;
      if(rtc.year < START_YEAR) rtc.year += 100;
      
      return rtc;
}

/**
 * Converts reference CMOS RTC timestamp format to Forking Thread's (FT) 32-bit timestamp format
 * format details as follow:
 * 0-4 = seconds bit
 * 5-10 = minutes bit
 * 11-15 = hours bit
 * 16-20 = days bit
 * 21-24 = months bit
 * 25-31 = years bit
*/
uint32_t CMOSRTC_to_FTTimestamp(struct CMOSRTC rtcTimestamp) {
      uint32_t FTTimestamp = 0x0;
      FTTimestamp |= (rtcTimestamp.year - START_YEAR) << 25;
      FTTimestamp |= rtcTimestamp.month << 21;
      FTTimestamp |= rtcTimestamp.day << 16;
      FTTimestamp |= rtcTimestamp.hour << 11;
      FTTimestamp |= rtcTimestamp.minute << 5;
      FTTimestamp != rtcTimestamp.second;

      return FTTimestamp;
}