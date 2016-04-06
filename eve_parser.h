#ifndef EVE_PARSER_H_
#define EVE_PARSER_H_

#include <assert.h> /* assert() */
#include <stdlib.h> /* NULL */
#include <stdint.h> /* uint*_t */
#include <ctype.h>  /* isdigit() */

struct Raw_Record {
	/*type | name | Position in input | Description. Ordered by size & pos*/
	uint64_t	orderID;        /*  1 OrderID */
	uint64_t	price;          /*  7 Price (.1 ISK increments) */
	uint64_t	reportedby;		/* 14 Person reported by */
	uint32_t	regionID;       /*  2 Region ID (location) */
	uint32_t	systemID;       /*  3 System ID (location) */
	uint32_t	stationID;      /*  4 Station ID (location) */
	uint32_t	typeID;         /*  5 Item type ID. */
	uint32_t	volMin;         /*  8 Minimum Volume (per txn) */
	uint32_t	volRem;         /*  9 Remaining Volume (txn) */
	uint32_t	volEnt;         /* 10 (original volume in txn ) */
	uint32_t	issued;         /* 11 Issued (date-time) */
	uint32_t	rtime;			/* 15 Time reported. */
	uint16_t	duration;       /* 12 Duration in days, 0-90 */
	int8_t		range;          /* 13 Reference: http://eveonline-third-party-documentation.readthedocs.org/en/latest/xmlapi/enumerations/#order-range
					* Buy range -1 = Station,
					* 0 = Solar System,
					* 5, 10, 20, or 40 Jumps 32767 Region.
					* In our code, -2 = region,
					* -1 = station, 0 = SS,
					* 5, 10, 20, and 40 jumps.
                                        */
	uint8_t		bid;			/* 6 1 = buy, 0 = sell. (bid, offer) */
};

typedef int (*Parser)(const char *str, struct Raw_Record *rec);

Parser
parser_factory(const uint32_t yr, const uint32_t mn, const uint32_t dy);

#endif
