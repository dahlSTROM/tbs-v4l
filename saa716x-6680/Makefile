saa716x_core-objs	:= saa716x_pci.o	\
			   saa716x_i2c.o	\
			   saa716x_cgu.o	\
			   saa716x_msi.o	\
			   saa716x_dma.o	\
			   saa716x_vip.o	\
			   saa716x_aip.o	\
			   saa716x_phi.o	\
			   saa716x_boot.o	\
			   saa716x_fgpi.o	\
			   saa716x_adap.o	\
			   saa716x_gpio.o	\
			   saa716x_greg.o	\
			   saa716x_spi.o

obj-$(CONFIG_SAA716X_CORE)	+= saa716x_core.o
obj-$(CONFIG_DVB_SAA716X_TBS)	+= saa716x_tbs-dvb.o
saa716x_tbs-dvb-objs += saa716x_tbs.o saa716x_input.o tbsctrl.o tbsci-i2c.o tbsci.o tbsmac.o

EXTRA_CFLAGS = -Idrivers/media/dvb-core/
