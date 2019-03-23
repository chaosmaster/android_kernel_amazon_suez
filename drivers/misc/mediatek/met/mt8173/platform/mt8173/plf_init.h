#ifndef __PLF_INIT_H__
#define __PLF_INIT_H__

extern struct metdevice met_emi;
extern struct metdevice met_cci400;
extern struct metdevice met_thermal;
extern struct metdevice met_gpudvfs;
extern struct metdevice met_gpu;
extern struct metdevice met_gpupwr;

extern unsigned int mt_get_emi_freq(void);

#ifndef NO_MET_EXT_DEV
#define NO_MET_EXT_DEV 1
#endif
#if NO_MET_EXT_DEV == 0
extern struct metdevice *met_ext_dev2[];
extern int met_ext_dev_lock(int flag);
extern int met_ext_dev_add(struct metdevice *metdev);
extern int met_ext_dev_del(struct metdevice *metdev);
static int met_ext_dev_max;
#endif

#endif /* __PLF_INIT_H__ */
