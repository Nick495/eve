#ifndef EVE_PARSER_H_
#define EVE_PARSER_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct rawRecord {
	uint64_t	orderid;	/* 1  |  8 OrderID */
	uint64_t	price;		/* 2  | 16 Price (.1 ISK increments) */
	uint64_t	reportedby;	/* 3  | 24 */
	uint32_t	issued;		/* 4  | 28 Issued (date-time) */
	uint32_t	regionid;	/* 5  | 32 Region ID */
	uint32_t	systemid;	/* 6  | 36 System ID  */
	uint32_t	stationid;	/* 7  | 40 Station ID */
	uint32_t	typeid;		/* 8  | 44 Item type ID. */
	uint32_t	volmin;		/* 9  | 48 Minimum Vol per txn */
	uint32_t	volrem;		/* 10 | 52 Remaining Volume */
	uint32_t	volent;		/* 11 | 56 Entered Volume */
	uint32_t	reportedtime;	/* 12 | 60 (date-time) */
	uint16_t	duration;	/* 13 | 62 Duration 0-90 (in days) */
	uint8_t		range;		/* 14 | 63 Reference:
		http://eveonline-third-party-documentation.readthedocs.org/
		en/latest/xmlapi/enumerations/#order-range
                             * Buy range: -1 = Station, 0 = Solar System,
                             * 5, 10, 20, or 40 Jumps 32767 Region.
                             * In our code: -1 = Station, 0 = Solar System,
                             * 5, 10, 20, or 40 Jumps 127 =  Region.
					*/
	uint8_t		bid;		/* 15 | 64 1 = buy, 0 = sell. */
};

#define rawRecordSize 64

void print_raw_record(struct rawRecord r);

typedef int (*Parser)(struct rawRecord *result);

Parser parser_factory(uint64_t);

uint32_t ejday(const uint32_t year, const uint32_t mon, const uint32_t d);

#endif /* EVE_LOADER_H_ */
