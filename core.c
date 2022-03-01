//////////////////////////////////////////////////////////////////////
//                     University of California, Riverside
//
//
//
//                             Copyright 2022
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng, Yu-Chia Liu
//
//   Description:
//     Core of Kernel Module for CSE202's Resource Container
//
////////////////////////////////////////////////////////////////////////

#include "blockmma.h"
#include <asm/segment.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include "core.h"

int task = 1;

static spinlock_t list_lock;

DEFINE_MUTEX(mp);
DEFINE_MUTEX(cnt);

struct LinkedList{
    	struct blockmma_cmd *c;
    	__u64 uc;
    	struct list_head list;
    	int count;
};
struct list_head lh;

// void printlist(void){
// 	struct list_head *ptr;
// 	struct LinkedList *my;
// 	list_for_each(ptr,&lh){
// 		my = list_entry(ptr, struct LinkedList, list);
// 		printk(KERN_ALERT "----------");
// 		printk(KERN_ALERT "%lld", my->c->tid);
// 	}
	
// }

extern struct miscdevice blockmma_dev;
/**
 * Enqueue a task for the caller/accelerator to perform computation.
 */
long blockmma_send_task(struct blockmma_cmd __user *user_cmd)
{
    struct LinkedList *temp_node;
    int i=0;
    void *mem1 = kmalloc(sizeof(float )*128*128, GFP_KERNEL);
    void *mem2 = kmalloc(sizeof(float )*128*128, GFP_KERNEL);
    void *mem3 = kmalloc(sizeof(float )*128*128, GFP_KERNEL);
    temp_node = kmalloc(sizeof(struct LinkedList), GFP_KERNEL);
    temp_node->c = kmalloc(sizeof(struct blockmma_cmd), GFP_KERNEL); 
    temp_node->c->a = (__u64)mem1;
    temp_node->c->b = (__u64)mem2;
    temp_node->c->c = (__u64)mem3;

    while (i<128){
    	copy_from_user(mem1 + i * 128 * sizeof(float),(void *)user_cmd->a + i * sizeof(float) * user_cmd->m, sizeof(float)*128);
    	copy_from_user(mem2 + i * 128 * sizeof(float),(void *)user_cmd->b + i * sizeof(float) * user_cmd->n, sizeof(float)*128);
    	copy_from_user(mem3 + i * 128 * sizeof(float),(void *)user_cmd->c + i * sizeof(float) * user_cmd->k, sizeof(float)*128);
    	i=i+1;
    }

        
    // ret = copy_from_user(mem1,(void *)user_cmd->a,sizeof(float)*128*128);
    // if(ret)
    // 	printk(KERN_ALERT"Error");
    // ret = copy_from_user(mem2,(void *)user_cmd->b,sizeof(float)*128*128);
    // if(ret)
    // 	printk(KERN_ALERT"Error");

    // ret = copy_from_user(mem3,(void *)user_cmd->c,sizeof(float)*128*128);
    // if(ret)
    // 	printk(KERN_ALERT"Error");
    temp_node->c->m = user_cmd->m;
    temp_node->c->n = user_cmd->n;
    temp_node->c->k = user_cmd->k;
    temp_node->c->tile = user_cmd->tile;
    temp_node->uc = user_cmd->c;
    temp_node->count = 0;

    // mutex_lock(&cnt);
    temp_node->c->tid = task;
    task = task + 1;
    // mutex_unlock(&cnt);

    
    temp_node->c->op = user_cmd->op;
    // printk(KERN_ALERT "%lld", temp_node->c->op);
    // printk(KERN_ALERT "%lld", temp_node->c->tid);
    // printk(KERN_ALERT "%lld", temp_node->c->a);
    // printk(KERN_ALERT "%lld", temp_node->c->b);
    // printk(KERN_ALERT "%lld", temp_node->c->c);
    // printk(KERN_ALERT "%lld", temp_node->c->m);
    // printk(KERN_ALERT "%lld", temp_node->c->n);
    // printk(KERN_ALERT "%lld", temp_node->c->k);
    // mutex_lock(&mp);
    spin_lock(&list_lock);
    list_add(&temp_node->list, &lh);
    spin_unlock(&list_lock);
    synchronize_rcu();
    // mutex_unlock(&mp);
        /*Init the list within the struct*/
    
    /*Add Node to Linked List*/
    /*
    INIT_LIST_HEAD(&temp_node->list);
    list_add_tail(&temp_node->list, &Head_Node);*/
 


    
/*
	struct blockmma_cmd *c;
    c = NULL;
    c = kmalloc(sizeof(struct blockmma_cmd), GFP_KERNEL);    

	if (c==NULL){
    printk(KERN_ALERT "Not correct");
    }
    else{
    printk(KERN_ALERT "something is done");
    }
    return 0;
    */
    return 0;
}

/**
 * Return until all outstanding tasks for the calling process are done
 */
int blockmma_sync(struct blockmma_cmd __user *user_cmd)
{
	struct list_head *ptr, *q;
	struct LinkedList *my;
	int i=0;
	// mutex_lock(&mp);
	rcu_read_lock();
	list_for_each(ptr,&lh){
		my = list_entry(ptr, struct LinkedList, list);
		if (my->count != 2){
			// mutex_unlock(&mp);
			rcu_read_unlock();
			return -1;
    	}
	}
	rcu_read_unlock();
	ptr = NULL;
	spin_lock(&list_lock);
	list_for_each(ptr,&lh){
		my = list_entry(ptr, struct LinkedList, list);
		if (my->count == 2){
			i=0;
    		while (i<128){
    			copy_to_user((void *)my->uc + i * sizeof(float)* my->c->k,
    				(void *)my->c->c + i * sizeof(float) * 128, 
    				sizeof(float)*128);
    			i=i+1;
    		}
			// copy_to_user((void *)my->uc, (void *)my->c->c, sizeof(float)*128*128);
			// my->count = 0;
    	}
	}


	list_for_each_safe(ptr, q, &lh){
        my= list_entry(ptr, struct LinkedList, list);
        kfree((void *)my->c->a);
        kfree((void *)my->c->b);
        kfree((void *)my->c->c);
        kfree((void *)my->c);
        list_del(ptr);
    }
    spin_unlock(&list_lock);
    synchronize_rcu();
  //   if(list_empty(&lh))
  //   {
  //   	printk(KERN_ALERT"Done");
  //   }
  //   else
  //   {
 	// 	list_for_each(ptr,&lh){
		// 	my = list_entry(ptr, struct LinkedList, list);
		// 	printk(KERN_ALERT"ELSE");
		// }
  //    }


	// mutex_unlock(&mp);
    
	return 0;


}

/**
 * Return the task id of a task to the caller/accelerator to perform computation.
 */
int blockmma_get_task(struct blockmma_hardware_cmd __user *user_cmd)
{
	// pid_t current->pid;
	struct list_head *ptr;
	struct LinkedList *my;
	// mutex_lock(&mp);
	spin_lock(&list_lock);
	list_for_each(ptr,&lh){
		
		my = list_entry(ptr, struct LinkedList, list);
		
		if (my->count == 0){
			my->count = 1;
			user_cmd->op = my->c->op;
			user_cmd->tid = my->c->tid;
			copy_to_user((void *)user_cmd->a, (void *)my->c->a ,sizeof(float)*128*128);
			copy_to_user((void *)user_cmd->b, (void *)my->c->b ,sizeof(float)*128*128);
			copy_to_user((void *)user_cmd->c, (void *)my->c->c ,sizeof(float)*128*128);

			// mutex_unlock(&mp);
			spin_unlock(&list_lock);
			// printk(KERN_ALERT "%lld", user_cmd->tid);
			synchronize_rcu();
			return user_cmd->tid;
		}
	}
		
	// mutex_unlock(&mp);
	spin_unlock(&list_lock);
	synchronize_rcu();
	// synchronize_rcu();


    return -1;
}
/**
 * Return until the task specified in the command is done.
 */
int blockmma_comp(struct blockmma_hardware_cmd __user *user_cmd)
{
	struct list_head *ptr;
	struct LinkedList *my;
	// mutex_lock(&mp);
	spin_lock(&list_lock);
	list_for_each(ptr,&lh){
		my = list_entry(ptr, struct LinkedList, list);
		if (my->c->tid == user_cmd->tid){

			my->c->op = user_cmd->op;
			// my->c->tid = user_cmd->tid;
			//copy_from_user((void *)user_cmd->a, (__u64)my->c->a,sizeof(float)*128*128);
    		//copy_from_user((void *)user_cmd->b, (__u64)my->c->b,sizeof(float)*128*128);
    		copy_from_user((void *)my->c->c, (void *)user_cmd->c,sizeof(float)*128*128);
    		my->count = 2;

			// mutex_unlock(&mp);
			spin_unlock(&list_lock);
			synchronize_rcu();
			return 0;
		}
	}
	// mutex_unlock(&mp);
	
	spin_unlock(&list_lock);
	synchronize_rcu();
    return -1;
}

/*
 * Tell us who wrote the module
 */
int blockmma_author(struct blockmma_hardware_cmd __user *user_cmd)
{
    struct blockmma_hardware_cmd cmd;
    char authors[] = "Mihir Patel (mpate125), 862324469, Vishv Patel (vpate062), 862322103 and Vineet Dhaimodker (vnaiq001), 862255153";
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd)))
    {
        return -1;
    }
    copy_to_user((void *)cmd.a, (void *)authors, sizeof(authors));
    return 0;
}

int blockmma_init(void)
{
    int ret =0;
    INIT_LIST_HEAD(&lh);
    mutex_init(&mp);
    mutex_init(&cnt);
    spin_lock_init(&list_lock);
    if ((ret = misc_register(&blockmma_dev)))
    {
        printk(KERN_ERR "Unable to register \"blockmma\" misc device\n");
        return ret;
    }
    printk("BlockMMA kernel module installed\n");
    return ret;
}

void blockmma_exit(void)
{
    printk("BlockMMA removed\n");
    misc_deregister(&blockmma_dev);
}