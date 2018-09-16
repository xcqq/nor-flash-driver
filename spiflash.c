#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/spi/spidev.h>
#include <linux/genhd.h>
#include <linux/fs.h>





static void flash_request(request_queue_t *req_q)
{
    /*todo transfer to spi flash*/
    struct request *req;
    while((req=blk_peek_request(req_q))!= NULL)
    {
        /*need filter fs request*/
        /*spi flash transfer here*/
        blk_start_request(req_q);
    }
}

static int spi_device_init(void)
{
    /*init spi device*/
    return 0;
}

static int flash_open(struct block_device *blk_dev,fmode_t mode)
{
    /*todo open*/
    return 0;
}

static int flash_release(struct gendisk *disk,fmode_t mode)
{
    /*todo release*/
    return 0
}

int flash_ioctl (struct block_device *, fmode_t, unsigned, unsigned long)
{
    return 0;
}
struct block_device_operations flash_fops = {
    .open = flash_open,
    .release = flash_release,
    .ioctl = flash_ioctl,
    .owner = THIS_MODULE,
};

struct spi_driver spi_dev = {
    .driver = {
        .owner = THIS_MODULE,
        .name = "spi nor flash",
    },

};

struct spi_flash_dev {
    request_queue *queue;
    gendisk disk;
    spinlock_t *lock;
};

static int block_device_init(void)
{
    int ret = 0;

    /*regist block device*/
    if (register_blkdev(BLOCK_EXT_MAJOR, "my_spi_flash"))
    {

        ret = -EIO;
        goto err;
    }

    /*regist queue*/
    dev->queue = blk_register_queue(flash_request, dev->lock);
    if(!dev->queue)
        goto err;
    blk_queue_max_hw_sectors(dev->queue, 255);
    blk_queue_logical_block_size(dev->queue, 512);
    
    /*init diskgen*/
    dev->disk->major = BLOCK_EXT_MAJOR;
    dev->disk->first_minor = 0;
    dev->disk->fops = flash_fops;
    dev->disk->queue = dev->queue;
    /*temp set to 512*/
    set_capacity(dev->disk, 512);
    add_disk(dev->disk);
    return 0;
err:
    return ret;
}

static int __init spi_nor_flash_init(void)
{
    int ret = 0;
    if((ret = block_device_init()) != 0 )
        goto err;
    return 0;

err:
    return ret;
}

static int spi_nor_flash_exit()
{
    return 0;
}

module_init(spi_nor_flash_init);
module_exit(spi_nor_flash_exit);