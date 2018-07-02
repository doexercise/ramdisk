#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,13,0)
#define blk_status_t int
#define BLK_STS_OK 0
#endif

struct mq_ext {
	void * addr;
	struct blk_mq_tag_set set;
	int major;
};

struct mq_ext ext;
unsigned long disk_size = 4;	// default 4GB

static blk_status_t mq_queue_rq_fn(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd)
{
	printk("%s function called\n", __func__);

	blk_mq_start_request(bd->rq);
	return BLK_STS_OK;
}

static void mq_complete_fn(struct request *rq)
{
	printk("%s function called\n", __func__);

	blk_mq_end_request(rq, BLK_STS_OK);
}

static enum blk_eh_timer_return mq_timeout_fn(struct request *rq, bool res)
{
	printk("%s function called\n", __func__);

	return BLK_EH_HANDLED;
}

struct blk_mq_ops mq_ops = {
	.queue_rq = mq_queue_rq_fn,
	.complete = mq_complete_fn,
	.timeout = mq_timeout_fn,
};

int mq_init_tag_set(struct blk_mq_tag_set * set) 
{
	set->ops = &mq_ops;
	set->nr_hw_queues = 1;
	set->queue_depth = 1;
	
	return blk_mq_alloc_tag_set(set);
}

void * mq_alloc_vdisk(unsigned long size)
{
	return vmalloc(size);
}

int __init mq_init(void)
{
	ext.addr = mq_alloc_vdisk(disk_size * 1024 * 1024 * 1024);
	if (ext.addr) {
		printk("Memory allocated for disk, size %lu GB\n", disk_size);
	}
	else {
		printk("Fail to alloc disk space\n");
	}

	if (mq_init_tag_set(&ext.set) < 0) {
		printk("Fail mq_init_tag_set()\n");
		goto err_mq;
	}
	else {
		printk("Success mq_init_tag_set()\n");
	}

	ext.major = register_blkdev(0, "multiq");
	if (ext.major < 0) {
		printk("Fail to register blkdev\n");
		goto err_blk;
	}

	return 0;

err_blk :
	blk_mq_free_tag_set(&ext.set);
err_mq :
	vfree(ext.addr);
	return -1;
}

void __exit mq_exit(void)
{
	printk("bye ~!\n");

	unregister_blkdev(ext.major, "multiq");

	blk_mq_free_tag_set(&ext.set);

	if (ext.addr) {
		printk("mem free\n");
		vfree(ext.addr);
		ext.addr = NULL;
	}
}

module_init(mq_init);
module_exit(mq_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("c.kun");
module_param(disk_size, long, 0644);