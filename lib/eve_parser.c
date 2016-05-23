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

/* Parse HH:MM:SS(.S...) time format */
static uint32_t
parse_timestamp(const char** const s)
{
	#define MIN_SECONDS 60
	unsigned int ret;
	const char* str = *s;
	{ /* Preconditions */
		assert(s != NULL);
		assert(*s != NULL);
	}
	/* Parse the HH:MM:SS(.S...) time format. */
	ret = ((str[0] - '0') * 10 + (str[1] - '0')) * HR_SECONDS;
	ret += ((str[3] - '0') * 10 + (str[4] - '0')) * MIN_SECONDS; /*skip :*/
	ret += (str[6] - '0') * 10 + (str[7] - '0'); /* skip ':' as above */
	if (str[8] == '.') { /* Handle optional trailing seconds */
		str += 9; /* Skip '.' */
		/* Now we're looking at tenths of seconds. If that
		 * tenth is >= 5 (i.e., >= .5 of a second), then
		 * increment to round to the nearest second.
		*/
		ret += (*str++ - '0' >= 5) ? 1 : 0;
		while (isdigit(*str)) {
			str++;
		}
	}
	*s = str;
	return ret;
}

/* Parse 'YYYY-MM-DD HH:MM:SS(.S...) datetime format */
static uint32_t
parse_datetime(const char** const s)
{
	uint32_t ret;
	{ /* Preconditions */
		assert(s != NULL);
		assert(*s != NULL);
	}
	{ /* Parse 'YYYY-MM-DD ' date format. */
		unsigned int year = 0, month = 0, day = 0;
		const char *str = *s;
		year = (str[0] - '0') * 1000 + (str[1] - '0') * 100
		    + (str[2] - '0') * 10 + (str[3] - '0');
		month = (str[5] - '0') * 10 + (str[6] - '0'); /* skip '-' */
		day = (str[8] - '0') * 10 + (str[9] - '0'); /* skip '-' */
		*s = str + 11; /* Skip trailing space. */
		ret = ejday(year, month, day);
	}
	ret += parse_timestamp(s);
	return ret;
}

/* Parses a decimal value from a string */
static uint64_t
parse_uint(const char** const s)
{
	uint64_t val = 0;
	const char* str = *s;
	{ /* Preconditions */
		assert(s != NULL);
		assert(*s != NULL);
	}
	while (isdigit(*str)) {
		val = val * 10 + (unsigned int)(*str++ - '0');
	}
	*s = str;
	return val;
}

/* Returns -2 on failure. */
static int8_t
range_to_byte(const unsigned int range)
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
parse_range(const char** const s)
{
	{ /* Preconditions */
		assert(s != NULL);
		assert(*s != NULL);
	}
	if (**s != '-') {
		return range_to_byte((unsigned int)parse_uint(s));
	}
	/* Handle negative values. */
	*s += 1;
	while (isdigit(**s)) { /* Be a good neighbor & skip remaining digits */
		*s += 1;
	}
	return -1;
}

static struct eve_txn
parse_raw_txnord(const char* str)
{
	struct eve_txn txn;
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
	txn.orderID = parse_uint(&str); str += SKIPLEN;
	txn.regionID = (uint32_t)parse_uint(&str); str += SKIPLEN;
	txn.systemID = (uint32_t)parse_uint(&str); str += SKIPLEN;
	txn.stationID = (uint32_t)parse_uint(&str); str += SKIPLEN;
	txn.typeID = (uint32_t)parse_uint(&str); str += SKIPLEN;
	txn.bid = (uint8_t)parse_uint(&str); str += SKIPLEN;
	txn.price = parse_uint(&str) * 100;
	if (*str == '.') { /* Cents & cent field are optional */
		str++;
		txn.price += (uint32_t)(*str++ - '0') * 10;
		if (isdigit(*str)) {
			txn.price += (uint32_t)(*str++ - '0');
		}
	}
	str += SKIPLEN;
	txn.volMin = (uint32_t)parse_uint(&str); str += SKIPLEN;
	txn.volRem = (uint32_t)parse_uint(&str); str += SKIPLEN;
	txn.volEnt = (uint32_t)parse_uint(&str); str += SKIPLEN;
	txn.issued = parse_datetime(&str); str += SKIPLEN;
	txn.duration = (uint16_t)parse_uint(&str); /* Day(s) */
	while (!isdigit(*str)) { /* handle ':', "Day", and "Days" trailers */
		str++;
	}
	parse_timestamp(&str); str += SKIPLEN;
	txn.range = parse_range(&str); str += SKIPLEN;
	txn.reportedby = parse_uint(&str); str += SKIPLEN;
	txn.rtime = parse_datetime(&str);
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
has_bad_value(const struct eve_txn* const txn)
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
parse_txn(const char *str, struct eve_txn *txn)
{
	{ /* Preconditions */
		assert(str != NULL);
		assert(txn != NULL);
	}
	*txn = parse_raw_txnord(str);
	return has_bad_value(txn);
}

int
parse_txn_pt(const char *str, struct eve_txn *txn)
{
	{ /* Preconditions */
		assert(str != NULL);
		assert(txn != NULL);
	}
	*txn = parse_raw_txnord(str);
	txn->issued = pt_to_utc(txn->issued);
	txn->rtime = pt_to_utc(txn->rtime);
	return has_bad_value(txn);
}

int
parse_txn_pt_bo(const char* const str, struct eve_txn* txn)
{
	{ /* Preconditions */
		assert(str != NULL);
		assert(txn != NULL);
	}
	*txn = parse_raw_txnord(str);
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
	const uint64_t parsedTime = ejday(year, month, day);
	if (parsedTime < D20070101) {
		return parse_txn_pt_bo;
	} else if (parsedTime < D20071001) {
		return parse_txn_pt;
	} else {
		return parse_txn;
	}
}
