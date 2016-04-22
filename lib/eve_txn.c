#include "eve_txn.h"

void
print_eve_txn(struct eve_txn *t)
{
	assert(r != NULL);

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
	printf(" %u ", t->range);
	printf(" %llu ", t->reportedby);
	printf(" %u \n", t->rtime);

	return;
}
