static void sbull_transfer(struct sbull_dev *dev, unsigned long sector,

    unsigned long nsect, char *buffer, int write) {

    unsigned long offset = sector * KERNEL_SECTOR_SIZE;
    unsigned long nbytes = nsect * KERNEL_SECTOR_SIZE;
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
        if (COMPRESSION_ENABLED){
            comp_size[sector][0] = nbytes;
            if (encryption_enabled) { encryptDecrypt(buffer, nbytes); }
            ret = crypto_comp_compress(comp, buffer, nbytes, output_buffer, &dsize);

            if (ret) {
                pr_err("compress error!");
                goto out;
            }
            printk(KERN_ALERT "dsize :%d\n", dsize);
            if (dsize) {
                comp_size[sector][1] = dsize;
                if (compression_ratio != 0) {
                    compression_ratio = (compression_ratio + ((dsize * 100) / (unsigned int)nbytes))/2;
            }
            else {
                compression_ratio = (dsize * 100) / (unsigned int)nbytes;
            }
            printk(KERN_CONT "compression ratio %d \n", compression_ratio);

            } else {
                printk(KERN_ALERT "compression failed\n");
                comp_size[sector][1] = nbytes;
            }

            memcpy(dev->data + offset, output_buffer, nbytes);
            kfree(output_buffer);
        }
        else{
            memcpy(dev->data + offset, buffer, nbytes);
    }
    } else {
    if(COMPRESSION_ENABLED){
        dsize = comp_size[sector][0];
        if(!comp_size[sector][1]) goto out;
        slen = comp_size[sector][1];

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
        kfree(output_buffer2);
        kfree(output_buffer3);
    }
    else{
        memcpy(buffer, dev->data + offset, nbytes);
    }
    }
    out:

    kfree(output_buffer);
    return ;
}

/*------------------------*/


int sbull_ioctl (struct block_device *bdev, fmode_t mode,

    unsigned int cmd, unsigned long arg)

    {
    int ret = 0;
    switch(cmd){
        case SBULL_PRINT_INFO:
            switch (COMPRESSION_ENABLED) {
                case 0: printk(KERN_INFO "Compression disabled\n"); break;
                case 1: printk(KERN_INFO "Compression enabled\n"); break;
            }
            break;

            case SBULL_ENABLE_COMPRESS:
                COMPRESSION_ENABLED = 1;
                break;

            case SBULL_DISABLE_COMPRESS:
                COMPRESSION_ENABLED = 0;
                break;

            default:
                ret = -ENOTTY;
                break;
    }

    return ret;
}
