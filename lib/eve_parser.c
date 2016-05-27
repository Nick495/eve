#include "eve_parser.h"

#define HR_SECONDS 3600

/* Fast converter between years and Epoch normalized Julian Days, in seconds */
static uint32_t
ejday(unsigned int year, unsigned int month, unsigned int day)
{
	#define E_JDAY 719558       /* Julian Day of epoch (1970-1-1) */
	#define DAY_SECONDS 86400
	return (year * 365 + year / 4 - year / 100 + year / 400
		+ (month * 306 + 5) / 10 + day - 1 - E_JDAY) * DAY_SECONDS;
}

/* Parse HH:MM:SS(.S...) time format into a number of seconds */
static const char* restrict
parse_timestamp(const char* restrict s, uint32_t* restrict sectot)
{
	#define MIN_SECONDS 60
	{ /* Preconditions */
		assert(s != NULL);
	}
	/* Parse the HH:MM:SS(.S...) time format. */
	*sectot = ((s[0] - '0') * 10 + (s[1] - '0')) * HR_SECONDS;
	*sectot += ((s[3] - '0')*10 + (s[4] - '0')) * MIN_SECONDS; /*skip ':'*/
	*sectot += (s[6] - '0') * 10 + (s[7] - '0'); /* skip ':' again */
	s += 8;
	if (*s == '.') { /* Handle optional trailing seconds */
		s++; /* Skip '.' */
		/* Now we're looking at tenths of seconds. If that
		 * tenth is >= 5 (i.e., >= .5 of a second), then
		 * increment to round to the nearest second.
		 *
		 * We have to skip the remaining digits for the next parser.
		*/
		*sectot += (*s++ - '0' >= 5) ? 1 : 0;
		while (isdigit(*s)) {
			s++;
		}
	}
	return s;
}

/* Parse 'YYYY-MM-DD HH:MM:SS(.S...) datetime format to seconds since epoch */
static const char* restrict
parse_datetime(const char* restrict s, uint32_t* restrict esecs)
{
	{ /* Preconditions */
		assert(s != NULL);
	}
	{ /* Parse 'YYYY-MM-DD ' date format. */
		unsigned int year = 0, month = 0, day = 0;
		year = (s[0] - '0') * 1000 + (s[1] - '0') * 100
		    + (s[2] - '0') * 10 + (s[3] - '0');
		month = (s[5] - '0') * 10 + (s[6] - '0'); /* skip '-' */
		day = (s[8] - '0') * 10 + (s[9] - '0'); /* skip '-' */
		s += 11; /* Skip trailing space. */
		s = parse_timestamp(s, esecs);
		*esecs += ejday(year, month, day);
	}
	return s;
}

/* Parses a decimal value from a string */
/* TODO: Find a way to write a generic version of this function */
static const char* restrict
parse_uint64(const char* restrict s, uint64_t* restrict val)
{
	{ /* Preconditions */
		assert(s != NULL);
	}
	while (isdigit(*s)) {
		*val = (uint64_t)(*val * 10 + (unsigned int)(*s++ - '0'));
	}
	return s;
}
static const char* restrict
parse_uint32(const char* restrict s, uint32_t* restrict val)
{
	{ /* Preconditions */
		assert(s != NULL);
	}
	while (isdigit(*s)) {
		*val = (uint32_t)(*val * 10 + (unsigned int)(*s++ - '0'));
	}
	return s;
}
static const char* restrict
parse_uint16(const char* restrict s, uint16_t* restrict val)
{
	{ /* Preconditions */
		assert(s != NULL);
	}
	while (isdigit(*s)) {
		*val = (uint16_t)(*val * 10 + (unsigned int)(*s++ - '0'));
	}
	return s;
}
static const char* restrict
parse_uint8(const char* restrict s, uint8_t* restrict val)
{
	{ /* Preconditions */
		assert(s != NULL);
	}
	while (isdigit(*s)) {
		*val = (uint8_t)(*val * 10 + (unsigned int)(*s++ - '0'));
	}
	return s;
}

/* Parses the duration field. */
static const char* restrict
parse_duration(const char* restrict s, uint16_t* restrict duration)
{
	{ /* Preconditions */
		assert(s != NULL);
	}
	{ /* Parse the duration. 32 bit tmp value is purely for code re-use. */
		s = parse_uint16(s, duration); /* DD */
		while (!isdigit(*s)) { /* Skip ':', "Day", & "Days" trailers */
			s++;
		}
		while(isdigit(*s)) { /* Skip 1 or 2 hours. */
			s++;
		}
		/* The remaining string is ':MM:SS', which is always 0 secs */
		/* So let's skip it. */
		s += 6;
	}
	return s;
}

/* Convienence function for parse_range. Returns -2 on failure. */
static int8_t
range_to_byte(const uint32_t range)
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

/* Parses the range field (which can contain negative values) correctly. */
static const char* restrict
parse_range(const char* restrict s, int8_t* restrict range)
{
	uint32_t rangetmp = 0;
	{ /* Preconditions */
		assert(s != NULL);
	}
	if (*s != '-') { /* Positive values can be handled as normal. */
		s = parse_uint32(s, &rangetmp);
		*range = range_to_byte(rangetmp);
		return s;
	}
	/* The only possible negative value is -1, so just ignore the digits */
	s++;
	while (isdigit(*s)) { /* Skip remaining digits for following funcs */
		s++;
	}
	*range = -1;
	return s;
}

/* Parses an entire transaction record. */
static struct eve_txn
parse_raw_txn(const char* restrict str)
{
	struct eve_txn txn = { 0 };
	{ /* Preconditions */
		assert(str != NULL);
	}
#define SKIPLEN 3 /* Our fields have a 3 character seperator ' , ' or '","' */
	/*
	 * Input Order:
	 * orderid, regionid, systemid, stationid, typeid, bid, price, volmin,
	 * volrem, volent, issued, duration, range, reportedby, rtime
	 */
	if (*str == '"') { /* Some formats contain a leading '"' */
		str++;
	}
	str = parse_uint64(str, &txn.orderID) + SKIPLEN;
	str = parse_uint32(str, &txn.regionID) + SKIPLEN;
	if (*str == '-') {
		/* This regionID field has random -1 values, no idea why.
		 * TODO: Investigate why.
		*/
		goto fail_bad_val;
	}
	str = parse_uint32(str, &txn.systemID) + SKIPLEN;
	str = parse_uint32(str, &txn.stationID) + SKIPLEN;
	str = parse_uint32(str, &txn.typeID) + SKIPLEN;
	str = parse_uint8(str, &txn.bid) + SKIPLEN;
	str = parse_uint64(str, &txn.price);
	txn.price *= 100;
	if (*str == '.') { /* Cents & cent field are optional */
		str++;
		txn.price += (uint64_t)(*str++ - '0') * 10;
		if (isdigit(*str)) {
			txn.price += (uint64_t)(*str++ - '0');
		}
	}
	str += SKIPLEN;
	str = parse_uint32(str, &txn.volMin) + SKIPLEN;
	str = parse_uint32(str, &txn.volRem) + SKIPLEN;
	str = parse_uint32(str, &txn.volEnt) + SKIPLEN;
	str = parse_datetime(str, &txn.issued) + SKIPLEN;
	str = parse_duration(str, &txn.duration) + SKIPLEN;
	str = parse_range(str, &txn.range) + SKIPLEN;
	str = parse_uint64(str, &txn.reportedby) + SKIPLEN;
	str = parse_datetime(str, &txn.rtime);
	return txn;

fail_bad_val:
	txn.bid = 20; /* Random value to cause failure. */
	return txn;
}

/* Converts a pacific timestamp to UTC. Faster than mktime() by a lot. */
static uint32_t
pt_to_utc(const uint32_t pacificTime)
{
	/*
	 * YYYY-MM-DD-HH in PT (These are the date-times where daylight savings
	 * time apparently switches (according to tzdump on my machine) within
	 * the range of dates that we care about).
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

/* Returns 0 if no badval, type of bad value otherwise. */
static int
has_bad_value(const struct eve_txn* const restrict txn)
{
	{ /* Preconditions */
		assert(txn != NULL);
	}
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
parse_txn(char* const restrict str, struct eve_txn* restrict txn)
{
	{ /* Preconditions */
		assert(str != NULL);
		assert(txn != NULL);
	}
	*txn = parse_raw_txn(str);
	return has_bad_value(txn);
}

int
parse_txn_pt(char* const restrict str, struct eve_txn* restrict txn)
{
	{ /* Preconditions */
		assert(str != NULL);
		assert(txn != NULL);
	}
	*txn = parse_raw_txn(str);
	txn->issued = pt_to_utc(txn->issued);
	txn->rtime = pt_to_utc(txn->rtime);
	return has_bad_value(txn);
}

int
parse_txn_pt_bo(char* const restrict str, struct eve_txn* restrict txn)
{
	{ /* Preconditions */
		assert(str != NULL);
		assert(txn != NULL);
	}
	*txn = parse_raw_txn(str);
	/*
	 * Buy order ranges are incortxnt for this period, so if it's a bid, 1,
	 * then assume the range is the smallest possible, station.
	 * Similarly, the range for sell orders (0) is always 0, so:
	*/
	txn->range = -1 * txn->bid;
	txn->issued = pt_to_utc(txn->issued);
	txn->rtime = pt_to_utc(txn->rtime);
	return has_bad_value(txn);
}

eve_txn_parser
eve_txn_parser_strategy(uint32_t year, uint32_t month, uint32_t day)
{
	#define D20070101 1167609600 /* Consult formats.txt for dates. */
	#define D20071001 1191369600
	const uint64_t edelta = ejday(year, month, day); /*seconds from epoch*/
	if (edelta < D20070101) {
		return parse_txn_pt_bo;
	} else if (edelta < D20071001) {
		return parse_txn_pt;
	} else {
		return parse_txn;
	}
}
