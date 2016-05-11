#include "eve_parser.h"

#define DAY_SECONDS 86400
#define HR_SECONDS 3600
#define MIN_SECONDS 60

/* Fast converter between years and Epoch normalized Julian Days, in seconds */
static uint32_t
ejday(unsigned int year, unsigned int month, unsigned int day)
{
	#define E_JDAY 719558       /* Julian Day of epoch (1970-1-1) */
	return (year * 365 + year/4 - year/100 + year/400
	    + (month * 306 + 5) / 10 + day - 1 - E_JDAY) * DAY_SECONDS;
}

/* Converts a pacific timestamp to UTC. Faster than mktime() by a lot. */
static uint32_t
pt_to_utc(const uint32_t pacificTime)
{
	/*
	 * YYYY-MM-DD-HH in PT (These are the date-times where daylight savings
	 * time apparently switches (according to tzdump on my machine) within
	 * the range of dates that we care about.
	*/
	#define D2006040203 1144033200
	#define D2006102901 1162256400
	#define D2007031103 1173754800
	if (pacificTime < D2006040203) {
		return pacificTime + 8 * HR_SECONDS;
	} else if (pacificTime < D2006102901) {
		return pacificTime + 7 * HR_SECONDS;
	} else if (pacificTime < D2007031103) {
		return pacificTime + 8 * HR_SECONDS;
	} else { /* Data is UTC after this time period, so else() is fine. */
		return pacificTime + 7 * HR_SECONDS;
	}
}

/* Parses a decimal value from a string */
static uint64_t
parse_uint64(const char **s)
{
	{ /* Preconditions */
		assert(s != NULL);
		assert(*s != NULL);
	}
	uint64_t val = 0;
	const char *str = *s;
	while (isdigit(*str)) {
		val = val * 10 + (unsigned int)(*str++ - '0');
	}
	*s = str;
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
	case 32767: /* Sometimes signed, sometimes not apparently */
	case 65535:
		return 127;
	default:
		return -2;
	}
}

static int8_t
parse_range(const char **s)
{
	{ /* Preconditions */
		assert(s != NULL);
		assert(*s != NULL);
	}

	if (**s != '-') {
		return range_to_byte((unsigned int)parse_uint64(s));
	}

	/* Handle negative values. */
	*s += 1;
	while (isdigit(**s)) { /* Be a good neighbor & skip remaining digits */
		*s += 1;
	}
	return -1;
}

static uint32_t
parse_datetime(const char **s)
{
	uint32_t val;
	{ /* Preconditions */
		assert(s != NULL);
		assert(*s != NULL);
	}
	{ /* Parse 'YYYY-MM-DD ' date format. */
		unsigned int powers[4] = {1000, 100, 10, 1};
		unsigned int year = 0, month = 0, day = 0, i;
		const char *str = *s;
		for (i = 0; i < 4; ++i) {
			year += (*str++ - '0') * powers[i];
		}
		str++; /* Skip '-' */
		for (i = 0; i < 2; ++i) {
			month += (*str++ - '0') * powers[i];
		}
		str++; /* Skip '-' */
		for (i = 0; i < 2; ++i) {
			day += (*str++ - '0') * powers[i];
		}
		str++; /* Skip ' ' */
		val = ejday(year, month, day);
		*s = str;
	}
	{ /* Parse HH:MM:SS(.S...) time format */
		unsigned int powers[2] = {10, 1};
		unsigned int tmp, i;
		const char *str = *s;
		for (i = 0, tmp = 0; i < 2; ++i) {
			tmp += (*str++ - '0') * powers[i];
		}
		val += tmp * HR_SECONDS;
		str++; /* Skip ':' */
		for (i = 0, tmp = 0; i < 2; ++i) {
			tmp += (*str++ - '0') * powers[i];
		}
		val += tmp * MIN_SECONDS;
		str++; /* Skip second ':' */
		for (i = 0, tmp = 0; i < 2; ++i) {
			tmp += (*str++ - '0') * powers[i];
		}
		if (*str == '.') { /* Trailing seconds are optional */
			str++; /* Skip '.' */
			if ((*str++ - '0') >= 5) {
				/* Now we're looking at tenths of seconds.
				 * If that tenth is >= 5 (i.e., >= .5 of a
				 * second, increment in order to round to the
				 * nearest second)
				*/
				tmp++;
			}
		}
		val += tmp;
		while (isdigit(*str)) { /* Skip any trailing semi-seconds. */
			str++;
		}
	}
	return val;
}

static struct eve_txn
parse_raw_txnord(const char *str)
{
	struct eve_txn txn;

	assert(str != NULL);

#define SKIPLEN 3 /* Our fields have a 3 character seperator ' , ' or '","' */
	/*
	 * Input Order:
	 * orderid, regionid, systemid, stationid, typeid, bid, price, volmin,
	 * volrem, volent, issued, duration, range, reportedby, rtime
	 */
	if (*str == '"') {
		str++;
	}
	txn.orderID = parse_uint64(&str); str += SKIPLEN;
	txn.regionID = (uint32_t)parse_uint64(&str); str += SKIPLEN;
	txn.systemID = (uint32_t)parse_uint64(&str); str += SKIPLEN;
	txn.stationID = (uint32_t)parse_uint64(&str); str += SKIPLEN;
	txn.typeID = (uint32_t)parse_uint64(&str); str += SKIPLEN;
	txn.bid = (uint8_t)parse_uint64(&str); str += SKIPLEN;
	txn.price = parse_uint64(&str) * 100;
	if (*str == '.') { /* Cents & cent field are optional */
		str++;
		txn.price += (uint32_t)(*str++ - '0') * 10;
		if (isdigit(*str)) {
			txn.price += (uint32_t)(*str++ - '0');
		}
	}
	str += SKIPLEN;
	txn.volMin = (uint32_t)parse_uint64(&str); str += SKIPLEN;
	txn.volRem = (uint32_t)parse_uint64(&str); str += SKIPLEN;
	txn.volEnt = (uint32_t)parse_uint64(&str); str += SKIPLEN;
	txn.issued = parse_datetime(&str); str += SKIPLEN;
	txn.duration = (uint16_t)parse_uint64(&str); /* Day(s) */
	while (!isdigit(*str)) {
		str++;
	}
	/* There's an hour, min, and sec field that's never used, so skip. */
	parse_uint64(&str); str++; /* Hour: */
	parse_uint64(&str); str++; /* Min: */
	parse_uint64(&str); /* Sec(.)  */
	if (*str == '.') {
		str++;
		parse_uint64(&str); /* Handle optional fractional seconds. */
	}
	str += SKIPLEN;
	txn.range = parse_range(&str); /* Special since range has negatives. */
	str += SKIPLEN;
	txn.reportedby = parse_uint64(&str); str += SKIPLEN;
	/* Year, month, day again */
	txn.rtime = parse_datetime(&str);

	return txn;
}

/* Returns 0 if no badval, type of bad value otherwise. */
static int
has_bad_value(const struct eve_txn *txn)
{
	assert(txn != NULL);
	if (txn->issued > txn->rtime) {
		return 1;
	} else if (txn->bid > 1) {
		return 2;
	} else if (txn->range == -2) {
		return 3;
	}
	return 0;
}

/* Check formats.txt for specifics of each format. */
int
parse_txn_pt_bo(const char *str, struct eve_txn *txn)
{
	{ /* Preconditions */
		assert(str != NULL);
		assert(txn != NULL);
	}
	*txn = parse_raw_txnord(str);
	/*
	 * Buy order ranges are incortxnt for this period, so if it's a bid, or
	 * buy order then assume the range is the smallest possible, station.
	*/
	if (txn->bid == 1) {
		txn->range = -1;
	}
	txn->issued = pt_to_utc(txn->issued);
	txn->rtime = pt_to_utc(txn->rtime);
	return has_bad_value(txn);
}

int
parse_txn_pt(const char *str, struct eve_txn *txn)
{
	{
		assert(str != NULL);
		assert(txn != NULL);
	}
	*txn = parse_raw_txnord(str);
	txn->issued = pt_to_utc(txn->issued);
	txn->rtime = pt_to_utc(txn->rtime);
	return has_bad_value(txn);
}

int
parse(const char *str, struct eve_txn *txn)
{
	{
		assert(str != NULL);
		assert(txn != NULL);
	}
	*txn = parse_raw_txnord(str);
	return has_bad_value(txn);
}

eve_txn_parser
eve_txn_parser_factory(uint32_t year, uint32_t month, uint32_t day)
{
	#define D20070101 1167609600
	#define D20071001 1191369600
#if 0
	#define D20100718 1279584000
	#define D20110213 1297468800
#endif
	const uint64_t parsedTime = ejday(year, month, day);
	if (parsedTime < D20070101) {
		return parse_txn_pt_bo;
	} else if (parsedTime < D20071001) {
		return parse_txn_pt;
	} else {
		return parse;
	}
}
