#include "eve_txn.h"

void
print_eve_txn(struct eve_txn *t)
{
	assert(t != NULL);

#if 0
	printf(" %llu ", t->orderID);
	printf(" %u ", t->regionID);
	printf(" %u ", t->systemID);
	printf(" %u ", t->stationID);
	printf(" %u ", t->typeID);
	printf(" %u ", t->bid);
	printf(" %llu ", t->price);
	printf(" %u ", t->volMin);
	printf(" %u ", t->volRem);
	printf(" %u ", t->volEnt);
	printf(" %u ", t->issued);
	printf(" %u ", t->duration);
	printf(" %d ", t->range);
	printf(" %llu ", t->reportedby);
	printf(" %u \n", t->rtime);
#endif

	printf("%llu %u %u %u %u %u %llu %u %u %u %u %u %d %llu %u\n",
	    t->orderID, t->regionID, t->systemID, t->stationID, t->typeID,
	    t->bid, t->price, t->volMin, t->volRem, t->volEnt, t->issued,
	    t->duration, t->range, t->reportedby, t->rtime);
	return;
}
