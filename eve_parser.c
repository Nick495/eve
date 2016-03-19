#include "eve_parser.h"
#include "token.h"

#define E_JDAY 719558       /* Julian Day of epoch (1970-1-1) */
#define SEC_PER_DAY 86400
#define SEC_PER_HOUR 3600
#define SEC_PER_MIN 60

/* Fast converter between years and Epoch normalized Julian Days, in seconds */
static uint32_t ejday(const uint32_t yr, const uint32_t mn, const uint32_t dy)
{
	return (yr*365 + yr/4 - yr/100 + yr/400 + (mn * 306 + 5) / 10
	    + dy - 1 - E_JDAY) * SEC_PER_DAY;
}

/* Converts a pacific timestamp to UTC. Faster than mktime() by a lot. */
static uint32_t pt_to_utc(uint32_t pacificTime)
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
static int8_t range_to_byte(int range)
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
	case 65535:
		return 127;
	default:
		return -2;
	}
}

static uint64_t parse_num(const char **s)
{
	const char *str = *s;
	uint64_t val = 0;

	while (!isdigit(*str) && *str != '\0')
		str++; /* Skip leading non-digits */

	while (isdigit(*str))
		val = val * 10 + *str++ - '0'; /* Assume base 10 */

	*s = str;

	return val;
}

static int8_t parse_range(const char **s)
{
	const char *str = *s;
	int8_t val = 0;

	while (!isdigit(*str) && *str != '\0' && *str != '-')
		str++; /* Skip leading non-digits */

	if (*str == '-') {
		return -1;
	}

	*s = str;
	return range_to_byte(parse_num(s));
}

/*
 * Return 0 on successful parsing, -1 on malformed input and -2 on alloc error.
 * 1 is returned for improper data.
 *
*/
/* It's "abstraction violating" but I'm not bothering to check for
 * memory allocation in set_token because I know that small datasets
 * < 9 bytes are copied to internal buffers and therefore there's no
 * memory allocation occuring to fail anyway.
*/
/* Input Order:
 * orderid, regionid, systemid, stationid, typeid, bid, price, volmin,
 * volrem, volent, issued, duration, range, reportedby, rtime
*/
void parser(const char *str, token *tokens, const size_t tokenCount)
{
	uint64_t a = 0;
	int b,c,d;

	uint32_t u32 = 0;
	int8_t i8 = 0;
	uint8_t u8 = 0;

	/* Preconditions */
	assert(tokenCount > 14);

	a = parse_num(&str);
	set_token(&tokens[0], &a, sizeof(uint64_t));	/* orderid */
	u32 = (uint32_t) parse_num(&str);
	set_token(&tokens[1], &u32, sizeof(uint32_t));	/* regionid */
	u32 = (uint32_t) parse_num(&str);
	set_token(&tokens[2], &u32, sizeof(uint32_t));	/* systemid */
	u32 = (uint32_t) parse_num(&str);
	set_token(&tokens[3], &u32, sizeof(uint32_t));	/* stationid */
	u32 = (uint32_t) parse_num(&str);
	set_token(&tokens[4], &u32, sizeof(uint32_t));	/* typeid */
	i8 = (int8_t) (parse_num(&str));
	set_token(&tokens[5], &i8, sizeof(int8_t));	/* bid */
	a = parse_num(&str) * 100;
	if (*str == '.') {
		//printf("DEBUG: GOT HERE!\n");
		str++;
		a += (*str++ - '0') * 10;
		if (isdigit(*str)) {
			a += (*str++ - '0');
		}
	}
	set_token(&tokens[6], &a, sizeof(uint64_t));	/* price*/
	u32 = (uint32_t) parse_num(&str);
	set_token(&tokens[7], &u32, sizeof(uint32_t));	/* volmin */
	u32 = (uint32_t) parse_num(&str);
	set_token(&tokens[8], &u32, sizeof(uint32_t));	/* volrem */
	u32 = (uint32_t) parse_num(&str);
	set_token(&tokens[9], &u32, sizeof(uint32_t));	/* volent */

	b = parse_num(&str);				/* issuedYear */
	c = parse_num(&str);				/* issuedMonth */
	d = parse_num(&str);				/* issuedDay */
	u32 = ejday(b, c, d);

	b = parse_num(&str);				/* issuedHour */
	c = parse_num(&str);				/* issuedMin */
	d = parse_num(&str);				/* issuedSec */
	u32 += b * SEC_PER_HOUR + c * SEC_PER_MIN + d;
	if (isdigit(*str) && *str - '0' > 5) {
		u32++; /* Round time to the nearest second. */
	}
	set_token(&tokens[10], &u32, sizeof(uint32_t));	/* issued */

	u32 = parse_num(&str);				/* duration: XX Days*/
	b = parse_num(&str);				/* Hour */
	b = parse_num(&str);				/* Min */
	b = parse_num(&str);				/* Sec  */
	set_token(&tokens[11], &u32, sizeof(uint32_t));	/* Duration */

	i8 = parse_range(&str);	/* Special since range has negatives. */
	set_token(&tokens[12], &i8, sizeof(int8_t));	/* range */

	a = parse_num(&str);
	set_token(&tokens[13], &a, sizeof(uint64_t));	/* reportedby */

	b = parse_num(&str);				/* reportedYear */
	c = parse_num(&str);				/* reportedMonth */
	d = parse_num(&str);				/* reportedDay */
	u32 = ejday(b, c, d);

	b = parse_num(&str);				/* reportedHour */
	c = parse_num(&str);				/* reportedMin */
	d = parse_num(&str);				/* reportedSec */
	u32 += b * SEC_PER_HOUR + c * SEC_PER_MIN + d;
	if (isdigit(*str) && *str - '0' > 5) {
		u32++; /* Round time to the nearest second. */
	}
	set_token(&tokens[14], &u32, sizeof(uint32_t));	/* reportedtime */
}

/*
 * Historical caveats: Buy order ranges incorrect, and time in Pacific.
 * There's no specifier in C89 for int8_t, so we get some compiler warnings
 * with %u. In C99 we'd use %hhu.
 *
*/
int parse_pt_bo(const char *str, token *tokens, const size_t tokenCount)
{
	/* Preconditions */
	assert(tokenCount > 14);

	parser(str, tokens, tokenCount);
	/* Buy order ranges are incorrect for this period. */
	/* If it's a buy order (bid) then estimate the range as the smallest.*/
	if (*(uint8_t *)tokens[5].ptr > '1') {
		return 1;
	} else if (*(uint8_t *)tokens[5].ptr == '1') {
		*(int8_t *)tokens[12].ptr = range_to_byte(-1);
	}

	/* If we have a bad range, or an issued time earlier than reported
	 * return bad value.
	*/
	if ((*(int8_t *)tokens[12].ptr == -2) ||
	    (*(uint32_t *)tokens[10].ptr > *(uint32_t *)tokens[14].ptr)) {
		return 1;
	}

	/* Convert pacific time stamps to UTC. */
	*(uint32_t *)tokens[10].ptr = pt_to_utc(*(uint32_t*)tokens[10].ptr);
	*(uint32_t *)tokens[14].ptr = pt_to_utc(*(uint32_t*)tokens[14].ptr);

	return 0;
}

/* Historical caveat: Time in Pacific. (Buy order ranges now correct) */
int parse_pt(const char *str, token *tokens, const size_t tokenCount)
{
	/* Preconditions */
	assert(tokenCount > 14);

	parser(str, tokens, tokenCount);

	/* If we have a bad range, or an issued time earlier than reported
	 * return bad value.
	*/
	if ((*(int8_t *)tokens[12].ptr == -2) ||
	    (*(uint32_t *)tokens[10].ptr > *(uint32_t *)tokens[14].ptr)) {
		return 1;
	}

	/* Convert pacific time stamps to UTC. */
	*(uint32_t *)tokens[10].ptr = pt_to_utc(*(uint32_t*)tokens[10].ptr);
	*(uint32_t *)tokens[14].ptr = pt_to_utc(*(uint32_t*)tokens[14].ptr);

	return 0;
}

/* Historical caveat: Switch to UTC. */
int parse(const char *str, token *tokens, const size_t tokenCount)
{
	/* Preconditions */
	assert(tokenCount > 14);

	parser(str, tokens, tokenCount);

	/* If we have a bad range, or an issued time earlier than reported
	 * return bad value.
	*/
	if ((*(int8_t *)tokens[12].ptr == -2) ||
	    (*(uint32_t *)tokens[10].ptr > *(uint32_t *)tokens[14].ptr)) {
		return 1;
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
		return parse_pt_bo;
	} else if (parsedTime < D20071001) {
		return parse_pt;
	} else {
		return parse;
	}
}
