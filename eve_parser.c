#include "eve_parser.h"

#define E_JDAY 719558       /* Julian Day of epoch (1970-1-1) */
#define SEC_PER_DAY 86400
#define SEC_PER_HOUR 3600
#define SEC_PER_MIN 60

uint32_t ejday(const uint32_t year, const uint32_t mon, const uint32_t d)
{
	return (year*365 + year/4 - year/100 + year/400 + (mon * 306 + 5) / 10
		+ d - 1 - E_JDAY) * SEC_PER_DAY;
}

static uint32_t convert_pt_utc(uint32_t pacificTime)
{
	/* Attempt to switch PT to UTC (including daylight savings time. :\) */
	/* YYYY-MM-DD-HH in PT (These are the date-times where daylight savings
	* time apparently switches (according to tzdump on my machine) within
	* the range of dates that we care about. */
	#define D2006040203 1144033200
	#define D2006102901 1162256400
	#define D2007031103 1173754800
	#define D2007110400 1194307200

	if (pacificTime < D2006040203) {
		return pacificTime + 8 * SEC_PER_HOUR;
	} else if (pacificTime < D2006102901) {
		return pacificTime + 7 * SEC_PER_HOUR;
	} else if (pacificTime < D2007031103) {
		return pacificTime + 8 * SEC_PER_HOUR;
	} else {
		/* Techincally this should be an else-if, but since this time
		* period covers the remaining window where the historical data
		* is reported in PT (rather than UTC later) it doesn't matter
		*/
		return pacificTime + 7 * SEC_PER_HOUR;
	}
}

/*
 * Return 0 on successful parsing and 1 on failure. (malformed input).
 *
 * Historical caveats: Buy order ranges incorrect, and time in Pacific.
 * Tenth - '0' > 5 (Checks the Most Significant Byte of the decimal portion
 * to round to the nearest whole second integer.)

 * Mapping 32767 -> -2 reduces range's solution space from 2 to 1 byte.
 * There's no specifier in C89 for int8_t, so we get some compiler warnings
 * with %u. In C99 we'd use %hhu.

 * There's a lot of code repition in these parse_v* functions, which could be
 * factored out to a seperate function, but I prefer the interface that's used
 * for clarity.
*/
int parse_v1(struct rawRecord *result)
{
	uint64_t orderid, price, reportedby;
	uint32_t regionid, systemid, stationid, typeid, priceTenths, volmin;
	uint32_t volrem, volent, rtimeDay, rtimeHour, rtimeMin, rtimeSec;
	uint32_t issuedYear, issuedMonth, issuedDay, issuedHour, issuedMin;
	uint32_t issuedSec, duration;
	int32_t range;
	int8_t bid;
	uint8_t issuedSecTenth, rtimeSecTenth;
	/* orderid, regionid, systemid, stationid, typeid, bid, price, volmin,
	 * volrem, volent, issued, duration, range, reportedby, rtime
	*/
	if (scanf("%llu , %u , %u , %u , %u , %c , %llu.%u , %u , %u , %u , "
		"%u-%u-%u %u:%u:%u.%c%*[^,], %u:%*u:%*u:%*u.%*c%*[^,], %u , "
		"%llu , %u-%u-%u %u:%u:%u.%c%*[^\n]\n",
		&orderid, &regionid, &systemid, &stationid, &typeid, &bid,
		&price, &priceTenths, &volmin, &volrem, &volent, &issuedYear,
		&issuedMonth, &issuedDay, &issuedHour, &issuedMin, &issuedSec,
		&issuedSecTenth, &duration, &range, &reportedby, &rtimeYear,
		&rtimeMonth, &rtimeDay, &rtimeHour, &rtimeMin, &rtimeSec,
		&rtimeSecTenth) != 28) {
		return 1; /* Malformed input. */
	}

	result->orderid = orderid;
	result->regionid = regionid;
	result->systemid = systemid;
	result->stationid = stationid;
	result->typeid = typeid;
	result->bid = bid - '0';
	result->price = price * 100 + priceTenths;
	result->volmin = volmin;
	result->volrem = volrem;
	result->volent = volent;
	result->issued = convert_pt_utc(
		ejday(issuedYear, issuedMonth, issuedDay)
		+ SEC_PER_HOUR * issuedHour + SEC_PER_MIN * issuedMin
		+ issuedSec + ((issuedSecTenth - '0' > 5) ? 1 : 0));
	result->duration = duration;

	/* Buy order ranges incorrect for this portion of time, so
	 * conservatively assume that their ranges are station (smallest).
	*/
	if (bid) {
		result->range = -1;
	} else {
		result->range = range;
	}

	result->reportedby = reportedby;
	result->reportedtime = convert_pt_utc(
		ejday(rtimeYear, rtimeMonth, rtimeDay)
		+ SEC_PER_HOUR * rtimeHour + SEC_PER_MIN * rtimeMin
		+ rtimeSec + ((rtimeSecTenth - '0' > 5) ? 1 : 0));

	return 0;
}

/* Historical caveat: Time in Pacific. */
int parse_v2(struct rawRecord *result)
{
	uint64_t orderid, price, reportedby;
	uint32_t regionid, systemid, stationid, typeid, priceTenths;
	uint32_t issuedYear, issuedMonth, issuedDay, issuedHour, issuedMin,
	uint32_t issuedSec;
	uint32_t rtimeYear, rtimeMonth, rtimeDay, rtimeHour, rtimeMin, rtimeSec;
	uint32_t volmin, volrem, volent, duration;
	int32_t range;
	int8_t bid;
	uint8_t issuedSecTenth, rtimeSecTenth;

	if (scanf("%llu , %u , %u , %u , %u , %c , %llu.%u , %u , %u , %u , "
		"%u-%u-%u %u:%u:%u.%c%*[^,], %u:%*u:%*u:%*u.%*u%*[^,], %u , "
		"%llu , %u-%u-%u %u:%u:%u.%c%*[^\n]\n",
		&orderid, &regionid, &systemid, &stationid, &typeid, &bid,
		&price, &priceTenths, &volmin, &volrem, &volent, &issuedYear,
		&issuedMonth, &issuedDay, &issuedHour, &issuedMin, &issuedSec,
		&issuedSecTenth, &duration, &range, &reportedby, &rtimeYear,
		&rtimeMonth, &rtimeDay, &rtimeHour, &rtimeMin, &rtimeSec,
		&rtimeSecTenth) != 28) {
		return 1;
	}

	result->orderid = orderid;
	result->regionid = regionid;
	result->systemid = systemid;
	result->stationid = stationid;
	result->typeid = typeid;
	result->bid = bid;
	result->price = price * 100 + priceTenths;
	result->volmin = volmin;
	result->volrem = volrem;
	result->volent = volent;
	result->issued = convert_pt_utc(
		ejday(issuedYear, issuedMonth, issuedDay)
		+ SEC_PER_HOUR * issuedHour + SEC_PER_MIN * issuedMin
		+ issuedSec + ((issuedSecTenth > 5) ? 1 : 0));
	result->duration = duration;
	result->range = range_to_bitfield(range);
	result->reportedby = reportedby;
	result->reportedtime = convert_pt_utc(
		ejday(rtimeYear, rtimeMonth, rtimeDay)
		+ SEC_PER_HOUR * rtimeHour + SEC_PER_MIN * rtimeMin
		+ rtimeSec + ((rtimeSecTenth > 5) ? 1 : 0));

	return 0;
}

/* Historical caveat: Switch to UTC. */
int parse_v3(struct rawRecord *result)
{
	uint64_t orderid, price, reportedby;
	uint32_t regionid, systemid, stationid, typeid, priceTenths, volmin;
	uint32_t issuedYear, issuedMonth, issuedDay, issuedHour, issuedMin;
	uint32_t issuedSec, duration, volrem, volent;
	uint32_t rtimeYear, rtimeMonth, rtimeDay, rtimeHour, rtimeMin, rtimeSec;
	int32_t range;
	int8_t bid;
	uint8_t issuedSecTenth, rtimeSecTenth;
	if (scanf("%llu , %u , %u , %u , %u , %c , %llu.%u , %u , %u , %u , "
		"%u-%u-%u %u:%u:%u.%c%*[^,], %u:%*u:%*u:%*u.%*c%*[^,], %u , "
		"%llu , %u-%u-%u %u:%u:%u.%c%*[^\n]\n",
		&orderid, &regionid, &systemid, &stationid, &typeid, &bid,
		&price, &priceTenths, &volmin, &volrem, &volent, &issuedYear,
		&issuedMonth, &issuedDay, &issuedHour, &issuedMin, &issuedSec,
		&issuedSecTenth, &duration, &range, &reportedby, &rtimeYear,
		&rtimeMonth, &rtimeDay, &rtimeHour, &rtimeMin, &rtimeSec,
		&rtimeSecTenth) != 28) {
		return 1;
	}

	result->orderid = orderid;
	result->regionid = regionid;
	result->systemid = systemid;
	result->stationid = stationid;
	result->typeid = typeid;
	result->bid = bid;
	result->price = price * 100 + priceTenths;
	result->volmin = volmin;
	result->volrem = volrem;
	result->volent = volent;
	result->issued = ejday(issuedYear, issuedMonth, issuedDay)
		+ SEC_PER_HOUR * issuedHour + SEC_PER_MIN * issuedMin
		+ issuedSec + ((issuedSecTenth > 5) ? 1 : 0);
	result->duration = duration;
	result->range = range_to_bitfield(range);
	result->reportedby = reportedby;
	result->reportedtime = ejday(rtimeYear, rtimeMonth, rtimeDay)
		+ SEC_PER_HOUR * rtimeHour + SEC_PER_MIN * rtimeMin
		+ rtimeSec + ((rtimeSecTenth > 5) ? 1 : 0);

	return 0;
}

/* Historical caveat: Switched duration format */
int parse_v4(struct rawRecord *result)
{
	uint64_t orderid, price, reportedby;
	uint32_t regionid, systemid, stationid, typeid, priceTenths, volmin;
	uint32_t volrem, volent, issuedYear, issuedMonth, issuedDay, issuedHour;
	uint32_t issuedMin, issuedSec, duration,
	uint32_t rtimeYear, rtimeMonth, rtimeDay, rtimeHour, rtimeMin, rtimeSec;
	int32_t range;
	int8_t bid;
	/* We now only have 26 fields because we no longer have *SecTenths. */
	if (scanf("%llu , %u , %u , %u , %u , %c , %llu.%u , %u , %u , %u , "
		"%u-%u-%u %u:%u:%u , %u day %*[^ ] %*u:%*u:%*u , %u , %llu , "
		"%u-%u-%u %u:%u:%u %*[^\n]\n", &orderid, &regionid, &systemid,
		&stationid, &typeid, &bid, &price, &priceTenths, &volmin,
		&volrem, &volent, &issuedYear, &issuedMonth, &issuedDay,
		&issuedHour, &issuedMin, &issuedSec, &duration, &range,
		&reportedby, &rtimeYear, &rtimeMonth, &rtimeDay, &rtimeHour,
		&rtimeMin,&rtimeSec) != 26) {
		return 1;
	}

	result->orderid = orderid;
	result->regionid = regionid;
	result->systemid = systemid;
	result->stationid = stationid;
	result->typeid = typeid;
	result->bid = bid;
	result->price = price * 100 + priceTenths;
	result->volmin = volmin;
	result->volrem = volrem;
	result->volent = volent;
	result->issued = ejday(issuedYear, issuedMonth, issuedDay)
		+ SEC_PER_HOUR * issuedHour + SEC_PER_MIN * issuedMin
		+ issuedSec;
	result->duration = duration;
	result->range = range_to_bitfield(range);
	result->reportedby = reportedby;
	result->reportedtime = ejday(rtimeYear, rtimeMonth, rtimeDay)
		+ SEC_PER_HOUR * rtimeHour + SEC_PER_MIN * rtimeMin + rtimeSec;

	return 0;
}

/* Historical caveat: Switched to quoted fields. */
int parse_v5(struct rawRecord *result)
{
	uint64_t orderid, price, reportedby;
	int32_t range;
	uint32_t regionid, systemid, stationid, typeid, priceTenths, volmin;
	uint32_t volrem, volent, issuedYear, issuedMonth, issuedDay, issuedHour;
	uint32_t issuedMin, issuedSec, duration, rtimeYear, rtimeMonth;
	uint32_t rtimeDay, rtimeHour, rtimeMin, rtimeSec;
	int8_t bid;
	if (scanf("\"%llu\",\"%u\",\"%u\",\"%u\",\"%u\",\"%c\",\"%llu.%u\",\"%u\","
		"\"%u\",\"%u\",\"%u-%u-%u %u:%u:%u\",\"%u day %*[^ ] %*u:%*u:%*u\","
		"\"%u\",\"%llu\",\"%u-%u-%u %u:%u:%u %*[^\n]\n", &orderid,
		&regionid, &systemid, &stationid, &typeid, &bid, &price,
		&priceTenths, &volmin, &volrem, &volent, &issuedYear,
		&issuedMonth, &issuedDay, &issuedHour, &issuedMin, &issuedSec,
		&duration, &range, &reportedby, &rtimeYear, &rtimeMonth,
		&rtimeDay, &rtimeHour, &rtimeMin,&rtimeSec) != 26) {
		return 1;
	}

	result->orderid = orderid;
	result->regionid = regionid;
	result->systemid = systemid;
	result->stationid = stationid;
	result->typeid = typeid;
	result->bid = bid;
	result->price = price * 100 + priceTenths;
	result->volmin = volmin;
	result->volrem = volrem;
	result->volent = volent;
	result->issued = ejday(issuedYear, issuedMonth, issuedDay)
		+ SEC_PER_HOUR * issuedHour + SEC_PER_MIN * issuedMin
		+ issuedSec;
	result->duration = duration;
	result->range = range_to_bitfield(range);
	result->reportedby = reportedby;
	result->reportedtime = ejday(rtimeYear, rtimeMonth, rtimeDay)
		+ SEC_PER_HOUR * rtimeHour + SEC_PER_MIN * rtimeMin + rtimeSec;

	return 0;
}

Parser parser_factory(uint64_t parsedTime)
{
	#define D20070101 1167609600
	#define D20071001 1191369600
	#define D20100718 1279584000
	#define D20110213 1297468800
	if (parsedTime < D20070101) {
		return parse_v1;
	} else if (parsedTime < D20071001) {
		return parse_v2;
	} else if (parsedTime < D20100718) {
		return parse_v3;
	} else if (parsedTime < D20110213) {
		return parse_v4;
	} else {
		return parse_v5;
	}
}
