/******************************************************************************
 *
 *   Created by Alejandro Gómez <alegzc@yahoo.com.mx>
 *
 *****************************************************************************/
#include <linux/init.h>  /* module_init() & module_exit() */
#include <linux/module.h> /* module stuff */
#include <linux/fs.h> /* all of it! */
#include <linux/cdev.h> /* CDEV structure */
#include <linux/fcntl.h>  /* def of O_RDONLY and others */
#include <linux/errno.h> /* error numbers */
#include <linux/ioport.h> /* I/O {Port,Mem} registration */
#include <asm/uaccess.h> /* copy_form_user & to_user */
#include <linux/kernel.h> /* barrier memory ; sin cambios en la ejecución de inst.*/
#include <asm/system.h>   /* rmw () and friends -> ReadMemoryBarrier() */
#include <linux/slab.h>  /* kmalloc */
//#include <linux/delay.h> /* Retardos */
#include <linux/interrupt.h> /* interrupciones */
#include <linux/delay.h> /* usleep() and friends */

#define HELLO_MAJOR 60 
#define HELLO_MINOR  0
#define HELLO_MODULE_NAME "bitswapper"
#define HELLO_SIZE  16
#define HELLO_BASEADDR 0xBABE0000
#define HELLO_DATA_IN_OFFSET  0
#define HELLO_DATA_OUT_OFFSET 4

#ifdef DIRECT_IO
/* INPUT DATA */
#define GetData8(addr)		(unsigned char)(*(volatile unsigned int *)(addr))
#define GetData32(addr)         (unsigned  int)(*(volatile unsigned int *)(addr))

/* OUTPUT DATA */
#define PutData8(dat,addr)   (*(volatile unsigned int *)(addr) = ((unsigned char)(dat)))
#define PutData32(dat,addr)  (*(volatile unsigned int *)(addr) = ((unsigned  int)(dat)))

#endif

struct dispositivo {
	int val; /* sólo para prueba */
        struct file_operations *fops;
        struct cdev cdev;	/* Char device structure */
};

struct dispositivo *hello_dev;
unsigned char myVal;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 ssize_t hello_write(struct file *filp, 
                      const char __user *buf, size_t count,
                      loff_t *f_pos)
{
	int test = 0;
	//myVal = hello_dev->val;
	myVal++;
	//hello_dev->val = myVal;
	printk(KERN_ERR "Hello: Petición de escritura... %d", myVal);	
#ifdef ESYS
	mb();
	PutData32(0xA0A0AAAA, (HELLO_BASEADDR + HELLO_DATA_IN_OFFSET)); // Sólo para Embedded System
	mb(); /* memory barrier */
	test = GetData32((HELLO_BASEADDR + HELLO_DATA_OUT_OFFSET));
	mb();
	printk(KERN_ERR "Hello: sended(0xA0A0AAAA)\treceived(%8x)\n",test);
	mb();
#endif
 	return count;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int hello_open(struct inode *inode, struct file *filp) {
	struct cdev *device = NULL;
	dev_t dev_num = 0;
	
	/* Obteniendo MAJOR y MINOR de la estructura INODE */
	dev_num = MKDEV( imajor(inode) , iminor(inode) ) ;
	
	device = kmalloc(sizeof(struct cdev), GFP_KERNEL); //Normal allocation
	device->dev = dev_num;
	filp->private_data = device;
	return 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int hello_release(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL ;
	return 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - -
struct file_operations hello_fops = {
	.owner =    THIS_MODULE,
	.read =     NULL, //hello_read,
	.write =    hello_write,
//	.ioctl =    NULL,//master_ioctl,
	.open =     hello_open,
	.release =  hello_release,
};

static int __init hello_init(void) {
	dev_t dev = 0; /* Almacenará numero Mayor y Menor */
	int result;
	struct resource *dev_io_mem ; /* I/O Mem resource port */
	struct resource *dev_io_ports ; /* I/O resource port */

/* núm Mayor y Menor del disp */
	dev = MKDEV(HELLO_MAJOR, HELLO_MINOR); 
	printk(KERN_ERR "hello: dev = %x", dev);

/* registro del dispositivo hello */
	result = register_chrdev_region(dev, 1, HELLO_MODULE_NAME); 
	if (result < 0) {
		printk(KERN_ERR "Hello: error al registrar el dispositivo %d", HELLO_MAJOR);
		unregister_chrdev_region( dev , 1 ) ;
		return result; // to identify Error Number
	}
	printk(KERN_ERR "hello: register_chrdev_region... ok(%x)", result);

	hello_dev = kmalloc(sizeof(struct dispositivo), GFP_KERNEL);
	if (hello_dev == NULL) {
		result = -ENOMEM;
		printk(KERN_ERR "hello: error al obtener memoria del kernel");
		return result;
	}
	printk(KERN_ERR "hello: kmalloc... ok");

/* Inicialización de la estructura en 0 */
	memset(hello_dev, 0, sizeof(struct dispositivo));
	printk(KERN_ERR "hello: memset... ok");

	/* Inicialización de la estructura cdev dentro de hello_dev 
	 * y registro del dispositivo */
	cdev_init(&hello_dev->cdev, &hello_fops);
	hello_dev->cdev.owner = THIS_MODULE;
	hello_dev->cdev.ops = &hello_fops;
	hello_dev->cdev.dev = dev ;

	/* Registro y asignación del área de memoria de entrada y salida */ 
	dev_io_mem = request_mem_region(HELLO_BASEADDR, HELLO_SIZE , HELLO_MODULE_NAME);	
	if (dev_io_mem == NULL ) {
		printk(KERN_ERR "hello: error al registrar los puertos de E/S");
		release_mem_region(HELLO_BASEADDR, HELLO_SIZE);
		unregister_chrdev_region( dev , 1 ) ;
		return result; // to identify Error Number
	}
	printk(KERN_ERR "hello: request_mem_region... ok");

	/* Registro y asignación de puertos de entrada y salida */ 
	dev_io_ports = request_region(HELLO_BASEADDR, HELLO_SIZE , HELLO_MODULE_NAME);	
	if (dev_io_ports == NULL ) {
		printk(KERN_ERR "hello: error al registrar los puertos de E/S");
		release_region(HELLO_BASEADDR, HELLO_SIZE);
		unregister_chrdev_region( dev , 1 ) ;
		return result; // to identify Error Number
	}
	printk(KERN_ERR "hello: request_io_region... ok");

	//PutData8(0xFF,LEDS_OUT_PORT); /* Desactiva los leds */
	//printk(KERN_ERR "hello: PutData8()");


	result = cdev_add (&hello_dev->cdev, dev, 1); /* Listo y funcionando */
	if (result < 0){
		printk(KERN_ERR "hello: error %d el dispositivo %d no fue agregado al sistema", result, HELLO_MAJOR);
		cdev_del( &hello_dev->cdev );  /* CharDev removed */
		release_mem_region(HELLO_BASEADDR, HELLO_SIZE);
		unregister_chrdev_region( dev , 1 ) ;
		return result; // to identify Error Number
	}
	printk(KERN_ERR "hello: cdev_add... ok(%x)", result);

	printk(KERN_ERR "HELLO: MODULO EN LINEA Y OPERANDO...\n");
#ifdef ALEGZC
	printk(KERN_ERR "hello: mensaje de prueba\n");
#endif

	//PutData32(0xAA55CCFF,LEDS_OUT_PORT); /* Desactiva los leds */
	return 0; /* Inicialización exitosa */
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - -
static void __exit hello_exit(void) {
	dev_t dev = 0;
	printk(KERN_ERR "hello: terminando...  hello_exit");
	//PutData8(0x00,LEDS_OUT_PORT); /* Desactiva los leds */
	dev = MKDEV(HELLO_MAJOR, HELLO_MINOR);
	release_region(HELLO_BASEADDR,HELLO_SIZE); /* free io port */
	release_mem_region(HELLO_BASEADDR,HELLO_SIZE); /* free io mem */
	cdev_del( &hello_dev->cdev );  /* CharDev removed */
	unregister_chrdev_region( dev , 1 ); /* free Major & Minor */
	printk(KERN_ERR "Hello: Módulo fuera de línea... ");
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - -
module_init(hello_init);
module_exit(hello_exit);
// - - - - - - - - - - - - - - - - - - - - - - - - - - - -
MODULE_AUTHOR("Alejandro Gómez Conde");
MODULE_DESCRIPTION("Controlador para los leds de la tarjeta");
/* MODULE_VERSION("0.2"); */
MODULE_LICENSE("GPL v2");	/* license type */


