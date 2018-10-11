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
#include <linux/device.h>

struct spi_flash_dev
{
    struct request_queue *queue;
    struct gendisk disk;
    spinlock_t *lock;
};

static void flash_request(struct request_queue *req_q)
{
    /*todo transfer to spi flash*/
    struct request *req;
    while((req=blk_peek_request(req_q))!= NULL)
    {
        /*need filter fs request*/
        /*spi flash transfer here*/
        blk_start_request(req);
    }
}

static int spi_device_init(void)
{
    /*init spi device*/
    return 0;
}

static int flash_open(struct block_device *blk_dev,fmode_t mode)
{
    struct spi_flash_dev *dev = blk_dev->bd_disk->private_data;
    /*todo open*/
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

static int block_device_init(void)
{
    int ret = 0;
    /*regist block device*/
    if (register_blkdev(BLOCK_EXT_MAJOR, "my_spi_flash"))
    {

        ret = -EIO;
        goto err_blk;
    }
    flash_dev = kzalloc(sizeof(struct spi_flash_dev), GFP_KERNEL);
    if(flash_dev == NULL)
    {
        ret = -ENOMEM;
        goto err_alloc;
    }
    /*regist queue*/
    flash_dev->queue = blk_init_queue(flash_request, flash_dev->lock);
    if (!flash_dev->queue)
    {
        goto err_queue;
        ret = -EIO;
    }
    blk_queue_max_hw_sectors(flash_dev->queue, 255);
    blk_queue_logical_block_size(flash_dev->queue, 512);

    /*init diskgen*/
    flash_dev->disk.major = BLOCK_EXT_MAJOR;
    flash_dev->disk.first_minor = 0;
    flash_dev->disk.fops = &flash_fops;
    flash_dev->disk.queue = flash_dev->queue;
    /*temp set to 512*/
    set_capacity(&flash_dev->disk, 512);
    add_disk(&flash_dev->disk);
    return 0;

err_disk:
    put_disk(&flash_dev->disk);
err_queue:
    if (flash_dev->queue)
        blk_cleanup_queue(flash_dev->queue);
err_alloc:
    kfree(flash_dev);
err_blk:
    unregister_blkdev(BLOCK_EXT_MAJOR, "my_spi_flash");
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

static void spi_nor_flash_exit(void)
{
    put_disk(&flash_dev->disk);
    if (flash_dev->queue)
        blk_cleanup_queue(flash_dev->queue);
    kfree(flash_dev);
    unregister_blkdev(BLOCK_EXT_MAJOR, "my_spi_flash");
}

module_init(spi_nor_flash_init);
module_exit(spi_nor_flash_exit);
