/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */
#include <autoconf.h>
#include <aos/debug.h>
#include <sel4/sel4.h>
#include <stdio.h>

void debug_cap_identify(seL4_CPtr cap)
{
#ifdef CONFIG_DEBUG_BUILD
    int type = seL4_DebugCapIdentify(cap);
    printf("Cap type number is %d\n", type);
#endif /* CONFIG_DEBUG_BUILD */
}

void debug_print_bootinfo(seL4_BootInfo *info)
{
    ZF_LOGD("Node %lu of %lu", (long)info->nodeID, (long)info->numNodes);
    ZF_LOGD("IOPT levels:     %u", (int)info->numIOPTLevels);
    ZF_LOGD("IPC buffer:      %p", info->ipcBuffer);
    ZF_LOGD("Empty slots:     [%lu --> %lu)", (long)info->empty.start, (long)info->empty.end);
    ZF_LOGD("sharedFrames:    [%lu --> %lu)", (long)info->sharedFrames.start, (long)info->sharedFrames.end);
    ZF_LOGD("userImageFrames: [%lu --> %lu)", (long)info->userImageFrames.start, (long)info->userImageFrames.end);
    ZF_LOGD("userImagePaging: [%lu --> %lu)", (long)info->userImagePaging.start, (long)info->userImagePaging.end);
    ZF_LOGD("untypeds:        [%lu --> %lu)", (long)info->untyped.start, (long)info->untyped.end);
    ZF_LOGD("Initial thread domain: %u\n", (int)info->initThreadDomain);
    ZF_LOGD("Initial thread cnode size: %u", info->initThreadCNodeSizeBits);
    ZF_LOGD("List of untypeds");
    ZF_LOGD("------------------");
    ZF_LOGD("Paddr    | Size   | Device");

    int sizes[CONFIG_WORD_SIZE] = {0};
    for (int i = 0; i < CONFIG_MAX_NUM_BOOTINFO_UNTYPED_CAPS && i < (info->untyped.end - info->untyped.start); i++) {
        int index = info->untypedList[i].sizeBits;
        assert(index < ARRAY_SIZE(sizes));
        sizes[index]++;
        ZF_LOGD("%p | %zu | %d", (void *)info->untypedList[i].paddr, (size_t)info->untypedList[i].sizeBits,
                (int)info->untypedList[i].isDevice);
    }

    ZF_LOGD("Untyped summary\n");
    for (int i = 0; i < ARRAY_SIZE(sizes); i++) {
        if (sizes[i] != 0) {
            ZF_LOGD("%d untypeds of size %d\n", sizes[i], i);
        }
    }
}

void debug_print_fault(seL4_MessageInfo_t tag, const char *thread_name)
{
    seL4_Fault_t fault = seL4_getFault(tag);
    switch (seL4_Fault_get_seL4_FaultType(fault)) {
    case seL4_Fault_VMFault:
        assert(seL4_MessageInfo_get_length(tag) == seL4_VMFault_Length);
        printf("%sPagefault from [%s]: %s %s at PC: %p vaddr: %p, FSR %p%s\n",
               COLOR_ERROR,
               thread_name,
               debug_is_read_fault() ? "read" : "write",
               seL4_Fault_VMFault_get_PrefetchFault(fault) ? "prefetch fault" : "fault",
               (void *)seL4_Fault_VMFault_get_IP(fault),
               (void *)seL4_Fault_VMFault_get_Addr(fault),
               (void *)seL4_Fault_VMFault_get_FSR(fault),
               COLOR_NORMAL);
        break;

    case seL4_Fault_UnknownSyscall:
        assert(seL4_MessageInfo_get_length(tag) == seL4_UnknownSyscall_Length);
        printf("%sBad syscall from [%s]: scno %"PRIuPTR" at PC: %p%s\n",
               COLOR_ERROR,
               thread_name,
               seL4_Fault_UnknownSyscall_get_Syscall(fault),
               (void *) seL4_Fault_UnknownSyscall_get_FaultIP(fault),
               COLOR_NORMAL
              );

        break;

    case seL4_Fault_UserException:
        assert(seL4_MessageInfo_get_length(tag) == seL4_UserException_Length);
        printf("%sInvalid instruction from [%s] at PC: %p%s\n",
               COLOR_ERROR,
               thread_name,
               (void *)seL4_Fault_UserException_get_FaultIP(fault),
               COLOR_NORMAL);
        break;

    case seL4_Fault_CapFault:
        printf("%sCap fault from [%s] in phase %s\nPC = %p\nCPtr = %p%s\n",
               COLOR_ERROR, thread_name,
               seL4_Fault_CapFault_get_InRecvPhase(fault) ? "receive" : "send",
               (void *) seL4_Fault_CapFault_get_IP(fault),
               (void *) seL4_Fault_CapFault_get_Addr(fault),
               COLOR_NORMAL);
        break;
    default:
        /* What? Why are we here? What just happened? */
        printf("Unknown fault from [%s]: %"PRIuPTR" (length = %"PRIuPTR")\n", thread_name,
                seL4_MessageInfo_get_label(tag), seL4_MessageInfo_get_length(tag));
        break;
    }
}

void debug_dump_registers(seL4_CPtr tcb)
{
    seL4_UserContext context;
    int error;
    const int num_regs = sizeof(context) / sizeof(seL4_Word);

    error = seL4_TCB_ReadRegisters(tcb, false, 0, num_regs, &context);
    if (error) {
        ZF_LOGE("Failed to read registers for tcb 0x%lx, error %d", (long) tcb, error);
        return;
    }

    printf("Register dump:\n");
    for (int i = 0; i < num_regs; i++) {
        printf("%s\t:0x%lx\n", register_names[i], (long) ((seL4_Word * )&context)[i]);
    }
}


