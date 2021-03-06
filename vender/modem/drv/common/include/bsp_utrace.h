/*************************************************************************
*   版权所有(C) 2010-2015, 深圳华为技术有限公司.
*
*   文 件 名 :  bsp_utrace.h
*
*
*   描    述 :  CoreSight Trace
*
*   修改记录 :
*
*************************************************************************/
#ifndef _BSP_UTRACE_H_
#define _BSP_UTRACE_H_
    
#ifdef __cplusplus
    extern "C"
    {
#endif

#include "osl_types.h"

#ifdef ENABLE_BUILD_UTRACE

s32 bsp_utrace_init(void);
void bsp_utrace_stop(void);
void bsp_utrace_suspend(void);
void bsp_utrace_resume(void);

#else

static s32 inline bsp_utrace_init(void)
{
    return 0;
}

static void inline bsp_utrace_stop(void)
{
    return;
}

static void inline bsp_utrace_suspend(void)
{
    return;
}

static void inline bsp_utrace_resume(void)
{
    return;
}

#endif


#ifdef __cplusplus
}
#endif

#endif /* _BSP_UTRACE_H_ */


