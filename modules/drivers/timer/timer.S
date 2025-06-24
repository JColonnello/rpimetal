#include <drivers/base.h>
#include "timer.h"

.global stop_generic_timer
stop_generic_timer:
	msr CNTP_CTL_EL0, xzr
	eret
