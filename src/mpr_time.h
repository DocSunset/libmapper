#ifndef TIME_H_INCLUDED
#define TIME_H_INCLUDED

#include <mapper/mapper_constants.h>

/*! Get the current time. */
double mpr_get_current_time(void);

/*! Return the difference in seconds between two mpr_times.
 *  \param minuend      The minuend.
 *  \param subtrahend   The subtrahend.
 *  \return             The difference a-b in seconds. */
double mpr_time_get_diff(mpr_time minuend, mpr_time subtrahend);

#endif // TIME_H_INCLUDED

