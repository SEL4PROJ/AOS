#include <sos/gen_config.h>
#define VIRTUAL_UART_RECV_IRQ 0xfff

#ifdef CONFIG_SOS_GDB_ENABLED
#define DEBUGGER_FAULT_BIT BIT(62)
seL4_Error debugger_init(cspace_t *cspace, seL4_IRQControl irq_control, seL4_CPtr recv_ep);
void debugger_register_thread(seL4_CPtr ep, seL4_Word badge, seL4_CPtr tcb);
void debugger_deregister_thread(seL4_CPtr ep, seL4_Word badge);
#endif /* CONFIG_SOS_GDB_ENABLED */