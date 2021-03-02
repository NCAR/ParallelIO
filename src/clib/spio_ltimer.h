#ifndef __SPIO_LTIMER_H__
#define __SPIO_LTIMER_H__

void spio_ltimer_start(const char *timer_name);
void spio_ltimer_stop(const char *timer_name);
double spio_ltimer_get_wtime(const char *timer_name);

#endif /* __SPIO_LTIMER_H__ */

