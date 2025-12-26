#include <drivers/display.h>
#include <resources/zorzal.h>
#include <stddef.h>

int kernel_start()
{
	lfb_showpicture(header_data, height, width);

	return 0;
}
