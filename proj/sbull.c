/*
 * Sample disk driver, from the beginning.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/timer.h>
#include <linux/types.h>	/* size_t */
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/hdreg.h>	/* HDIO_GETGEO */
#include <linux/kdev_t.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>	/* invalidate_bdev */
#include <linux/bio.h>

#include <linux/crypto.h> //for compression

#include <crypto/acompress.h>
#include <crypto/hash.h>

MODULE_LICENSE("Dual BSD/GPL");

static int sbull_major = 0;
static int hardsect_size = 512;
static int nsectors = 1024 * 1024;	/* How big the drive is */
static int ndevices = 1;

int encryption_enabled;
int input_key;
const char key = 5;

unsigned int compression_ratio = 0;

#define COMP_BUF_SIZE 512

/*
 * Minor number and partition management.
 */
#define SBULL_MINORS	16
#define MINOR_SHIFT	4
#define DEVNUM(kdevnum)	(MINOR(kdev_t_to_nr(kdevnum)) >> MINOR_SHIFT

/*
 * We can tweak our hardware sector size, but the kernel talks to us
 * in terms of small sectors, always.
 */
#define KERNEL_SECTOR_SIZE	512

/*
 * After this much idle time, the driver will simulate a media change.
 */
#define INVALIDATE_DELAY	30*HZ

/*
 * The internal representation of our device.
 */
struct sbull_dev {
	// TODO: complete
	int size;
	int users;
	char *data;
	spinlock_t lock;

	struct request_queue *queue;
	struct gendisk *gd;
};

static struct sbull_dev *Devices = NULL;

/*
 * Handle an I/O request.
 */
static void encryptDecrypt(char* buffer, int nbytes) {
	// TODO: complete
	int i;
	for (i = 0; i < nbytes; i++) {
		buffer[i] = buffer[i] ^ key;
	}
}

static void sbull_transfer(struct sbull_dev *dev, unsigned long sector,
		unsigned long nsect, char *buffer, int write)
{
	// TODO: complete
	unsigned long offset = sector * KERNEL_SECTOR_SIZE;
	unsigned long nbytes = nsect * KERNEL_SECTOR_SIZE;
	unsigned int dlen = COMP_BUF_SIZE;
	unsigned int slen;
	int ret;
	//struct crypto_comp *tfm;

	const char *name = "sbulla";
	struct crypto_comp *comp;
	int *dsize = vmalloc(sizeof(int));
	//double prev, next;
	
	comp = crypto_alloc_comp(name, 0, 0);
	if ((offset + nbytes) > dev->size) {
		printk(KERN_NOTICE "Beyond-end write (%ld %ld)\n", offset, nbytes);
		return;
	}
	if (write) {
		if (encryption_enabled) { encryptDecrypt(buffer, nbytes); }
		//comp = crypto_alloc_comp(name, 0, 0);
		*dsize = nbytes;
		ret = crypto_comp_compress(comp, (void *)buffer, nbytes, dev->data+offset, dsize); 
		if (ret) {
			pr_err("compress error!");
			goto out;
		}
		//next = nbytes;
		if (dsize) {
			//prev =*dsize;
			if (compression_ratio) {
				compression_ratio = (compression_ratio + ((nbytes * 100) / (*dsize)))/2;
			}
			else {
				compression_ratio = (nbytes * 100) / (*dsize);
			}
		} else printk(KERN_INFO "compression failed\n");
				
		//memcpy(dev->data + offset, buffer, nbytes);
	} else {
		
		//memcpy(buffer, dev->data + offset, nbytes);
		slen = dlen;
		ret = crypto_comp_decompress(comp, dev->data+offset, slen, buffer, &dlen);
		if(ret){
			pr_err("decompress error!");
			goto out;
		}
		//memcpy(buffer, dev->data + offset, nbytes);
		if (encryption_enabled && key == input_key) { encryptDecrypt(buffer, nbytes); }
	}
out:
	return ;
}


/*
 * Transfer a single BIO.
 */
static int sbull_xfer_bio(struct sbull_dev *dev, struct bio *bio)
{
	// TODO: complete
	struct bvec_iter i;
	struct bio_vec bvec;
	sector_t sector = bio->bi_iter.bi_sector;
	bio_for_each_segment(bvec, bio, i) {
		char *buffer = __bio_kmap_atomic(bio, i);

		sbull_transfer(dev, sector, bio_cur_bytes(bio) >> 9,
					buffer, bio_data_dir(bio) == WRITE);

		sector += bio_cur_bytes(bio) >> 9;
		__bio_kunmap_atomic(buffer);
	}
	return 0; /* Always "succeed" */
}


/*
 * The direct make request version.
 */
static void sbull_make_request(struct request_queue *q, struct bio *bio)
{
	struct sbull_dev *dev = q->queuedata;
	int status;

	status = sbull_xfer_bio(dev, bio);
    bio_endio(bio);	
}


/*
 * Open and close.
 */
/* changed open and release function */

static int sbull_open(struct block_device *bdev, fmode_t mode)
{
	
    struct sbull_dev *dev = bdev->bd_disk->private_data;
	// TODO: complete

	spin_lock(&dev->lock);
	if (!dev->users) check_disk_change(bdev);
	dev->users++;
	spin_unlock(&dev->lock);
    return 0;
}

static void sbull_release(struct gendisk *disk, fmode_t mode)
{
	struct sbull_dev *dev = disk->private_data;
    
	// TODO: complete
	spin_lock(&dev->lock);
	dev->users--;
	spin_unlock(&dev->lock);
}

/*
 * The ioctl() implementation
 */

int sbull_ioctl (struct block_device *bdev, fmode_t mode,
                 unsigned int cmd, unsigned long arg)
{
    /* TODO : Write your codes */
    int buf, ret = 0;
	//struct sbull_dev *dev = bdev->bd_disk->private_data;

	switch (cmd) {
		case 0:
			// change mode
			if (encryption_enabled == 0) {
				input_key = 0;
				printk("Change mode\n Encrypt mode\n");
				encryption_enabled = 1;
			} else if (encryption_enabled == 1) {
				printk("Change mode\n Decrypt mode\n");
				encryption_enabled = 0;
			}
			break;
		case 1:
			// receive key
			copy_from_user((int*)&buf, (int*)arg, sizeof(int));
			input_key = (char)buf;
		default:
			ret = -ENOTTY;
			break;
	}

	return ret; /* unknown command */
}



/*
 * The device operations structure.
 */
static struct block_device_operations sbull_ops = {
	// TODO: complete
	.owner = THIS_MODULE,
	.open = sbull_open,
	.release = sbull_release,
	.ioctl = sbull_ioctl
};


/*
 * Set up our internal device.
 */
static void setup_device(struct sbull_dev *dev, int which)
{
    // TODO: complete
    /*
     * Get some memory.
     */
	memset(dev, 0, sizeof(struct sbull_dev));
	dev->size = nsectors * hardsect_size;
	dev->data = vmalloc(dev->size);
	if (dev->data == NULL) {
		printk(KERN_NOTICE "vmalloc failure.\n");
		return;
	}
	spin_lock_init(&dev->lock);
    
    /*
     * The I/O queue, depending on whether we are using our own
     * make_request function or not.
     */
    dev->queue = blk_alloc_queue(GFP_KERNEL);
    if (dev->queue == NULL)
        goto out_vfree;
    blk_queue_make_request(dev->queue, (make_request_fn *)sbull_make_request);		
    dev->queue->queuedata = dev;
    /*
     * And the gendisk structure.
     */
	dev->gd = alloc_disk(SBULL_MINORS);
	if (!dev->gd) {
		printk(KERN_NOTICE "alloc_disk failure\n");
		goto out_vfree;
	}
	dev->gd->major = sbull_major;
	dev->gd->first_minor = which * SBULL_MINORS;
	dev->gd->fops = &sbull_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;
	snprintf(dev->gd->disk_name, 32, "sbull%c", which + 'a');
	set_capacity(dev->gd, nsectors * (hardsect_size / KERNEL_SECTOR_SIZE));
	add_disk(dev->gd);
    
    return;

out_vfree:
    if (dev->data)
        vfree(dev->data);
}



static int __init sbull_init(void)
{
	int i;
	/*
	 * Get registered.
	 */
	sbull_major = register_blkdev(sbull_major, "sbull");
	if (sbull_major <= 0) {
		printk(KERN_WARNING "sbull: unable to get major number\n");
		return -EBUSY;
	}
	/*
	 * Allocate the device array, and initialize each one.
	 */
	Devices = kmalloc(ndevices*sizeof (struct sbull_dev), GFP_KERNEL);
	if (Devices == NULL)
		goto out_unregister;
	for (i = 0; i < ndevices; i++) 
		setup_device(Devices + i, i);

    return 0;

  out_unregister:
	unregister_blkdev(sbull_major, "sbd");
	return -ENOMEM;
}

static void sbull_exit(void)
{
	int i;

	for (i = 0; i < ndevices; i++) {
		struct sbull_dev *dev = Devices + i;

		if (dev->gd) {
			del_gendisk(dev->gd);
			put_disk(dev->gd);
		}
		if (dev->queue) {
			kobject_put (&dev->queue->kobj);
		}
		if (dev->data)
			vfree(dev->data);
	}
	printk(KERN_INFO "compression ratio : %ld\n", compression_ratio);
	unregister_blkdev(sbull_major, "sbull");
	kfree(Devices);
}
	
module_init(sbull_init);
module_exit(sbull_exit);
