#include "eve_txn.h"

void
print_eve_txn(struct eve_txn *t)
{
	{ /* Preconditions */
		assert(t != NULL);
	}
	/* Print the struct in input order. */
	printf("%llu %u %u %u %u %u %llu %u %u %u %u %u %d %llu %u\n",
	    t->orderID, t->regionID, t->systemID, t->stationID, t->typeID,
	    t->bid, t->price, t->volMin, t->volRem, t->volEnt, t->issued,
	    t->duration, t->range, t->reportedby, t->rtime);
	return;
}
