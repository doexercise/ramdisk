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
	struct request_queue * q;
	struct gendisk * disk;
};

struct mq_ext ext;
unsigned long disk_size = 4;	// default 4GB

void write_copy(struct request *req, void *dst)
{
	void *src;
	struct bio_vec bvec;
	struct req_iterator iter;

	rq_for_each_segment(bvec, req, iter) {
		src = page_address(bvec.bv_page) + bvec.bv_offset;
		memcpy(dst, src, bvec.bv_len);
		dst += bvec.bv_len;
	}
}

void read_copy(struct request *req, void *src)
{
	void *dst;
	struct bio_vec bvec;
	struct req_iterator iter;

	rq_for_each_segment(bvec, req, iter) {
		dst = page_address(bvec.bv_page) + bvec.bv_offset;
		memcpy(dst, src, bvec.bv_len);
		src += bvec.bv_len;
	}
}

static blk_status_t mq_queue_rq_fn(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd)
{
	struct request *req = bd->rq;
	void *addr;

	//printk("%s function called\n", __func__);

	blk_mq_start_request(req);

	addr = ext.addr + (blk_rq_pos(req) << 9);
	if (op_is_write(req_op(req))) {
		write_copy(req, addr);
	}
	else {
		read_copy(req, addr);
	}
	blk_mq_end_request(req, BLK_STS_OK);

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
	.queue_rq	= mq_queue_rq_fn,
	.complete	= mq_complete_fn,
	.timeout	= mq_timeout_fn,
};

int mq_open(struct block_device *bdev, fmode_t mode) 
{ return 0; }

void mq_release(struct gendisk *disk, fmode_t mode) 
{ }

struct block_device_operations mq_fops = {
	.owner		= THIS_MODULE,
	.open		= mq_open,
	.release	= mq_release,
};

int mq_init_blk_dev(void)
{
	ext.q = blk_mq_init_queue(&ext.set);
	if (IS_ERR(ext.q)) {
		printk("Fail to alloc request queue\n");
		return -ENOMEM;
	}

	ext.disk = alloc_disk_node(1, NUMA_NO_NODE);
	if (!ext.disk) {
		printk("Fail to alloc disk\n");
		goto err;
	}

	set_capacity(ext.disk, disk_size * 1024 * 1024 * 1024 / 512 );
	ext.disk->major = ext.major;
	ext.disk->fops = &mq_fops;
	ext.disk->queue = ext.q;
	sprintf(ext.disk->disk_name, "ckmq");
	add_disk(ext.disk);

	return 0;

err :
	blk_cleanup_queue(ext.q);
	return -ENOMEM;
}

void mq_remove_blk_dev(void)
{
	del_gendisk(ext.disk);
	blk_cleanup_queue(ext.q);
	put_disk(ext.disk);
}

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

	if (mq_init_blk_dev() < 0)
		goto err_blk;

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

	mq_remove_blk_dev();

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