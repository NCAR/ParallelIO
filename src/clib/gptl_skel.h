#ifndef __GPTL_SKEL_H__
#define __GPTL_SKEL_H__

/* Defining GPTL macros to noops allows us to write cleaner
 * code without #ifdef TIMING blocks around these function
 * names
 */
#ifndef TIMING
#define GPTLstart(tname) do{ }while(0)
#define GPTLstop(tname) do{ }while(0)
#endif

#endif /* __GPTL_SKEL_H__ */
