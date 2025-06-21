// File: LocalTimeHelper.h
#ifndef LOCAL_TIME_HELPER_H
#define LOCAL_TIME_HELPER_H

#include <WiFi.h>
#include <time.h>

class LocalTimeHelper
{
public:
    static void initNTP(const char *ntp1 = "pool.ntp.org", const char *ntp2 = "time.nist.gov", long gmtOffset_sec = 7 * 3600, int daylightOffset_sec = 0)
    {
        configTime(gmtOffset_sec, daylightOffset_sec, ntp1, ntp2);
    }

    static bool getTimeInfo(struct tm &timeinfo, int maxRetries = 20, int delayMs = 500)
    {
        int retry = 0;
        while (!getLocalTime(&timeinfo) && retry < maxRetries)
        {
            delay(delayMs);
            retry++;
        }
        return retry < maxRetries;
    }
};

#endif