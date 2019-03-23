#ifndef _MT_GPU_METMONITOR_H_
#define _MT_GPU_METMONITOR_H_

/* extern struct metdevice met_gpu; */

/*
    GPU monitor HAL comes from alps\mediatek\kernel\include\linux\mtk_gpu_utility.h

    mtk_get_gpu_memory_usage(unsigned int* pMemUsage) in unit of bytes

    mtk_get_gpu_xxx_loading are in unit of %
*/
extern bool mtk_get_gpu_memory_usage(unsigned int *pMemUsage);

extern bool mtk_get_gpu_loading(unsigned int *pLoading);

extern bool mtk_get_gpu_block(unsigned int *pBlock);

extern bool mtk_get_gpu_idle(unsigned int *pIlde);

extern bool mtk_get_gpu_GP_loading(unsigned int *pLoading);

extern bool mtk_get_gpu_PP_loading(unsigned int *pLoading);

extern bool mtk_get_gpu_power_loading(unsigned int *pLoading);

extern bool mtk_get_gpu_freq(unsigned int *pFreq);
extern void mt_gpufreq_setfreq_registerCB(sampler_func pCB);

#endif /* _MT_GPU_METMONITOR_H_ */
