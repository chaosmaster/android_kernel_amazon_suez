#include <linux/kernel.h>
#include <linux/module.h>

#include "../../core/met_drv.h"
#include "plf_init.h"

static const char strTopology[] = "LITTLE:0,1|BIG:2,3";

static int __init met_plf_init(void)
{
#if NO_MET_EXT_DEV == 0
	int i = 0;
	met_ext_dev_max = met_ext_dev_lock(1);

	for (i = 0; i < met_ext_dev_max; i++) {
		if (met_ext_dev2[i] != NULL)
			met_register(met_ext_dev2[i]);
	}
#endif
	met_register(&met_emi);
	met_register(&met_cci400);
	met_register(&met_thermal);
	met_register(&met_gpudvfs);
	met_register(&met_gpu);
	met_register(&met_gpupwr);

	met_set_platform("mt8173", 1);
	met_set_topology(strTopology, 1);

	return 0;
}

static void __exit met_plf_exit(void)
{

#if NO_MET_EXT_DEV == 0
	int i = 0;
	for (i = 0; i < met_ext_dev_max; i++) {
		if (met_ext_dev2[i] != NULL)
			met_deregister(met_ext_dev2[i]);
	}
	met_ext_dev_lock(0);
#endif
	met_deregister(&met_emi);
	met_deregister(&met_cci400);
	met_deregister(&met_thermal);
	met_deregister(&met_gpudvfs);
	met_deregister(&met_gpu);
	met_deregister(&met_gpupwr);

	met_set_platform(NULL, 0);
	met_set_topology(NULL, 0);

}
module_init(met_plf_init);
module_exit(met_plf_exit);
MODULE_AUTHOR("DT_DM5");
MODULE_DESCRIPTION("MET_MT8173");
MODULE_LICENSE("GPL");
