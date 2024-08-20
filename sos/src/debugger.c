#include <sos/gen_config.h>
#ifdef CONFIG_SOS_GDB_ENABLED
#include <sel4runtime.h>
#include <errno.h>
#include <cspace/cspace.h>
#include <gdb.h>
#include <libco.h>

#include "threads.h"
#include "vmem_layout.h"
#include "utils.h"
#include "debugger.h"
#include "mapping.h"

#define IRQ_BIT BIT(63)

#define STACK_SIZE 4096
static char t_main_stack[STACK_SIZE];
static char t_invocation_stack[STACK_SIZE];
static bool in_resume_state = false;

struct pending_registration {
	uint64_t id;
	seL4_CPtr tcb;
};

struct pending_registrations {
	int head;
	int tail;
	struct pending_registration pending[MAX_THREADS];
};

struct pending_registrations pending_registrations = {0};

bool any_pending_registrations() {
	return (pending_registrations.tail - pending_registrations.head) > 0;
}

void add_pending_registration(uint64_t id, seL4_CPtr tcb) {
	pending_registrations.pending[pending_registrations.tail % MAX_THREADS] =
		(struct pending_registration) {
			.id = id,
			.tcb = tcb
	};
	pending_registrations.tail++;
}

struct pending_registration get_pending_registration() {
	struct pending_registration ret = pending_registrations.pending[pending_registrations.head % MAX_THREADS];
	pending_registrations.head++;
	return ret;
}

void _putchar(char character) {
    uart_putchar(character);
}

/* Input buffer */
static char input[BUFSIZE];

/* Output buffer */
static char output[BUFSIZE];

typedef enum event_state {
    eventState_none = 0,
    eventState_waitingForInputEventLoop,
    eventState_waitingForInputInvocation
} event_state_t;

cothread_t t_event, t_main, t_invocation;

typedef struct debugger_data {
	seL4_CPtr ep;
	seL4_CPtr reply;
} debugger_data_t;

static debugger_data_t debugger_data = {};
static sos_thread_t *debugger_thread = NULL;
static event_state_t state = eventState_none;
static bool detached = false;

struct UARTRecvBuf {
    unsigned int head;
    unsigned int tail;
    char data[2048];
};

struct UARTRecvBuf *uart_recv_buf = 0;
gdb_inferior_t *sos_inferior = NULL;

void suspend_system() {
	for (int i = 0; i < MAX_THREADS; i++) {
		if (sos_inferior->threads[i].enabled) {
			seL4_TCB_Suspend(sos_inferior->threads[i].tcb);
		}
	}
}

void resume_system() {
	for (int i = 0; i < MAX_THREADS; i++) {
		if (!sos_inferior->threads[i].enabled) continue;
		if (sos_inferior->threads[i].wakeup) {
			seL4_TCB_Resume(sos_inferior->threads[i].tcb);
		}
	}
}

char gdb_get_char(event_state_t new_state) {
    while (uart_recv_buf->tail == uart_recv_buf->head) {
        // Wait for the virt to tell us some input has come through
        state = new_state;
        co_switch(t_event);
    }

    char c = uart_recv_buf->data[uart_recv_buf->tail % 2048];
	uart_recv_buf->tail++;

    return c;
}

void gdb_put_char(char c) {
	uart_putchar_gdb(c);
}

char *get_packet(event_state_t new_state) {
    char c;
    int count;
    /* Checksum and expected checksum */
    unsigned char cksum, xcksum;
    char *buf = input;
    (void) buf;

    while (1) {
        /* Wait for the start character - ignoring all other characters */
        c = gdb_get_char(new_state);
        while (c != '$') {
            /* Ctrl-C character - should result in an interrupt */
            if (c == 3) {
                buf[0] = c;
                buf[1] = 0;
                return buf;
            }
            c = gdb_get_char(new_state);
        }
        retry:
        /* Initialize cksum variables */
        cksum = 0;
        xcksum = -1;
        count = 0;
        (void) xcksum;

        /* Read until we see a # or the buffer is full */
        while (count < BUFSIZE - 1) {
            c = gdb_get_char(new_state);
            if (c == '$') {
                goto retry;
            } else if (c == '#') {
                break;
            }
            cksum += c;
            buf[count++] = c;
        }

        /* Null terminate the string */
        buf[count] = 0;

        if (c == '#') {
            c = gdb_get_char(new_state);
            xcksum = hexchar_to_int(c) << 4;
            c = gdb_get_char(new_state);
            xcksum += hexchar_to_int(c);

            if (cksum != xcksum) {
                gdb_put_char('-');   /* checksum failed */
            } else {
                gdb_put_char('+');   /* checksum success, ack*/

                if (buf[2] == ':') {
                    gdb_put_char(buf[0]);
                    gdb_put_char(buf[1]);

                    return &buf[3];
                }

                return buf;
            }
        }
    }

    return NULL;
}

static void put_packet(char *buf, event_state_t new_state)
{
    uint8_t cksum;
    for (;;) {
        gdb_put_char('$');
        for (cksum = 0; *buf; buf++) {
            cksum += *buf;
            gdb_put_char(*buf);
        }
        gdb_put_char('#');
        gdb_put_char(int_to_hexchar(cksum >> 4));
        gdb_put_char(int_to_hexchar(cksum % 16));
        char c = gdb_get_char(new_state);
        if (c == '+') break;
    }
}

/*
 * Used for sending a packet to GDB that is not exactly a response i.e. when an event such as a
 * fault or thread_spawn occurs.
 */
void notify_gdb() {
    put_packet(output, eventState_waitingForInputInvocation);
    // Go back to waiting for normal input after we send the fault packet to the host
    state = eventState_waitingForInputEventLoop;
    co_switch(t_event);
}


bool handle_debugger_register(seL4_Word badge, seL4_CPtr tcb) {
	/* Register the thread as an inferior */
	gdb_thread_t *thread = gdb_register_thread(sos_inferior, badge, tcb);
	if (!thread) {
		printf("GDB: Failed to register thread. You have too many active concurrent threads\n");
		return false;
	}

	if (!detached) {
		gdb_thread_spawn(thread, output);
		/* We suspend the system here (after adding the new thread).
		   This is fine on single-core. */
		suspend_system();
		t_invocation = co_derive((void *) t_invocation_stack, STACK_SIZE, notify_gdb);
		co_switch(t_invocation);
	}

	return true;
}

void handle_debugger_deregister(gdb_thread_t *thread) {

	/* Remove the thread from GDB */
	gdb_thread_exit(thread, output);

	if (!detached) {
		suspend_system();
		t_invocation = co_derive((void *) t_invocation_stack, STACK_SIZE, notify_gdb);
		co_switch(t_invocation);
	}
}

static void gdb_event_loop() {
    bool resume = false;
    /* The event loop runs perpetually if we are in the standard event loop phase */
    while (true) {
        char *input = get_packet(eventState_waitingForInputEventLoop);
        if (detached || input[0] == 3) {
            /* If we got a ctrl-c packet, we should suspend the whole system */
            suspend_system();
            in_resume_state = false;
            detached = false;
        }
        resume = gdb_handle_packet(input, output, &detached);

        if (!resume || detached) {
	        put_packet(output, eventState_waitingForInputEventLoop);
        }

        if (resume) {
        	if (any_pending_registrations()) {
        		struct pending_registration pr = get_pending_registration();
				bool success = handle_debugger_register(pr.id, pr.tcb);
				if (!success) {
					while (any_pending_registrations()) {
						get_pending_registration();
					}
					resume_system();
					in_resume_state = true;
				}
        	} else {
	            resume_system();
	            in_resume_state = true;
        	}
        }
    }
}

#define LABEL_DEBUGGER_REGISTER 1
#define LABEL_DEBUGGER_DEREGISTER 2

void seL4_event_loop() {
	bool have_reply = false;
	seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, 0);

	while (1) {
		seL4_Word badge = 0;
		seL4_MessageInfo_t message;

		if (have_reply) {
			message = seL4_ReplyRecv(debugger_data.ep, reply_msg, &badge, debugger_data.reply);
		} else {
			message = seL4_Recv(debugger_data.ep, &badge, debugger_data.reply);
		}

		seL4_Word label = seL4_MessageInfo_get_label(message);

		if (badge & IRQ_BIT) {
			/* Deal with a UART recv notification */
		    if (state == eventState_waitingForInputEventLoop) {
        		state = eventState_none;
        		co_switch(t_main);
    		} else if (state == eventState_waitingForInputInvocation) {
        		state = eventState_none;
        		co_switch(t_invocation);
    		}
    		have_reply = false;
		} else if (badge & DEBUGGER_FAULT_BIT) {
			/* Deal with a fault from a debugee thread */
			seL4_Word reply_mr = 0;
			seL4_Word id = badge & ~DEBUGGER_FAULT_BIT;

    		if (label != seL4_Fault_DebugException) {
	    		debug_print_fault(message, "");
    		}
    		have_reply = false;

			if (!detached) {
	    		suspend_system();

				gdb_thread_t *faulting_thread = NULL;
				for (int i = 0; i < 256; i++) {
					if (id == sos_inferior->threads[i].id) {
						faulting_thread = &sos_inferior->threads[i];
						break;
					}
				}

				in_resume_state = false;
				have_reply = gdb_handle_fault(faulting_thread, label, &reply_mr, output);
				t_invocation = co_derive((void *) t_invocation_stack, STACK_SIZE, notify_gdb);
				co_switch(t_invocation);

				if (have_reply) {
					reply_msg = seL4_MessageInfo_new(0, 0, 0, 0);
				}
			}
		} else {
			/* Deal with a thread registration/deregistration */
			assert(badge == 0);
			if (label == LABEL_DEBUGGER_REGISTER) {
				seL4_Word id = seL4_GetMR(0);
				seL4_CPtr tcb = seL4_GetMR(1);

				int i = 0;
				for (; i < MAX_THREADS; i++) {
					if (id == sos_inferior->threads[i].id) {
						break;
					}
				}

				/* We only register if this ID is not already registered
				 *
				 * We do this check for two reasons:
				 * 	1. TCB_Suspend() cancels an in-progress IPC, and when the thread is resumed, it
				 *     will attempt the IPC again, which can result in duplicate entries. In
				 *     particular, this will happen for the thread that is calling
				 *     debugger_register, meaning that it will be called twice for every thread as
				 *     handle_debugger_register() suspends the calling thread.
				 *  2. If you accidentally provide two identical IDs, this can cause issues
				 *     where GDB gets confused. Adding a print would help debug this, but it
				 * 	   would be noisy because of reason 1.
				 */
				if (i == MAX_THREADS && in_resume_state) {
					in_resume_state = false;
					handle_debugger_register(id, tcb);
				} else if (i == MAX_THREADS && !in_resume_state) {
					seL4_TCB_Suspend(tcb);
					add_pending_registration(id, tcb);
				}

			} else if (label == LABEL_DEBUGGER_DEREGISTER) {
				seL4_Word id = seL4_GetMR(0);

				int i = 0;
				for (; i < MAX_THREADS; i++) {
					if (id == sos_inferior->threads[i].id) {
						break;
					}
				}

				if (i != MAX_THREADS) {
					handle_debugger_deregister(&sos_inferior->threads[i]);
				}
			}

			reply_msg = seL4_MessageInfo_new(0, 0, 0, 0);
			have_reply = true;
		}
	}
}

void debugger_main(void *data) {

	/* Register the main GDB thread */
	sos_inferior = gdb_register_inferior(0, seL4_CapInitThreadVSpace);
	gdb_register_thread(sos_inferior, 0, seL4_CapInitThreadTCB);

	/* Suspend the threads in the system */
	suspend_system();

	printf("Awaiting GDB connection...\n");

	t_main = co_active();
	t_event = co_derive((void *) t_main_stack, STACK_SIZE, seL4_event_loop);

	gdb_event_loop();
}

/*
 * Register a thread into GDB
 */
void debugger_register_thread(seL4_CPtr ep, seL4_Word badge, seL4_CPtr tcb) {
	seL4_MessageInfo_t msginfo = seL4_MessageInfo_new(LABEL_DEBUGGER_REGISTER, 0, 0, 2);
	seL4_SetMR(0, badge);
	seL4_SetMR(1, tcb);

	/* Somewhat annoyingly, this call gets executed twice as this thread must be suspended,
	   which cancels the call operation and has it be reattempted when the thread is resumed.
	   If you are not careful with writing the debugger thread to handle this gracefully, it
	   can result in an infinite loop */
	msginfo = seL4_Call(ep, msginfo);
}

/*
 * Deregister a thread from GDB
 */
void debugger_deregister_thread(seL4_CPtr ep, seL4_Word badge) {
	seL4_MessageInfo_t msginfo = seL4_MessageInfo_new(LABEL_DEBUGGER_DEREGISTER, 0, 0, 1);
	seL4_SetMR(0, badge);
	msginfo = seL4_Call(ep, msginfo);
}

/*
 * Initialize the debugging subsystem
 *
 * We do the setup inside the init function as opposed to the debugger_main function because the
 * the utility libraries are not thread-safe.
 *
 * TODO: fix memory leaks
 */
seL4_Error debugger_init(cspace_t *cspace, seL4_IRQControl irq_control, seL4_CPtr recv_ep) {
	/* Create a reply object */
	seL4_CPtr reply;
	ut_t *reply_ut = alloc_retype(&reply, seL4_ReplyObject, seL4_ReplyBits);
	if (reply_ut == NULL) {
		return ENOMEM;
	}

	/* Create a notification object for binding to the GDB thread */
	seL4_CPtr bound_ntfn;
	ut_t *ntfn_ut = alloc_retype(&bound_ntfn, seL4_NotificationObject, seL4_NotificationBits);
	if (ntfn_ut == NULL) {
		return ENOMEM;
	}

	/* Create the IRQ handler cap for the virtual UART recv interrupt */
	seL4_CPtr irq_handler = cspace_alloc_slot(cspace);
	if (irq_handler == 0) {
		return ENOMEM;
	}

	seL4_Error err = cspace_irq_control_get(cspace, irq_handler, irq_control,
											VIRTUAL_UART_RECV_IRQ, false);
	if (err) {
		return err;
	}

	/* Mint a badged notification cap for the IRQ to be delivered using */
	seL4_Word badge = IRQ_BIT | BIT(0);
	seL4_CPtr badged_ntfn = cspace_alloc_slot(cspace);
	if (badged_ntfn == 0) {
		return ENOMEM;
	}

	err = cspace_mint(cspace, badged_ntfn, cspace, bound_ntfn,
				seL4_CanWrite, badge);
	if (err) {
		return err;
	}

	/* Set the IRQ to be delivered to this notification */
	err = seL4_IRQHandler_SetNotification(irq_handler, badged_ntfn);
	if (err) {
		return err;
	}

	/* Map in the shared ring buffer with the kernel for
	   receiving UART data */
	err = map_frame(cspace, seL4_CapUARTRecvBuffer, seL4_CapInitThreadVSpace,
							   SOS_UART_RECV_BUF_ADDRESS, seL4_AllRights,
							   seL4_ARM_Default_VMAttributes);
	if (err) {
		return err;
	}
	uart_recv_buf = SOS_UART_RECV_BUF_ADDRESS;

	/* Mint a badged fault endpoint cap for the main SOS thread */
	badge = DEBUGGER_FAULT_BIT;
	seL4_CPtr badged_fault_ep = cspace_alloc_slot(cspace);
	if (badged_fault_ep == 0) {
		return ENOMEM;
	}
	err = cspace_mint(cspace, badged_fault_ep, cspace, recv_ep, seL4_AllRights, badge);
	if (err) {
		return err;
	}

	/* Set the priority of the main SOS thread to maxprio - 1 and
   	   set its fault handler to be a badged copy of the GDB recv endpoint */
	 err = seL4_TCB_SetSchedParams(seL4_CapInitThreadTCB, seL4_CapInitThreadTCB, seL4_MaxPrio,
	 							   seL4_MaxPrio - 1, seL4_CapInitThreadSC, badged_fault_ep);

	/* Start the debugger thread */
	debugger_data = (debugger_data_t) {
		.ep = recv_ep,
		.reply = reply
	};

	debugger_thread = debugger_spawn(debugger_main, NULL, 0, bound_ntfn);
	if (debugger_thread == NULL) {
		return ENOMEM;
	}

	return seL4_NoError;
}

#endif /* CONFIG_SOS_GDB_ENABLED */