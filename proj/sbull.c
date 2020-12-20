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

//char *output_buffer;
unsigned int compression_ratio = 0;
unsigned int comp_size[1024*1024][2] = {0,};
struct crypto_comp *comp;

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
		unsigned long nsect, char *buffer, int write) {
	// TODO: complete
	unsigned long offset = sector * KERNEL_SECTOR_SIZE;
	unsigned long nbytes = nsect * KERNEL_SECTOR_SIZE;
	//unsigned int dlen = COMP_BUF_SIZE;
	unsigned int slen = 0;
	int ret = 0;
	unsigned int dsize = nbytes;
	
	char *output_buffer = kmalloc(dsize, GFP_KERNEL);
	char *output_buffer2, *output_buffer3;
	memset(output_buffer, 0, dsize);
	
	if ((offset + nbytes) > dev->size) {
		printk(KERN_NOTICE "Beyond-end write (%ld %ld)\n", offset, nbytes);
		return;
	}
	if (write) {
		comp_size[sector][0] = nbytes;
		if (encryption_enabled) { encryptDecrypt(buffer, nbytes); }
		printk(KERN_INFO "[comp] buffer : %s\n", buffer);
		ret = crypto_comp_compress(comp, buffer, nbytes, output_buffer, &dsize); 
		printk(KERN_INFO "[comp] compressed buffer : %s\n", output_buffer);
		if (ret) {
			pr_err("compress error!");
			goto out;
		}
		//next = nbytes;
		printk(KERN_INFO "[comp]nbytes :%ld, dsize :%u\n", nbytes, dsize);
		if (dsize) {
			//prev =*dsize;
			comp_size[sector][1] = dsize;
			if (compression_ratio) {
				compression_ratio = (compression_ratio + ((dsize * 100) / (unsigned int)nbytes))/2;
			}
			else {
				compression_ratio = (dsize * 100) / (unsigned int)nbytes;
			}
			printk(KERN_INFO "compression ratio : %u %\n", compression_ratio);
		} else {
			printk(KERN_ALERT "compression failed\n");
			comp_size[sector][1] = nbytes;
		}
		memcpy(dev->data + offset, output_buffer, dsize);
	} else {
		dsize = nbytes;
		if (!comp_size[sector][1]) goto out;
		slen = comp_size[sector][1];
		printk(KERN_INFO "[decomp] nbytes:%ld, dsize:%u\n", nbytes, slen);
		output_buffer2 = kmalloc(slen, GFP_KERNEL);
		memset(output_buffer2, 0, slen);
		memcpy(output_buffer2, dev->data + offset, slen);
		
		output_buffer3 = kmalloc(dsize, GFP_KERNEL);
		memset(output_buffer3, 0, dsize);

		if (!comp) printk(KERN_ALERT "comp NULL\n");
		ret = crypto_comp_decompress(comp, output_buffer2, slen, output_buffer3, &dsize);
		if(ret){
			pr_err("decompress error!");
			goto out;
		}
		if (!dsize) {
			printk(KERN_ALERT "decomp error! size over the buffer\n");
			dsize = comp_size[sector][0];
		}
		memcpy(buffer, output_buffer3, dsize);
		if (encryption_enabled && key == input_key) { encryptDecrypt(buffer, nbytes); }
		kfree(output_buffer2);
		kfree(output_buffer3);
	}
out:
	kfree(output_buffer);
	//kfree(dsize);
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
	//comp = crypto_alloc_comp("lzo", 0, 0);
    
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
	
	comp = crypto_alloc_comp("lzo", 0, 0);
	if (!comp) printk(KERN_ALERT "crypto_comp didnt create at all\n");
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
	printk(KERN_INFO "final compression ratio : %u\n", compression_ratio);
	unregister_blkdev(sbull_major, "sbull");
	kfree(Devices);
}
	
module_init(sbull_init);
module_exit(sbull_exit);
