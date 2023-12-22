#include "include.h"
#include "arm_arch.h"
#include "typedef.h"
#include "arm_arch.h"
#include "sys_config.h"

#if(CFG_SOC_NAME == SOC_BK7231N)
#include "icu_pub.h"
#include "spi_pub.h"
#include "sys_ctrl_pub.h"
#include "drv_model_pub.h"
#include "mem_pub.h"
#include "rtos_pub.h"
#include "general_dma_pub.h"
#include "general_dma.h"
#include "spi_bk7231n.h"

#define MAX_LEN_ONCE    4095   //单次收发最大长度4095

#if CFG_USE_SPI_MASTER
struct bk_spi_dev {
    UINT8 init_dma_tx:1;
    UINT8 init_dma_rx:1;
	UINT8 last_pack:1;			//是否为最后一包
	UINT8 busy_flag:1;
    UINT8 spci_flag:1;          //特殊标志，为1时， 使用dma中断，可以支持大于4K数据发送，仅send接口
    UINT8 undef:3;
    beken_semaphore_t   finish_sem;
};

static void bk_spi_master_config(UINT32 mode, UINT32 rate);

static struct bk_spi_dev *spi_dev;

static void bk_spi_configure(UINT32 rate, UINT32 mode)
{
	UINT32 param;

	param = 0;
	sddev_control(SPI_DEV_NAME, CMD_SPI_INIT_MSTEN, (void *)&param);

	/* data bit width */
	param = 0;
	sddev_control(SPI_DEV_NAME, CMD_SPI_SET_BITWIDTH, (void *)&param);

	/* baudrate */
	BK_SPI_PRT("max_hz = %d \n", rate);
	sddev_control(SPI_DEV_NAME, CMD_SPI_SET_CKR, (void *)&rate);

	/* mode */
	if (mode & BK_SPI_CPOL)
		param = 1;
	else
		param = 0;
	sddev_control(SPI_DEV_NAME, CMD_SPI_SET_CKPOL, (void *)&param);

	/* CPHA */
	if (mode & BK_SPI_CPHA)
		param = 1;
	else
		param = 0;
	sddev_control(SPI_DEV_NAME, CMD_SPI_SET_CKPHA, (void *)&param);

	/* Master */
	param = 1;
	sddev_control(SPI_DEV_NAME, CMD_SPI_SET_MSTEN, (void *)&param);
	param = 1;
	sddev_control(SPI_DEV_NAME, CMD_SPI_SET_NSSMD, (void *)&param);

	/* enable spi */
	param = 1;
	sddev_control(SPI_DEV_NAME, CMD_SPI_UNIT_ENABLE, (void *)&param);

	os_printf("spi_master:[CTRL]:0x%08x \n", REG_READ(SPI_CTRL));
}

static void bk_spi_unconfigure(void)
{
	sddev_control(SPI_DEV_NAME, CMD_SPI_DEINIT_MSTEN, NULL);
}

int bk_spi_master_deinit(void)
{
	if (spi_dev == NULL)
		return 0;

	if (spi_dev->finish_sem)
		rtos_deinit_semaphore(&spi_dev->finish_sem);

	if (spi_dev) {
		os_free(spi_dev);
		spi_dev = NULL;
	}

	bk_spi_unconfigure();
	return 0;
}

#if CFG_USE_SPI_DMA

#define SPI_RX_DMA_CHANNEL     GDMA_CHANNEL_1
#define SPI_TX_DMA_CHANNEL     GDMA_CHANNEL_3

void spi_dma_tx_enable(UINT8 enable);
void spi_dma_rx_enable(UINT8 enable);

//spi 中断，按时到达，最大发送4095字节
static void bk_spi_dma_finish_callback(UINT32 param)
{
	spi_dev->busy_flag = spi_dev->last_pack ? false : true;
	rtos_set_semaphore(&spi_dev->finish_sem);
}

//dma中断回调，会提前几个时钟到达，长度无限制
static void bk_spi_dma_irq_cb(UINT32 param)
{
    spi_dev->busy_flag = false;
	rtos_set_semaphore(&spi_dev->finish_sem);
}

static int spi_dma_master_tx_init(void)
{
	GDMACFG_TPYES_ST init_cfg;
	GDMA_CFG_ST en_cfg;

	os_memset(&init_cfg, 0, sizeof(GDMACFG_TPYES_ST));
	os_memset(&en_cfg, 0, sizeof(GDMA_CFG_ST));

	init_cfg.dstdat_width = 8;
	init_cfg.srcdat_width = 32;
	init_cfg.dstptr_incr =  0;
	init_cfg.srcptr_incr =  1;

	init_cfg.dst_start_addr = (void *)SPI_DAT;
	init_cfg.channel = SPI_TX_DMA_CHANNEL;
	init_cfg.prio = 0;
    if(spi_dev->spci_flag)
        init_cfg.fin_handler = bk_spi_dma_irq_cb;
    else
        init_cfg.fin_handler = NULL;
    init_cfg.src_module = GDMA_X_SRC_DTCM_RD_REQ;
	init_cfg.dst_module = GDMA_X_DST_GSPI_TX_REQ;
	sddev_control(GDMA_DEV_NAME, CMD_GDMA_CFG_TYPE4, (void *)&init_cfg);

    int param = 1;
    sddev_control(SPI_DEV_NAME, CMD_SPI_TXFINISH_EN, (void *)&param);                       //开接收中断
    spi_ctrl(CMD_SPI_SET_TX_FINISH_INT_CALLBACK, (void *) bk_spi_dma_finish_callback);   	//设置回调，在回调中发送信号量

	en_cfg.channel = SPI_TX_DMA_CHANNEL;
	en_cfg.param = 0;					// 0:not repeat 1:repeat
	sddev_control(GDMA_DEV_NAME, CMD_GDMA_CFG_WORK_MODE, (void *)&en_cfg);

	en_cfg.channel = SPI_TX_DMA_CHANNEL;
	en_cfg.param = 0; // src no loop
	sddev_control(GDMA_DEV_NAME, CMD_GDMA_CFG_SRCADDR_LOOP, &en_cfg);

    spi_dma_tx_enable(0);
	return 0;
}

static int spi_dma_master_rx_init(void)
{
	GDMACFG_TPYES_ST init_cfg;
	GDMA_CFG_ST en_cfg;

	os_memset(&init_cfg, 0, sizeof(GDMACFG_TPYES_ST));
	os_memset(&en_cfg, 0, sizeof(GDMA_CFG_ST));

	init_cfg.dstdat_width = 32;
	init_cfg.srcdat_width = 8;
	init_cfg.dstptr_incr =  1;
	init_cfg.srcptr_incr =  0;

	init_cfg.src_start_addr = (void *)SPI_DAT;
	init_cfg.channel = SPI_RX_DMA_CHANNEL;
	init_cfg.prio = 0;
    init_cfg.src_module = GDMA_X_SRC_GSPI_RX_REQ;
	init_cfg.dst_module = GDMA_X_DST_DTCM_WR_REQ;
	sddev_control(GDMA_DEV_NAME, CMD_GDMA_CFG_TYPE5, (void *)&init_cfg);

    int param = 1;
    sddev_control(SPI_DEV_NAME, CMD_SPI_RXFINISH_EN, (void *)&param);                       //开接收中断
    spi_ctrl(CMD_SPI_SET_RX_FINISH_INT_CALLBACK, (void *) bk_spi_dma_finish_callback);   //设置回调，在回调中发送信号量

	en_cfg.channel = SPI_RX_DMA_CHANNEL;
	en_cfg.param = 0;						// 0:not repeat 1:repeat
	sddev_control(GDMA_DEV_NAME, CMD_GDMA_CFG_WORK_MODE, (void *)&en_cfg);

	en_cfg.channel = SPI_RX_DMA_CHANNEL;
	en_cfg.param = 0; // src no loop
	sddev_control(GDMA_DEV_NAME, CMD_GDMA_CFG_DSTADDR_LOOP, &en_cfg);

    spi_dma_rx_enable(0);
	return 0;
}

static void bk_spi_master_config(UINT32 mode, UINT32 rate)
{
	UINT32 param;
	os_printf("mode:%d, rate:%d\r\n", mode, rate);
	bk_spi_configure(rate, mode);

	//disable tx/rx int disable
	param = 0;
	sddev_control(SPI_DEV_NAME, CMD_SPI_TXINT_EN, (void *)&param);

	param = 0;
	sddev_control(SPI_DEV_NAME, CMD_SPI_RXINT_EN, (void *)&param);

	//disable rx/tx over
	param = 0;
	sddev_control(SPI_DEV_NAME, CMD_SPI_RXOVR_EN, (void *)&param);

	param = 0;
	sddev_control(SPI_DEV_NAME, CMD_SPI_TXOVR_EN, (void *)&param);

	param = 1;
	sddev_control(SPI_DEV_NAME, CMD_SPI_SET_NSSMD, (void *)&param);
    sddev_control(SPI_DEV_NAME, CMD_SPI_SET_3_LINE, NULL);

	param = 0;
	sddev_control(SPI_DEV_NAME, CMD_SPI_SET_BITWIDTH, (void *)&param);

	//disable CSN intterrupt
	param = 0;
	sddev_control(SPI_DEV_NAME, CMD_SPI_CS_EN, (void *)&param);

	//clk test
	//param = 5000000;
	//sddev_control(SPI_DEV_NAME, CMD_SPI_SET_CKR, (void *)&param);

	os_printf("[CTRL]:0x%08x \n", REG_READ(SPI_CTRL));
	os_printf("[CONFIG]:0x%08x \n", REG_READ(SPI_CONFIG));
}

int bk_spi_master_dma_init(UINT32 mode, UINT32 rate, UINT32 flag)
{
	OSStatus result = 0;

    if(spi_dev)
        bk_spi_master_deinit();
	spi_dev = os_malloc(sizeof(struct bk_spi_dev));
	if (!spi_dev) {
		BK_SPI_PRT("[spi]:malloc memory for spi_dev failed\n");
		result = 1;
        goto _exit;
	}
	os_memset(spi_dev, 0, sizeof(struct bk_spi_dev));

    result = rtos_init_semaphore(&spi_dev->finish_sem, 1);
	if (result != kNoErr) {
		BK_SPI_PRT("[spi]: spi dam sem init failed\n");
        result = 2;
        goto _exit;
	}	
    
    spi_dev->spci_flag= flag;
	bk_spi_master_config(mode, rate);
	spi_dma_master_tx_init();
    spi_dev->init_dma_tx = 1;
	spi_dma_master_rx_init();
    spi_dev->init_dma_rx = 1;

	return result;
_exit:

	bk_spi_master_deinit();
    return result;
}

int bk_spi_master_dma_tx_init(UINT32 mode, UINT32 rate)
{
	OSStatus result = 0;
	if (spi_dev)
		bk_spi_master_deinit();

	spi_dev = os_malloc(sizeof(struct bk_spi_dev));
	if (!spi_dev) {
		os_printf("[spi]:malloc memory for spi_dev failed\n");
		result = -1;
		goto _exit;
	}
	os_memset(spi_dev, 0, sizeof(struct bk_spi_dev));

	result = rtos_init_semaphore(&spi_dev->finish_sem, 1);
	if (result != kNoErr) {
		os_printf("[spi]: spi tx semp init failed\n");
		goto _exit;
	}

    // 初始化spi
	bk_spi_master_config(mode, rate);
    // 初始化dma
	spi_dma_master_tx_init();
    spi_dev->init_dma_tx = 1;
	return 0;

_exit:
	bk_spi_master_deinit();
	return 1;
}

int bk_spi_master_dma_rx_init(UINT32 mode, UINT32 rate)
{
	OSStatus result = 0;
	
	if (spi_dev)
		bk_spi_master_deinit();

	spi_dev = os_malloc(sizeof(struct bk_spi_dev));
	if (!spi_dev) {
		os_printf("[spi]:malloc memory for spi_dev failed\n");
		result = -1;
		goto _exit;
	}
	os_memset(spi_dev, 0, sizeof(struct bk_spi_dev));

	result = rtos_init_semaphore(&spi_dev->finish_sem, 1);
	if (result != kNoErr) {
		os_printf("[spi]: spi tx semp init failed\n");
		goto _exit;
	}

	bk_spi_master_config(mode, rate);
    spi_dma_master_rx_init();
    spi_dev->init_dma_rx = 1;
	return 0;

_exit:
    bk_spi_master_deinit();
	return 1;
}

int bk_spi_master_dma_send(struct spi_message *spi_msg)
{
    if(spi_msg == NULL) {
        return -1;
    } else if(spi_msg->send_buf == NULL || spi_msg->send_len == 0) {
        return -1;
    }
    if(NULL == spi_dev) {
        bk_printf("spi send no init!\n");
        return -1;
    } else if(spi_dev->init_dma_tx == 0) {
        return -1;
    }
	spi_dev->busy_flag = true;
    int offset = 0;
    GDMA_CFG_ST en_cfg;
    int len = spi_msg->send_len ;

    if(spi_dev->spci_flag) {
        UINT32 param = 0;
        spi_ctrl(CMD_SPI_TXTRANS_EN, (void *)&param);               //为0时，为dma中断

        en_cfg.channel = SPI_TX_DMA_CHANNEL;
        en_cfg.param   = (UINT32)(spi_msg->send_buf + offset);		// dma dst addr
        // 设置dma start addr
        gdma_ctrl(CMD_GDMA_SET_SRC_START_ADDR, (void *)&en_cfg); 
        // 设置dma 发送长度
        en_cfg.param = spi_msg->send_len;   // dma translen
        gdma_ctrl(CMD_GDMA_SET_TRANS_LENGTH, (void *)&en_cfg);

        //开启dma传输
        spi_dev->busy_flag = true;
        spi_dma_tx_enable(1);
        rtos_get_semaphore(&spi_dev->finish_sem, BEKEN_NEVER_TIMEOUT);
    } else {
        while(len > 0) {
            spi_msg->send_len = len > MAX_LEN_ONCE ? MAX_LEN_ONCE : len;
            spi_dev->last_pack = spi_msg->send_len > MAX_LEN_ONCE ? false : true;
            // 设置spi 触发中断长度
            spi_ctrl(CMD_SPI_TXTRANS_EN, (void *)&spi_msg->send_len);

            en_cfg.channel = SPI_TX_DMA_CHANNEL;
            en_cfg.param   = (UINT32)(spi_msg->send_buf + offset);		// dma dst addr
            // 设置dma start addr
            gdma_ctrl(CMD_GDMA_SET_SRC_START_ADDR, (void *)&en_cfg); 
            // 设置dma 发送长度
            en_cfg.param = spi_msg->send_len;   // dma translen
            gdma_ctrl(CMD_GDMA_SET_TRANS_LENGTH, (void *)&en_cfg);

            //开启dma传输
            spi_dma_tx_enable(1);
            rtos_get_semaphore(&spi_dev->finish_sem, BEKEN_NEVER_TIMEOUT);
            len -= spi_msg->send_len;
            offset += spi_msg->send_len;
        }
    }
    return 0;
}

int bk_spi_master_dma_recv(struct spi_message *spi_msg)
{
    if(spi_msg == NULL) {
        return -1;
    } else if(spi_msg->recv_buf == NULL || spi_msg->recv_len == 0) {
        return -1;
    }

    if(NULL == spi_dev) {
        bk_printf("spi send no init!\n");
        return -1;
    } else if(spi_dev->init_dma_rx == 0) {
        return -1;
    }
	spi_dev->busy_flag = true;
    int offset = 0;
    GDMA_CFG_ST en_cfg;
    int len = spi_msg->recv_len ;

    while(len > 0) {
        spi_msg->recv_len = len > MAX_LEN_ONCE ? MAX_LEN_ONCE : len;
		spi_dev->last_pack = spi_msg->recv_len > MAX_LEN_ONCE ? false : true;

        spi_ctrl(CMD_SPI_RXTRANS_EN, (void *)&spi_msg->recv_len);       //设置接收触发中断

        en_cfg.channel = SPI_RX_DMA_CHANNEL;
        en_cfg.param   = (UINT32)(spi_msg->recv_buf + offset);		// dma dst addr
        gdma_ctrl(CMD_GDMA_SET_DST_START_ADDR, (void *)&en_cfg); 

        en_cfg.param   = spi_msg->recv_len;     // dma translen
        gdma_ctrl(CMD_GDMA_SET_TRANS_LENGTH, (void *)&en_cfg);    
        
        spi_dma_rx_enable(1);
        rtos_get_semaphore(&spi_dev->finish_sem, BEKEN_NEVER_TIMEOUT);
        len -= spi_msg->recv_len;
        offset += spi_msg->recv_len;
    }
    
    return 0;
}

// 全双工收发接口，也可仅发送或仅接收
//存在问题，收发同时时，接收会丢掉一个字节，在2M频率下
int bk_spi_master_dma_xfer(struct spi_message *spi_msg)
{
    if(spi_msg == NULL || NULL == spi_dev) {
        return -1;
    }
    if(spi_msg->recv_buf == NULL)   spi_msg->recv_len = 0;
    if(spi_msg->send_buf == NULL)   spi_msg->send_len = 0;
    //全双工模式，发送与接收长度必须一致
    if(spi_msg->recv_buf != NULL && spi_msg->send_buf != NULL && spi_msg->recv_len != spi_msg->send_len) {
        return -1;
    }

	spi_dev->busy_flag = true;
    int len = spi_msg->recv_len > 0 ? spi_msg->recv_len : spi_msg->send_len;
    int offset = 0;
    GDMA_CFG_ST en_cfg;

    while(len > 0) {
        if(spi_msg->recv_buf) {
            spi_msg->recv_len = len > MAX_LEN_ONCE ? MAX_LEN_ONCE : len;
			spi_dev->last_pack = spi_msg->recv_len > MAX_LEN_ONCE ? false : true;
            spi_ctrl(CMD_SPI_RXTRANS_EN, (void *)&spi_msg->recv_len);       //设置接收触发中断
            en_cfg.channel = SPI_RX_DMA_CHANNEL;
            en_cfg.param   = (UINT32)(spi_msg->recv_buf + offset);		    // dma dst addr
            gdma_ctrl(CMD_GDMA_SET_DST_START_ADDR, (void *)&en_cfg); 
            en_cfg.param   = spi_msg->recv_len;     // dma translen
            gdma_ctrl(CMD_GDMA_SET_TRANS_LENGTH, (void *)&en_cfg);   
        }

        if(spi_msg->send_buf) {
            spi_msg->send_len = len > MAX_LEN_ONCE ? MAX_LEN_ONCE : len;
			spi_dev->last_pack = spi_msg->send_len > MAX_LEN_ONCE ? false : true;
            spi_ctrl(CMD_SPI_TXTRANS_EN, (void *)&spi_msg->send_len);
            en_cfg.channel = SPI_TX_DMA_CHANNEL;
            en_cfg.param   = (UINT32)(spi_msg->send_buf + offset);		// dma dst addr
            gdma_ctrl(CMD_GDMA_SET_SRC_START_ADDR, (void *)&en_cfg); 
            en_cfg.param = spi_msg->send_len;   // dma translen
            gdma_ctrl(CMD_GDMA_SET_TRANS_LENGTH, (void *)&en_cfg);
        }
        if(spi_msg->send_buf && spi_msg->send_len > 0)
            spi_dma_tx_enable(1);
        if(spi_msg->recv_buf && spi_msg->recv_len > 0)
            spi_dma_rx_enable(1);
        rtos_get_semaphore(&spi_dev->finish_sem, BEKEN_NEVER_TIMEOUT);
        len = spi_msg->recv_len > 0 ? (len - spi_msg->recv_len) : (len - spi_msg->send_len);
        offset += spi_msg->recv_len > 0 ? spi_msg->recv_len : spi_msg->send_len;
    }
    return 0;
}

int bk_spi_get_status(void) {
	return spi_dev->busy_flag;
}

#endif  //CFG_USE_SPI_DMA
#endif  // CFG_USE_SPI_MASTER
#endif  //(CFG_SOC_NAME == SOC_BK7231N)
