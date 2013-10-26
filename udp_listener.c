#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/slab.h>

#include <linux/errno.h>	//For error handling

#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/delay.h>

#include <net/sock.h>
#include <linux/skbuff.h>
#include <linux/inet.h>

#define DEFAULT_PORT 666
#define CONNECT_PORT 333

#define MODULE_NAME "udp_command_listener"

#define INADDR_SEND INADDR_LOOPBACK

static spinlock_t m_lock;

static DEFINE_SPINLOCK(m_lock);

struct m_thread_t
{
	struct task_struct *m_thread;
	struct socket *m_socket;
	struct sockaddr_in m_addr_in;
	struct socket *m_socket_send;
	struct sockaddr_in m_addr_send;
	int status;
};

unsigned short * port;

struct m_thread_t *thread = NULL;

int m_socket_receive(struct socket *m_socket, struct sockaddr_in *m_addr, unsigned char *buffer, int length);
int m_socket_send(struct socket *m_socket, struct sockaddr_in *m_addr, unsigned char *buffer, int length);
int m_respond(unsigned char *buffer);

static void m_socket_start(void) {
	int recieved_size;
	int error;
	int buffer_size = 1024;
	unsigned char buffer[buffer_size + 1];

	// Starting kernel thread
//	lock_kernel();
	spin_lock(&m_lock);
	thread->status = 1;
	current->flags |= PF_NOFREEZE;

	// Setting as a daemon
//	daemonize("%s" MODULE_NAME);
	// Allowing this module to be killed
	allow_signal(SIGKILL);
	spin_unlock(&m_lock);
//	unlock_kernel();

	// Attempting to create the socket
	error = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &thread->m_socket);
	if(error < 0) {
		// ERROR HAPPENED
		goto exit;
	}
	error = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &thread->m_socket_send);
	if(error < 0) {
		// ERROR HAPPENED
		goto exit;
	}

	memset(&thread->m_addr_in, 0, sizeof(struct sockaddr));
	memset(&thread->m_addr_send, 0, sizeof(struct sockaddr));

	//ADDRESS_FAMILY INTERNET
	thread->m_addr_in.sin_family = AF_INET;
	thread->m_addr_send.sin_family = AF_INET;

	// Host To Network Long. Converting to network byte order (Big Endian)
	thread->m_addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
	thread->m_addr_send.sin_addr.s_addr = in_aton("127.0.0.1");

	thread->m_addr_in.sin_port = htons(DEFAULT_PORT);
	port = htons(CONNECT_PORT);
	thread->m_addr_send.sin_port = *port;

	error = thread->m_socket->ops->bind(thread->m_socket, (struct sockaddr *)&thread->m_addr_in, sizeof(struct sockaddr));
	if(error < 0) {
		// ERROR HAPPENED
		goto clean_and_exit;
	}
	error = thread->m_socket_send->ops->bind(thread->m_socket_send, (struct sockaddr *)&thread->m_addr_send, sizeof(struct sockaddr));
	if(error < 0) {
		// ERROR HAPPENED
		goto clean_and_exit;
	}

	printk(KERN_INFO "%s: Listening on port %d\n", MODULE_NAME, DEFAULT_PORT);

	// Main loop
	while(true) {
		memset(&buffer, 0, buffer_size + 1);
		recieved_size = m_socket_receive(thread->m_socket, &thread->m_addr_in, buffer, buffer_size);

		if(signal_pending(current)) {
			break;
		}

		if(recieved_size < 0) {
			// ERROR
		}
		else {
			// WE HAVE DATA!
			printk(KERN_INFO "%s: Received %d bytes\n", MODULE_NAME, recieved_size);
			printk("\n Data: %s", buffer);

			// Sending data back

			printk(KERN_INFO "\n%s:  Preparing data to send back", MODULE_NAME);

			memset(&buffer, 0 , buffer_size + 1);
			strcat(buffer, "Hello");

			printk(KERN_INFO "\n%s:  Sending data back", MODULE_NAME);

			m_socket_send(thread->m_socket_send, &thread->m_addr_send, buffer, strlen(buffer));

//			m_respond(buffer);

			printk(KERN_INFO "\n%s:  Data sent", MODULE_NAME);
		}
	}

	//GOTOs
	clean_and_exit:
		sock_release(thread->m_socket);
		sock_release(thread->m_socket_send);
		thread->m_socket = NULL;
		thread->m_socket_send = NULL;

	exit:
		thread->m_socket = NULL;
		thread->m_socket_send = NULL;
}

int m_socket_send(struct socket* socket, struct sockaddr_in* m_addr, unsigned char* buffer, int length) {
	struct msghdr message;
	struct iovec iov;
	mm_segment_t oldfs;
	int size = 0;

//	if(socket->sk == NULL) {
//		return 0;
//	}

	memset(&message,0,sizeof(message));

	iov.iov_base = buffer;
	iov.iov_len = length;

	message.msg_flags = 0;
	message.msg_name = m_addr;
	message.msg_namelen = sizeof(struct sockaddr_in);
	message.msg_control = NULL;
	message.msg_controllen = 0;
	message.msg_iov = &iov;
	message.msg_iovlen = 1;

	// Setting proper function space
	oldfs = get_fs();
	set_fs(KERNEL_DS);

	size = sock_sendmsg(socket, &message, length);

	set_fs(oldfs);

	return size;
}

int m_respond(unsigned char* buffer) {
			unsigned short * port;
			int len;
			struct msghdr msg;
			struct iovec iov;
			mm_segment_t oldfs;
//			struct sockaddr_in to;
//
//			/* receive packet */
//			skb = skb_dequeue(&foo->sk->sk_receive_queue);
//			printk("message len: %i message: %s\n", skb->len - 8, skb->data+8); /*8 for udp header*/

			/* generate answer message */
			memset(&thread->m_addr_send,0, sizeof(thread->m_addr_send));
			thread->m_addr_send.sin_family = AF_INET;
			thread->m_addr_send.sin_addr.s_addr = in_aton("127.0.0.1");
			port = htons(CONNECT_PORT);
			thread->m_addr_send.sin_port = htons(CONNECT_PORT);
			memset(&msg,0,sizeof(msg));
			msg.msg_name = &thread->m_addr_send;
			msg.msg_namelen = sizeof(thread->m_addr_send);
			/* send the message back */
			iov.iov_base = buffer;
			iov.iov_len  = strlen(buffer);
			msg.msg_control = NULL;
			msg.msg_controllen = 0;
			msg.msg_iov = &iov;
			msg.msg_iovlen = 1;
			/* adjust memory boundaries */
			oldfs = get_fs();
			set_fs(KERNEL_DS);
			len = sock_sendmsg(thread->m_socket_send, &msg, strlen(buffer));
			set_fs(oldfs);

			return 0;
}

int m_socket_receive(struct socket* socket, struct sockaddr_in* m_addr, unsigned char* buffer, int length) {
	struct msghdr message;
	struct iovec iov;
	mm_segment_t oldfs;
	int size = 0;

	iov.iov_base = buffer;
	iov.iov_len = length;

	message.msg_flags = 0;
	message.msg_name = m_addr;
	message.msg_namelen = sizeof(struct sockaddr_in);
	message.msg_control = NULL;
	message.msg_controllen = 0;
	message.msg_iov = &iov;
	message.msg_iovlen = 1;

	// Setting proper function space
	oldfs = get_fs();
	set_fs(KERNEL_DS);

	size = sock_recvmsg(socket, &message, length, message.msg_flags);

	set_fs(oldfs);

	return size;
}

int __init m_socket_init(void) {
	thread = kmalloc(sizeof(struct m_thread_t), GFP_KERNEL);
	memset(thread, 0, sizeof(struct m_thread_t));

	//Starting thread
	thread->m_thread = kthread_run((void *)m_socket_start, NULL, MODULE_NAME);
	if(IS_ERR(thread->m_thread)) {
		printk(KERN_INFO "%s: Unable to start kernel thread\n", MODULE_NAME);
		kfree(thread);
		thread = NULL;
		return -ENOMEM;
	}

	return 0;
}

void __exit m_socket_exit(void) {
	int error;

	if(thread->m_thread == NULL) {
		// Nothing to terminate
	}
	else {
//		lock_kernel();
//		error = kill_proc(thread->m_thread->pid, SIGKILL, 1);
		kill_pid(thread->m_thread->pid, SIGKILL, 1);
//		unlock_kernel();

		if(error < 0) {
			// ERROR TRYING TO TERMINATE THREAD
		}
		else {
			// Waiting for thread to die
			while(thread->status == 1) {
				msleep(10);
			}
		}
	}

	// Freeing memory
	if(thread->m_socket != NULL) {
		sock_release(thread->m_socket);
		thread->m_socket = NULL;
	}
	kfree(thread);
	thread = NULL;

	printk(KERN_INFO "%s: MODULE FINISHED", MODULE_NAME);
}

// Setting up module init stuff
module_init(m_socket_init);
module_exit(m_socket_exit);

MODULE_LICENSE("GPL");
