#include "eve_parser.h"
#include "token.h"

#define E_JDAY 719558       /* Julian Day of epoch (1970-1-1) */
#define SEC_PER_DAY 86400
#define SEC_PER_HOUR 3600
#define SEC_PER_MIN 60

static uint32_t ejday(const uint32_t yr, const uint32_t mn, const uint32_t dy)
{
	return (yr*365 + yr/4 - yr/100 + yr/400 + (mn * 306 + 5) / 10
	    + dy - 1 - E_JDAY) * SEC_PER_DAY;
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

/* Returns -2 on failure. */
static int8_t convert_range_byte(int range)
{
	switch(range) {
	case -1:
		return -1;
	case 0:
		return 0;
	case 5:
		return 5;
	case 10:
		return 10;
	case 20:
		return 20;
	case 40:
		return 40;
	case 32767:
	case 65335:
		return 127;
	default:
		return -2;
	}
}

static int set_tokens(token *tokens, uint64_t orderid, uint32_t regionid,
    uint32_t systemid, uint32_t stationid, uint32_t typeid, int8_t bid,
    uint64_t price, uint32_t volmin, uint32_t volrem, uint32_t volent,
    uint32_t issued, uint8_t duration, int8_t range, uint64_t reportedby,
    uint32_t rtime)
{
	/* Input Order:
	 * orderid, regionid, systemid, stationid, typeid, bid, price, volmin,
	 * volrem, volent, issued, duration, range, reportedby, rtime
	*/

	/* It's "abstraction violating" but I'm not bothering to check for
	 * memory allocation in set_token because I know that small datasets
	 * < 9 bytes are copied to internal buffers and therefore there's no
	 * memory allocation occuring to fail anyway.
	*/

	unsigned int i = 0;

	set_token(&tokens[i++],	&orderid,	sizeof(orderid));
	set_token(&tokens[i++],	&regionid,	sizeof(regionid));
	set_token(&tokens[i++], &systemid,	sizeof(systemid));
	set_token(&tokens[i++], &stationid,	sizeof(stationid));
	set_token(&tokens[i++], &typeid,	sizeof(typeid));
	set_token(&tokens[i++], &bid,		sizeof(bid));
	set_token(&tokens[i++], &price,		sizeof(price));
	set_token(&tokens[i++], &volmin,	sizeof(volmin));
	set_token(&tokens[i++], &volrem,	sizeof(volrem));
	set_token(&tokens[i++], &volent,	sizeof(volent));
	set_token(&tokens[i++], &issued,	sizeof(issued));
	set_token(&tokens[i++], &duration,	sizeof(duration));
	set_token(&tokens[i++], &range,		sizeof(range));
	set_token(&tokens[i++], &reportedby,	sizeof(reportedby));
	set_token(&tokens[i++], &rtime,		sizeof(rtime));

	return 0;
}

/*
 * Return 0 on successful parsing, -1 on malformed input and -2 on alloc error.
 * 1 is returned for improper data.
 *
 * Historical caveats: Buy order ranges incorrect, and time in Pacific.
 *
 * There's no specifier in C89 for int8_t, so we get some compiler warnings
 * with %u. In C99 we'd use %hhu.
 *
*/
int parse_v1(token *tokens, const size_t tokenCount)
{
	uint64_t orderid, price, reportedby;
	uint32_t issued, rtime;
	uint32_t regionid, systemid, stationid, typeid, volmin, volrem, volent;
	unsigned int issuedYear, issuedMonth, issuedDay, issuedHour;
	unsigned int issuedMin, issuedSec, rtimeYear, rtimeMonth, rtimeDay;
	unsigned int rtimeHour, rtimeMin, rtimeSec;
	int range;
	uint8_t priceTenths, issuedSecTenth, rtimeSecTenth, duration;
	int8_t bid;

	/* Precondition */
	assert(tokenCount > 14);

	if (scanf("%llu , %u , %u , %u , %u , %c , %llu.%u , %u , %u , %u , "
	    "%u-%u-%u %u:%u:%u.%c%*[^,], %u:%*u:%*u:%*u.%*c%*[^,], %u , "
	    "%llu , %u-%u-%u %u:%u:%u.%c%*[^\n]\n",
	    &orderid, &regionid, &systemid, &stationid, &typeid, &bid,
	    &price, &priceTenths, &volmin, &volrem, &volent, &issuedYear,
	    &issuedMonth, &issuedDay, &issuedHour, &issuedMin, &issuedSec,
	    &issuedSecTenth, &duration, &range, &reportedby, &rtimeYear,
	    &rtimeMonth, &rtimeDay, &rtimeHour, &rtimeMin, &rtimeSec,
	    &rtimeSecTenth) != 28) {
		return -1; /* Malformed input. */
	}

	bid = bid - '0';
	price = price * 100 + priceTenths;
	issued = convert_pt_utc(ejday(issuedYear, issuedMonth, issuedDay)
		+ SEC_PER_HOUR * issuedHour + SEC_PER_MIN * issuedMin
		+ issuedSec + ((issuedSecTenth - '0' > 5) ? 1 : 0));
	/* Buy order ranges incorrect for this portion of time, so
	 * conservatively assume that their ranges are station (smallest).
	*/
	if (bid) {
		range = -1;
	} else {
		range = convert_range_byte(range);
	}
	rtime = convert_pt_utc(ejday(rtimeYear, rtimeMonth, rtimeDay)
		+ SEC_PER_HOUR * rtimeHour + SEC_PER_MIN * rtimeMin
		+ rtimeSec + ((rtimeSecTenth - '0' > 5) ? 1 : 0));

	if (range == -2 || bid > 1 || (issued > rtime)) {
		return 1; /* Throw out bogus values. */
	}

	if (set_tokens(tokens, orderid, regionid, systemid, stationid, typeid,
		    bid, price, volmin, volrem, volent, issued, duration,
		    range, reportedby, rtime)) {
		return -2;
	}

	return 0;
}

/* Historical caveat: Time in Pacific. (Buy order ranges now correct) */
int parse_v2(token *tokens, const size_t tokenCount)
{
	uint64_t orderid, price, reportedby;
	uint32_t issued, rtime;
	uint32_t regionid, systemid, stationid, typeid, volmin, volrem, volent;
	unsigned int issuedYear, issuedMonth, issuedDay, issuedHour;
	unsigned int issuedMin, issuedSec, rtimeYear, rtimeMonth, rtimeDay;
	unsigned int rtimeHour, rtimeMin, rtimeSec;
	int range;
	uint8_t priceTenths, issuedSecTenth, rtimeSecTenth, duration;
	int8_t bid;

	/* Precondition */
	assert(tokenCount > 14);

	if (scanf("%llu , %u , %u , %u , %u , %c , %llu.%u , %u , %u , %u , "
	    "%u-%u-%u %u:%u:%u.%c%*[^,], %u:%*u:%*u:%*u.%*c%*[^,], %u , "
	    "%llu , %u-%u-%u %u:%u:%u.%c%*[^\n]\n",
	    &orderid, &regionid, &systemid, &stationid, &typeid, &bid,
	    &price, &priceTenths, &volmin, &volrem, &volent, &issuedYear,
	    &issuedMonth, &issuedDay, &issuedHour, &issuedMin, &issuedSec,
	    &issuedSecTenth, &duration, &range, &reportedby, &rtimeYear,
	    &rtimeMonth, &rtimeDay, &rtimeHour, &rtimeMin, &rtimeSec,
	    &rtimeSecTenth) != 28) {
		return -1; /* Malformed input. */
	}

	bid = bid - '0';
	range = convert_range_byte(range);
	price = price * 100 + priceTenths;
	issued = convert_pt_utc(ejday(issuedYear, issuedMonth, issuedDay)
		+ SEC_PER_HOUR * issuedHour + SEC_PER_MIN * issuedMin
		+ issuedSec + ((issuedSecTenth - '0' > 5) ? 1 : 0));
	rtime = convert_pt_utc(ejday(rtimeYear, rtimeMonth, rtimeDay)
		+ SEC_PER_HOUR * rtimeHour + SEC_PER_MIN * rtimeMin
		+ rtimeSec + ((rtimeSecTenth - '0' > 5) ? 1 : 0));

	if (range == -2 || bid > 1 || (issued > rtime)) {
		return 1; /* Throw out bogus values. */
	}

	if (set_tokens(tokens, orderid, regionid, systemid, stationid, typeid,
		    bid, price, volmin, volrem, volent, issued, duration,
		    range, reportedby, rtime)) {
		return 2;
	}

	return 0;
}

/* Historical caveat: Switch to UTC. */
int parse_v3(token *tokens, const size_t tokenCount)
{
	uint64_t orderid, price, reportedby;
	uint32_t issued, rtime;
	uint32_t regionid, systemid, stationid, typeid, volmin, volrem, volent;
	unsigned int issuedYear, issuedMonth, issuedDay, issuedHour;
	unsigned int issuedMin, issuedSec, rtimeYear, rtimeMonth, rtimeDay;
	unsigned int rtimeHour, rtimeMin, rtimeSec;
	int range;
	uint8_t priceTenths, issuedSecTenth, rtimeSecTenth, duration;
	int8_t bid;

	/* Precondition */
	assert(tokenCount > 14);

	if (scanf("%llu , %u , %u , %u , %u , %c , %llu.%u , %u , %u , %u , "
	    "%u-%u-%u %u:%u:%u.%c%*[^,], %u:%*u:%*u:%*u.%*c%*[^,], %u , "
	    "%llu , %u-%u-%u %u:%u:%u.%c%*[^\n]\n",
	    &orderid, &regionid, &systemid, &stationid, &typeid, &bid,
	    &price, &priceTenths, &volmin, &volrem, &volent, &issuedYear,
	    &issuedMonth, &issuedDay, &issuedHour, &issuedMin, &issuedSec,
	    &issuedSecTenth, &duration, &range, &reportedby, &rtimeYear,
	    &rtimeMonth, &rtimeDay, &rtimeHour, &rtimeMin, &rtimeSec,
	    &rtimeSecTenth) != 28) {
		return -1; /* Malformed input. */
	}

	bid = bid - '0';
	range = convert_range_byte(range);
	price = price * 100 + priceTenths;
	issued = ejday(issuedYear, issuedMonth, issuedDay)
		+ SEC_PER_HOUR * issuedHour + SEC_PER_MIN * issuedMin
		+ issuedSec + ((issuedSecTenth - '0' > 5) ? 1 : 0);
	rtime = ejday(rtimeYear, rtimeMonth, rtimeDay)
		+ SEC_PER_HOUR * rtimeHour + SEC_PER_MIN * rtimeMin
		+ rtimeSec + ((rtimeSecTenth - '0' > 5) ? 1 : 0);

	if (range == -2 || bid > 1 || (issued > rtime)) {
		return 1; /* Throw out bogus values. */
	}

	if (set_tokens(tokens, orderid, regionid, systemid, stationid, typeid,
		    bid, price, volmin, volrem, volent, issued, duration,
		    range, reportedby, rtime)) {
		return 2;
	}

	return 0;
}

/* Historical caveat: Switched duration format */
int parse_v4(token *tokens, const size_t tokenCount)
{
	uint64_t orderid, price, reportedby;
	uint32_t issued, rtime;
	uint32_t regionid, systemid, stationid, typeid, volmin, volrem, volent;
	unsigned int issuedYear, issuedMonth, issuedDay, issuedHour;
	unsigned int issuedMin, issuedSec, rtimeYear, rtimeMonth, rtimeDay;
	unsigned int rtimeHour, rtimeMin, rtimeSec;
	int range;
	uint8_t priceTenths, duration;
	int8_t bid;

	/* Precondition */
	assert(tokenCount > 14);

	/* We now only have 26 fields because we no longer have *SecTenths. */
	if (scanf("%llu , %u , %u , %u , %u , %c , %llu.%u , %u , %u , %u , "
		"%u-%u-%u %u:%u:%u , %u day %*[^ ] %*u:%*u:%*u , %u , %llu , "
		"%u-%u-%u %u:%u:%u %*[^\n]\n", &orderid, &regionid, &systemid,
		&stationid, &typeid, &bid, &price, &priceTenths, &volmin,
		&volrem, &volent, &issuedYear, &issuedMonth, &issuedDay,
		&issuedHour, &issuedMin, &issuedSec, &duration, &range,
		&reportedby, &rtimeYear, &rtimeMonth, &rtimeDay, &rtimeHour,
		&rtimeMin,&rtimeSec) != 26) {
		return -1;
	}

	bid = bid - '0';
	range = convert_range_byte(range);
	price = price * 100 + priceTenths;
	issued = ejday(issuedYear, issuedMonth, issuedDay)
		+ SEC_PER_HOUR * issuedHour + SEC_PER_MIN * issuedMin
		+ issuedSec;
	rtime = ejday(rtimeYear, rtimeMonth, rtimeDay)
		+ SEC_PER_HOUR * rtimeHour + SEC_PER_MIN * rtimeMin
		+ rtimeSec;

	if (range == -2 || bid > 1 || (issued > rtime)) {
		return 1; /* Throw out bogus values. */
	}

	if (set_tokens(tokens, orderid, regionid, systemid, stationid, typeid,
		    bid, price, volmin, volrem, volent, issued, duration,
		    range, reportedby, rtime)) {
		return 2;
	}

	return 0;
}

/* Historical caveat: Switched to quoted fields. */
int parse_v5(token *tokens, const size_t tokenCount)
{
	uint64_t orderid, price, reportedby;
	uint32_t issued, rtime;
	uint32_t regionid, systemid, stationid, typeid, volmin, volrem, volent;
	unsigned int issuedYear, issuedMonth, issuedDay, issuedHour;
	unsigned int issuedMin, issuedSec, rtimeYear, rtimeMonth, rtimeDay;
	unsigned int rtimeHour, rtimeMin, rtimeSec;
	int range;
	uint8_t priceTenths, duration;
	int8_t bid;

	/* Precondition */
	assert(tokenCount > 14);

	if (scanf("\"%llu\",\"%u\",\"%u\",\"%u\",\"%u\",\"%c\",\"%llu.%u\","
		    "\"%u\",\"%u\",\"%u\",\"%u-%u-%u %u:%u:%u\","
		    "\"%u day %*[^ ] %*u:%*u:%*u\",\"%u\",\"%llu\","
		    "\"%u-%u-%u %u:%u:%u %*[^\n]\n", &orderid, &regionid,
		    &systemid, &stationid, &typeid, &bid, &price, &priceTenths,
		    &volmin, &volrem, &volent, &issuedYear, &issuedMonth,
		    &issuedDay, &issuedHour, &issuedMin, &issuedSec, &duration,
		    &range, &reportedby, &rtimeYear, &rtimeMonth, &rtimeDay,
		    &rtimeHour, &rtimeMin,&rtimeSec) != 26) {
		return -1;
	}

	bid = bid - '0';
	range = convert_range_byte(range);
	price = price * 100 + priceTenths;
	issued = ejday(issuedYear, issuedMonth, issuedDay)
		+ SEC_PER_HOUR * issuedHour + SEC_PER_MIN * issuedMin
		+ issuedSec;
	rtime = ejday(rtimeYear, rtimeMonth, rtimeDay)
		+ SEC_PER_HOUR * rtimeHour + SEC_PER_MIN * rtimeMin
		+ rtimeSec;

	if (range == -2 || bid > 1 || (issued > rtime)) {
		return 1; /* Throw out bogus values. */
	}

	if (set_tokens(tokens, orderid, regionid, systemid, stationid, typeid,
		    bid, price, volmin, volrem, volent, issued, duration,
		    range, reportedby, rtime)) {
		return 2;
	}

	return 0;
}

Parser parser_factory(const uint32_t yr, const uint32_t mn, const uint32_t dy)
{
	#define D20070101 1167609600
	#define D20071001 1191369600
	#define D20100718 1279584000
	#define D20110213 1297468800

	const uint64_t parsedTime = ejday(yr, mn, dy);

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
