#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/genhd.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/of.h>

/* enable debug to print debug log*/
 #define DEBUG 1

/*set to zero to get a major device no dynamically */ 
#define MAJOR_DEVICE_NO 0
/*max partitions of block device*/
#define MINOR_DEVICE 32
/*the length of spi tx&rx buffer*/
/*10 byte is for spi command*/
#define SPI_DATA_LEN 1024
#define SPI_BUFF_LEN SPI_DATA_LEN+10
/*spi flash command*/
#define FLASH_PROGRAM   0x02
#define FLASH_READ      0x03
struct spi_flash_dev
{
    struct request_queue *queue;
    struct gendisk *disk;
    spinlock_t lock;
    spinlock_t spi_lock;
    struct spi_device *spi;
    char *tx_buf;
    char *rx_buf;
};

/*not need if we just use spi_write or spi_read function*/
// static ssize_t flash_spi_sync(struct spi_flash_dev dev,struct spi_message message)
// {
    
// }

static void flash_transfer(struct spi_flash_dev *dev,sector_t sector, char *buffer, unsigned long len,unsigned int dir)
{
    /* todo spi message transfer*/
    unsigned long flash_addr;
    unsigned int i, cnt,len_left;
    /*write operation*/
    if (dir == WRITE)
    {
        /*fill in spi buffer*/
        dev->tx_buf[0] = FLASH_PROGRAM;
        cnt = len / SPI_DATA_LEN + 1;
        len_left = len;
        for (i = 0; i < len; i++)
        {
            flash_addr = (sector + i * SPI_DATA_LEN / 256) * 256;
            dev->tx_buf[1] = flash_addr >> 16;
            dev->tx_buf[2] = flash_addr >> 8;
            dev->tx_buf[3] = flash_addr;
            if (len_left >= 256)
            {
                memcpy(&dev->tx_buf[4], buffer + i * SPI_DATA_LEN, SPI_DATA_LEN);
                len_left -= SPI_DATA_LEN;
            }
            else
            {
                memcpy(&dev->tx_buf[4], buffer + i *SPI_DATA_LEN, len_left);
            }
            spi_write(dev->spi, dev->tx_buf, len + 4);
        }
    }
    /*read operation*/
    else
    {
        dev->tx_buf[0] = FLASH_READ;
        /*limit transfer bytes because of tx/rx buffer length*/
        /*rest of spi buffer is used for spi cmd and addr*/
        cnt = len / SPI_DATA_LEN + 1;
        len_left = len;
        for (i = 0; i < len;i++)
        {
            flash_addr = (sector + i * SPI_DATA_LEN / 256) * 256;
            dev->tx_buf[1] = flash_addr >> 16;
            dev->tx_buf[2] = flash_addr >> 8;
            dev->tx_buf[3] = flash_addr;
            if(len_left >= SPI_DATA_LEN)
            {
                spi_write_then_read(dev->spi, dev->tx_buf, 4, dev->rx_buf, SPI_DATA_LEN);
                memcpy(buffer + i * SPI_DATA_LEN, dev->rx_buf, SPI_DATA_LEN);
                len_left -= SPI_DATA_LEN;
            }
            else
            {
                spi_write_then_read(dev->spi, dev->tx_buf, 4, dev->rx_buf, len_left);
                memcpy(buffer + i * SPI_DATA_LEN, dev->rx_buf, len_left);
            }
        }
    }
}

/*it's such a simple driver, so we just tranvers request*/
static int flash_transfer_req(struct spi_flash_dev *dev,struct request *req)
{
    struct bio_vec bio_v;
    struct req_iterator iter;

    /*do data transfer to flash*/
    rq_for_each_segment(bio_v,req,iter) {
        char *buffer = kmap_atomic(bio_v.bv_page);
        unsigned long offset = bio_v.bv_offset;
        unsigned long len = bio_v.bv_len;
        sector_t sector = iter.iter.bi_sector;
        flash_transfer(dev, sector, buffer + offset, len, bio_data_dir(iter.bio));
        kunmap(bio_v.bv_page);
    }
    return 0;
}

static void flash_request(struct request_queue *req_q)
{
    /*todo transfer to spi flash*/
    struct request *req;
    while ((req = blk_peek_request(req_q)) != NULL)
    {
        //dev_info(req->rq_disk->private_data->spi->dev,)
        pr_info("private_data %lu",req->rq_disk->private_data);
        flash_transfer_req(req->rq_disk->private_data, req);
        /*need filter fs request*/
        /*spi flash transfer here*/
        blk_start_request(req);
    }
}

static int flash_open(struct block_device *blk_dev,fmode_t mode)
{
    struct spi_flash_dev *dev = blk_dev->bd_disk->private_data;
    /*todo open*/
    /*get flash id when open, just for test*/

    return 0;
}

void flash_release(struct gendisk *disk,fmode_t mode)
{
    /*todo release*/
}

int flash_ioctl (struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg)
{
    /*no ioctl command support now*/
    int ret = -EINVAL;
    return ret;
}
struct block_device_operations flash_fops = {
    .open = flash_open,
    .release = flash_release,
    .ioctl = flash_ioctl,
    .owner = THIS_MODULE,
};


struct spi_flash_dev *flash_dev = NULL;
int major = 0;
static int block_device_init(struct spi_flash_dev *dev)
{
    int ret = 0;
    /*regist block device*/
    dev_dbg(&dev->spi->dev, "Init block device");
    major = register_blkdev(MAJOR_DEVICE_NO, "my_spi_flash");
    if(major <= 0)
    {
        pr_err("register blkdev fail");
        ret = -EIO;
        goto err_blk;
    }
    dev_dbg(&dev->spi->dev, "register blkdev major: %d", major);
    /*regist queue*/
    spin_lock_init(&flash_dev->lock);
    flash_dev->queue = blk_init_queue(flash_request, &flash_dev->lock);
    if (flash_dev->queue == NULL)
    {
        pr_err("queue init fail");
        goto err_queue;
        ret = -EIO;
    }
    dev_dbg(&dev->spi->dev, "queue init ok");
    blk_queue_max_hw_sectors(flash_dev->queue, 255);
    blk_queue_logical_block_size(flash_dev->queue, 512);

    /*init diskgen*/
    flash_dev->disk = alloc_disk(MINOR_DEVICE);
    flash_dev->disk->major = major;
    flash_dev->disk->first_minor = 0; 
    flash_dev->disk->fops = &flash_fops;
    flash_dev->disk->queue = flash_dev->queue;
    flash_dev->disk->private_data = dev;
    snprintf(flash_dev->disk->disk_name, 32, "spiblk0");
    /*temp set to 512*/
    set_capacity(flash_dev->disk, 512);
    dev_info(&dev->spi->dev, "add_disk major: %d, name:%s", flash_dev->disk->major, flash_dev->disk->disk_name);
    add_disk(flash_dev->disk);

    return 0;

err_disk:
    put_disk(flash_dev->disk);
err_queue:
    if (flash_dev->queue)
        blk_cleanup_queue(flash_dev->queue);
err_blk:
    unregister_blkdev(major, "my_spi_flash");
    return ret;
}

static void block_device_remove(void)
{
    if(flash_dev->disk)
        put_disk(flash_dev->disk);
    if (flash_dev->queue)
        blk_cleanup_queue(flash_dev->queue);
    unregister_blkdev(major, "my_spi_flash");
}

static int spi_device_probe(struct spi_device *dev)
{
    /*init spi device*/
    int ret = 0;
    dev_dbg(&dev->dev, "spi device probe enter");
    flash_dev = kzalloc(sizeof(struct spi_flash_dev), GFP_KERNEL);
    if(flash_dev == NULL)
    {
        dev_err(&dev->dev, "alloc mem fail");
        ret = -ENOMEM;
        goto cleanup;
    }
    flash_dev->tx_buf = kmalloc(SPI_BUFF_LEN, GFP_KERNEL);
    flash_dev->rx_buf = kmalloc(SPI_BUFF_LEN, GFP_KERNEL);
    if(!(flash_dev->tx_buf&&flash_dev->rx_buf))
    {
        ret = -ENOMEM;
        goto cleanup;
    }
    flash_dev->spi = dev;
    spin_lock_init(&flash_dev->spi_lock);
    dev_set_drvdata(&flash_dev->spi->dev, flash_dev);
    /*get flash id*/
    flash_dev->tx_buf[0] = 0x90;
    flash_dev->tx_buf[1] = 0x00;
    flash_dev->tx_buf[2] = 0x00;
    flash_dev->tx_buf[3] = 0x00;
    if(spi_write_then_read(flash_dev->spi, flash_dev->tx_buf, 4, flash_dev->rx_buf, 2)<0)
    {
        dev_err(&dev->dev, "get flash id fail");
        ret = -EIO;
        goto cleanup;
    }
    /*todo flash id judgement*/
    if((flash_dev->rx_buf[0]==0xEF)&&(flash_dev->rx_buf[1]==0x17))
        dev_info(&dev->dev, "get flash id success, flash is W25Q128FV");
    else
    {
        dev_info(&dev->dev, "get flash id success: Manufacturer ID: %X, Device ID: %X, flash is not W25Q128FV", flash_dev->rx_buf[0], flash_dev->rx_buf[1]);
        ret = -EIO;
        goto cleanup;
    }

    ret = block_device_init(flash_dev);
    if (ret < 0)
    {
        dev_err(&dev->dev, "block device init failed");
        goto cleanup;
    }
    return 0;
cleanup:
    dev_err(&dev->dev, "Device probe fail,exiting!!!!");
    block_device_remove();
    if(flash_dev->tx_buf)
        kfree(flash_dev->tx_buf);
    if(flash_dev->rx_buf)
        kfree(flash_dev->rx_buf);
    kfree(flash_dev);
    return ret;
}

static int spi_device_remove(struct spi_device *dev)
{
    /*remove spi device*/
    block_device_remove();
    if(flash_dev->tx_buf)
        kfree(flash_dev->tx_buf);
    if(flash_dev->rx_buf)
        kfree(flash_dev->rx_buf);
    kfree(flash_dev);
}

static const struct of_device_id spidev_ids[] = {
    {.compatible = "spiflash", }
};

static struct spi_driver spi_flash_driver = {
    .driver = {
        .name = "spiflash",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(spidev_ids),
    },
    .probe = spi_device_probe,
    .remove = spi_device_remove,
};


module_spi_driver(spi_flash_driver);
MODULE_LICENSE("GPL");
