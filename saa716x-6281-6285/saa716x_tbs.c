#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/mutex.h>

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <linux/kmod.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/device.h>

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#include <linux/i2c.h>

#include "saa716x_mod.h"

#include "saa716x_msi_reg.h"
#include "saa716x_gpio_reg.h"
#include "saa716x_dma_reg.h"
#include "saa716x_fgpi_reg.h"
#include "saa716x_greg_reg.h"

#include "saa716x_vip.h"
#include "saa716x_aip.h"
#include "saa716x_msi.h"
#include "saa716x_adap.h"
#include "saa716x_gpio.h"
#include "saa716x_spi.h"
#include "saa716x_priv.h"

#include "saa716x_input.h"

#include "saa716x_tbs.h"
#include "tbsctrl.h"

#include "tbsci-i2c.h"
#include "tbsci.h"

#include "tbs62x1fe.h"

#include "tbsmac.h"

unsigned int verbose;
module_param(verbose, int, 0644);
MODULE_PARM_DESC(verbose, "verbose startup messages, default is 1 (yes)");

unsigned int int_type;
module_param(int_type, int, 0644);
MODULE_PARM_DESC(int_type, "force Interrupt Handler type: 0=INT-A, 1=MSI, 2=MSI-X. default INT-A mode");

unsigned int ci_mode;
module_param(ci_mode, int, 0644);
MODULE_PARM_DESC(ci_mode, "for internal use only: default 0");

unsigned int ci_spd;
module_param(ci_spd, int, 0644);
MODULE_PARM_DESC(ci_spd, "for internal use only: default 0");

static unsigned int enable_ir = 1;
module_param(enable_ir, int, 0644);
MODULE_PARM_DESC(enable_ir, "Enable IR support for TBS cards: default 1");

#define DRIVER_NAME "SAA716x TBS"

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
static int __devinit saa716x_tbs_pci_probe(struct pci_dev *pdev, const struct pci_device_id *pci_id)
#else
static int saa716x_tbs_pci_probe(struct pci_dev *pdev, const struct pci_device_id *pci_id)
#endif
{
	struct saa716x_dev *saa716x;
	int err = 0;
	u32 data;

	saa716x = kzalloc(sizeof (struct saa716x_dev), GFP_KERNEL);
	if (saa716x == NULL) {
		printk(KERN_ERR "saa716x_tbs_pci_probe ERROR: out of memory\n");
		err = -ENOMEM;
		goto fail0;
	}

	saa716x->verbose	= verbose;
	saa716x->int_type	= int_type;
	saa716x->pdev		= pdev;
	saa716x->config	= (struct saa716x_config *) pci_id->driver_data;

	err = saa716x_pci_init(saa716x);
	if (err) {
		dprintk(SAA716x_ERROR, 1, "SAA716x PCI Initialization failed");
		goto fail1;
	}

	err = saa716x_cgu_init(saa716x);
	if (err) {
		dprintk(SAA716x_ERROR, 1, "SAA716x CGU Init failed");
		goto fail1;
	}

	err = saa716x_core_boot(saa716x);
	if (err) {
		dprintk(SAA716x_ERROR, 1, "SAA716x Core Boot failed");
		goto fail2;
	}
	dprintk(SAA716x_DEBUG, 1, "SAA716x Core Boot Success");

	err = saa716x_msi_init(saa716x);
	if (err) {
		dprintk(SAA716x_ERROR, 1, "SAA716x MSI Init failed");
		goto fail2;
	}

	err = saa716x_jetpack_init(saa716x);
	if (err) {
		dprintk(SAA716x_ERROR, 1, "SAA716x Jetpack core initialization failed");
		goto fail1;
	}

	if (ci_spd) {
		if ((saa716x->config->model_name[17] == 0x39) &&
			(saa716x->config->model_name[18] == 0x31))
		{
				saa716x->config->i2c_rate[0] = SAA716x_I2C_RATE_100;
                                saa716x->config->i2c_rate[1] = SAA716x_I2C_RATE_100;
		}

		if ((saa716x->config->model_name[18] == 0x38) ||
			((saa716x->config->model_name[16] == 0x36) &&
			(saa716x->config->model_name[17] == 0x38)))
			saa716x->config->i2c_rate[1] = SAA716x_I2C_RATE_100;
	}

	err = saa716x_i2c_init(saa716x);
	if (err) {
		dprintk(SAA716x_ERROR, 1, "SAA716x I2C Initialization failed");
		goto fail3;
	}
	saa716x_gpio_init(saa716x);

	if (enable_ir) {
		data = SAA716x_EPRD(MSI, MSI_CONFIG37);
		data &= 0xFCFFFFFF;
		data |= MSI_INT_POL_EDGE_ANY;
		SAA716x_EPWR(MSI, MSI_CONFIG37, data);
		SAA716x_EPWR(MSI, MSI_INT_ENA_SET_H, MSI_INT_EXTINT_4);

		saa716x_gpio_set_input(saa716x, 4);
		msleep(1);
	
		saa716x_input_init(saa716x);
	}

	err = saa716x_dvb_init(saa716x);
	if (err) {
		dprintk(SAA716x_ERROR, 1, "SAA716x DVB initialization failed");
		goto fail4;
	}

	return 0;

fail4:
	saa716x_dvb_exit(saa716x);
fail3:
	saa716x_i2c_exit(saa716x);
fail2:
	saa716x_pci_exit(saa716x);
fail1:
	kfree(saa716x);
fail0:
	return err;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
static void __devexit saa716x_tbs_pci_remove(struct pci_dev *pdev)
#else
static void saa716x_tbs_pci_remove(struct pci_dev *pdev)
#endif
{
	struct saa716x_dev *saa716x = pci_get_drvdata(pdev);
	struct saa716x_adapter *saa716x_adap = saa716x->saa716x_adap;
	int i;

	for (i = 0; i < saa716x->config->adapters; i++) {
		if (saa716x_adap->tbsci) {
			tbsci_release(saa716x_adap);
			tbsci_i2c_remove(saa716x_adap);
		}
		saa716x_adap++;
	}
	
	if (enable_ir) {
		SAA716x_EPWR(MSI, MSI_INT_ENA_CLR_H, MSI_INT_EXTINT_4);
		saa716x_input_fini(saa716x);
	}

	saa716x_dvb_exit(saa716x);
	saa716x_i2c_exit(saa716x);
	saa716x_pci_exit(saa716x);
	kfree(saa716x);
}

static irqreturn_t saa716x_tbs6281_pci_irq(int irq, void *dev_id)
{
	struct saa716x_dev *saa716x	= (struct saa716x_dev *) dev_id;

	u32 stat_h, stat_l, mask_h, mask_l;
	u32 fgpiStatus;
	u32 activeBuffer;

	if (unlikely(saa716x == NULL)) {
		printk("%s: saa716x=NULL", __func__);
		return IRQ_NONE;
	}

	stat_l = SAA716x_EPRD(MSI, MSI_INT_STATUS_L);
	stat_h = SAA716x_EPRD(MSI, MSI_INT_STATUS_H);
	mask_l = SAA716x_EPRD(MSI, MSI_INT_ENA_L);
	mask_h = SAA716x_EPRD(MSI, MSI_INT_ENA_H);

	dprintk(SAA716x_DEBUG, 1, "MSI STAT L=<%02x> H=<%02x>, CTL L=<%02x> H=<%02x>",
		stat_l, stat_h, mask_l, mask_h);

	if (!((stat_l & mask_l) || (stat_h & mask_h)))
		return IRQ_NONE;

	if (stat_l)
		SAA716x_EPWR(MSI, MSI_INT_STATUS_CLR_L, stat_l);

	if (stat_h)
		SAA716x_EPWR(MSI, MSI_INT_STATUS_CLR_H, stat_h);

	if (enable_ir) {
		if (stat_h & MSI_INT_EXTINT_4)
			saa716x_input_irq_handler(saa716x);
	}

	if (stat_l) {
		if (stat_l & MSI_INT_TAGACK_FGPI_1) {

			fgpiStatus = SAA716x_EPRD(FGPI1, INT_STATUS);
			activeBuffer = (SAA716x_EPRD(BAM, BAM_FGPI1_DMA_BUF_MODE) >> 3) & 0x7;
			dprintk(SAA716x_DEBUG, 1, "fgpiStatus = %04X, buffer = %d",
				fgpiStatus, activeBuffer);
			if (activeBuffer > 0)
				activeBuffer -= 1;
			else
				activeBuffer = 7;
			if (saa716x->fgpi[1].dma_buf[activeBuffer].mem_virt) {
				u8 * data = (u8 *)saa716x->fgpi[1].dma_buf[activeBuffer].mem_virt;
				dprintk(SAA716x_DEBUG, 1, "%02X%02X%02X%02X",
					data[0], data[1], data[2], data[3]);
				dvb_dmx_swfilter_packets(&saa716x->saa716x_adap[0].demux, data, 348);
			}
			if (fgpiStatus) {
				SAA716x_EPWR(FGPI1, INT_CLR_STATUS, fgpiStatus);
			}
		}
		if (stat_l & MSI_INT_TAGACK_FGPI_3) {

			fgpiStatus = SAA716x_EPRD(FGPI3, INT_STATUS);
			activeBuffer = (SAA716x_EPRD(BAM, BAM_FGPI3_DMA_BUF_MODE) >> 3) & 0x7;
			dprintk(SAA716x_DEBUG, 1, "fgpiStatus = %04X, buffer = %d",
				fgpiStatus, activeBuffer);
			if (activeBuffer > 0)
				activeBuffer -= 1;
			else
				activeBuffer = 7;
			if (saa716x->fgpi[3].dma_buf[activeBuffer].mem_virt) {
				u8 * data = (u8 *)saa716x->fgpi[3].dma_buf[activeBuffer].mem_virt;
				dprintk(SAA716x_DEBUG, 1, "%02X%02X%02X%02X",
					data[0], data[1], data[2], data[3]);
				dvb_dmx_swfilter_packets(&saa716x->saa716x_adap[1].demux, data, 348);
			}
			if (fgpiStatus) {
				SAA716x_EPWR(FGPI3, INT_CLR_STATUS, fgpiStatus);
			}
		}
	}

	saa716x_msi_event(saa716x, stat_l, stat_h);

	return IRQ_HANDLED;
}

static irqreturn_t saa716x_tbs6285_pci_irq(int irq, void *dev_id)
{
	struct saa716x_dev *saa716x	= (struct saa716x_dev *) dev_id;

	u32 stat_h, stat_l, mask_h, mask_l;
	u32 fgpiStatus;
	u32 activeBuffer;

	if (unlikely(saa716x == NULL)) {
		printk("%s: saa716x=NULL", __func__);
		return IRQ_NONE;
	}

	stat_l = SAA716x_EPRD(MSI, MSI_INT_STATUS_L);
	stat_h = SAA716x_EPRD(MSI, MSI_INT_STATUS_H);
	mask_l = SAA716x_EPRD(MSI, MSI_INT_ENA_L);
	mask_h = SAA716x_EPRD(MSI, MSI_INT_ENA_H);

	dprintk(SAA716x_DEBUG, 1, "MSI STAT L=<%02x> H=<%02x>, CTL L=<%02x> H=<%02x>",
		stat_l, stat_h, mask_l, mask_h);

	if (!((stat_l & mask_l) || (stat_h & mask_h)))
		return IRQ_NONE;

	if (stat_l)
		SAA716x_EPWR(MSI, MSI_INT_STATUS_CLR_L, stat_l);

	if (stat_h)
		SAA716x_EPWR(MSI, MSI_INT_STATUS_CLR_H, stat_h);

	if (enable_ir) {
		if (stat_h & MSI_INT_EXTINT_4)
			saa716x_input_irq_handler(saa716x);
	}

	if (stat_l) {
		if (stat_l & MSI_INT_TAGACK_FGPI_0) {

			fgpiStatus = SAA716x_EPRD(FGPI0, INT_STATUS);
			activeBuffer = (SAA716x_EPRD(BAM, BAM_FGPI0_DMA_BUF_MODE) >> 3) & 0x7;
			dprintk(SAA716x_DEBUG, 1, "fgpiStatus = %04X, buffer = %d",
				fgpiStatus, activeBuffer);
			if (activeBuffer > 0)
				activeBuffer -= 1;
			else
				activeBuffer = 7;
			if (saa716x->fgpi[0].dma_buf[activeBuffer].mem_virt) {
				u8 * data = (u8 *)saa716x->fgpi[0].dma_buf[activeBuffer].mem_virt;
				dprintk(SAA716x_DEBUG, 1, "%02X%02X%02X%02X",
					data[0], data[1], data[2], data[3]);
				dvb_dmx_swfilter_packets(&saa716x->saa716x_adap[3].demux, data, 348);
			}
			if (fgpiStatus) {
				SAA716x_EPWR(FGPI0, INT_CLR_STATUS, fgpiStatus);
			}
		}
		if (stat_l & MSI_INT_TAGACK_FGPI_1) {

			fgpiStatus = SAA716x_EPRD(FGPI1, INT_STATUS);
			activeBuffer = (SAA716x_EPRD(BAM, BAM_FGPI1_DMA_BUF_MODE) >> 3) & 0x7;
			dprintk(SAA716x_DEBUG, 1, "fgpiStatus = %04X, buffer = %d",
				fgpiStatus, activeBuffer);
			if (activeBuffer > 0)
				activeBuffer -= 1;
			else
				activeBuffer = 7;
			if (saa716x->fgpi[1].dma_buf[activeBuffer].mem_virt) {
				u8 * data = (u8 *)saa716x->fgpi[1].dma_buf[activeBuffer].mem_virt;
				dprintk(SAA716x_DEBUG, 1, "%02X%02X%02X%02X",
					data[0], data[1], data[2], data[3]);
				dvb_dmx_swfilter_packets(&saa716x->saa716x_adap[2].demux, data, 348);
			}
                        if (fgpiStatus) {
				SAA716x_EPWR(FGPI1, INT_CLR_STATUS, fgpiStatus);
			}
		}
		if (stat_l & MSI_INT_TAGACK_FGPI_2) {

			fgpiStatus = SAA716x_EPRD(FGPI2, INT_STATUS);
			activeBuffer = (SAA716x_EPRD(BAM, BAM_FGPI2_DMA_BUF_MODE) >> 3) & 0x7;
			dprintk(SAA716x_DEBUG, 1, "fgpiStatus = %04X, buffer = %d",
				fgpiStatus, activeBuffer);
			if (activeBuffer > 0)
				activeBuffer -= 1;
			else
				activeBuffer = 7;
			if (saa716x->fgpi[2].dma_buf[activeBuffer].mem_virt) {
				u8 * data = (u8 *)saa716x->fgpi[2].dma_buf[activeBuffer].mem_virt;
				dprintk(SAA716x_DEBUG, 1, "%02X%02X%02X%02X",
					data[0], data[1], data[2], data[3]);
				dvb_dmx_swfilter_packets(&saa716x->saa716x_adap[1].demux, data, 348);
			}
			if (fgpiStatus) {
				SAA716x_EPWR(FGPI2, INT_CLR_STATUS, fgpiStatus);
			}
		}
		if (stat_l & MSI_INT_TAGACK_FGPI_3) {
			
			fgpiStatus = SAA716x_EPRD(FGPI3, INT_STATUS);
			activeBuffer = (SAA716x_EPRD(BAM, BAM_FGPI3_DMA_BUF_MODE) >> 3) & 0x7;
			dprintk(SAA716x_DEBUG, 1, "fgpiStatus = %04X, buffer = %d",
				fgpiStatus, activeBuffer);
				if (activeBuffer > 0)
					activeBuffer -= 1;
					else
						activeBuffer = 7;
				if (saa716x->fgpi[3].dma_buf[activeBuffer].mem_virt) {
						u8 * data = (u8 *)saa716x->fgpi[3].dma_buf[activeBuffer].mem_virt;
						dprintk(SAA716x_DEBUG, 1, "%02X%02X%02X%02X",
							data[0], data[1], data[2], data[3]);
					dvb_dmx_swfilter_packets(&saa716x->saa716x_adap[0].demux, data, 348);
				}
				if (fgpiStatus) {
					SAA716x_EPWR(FGPI3, INT_CLR_STATUS, fgpiStatus);
					}
				}
	}

	saa716x_msi_event(saa716x, stat_l, stat_h);

	return IRQ_HANDLED;
}

static int load_config_tbs6281(struct saa716x_dev *saa716x)
{
	int ret = 0;

	return ret;
}

static int load_config_tbs6285(struct saa716x_dev *saa716x)
{
	int ret = 0;

	return ret;
}

#define SAA716x_MODEL_TURBOSIGHT_TBS6281 "TurboSight TBS 6281"
#define SAA716x_DEV_TURBOSIGHT_TBS6281   "DVB-T/T2/C"

static struct tbs62x1fe_config tbs6281fe_config = {
	.tbs62x1fe_address = 0x64,

	.tbs62x1_ctrl1 = tbsctrl1,
	.tbs62x1_ctrl2 = tbsctrl2,
};

static int saa716x_tbs6281_frontend_attach(struct saa716x_adapter *adapter, int count)
{
	struct saa716x_dev *saa716x = adapter->saa716x;
	struct saa716x_i2c *i2c0 = &saa716x->i2c[0];
	struct saa716x_i2c *i2c1 = &saa716x->i2c[1];
	u8 mac[6];

	if (count == 0 || count == 1) {
		saa716x_gpio_set_output(saa716x, count ? 2 : 16);
		msleep(1);
		saa716x_gpio_write(saa716x, count ? 2 : 16, 0);
		msleep(50);
		saa716x_gpio_write(saa716x, count ? 2 : 16, 1);
		msleep(100);

		dprintk(SAA716x_ERROR, 1, "Probing for TBS6281FE %d", count);
		adapter->fe = dvb_attach(tbs62x1fe_attach, &tbs6281fe_config,
                                	count ? &i2c1->i2c_adapter : &i2c0->i2c_adapter);

		if (!adapter->fe)
			goto exit;

		tbs_read_mac(&i2c1->i2c_adapter, 160 + 16*count, mac);
		memcpy(adapter->dvb_adapter.proposed_mac, mac, 6);
		printk(KERN_INFO "TurboSight TBS6281 DVB-T2 card MAC=%pM\n",
			adapter->dvb_adapter.proposed_mac);

		dprintk(SAA716x_ERROR, 1, "Done!");
	}

	return 0;
exit:
	printk(KERN_ERR "%s: frontend initialization failed\n",
					adapter->saa716x->config->model_name);
	dprintk(SAA716x_ERROR, 1, "Frontend attach failed");
	return -ENODEV;
}

static struct saa716x_config saa716x_tbs6281_config = {
	.model_name		= SAA716x_MODEL_TURBOSIGHT_TBS6281,
	.dev_type		= SAA716x_DEV_TURBOSIGHT_TBS6281,
	.boot_mode		= SAA716x_EXT_BOOT,
	.load_config		= &load_config_tbs6281,
	.adapters		= 2,
	.frontend_attach	= saa716x_tbs6281_frontend_attach,
	.irq_handler		= saa716x_tbs6281_pci_irq,
	.i2c_rate[0]		= SAA716x_I2C_RATE_400,
	.i2c_rate[1]            = SAA716x_I2C_RATE_400,
	.adap_config		= {
		{
			/* adapter 0 */
			.ts_port = 1
		},
		{
			/* adapter 1 */
			.ts_port = 3
		},
	}
};


#define SAA716x_MODEL_TURBOSIGHT_TBS6285 "TurboSight TBS 6285"
#define SAA716x_DEV_TURBOSIGHT_TBS6285   "DVB-T/T2/C"

static struct tbs62x1fe_config tbs6285fe_config0 = {
	.tbs62x1fe_address = 0x64,

	.tbs62x1_ctrl1 = tbsctrl1,
	.tbs62x1_ctrl2 = tbsctrl2,
};

static struct tbs62x1fe_config tbs6285fe_config1 = {
	.tbs62x1fe_address = 0x66,

	.tbs62x1_ctrl1 = tbsctrl1,
	.tbs62x1_ctrl2 = tbsctrl2,
};

static int saa716x_tbs6285_frontend_attach(struct saa716x_adapter *adapter, int count)
{
	struct saa716x_dev *saa716x = adapter->saa716x;
	struct saa716x_i2c *i2c0 = &saa716x->i2c[0];
	struct saa716x_i2c *i2c1 = &saa716x->i2c[1];
	u8 mac[6];

	if (count == 2) {
		dprintk(SAA716x_ERROR, 1, "Probing for TBS62x1FE %d", count);
		adapter->fe = dvb_attach(tbs62x1fe_attach, &tbs6285fe_config0,
                                			&i2c0->i2c_adapter);

		if (!adapter->fe)
			goto exit;

		tbs_read_mac(&i2c0->i2c_adapter, 160 + 16*count, mac);
		memcpy(adapter->dvb_adapter.proposed_mac, mac, 6);
		printk(KERN_INFO "TurboSight TBS6285 DVB-T2 card port%d MAC=%pM\n",
			count, adapter->dvb_adapter.proposed_mac);

		dprintk(SAA716x_ERROR, 1, "Done!");
	}

	if (count == 3) {
		dprintk(SAA716x_ERROR, 1, "Probing for TBS62x1FE %d", count);
		adapter->fe = dvb_attach(tbs62x1fe_attach, &tbs6285fe_config1,
                                			&i2c0->i2c_adapter);
		if (!adapter->fe)
			goto exit;

		tbs_read_mac(&i2c0->i2c_adapter, 160 + 16*count, mac);
		memcpy(adapter->dvb_adapter.proposed_mac, mac, 6);
		printk(KERN_INFO "TurboSight TBS6285 DVB-T2 card port%d MAC=%pM\n",
			count, adapter->dvb_adapter.proposed_mac);

		dprintk(SAA716x_ERROR, 1, "Done!");
	}

	if (count == 0) {
		dprintk(SAA716x_ERROR, 1, "Probing for TBS62x1FE %d", count);
		adapter->fe = dvb_attach(tbs62x1fe_attach, &tbs6285fe_config0,
                                			&i2c1->i2c_adapter);

		if (!adapter->fe)
			goto exit;

		tbs_read_mac(&i2c0->i2c_adapter, 160 + 16*count, mac);
		memcpy(adapter->dvb_adapter.proposed_mac, mac, 6);
		printk(KERN_INFO "TurboSight TBS6285 DVB-T2 card port%d MAC=%pM\n",
			count, adapter->dvb_adapter.proposed_mac);

		dprintk(SAA716x_ERROR, 1, "Done!");
	}

	if (count == 1) {
		dprintk(SAA716x_ERROR, 1, "Probing for TBS62x1FE %d", count);
		adapter->fe = dvb_attach(tbs62x1fe_attach, &tbs6285fe_config1,
                                			&i2c1->i2c_adapter);
		if (!adapter->fe)
			goto exit;

		tbs_read_mac(&i2c0->i2c_adapter, 160 + 16*count, mac);
		memcpy(adapter->dvb_adapter.proposed_mac, mac, 6);
		printk(KERN_INFO "TurboSight TBS6285 DVB-T2 card port%d MAC=%pM\n",
			count, adapter->dvb_adapter.proposed_mac);

		dprintk(SAA716x_ERROR, 1, "Done!");
	}

	return 0;
exit:
	printk(KERN_ERR "%s: frontend initialization failed\n",
					adapter->saa716x->config->model_name);
	dprintk(SAA716x_ERROR, 1, "Frontend attach failed");
	return -ENODEV;
}

static struct saa716x_config saa716x_tbs6285_config = {
	.model_name		= SAA716x_MODEL_TURBOSIGHT_TBS6285,
	.dev_type		= SAA716x_DEV_TURBOSIGHT_TBS6285,
	.boot_mode		= SAA716x_EXT_BOOT,
	.load_config		= &load_config_tbs6285,
	.adapters		= 4,
	.frontend_attach	= saa716x_tbs6285_frontend_attach,
	.irq_handler		= saa716x_tbs6285_pci_irq,
	.i2c_rate[0]		= SAA716x_I2C_RATE_400,
	.i2c_rate[1]            = SAA716x_I2C_RATE_400,
	.adap_config		= {
		{
			/* adapter 0 */
			.ts_port = 3
		},
		{
			/* adapter 1 */
			.ts_port = 2
		},
		{
			/* adapter 2 */
			.ts_port = 1
		},
		{
			/* adapter 3 */
			.ts_port = 0
		}
	}
};

static struct pci_device_id saa716x_tbs_pci_table[] = {
	MAKE_ENTRY(TURBOSIGHT_TBS6281_SUBVENDOR, TURBOSIGHT_TBS6281_SUBDEVICE, SAA7160, &saa716x_tbs6281_config),
	MAKE_ENTRY(TURBOSIGHT_TBS6285_SUBVENDOR, TURBOSIGHT_TBS6285_SUBDEVICE, SAA7160, &saa716x_tbs6285_config),
	{ }
};
MODULE_DEVICE_TABLE(pci, saa716x_tbs_pci_table);

static struct pci_driver saa716x_tbs_pci_driver = {
	.name		= DRIVER_NAME,
	.id_table	= saa716x_tbs_pci_table,
	.probe		= saa716x_tbs_pci_probe,
	.remove		= saa716x_tbs_pci_remove,
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
static int __devinit saa716x_tbs_init(void)
#else
static int saa716x_tbs_init(void)
#endif
{
	return pci_register_driver(&saa716x_tbs_pci_driver);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
static void __devexit saa716x_tbs_exit(void)
#else
static void saa716x_tbs_exit(void)
#endif
{
	return pci_unregister_driver(&saa716x_tbs_pci_driver);
}

module_init(saa716x_tbs_init);
module_exit(saa716x_tbs_exit);

MODULE_DESCRIPTION("SAA716x TBS driver");
MODULE_AUTHOR("Konstantin Dimitrov <kosio.dimitrov@gmail.com>");
MODULE_LICENSE("GPL");
