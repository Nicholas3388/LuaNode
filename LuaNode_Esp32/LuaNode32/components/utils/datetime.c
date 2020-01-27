/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*-
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *    The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *    from: Utah $Hdr: clock.c 1.18 91/01/21$
 *    from: @(#)clock.c    8.2 (Berkeley) 1/12/94
 *    from: NetBSD: clock_subr.c,v 1.6 2001/07/07 17:04:02 thorpej Exp
 *    and
 *    from: src/sys/i386/isa/clock.c,v 1.176 2001/09/04
 */

#include "datetime.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "os_time.h"

struct clocktime
{
    int year; /* year (4 digit year) */
    int mon;  /* month (1 - 12) */
    int day;  /* day (1 - 31) */
    int hour; /* hour (0 - 23) */
    int min;  /* minute (0 - 59) */
    int sec;  /* second (0 - 59) */
    int dow;  /* day of week (0 - 6; 0 = Sunday) */
    int usec; /* micro seconds */
};

#define days_in_year(y) (leapyear(y) ? 366 : 365)

#define FEBRUARY 2
#define days_in_month(y, m) (month_days[(m)-1] + (m == FEBRUARY ? leapyear(y) : 0))

/* Day of week. Days are counted from 1/1/1970, which was a Thursday */
#define day_of_week(days) (((days) + 4) % 7)

static const int month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

#define POSIX_BASE_YEAR 1970
#define SECDAY (24 * 60 * 60)

/*
 * This inline avoids some unnecessary modulo operations
 * as compared with the usual macro:
 *   ( ((year % 4) == 0 &&
 *      (year % 100) != 0) ||
 *     ((year % 400) == 0) )
 * It is otherwise equivalent.
 */
static int leapyear(int year)
{
    int rv = 0;

    if ((year & 3) == 0)
    {
        rv = 1;
        if ((year % 100) == 0)
        {
            rv = 0;
            if ((year % 400) == 0)
                rv = 1;
        }
    }
    return (rv);
}

int isLeapyear(int year) { return leapyear(year); }

static int clocktime_to_timeval(const struct clocktime *ct, struct os_timeval *tv)
{
    int i, year, days;

    year = ct->year;

    /* Sanity checks. */
    if (year < POSIX_BASE_YEAR || ct->mon < 1 || ct->mon > 12 || ct->day < 1 ||
        ct->day > days_in_month(year, ct->mon) || ct->hour < 0 || ct->hour > 23 || ct->min < 0 || ct->min > 59 ||
        ct->sec < 0 || ct->sec > 59 || ct->usec < 0 || ct->usec > 999999)
    {
        return (-1);
    }

    /*
     * Compute days since start of time
     * First from years, then from months.
     */
    days = 0;
    for (i = POSIX_BASE_YEAR; i < year; i++) days += days_in_year(i);

    /* Months */
    for (i = 1; i < ct->mon; i++) days += days_in_month(year, i);
    days += (ct->day - 1);

    tv->tv_sec = (((int64_t)days * 24 + ct->hour) * 60 + ct->min) * 60 + ct->sec;
    tv->tv_usec = ct->usec;

    return (0);
}

static int timeval_to_clocktime(const struct os_timeval *tv, const struct os_timezone *tz, struct clocktime *ct)
{
    int i, year, days;
    int64_t rsec; /* remainder seconds */
    int64_t secs;

    secs = tv->tv_sec;
    if (tz != NULL)
    {
        /* Convert utctime to localtime */
        secs -= tz->tz_minuteswest * 60;
        secs += tz->tz_dsttime ? 3600 : 0;
    }

    if (secs < 0 || tv->tv_usec < 0 || tv->tv_usec > 999999)
    {
        return (-1);
    }

    days = secs / SECDAY;
    rsec = secs % SECDAY;

    ct->dow = day_of_week(days);

    /* Subtract out whole years, counting them in i. */
    for (year = POSIX_BASE_YEAR; days >= days_in_year(year); year++) days -= days_in_year(year);
    ct->year = year;

    /* Subtract out whole months, counting them in i. */
    for (i = 1; days >= days_in_month(year, i); i++) days -= days_in_month(year, i);
    ct->mon = i;

    /* Days are what is left over (+1) from all that. */
    ct->day = days + 1;

    /* Hours, minutes, seconds are easy */
    ct->hour = rsec / 3600;
    rsec = rsec % 3600;
    ct->min = rsec / 60;
    rsec = rsec % 60;
    ct->sec = rsec;
    ct->usec = tv->tv_usec;

    return (0);
}

static const char *parse_number(const char *str, int digits, int *val)
{
    const char *cp;
    const char *end;

    *val = 0;
    cp = str;
    end = str + digits;
    while (cp < end)
    {
        if (!isdigit((int)*cp))
        {
            return (NULL);
        }
        *val *= 10;
        *val += (*cp - '0');
        cp++;
    }
    return (end);
}

/*
 * Parse the datetime string in RFC 3339 format. Some examples of valid
 * datetime strings are:
 * 2016-03-02T22:44:00                  UTC time (implicit)
 * 2016-03-02T22:44:00Z                 UTC time (explicit)
 * 2016-03-02T22:44:00-08:00            PST timezone
 * 2016-03-02T22:44:00.1                fractional seconds
 * 2016-03-02T22:44:00.101+05:30        fractional seconds with timezone
 */
int parse_datetime(const char *input, struct os_timeval *tv, struct os_timezone *tz)
{
    int digits, sign;
    int off_hour, off_min;
    const char *cp;
    const char *ep;
    struct clocktime ct;

    bzero(&ct, sizeof(struct clocktime));
    bzero(tv, sizeof(struct os_timeval));
    bzero(tz, sizeof(struct os_timezone)); /* default to UTC time */

    cp = input;
    cp = parse_number(cp, 4, &ct.year);
    if (cp == NULL || *cp != '-')
    {
        goto err;
    }

    cp = parse_number(cp + 1, 2, &ct.mon);
    if (cp == NULL || *cp != '-')
    {
        goto err;
    }

    cp = parse_number(cp + 1, 2, &ct.day);
    if (cp == NULL || *cp != 'T')
    {
        goto err;
    }

    cp = parse_number(cp + 1, 2, &ct.hour);
    if (cp == NULL || *cp != ':')
    {
        goto err;
    }

    cp = parse_number(cp + 1, 2, &ct.min);
    if (cp == NULL || *cp != ':')
    {
        goto err;
    }

    cp = parse_number(cp + 1, 2, &ct.sec);
    if (cp == NULL)
    {
        goto err;
    }

    /* parse fractional seconds if specified */
    if (*cp == '.')
    {
        ep = ++cp;
        while (isdigit((int)*ep)) { ep++; }
        digits = ep - cp;
        if (digits <= 0 || digits > 6)
        {
            goto err;
        }

        cp = parse_number(cp, digits, &ct.usec);
        if (cp == NULL)
        {
            goto err;
        }

        /*
         * The number of digits in the fractional seconds determines
         * the resolution.
         *
         * .1       1 part out of 10        100000  usec
         * .01      1 part out of 100       10000   usec
         * .001     1 part out of 1000      1000    usec
         * .0001    1 part out of 10000     100     usec
         * .00001   1 part out of 100000    10      usec
         * .000001  1 part out of 1000000   1       usec
         */
        while (digits++ < 6) { ct.usec *= 10; }
    }

    if (*cp == 'Z' || *cp == 'z')
    {
        cp++;
    }
    else if (*cp == '+' || *cp == '-')
    {
        sign = (*cp == '+') ? +1 : -1;
        cp = parse_number(cp + 1, 2, &off_hour);
        if (cp == NULL || *cp != ':')
        {
            goto err;
        }

        cp = parse_number(cp + 1, 2, &off_min);
        if (cp == NULL)
        {
            goto err;
        }

        if (off_hour < 0 || off_hour > 23 || off_min < 0 || off_min > 59)
        {
            goto err;
        }

        /*
         * Allow time zone offsets of up to 18 hours from the GMT.
         * https://docs.oracle.com/javase/8/docs/api/java/time/ZoneOffset.html
         */
        tz->tz_minuteswest = off_hour * 60 + off_min;
        if (tz->tz_minuteswest > 18 * 60)
        {
            goto err;
        }

        /*
         * Positive GMT offsets (i.e. timezones to the east of GMT) are
         * represented with a negative 'tz_minuteswest' value.
         */
        if (sign > 0)
        {
            tz->tz_minuteswest = -tz->tz_minuteswest;
        }
    }
    else
    {
        /*
         * 'time offset' is not specified so date/time defaults to UTC.
         */
    }

    if (*cp != '\0')
    {
        goto err;
    }

    if (clocktime_to_timeval(&ct, tv) != 0)
    {
        goto err;
    }

    /* Convert localtime to utctime */
    tv->tv_sec += tz->tz_minuteswest * 60;
    tv->tv_sec -= tz->tz_dsttime ? 3600 : 0;
    return (0);
err:
    return (-1);
}

int format_datetime(const struct os_timeval *tv, const struct os_timezone *tz, char *ostr, int olen)
{
    char *cp;
    int rc, rlen, minswest;
    int off_hour, off_min, sign;
    struct clocktime ct;

    rc = timeval_to_clocktime(tv, tz, &ct);
    if (rc != 0)
    {
        goto err;
    }

    cp = ostr;
    rlen = olen;

    rc = snprintf(cp, rlen, "%04d-%02d-%02dT%02d:%02d:%02d", ct.year, ct.mon, ct.day, ct.hour, ct.min, ct.sec);
    cp += rc;
    rlen -= rc;
    if (rc < 0 || rlen <= 0)
    {
        goto err;
    }

    if (ct.usec != 0)
    {
        rc = snprintf(cp, rlen, ".%06d", ct.usec);
        cp += rc;
        rlen -= rc;
        if (rc < 0 || rlen <= 0)
        {
            goto err;
        }
    }

    if (tz != NULL)
    {
        minswest = tz->tz_minuteswest;
        if (tz->tz_dsttime)
        {
            minswest -= 60;
        }
    }
    else
    {
        minswest = 0;
    }

    if (minswest < 0)
    {
        sign = '+';
        minswest = -minswest;
    }
    else
    {
        sign = '-';
    }

    off_hour = minswest / 60;
    off_min = minswest % 60;
    if (off_hour || off_min)
    {
        rc = snprintf(cp, rlen, "%c%02d:%02d", sign, off_hour, off_min);
        cp += rc;
        rlen -= rc;
        if (rc < 0 || rlen <= 0)
        {
            goto err;
        }
    }
    return (0);

err:
    return (-1);
}
