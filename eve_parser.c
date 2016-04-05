#include "eve_parser.h"

#define SEC_PER_DAY 86400
#define SEC_PER_HOUR 3600
#define SEC_PER_MIN 60

/* Fast converter between years and Epoch normalized Julian Days, in seconds */
static uint32_t
ejday(unsigned int year, unsigned int month, unsigned int day)
{
	#define E_JDAY 719558       /* Julian Day of epoch (1970-1-1) */
	return (year*365 + year/4 - year/100 + year/400
	    + (month * 306 + 5) / 10 + day - 1 - E_JDAY) * SEC_PER_DAY;
}

/* Converts a pacific timestamp to UTC. Faster than mktime() by a lot. */
static uint32_t
pt_to_utc(const uint32_t pacificTime)
{
	/* Attempt to switch PT to UTC (including daylight savings time. :\) */
	/* YYYY-MM-DD-HH in PT (These are the date-times where daylight savings
	* time apparently switches (according to tzdump on my machine) within
	* the range of dates that we care about. */
	#define D2006040203 1144033200
	#define D2006102901 1162256400
	#define D2007031103 1173754800
#if 0
	#define D2007110400 1194307200
#endif

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

static uint64_t
parse_uint64(const char **s)
{
	assert(s != NULL);

	uint64_t val = 0;
	while (isdigit(**s)) {
		val = val * 10 + (unsigned int)**s - '0';
		*s += 1;
	}

	return val;
}

/* Returns -2 on failure. */
static int8_t
range_to_byte(unsigned int range)
{
	switch(range) {
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
	case 32767: /* Sometimes signed, sometimes not. */
	case 65535:
		return 127;
	default:
		return -2;
	}
}

static int8_t
parse_range(const char **s)
{
	assert(s != NULL);

	if (**s == '-') {
		*s += 1;
		while (isdigit(**s)) {
			*s += 1;
		}
		return -1;
	}

	return range_to_byte((unsigned int)parse_uint64(s));
}

static uint32_t
parse_datetime(const char **s)
{
	assert(s != NULL);
	uint32_t val = 0;
	unsigned int year;
	unsigned int month;
	unsigned int days;
	unsigned int fractionalSeconds;

	/* 'YYYY-MM-DD ' is the format. */
	/* Warning, there be dragons here. */
	year =
	((*s)[0]-'0')*1000 + ((*s)[1]-'0')*100 + ((*s)[2]-'0')*10 + ((*s)[3]-'0');
	*s += 5;  /* Skip the '-' */
	month = ((*s)[0] - '0') * 10 + ((*s)[1] - '0'); *s += 3; /* Skip '-' */
	days = ((*s)[0] - '0') * 10 + ((*s)[1] - '0'); *s += 3; /* Skip ' ' */

	/* Time format is HH:MM:SS(.S...) */
	val = ejday(year, month, days);
	/* 'HH:' */
	val += (((*s)[0] - '0') * 10 + ((*s)[1] - '0')) * SEC_PER_HOUR; *s += 3;
	val += (((*s)[0] - '0') * 10 + ((*s)[1] - '0')) * SEC_PER_MIN; *s += 3;
	val += (((*s)[0] - '0') * 10 + ((*s)[1] - '0')); *s += 2; /* SS(.S...) */

	if (**s == '.') { /* Trailing seconds are optional */
		*s += 1;
		fractionalSeconds = (uint32_t)(**s - '0') * 10; *s += 1;
		if (isdigit(**s)) {
			fractionalSeconds += (uint32_t)(**s - '0'); *s += 1;
		}
	}

	while (isdigit(**s)) { /* Skip any trailing seconds. */
		*s += 1;
	}

	if (fractionalSeconds > 50) { /* It's in tenths as opposed to ones */
		val++;
	}

	return val;
}

/* Input Order:
 * orderid, regionid, systemid, stationid, typeid, bid, price, volmin,
 * volrem, volent, issued, duration, range, reportedby, rtime
*/
static struct Raw_Record
parser(const char *str)
{
	struct Raw_Record rec;

	assert(str != NULL);

#define SKIPLEN 3 /* Our fields have a 3 character seperator */
	rec.bid = 3; /* Catch bad input */

	if (*str == '"') {
		str++;
	}
	rec.orderID = parse_uint64(&str); str += SKIPLEN;
	rec.regionID = (uint32_t)parse_uint64(&str); str += SKIPLEN;
	rec.systemID = (uint32_t)parse_uint64(&str); str += SKIPLEN;
	rec.stationID = (uint32_t)parse_uint64(&str); str += SKIPLEN;
	rec.typeID = (uint32_t)parse_uint64(&str); str += SKIPLEN;
	rec.bid = (uint8_t)parse_uint64(&str); str += SKIPLEN;
	rec.price = parse_uint64(&str) * 100;
	if (*str == '.') { /* Cents & cent field are optional */
		str++;
		rec.price += (uint32_t)(*str++ - '0') * 10;
		if (isdigit(*str)) {
			rec.price += (uint32_t)(*str++ - '0');
		}
	}
	str += SKIPLEN;
	rec.volMin = (uint32_t)parse_uint64(&str); str += SKIPLEN;
	rec.volRem = (uint32_t)parse_uint64(&str); str += SKIPLEN;
	rec.volEnt = (uint32_t)parse_uint64(&str); str += SKIPLEN;
	rec.issued = parse_datetime(&str); str += SKIPLEN;
	rec.duration = (uint16_t)parse_uint64(&str); /* Day(s) */
	while (!isdigit(*str)) {
		str++;
	}
	/* There's an hour, min, and sec field that's never used, so skip. */
	parse_uint64(&str); str++; /* Hour: */
	parse_uint64(&str); str++; /* Min: */
	parse_uint64(&str); /* Sec(.)  */
	if (*str == '.') {
		str++;
		parse_uint64(&str); /* Handle fractional seconds. */
	}
	str += SKIPLEN;

	rec.range = parse_range(&str); /* Special since range has negatives. */
	str += SKIPLEN;

	rec.reportedby = parse_uint64(&str); str += SKIPLEN;

	/* Year, month, day again */
	rec.rtime = parse_datetime(&str);

	return rec;
}

/* Returns 0 on success, type of bad value otherwise. */
static int
has_badval(const struct Raw_Record *rec)
{
	if (rec->issued > rec->rtime) {
		return 1;
	} else if (rec->bid > 1) {
		return 2;
	} else if (rec->range == -2) {
		return 3;
	}

	return 0;
}

/* Check formats.txt for specifics of each format. */
int
parse_pt_bo(const char *str, struct Raw_Record *rec)
{
	assert(str != NULL);
	assert(rec != NULL);

	*rec = parser(str);

	/* Buy order ranges are incorrect for this period. */
	/* If it's a buy order (bid) then estimate the range as the smallest.*/
	if (rec->bid == 1) {
		rec->range = -1;
	}

	/* Convert pacific time stamps to UTC. */
	rec->issued = pt_to_utc(rec->issued);
	rec->rtime = pt_to_utc(rec->rtime);

	return has_badval(rec);
}

int
parse_pt(const char *str, struct Raw_Record *rec)
{
	assert(str != NULL);
	assert(rec != NULL);

	*rec = parser(str);

	/* Convert pacific time stamps to UTC. */
	rec->issued = pt_to_utc(rec->issued);
	rec->rtime = pt_to_utc(rec->rtime);

	return has_badval(rec);
}

int
parse(const char *str, struct Raw_Record *rec)
{
	assert(str != NULL);
	assert(rec != NULL);

	*rec = parser(str);

	return has_badval(rec);
}

Parser
parser_factory(unsigned int year, unsigned int month, unsigned int day)
{
	#define D20070101 1167609600
	#define D20071001 1191369600
#if 0
	#define D20100718 1279584000
	#define D20110213 1297468800
#endif

	const uint64_t parsedTime = ejday(year, month, day);

	if (parsedTime < D20070101) {
		return parse_pt_bo;
	} else if (parsedTime < D20071001) {
		return parse_pt;
	} else {
		return parse;
	}
}
