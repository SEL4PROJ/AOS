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
#include <utils/util.h>
#include <stdbool.h>
#include <sel4/sel4.h>
#include <elf/elf.h>
#include <string.h>
#include <assert.h>
#include <cspace/cspace.h>

#include "frame_table.h"
#include "ut.h"
#include "mapping.h"
#include "elfload.h"

/*
 * Convert ELF permissions into seL4 permissions.
 */
static inline seL4_CapRights_t get_sel4_rights_from_elf(unsigned long permissions)
{
    bool canRead = permissions & PF_R || permissions & PF_X;
    bool canWrite = permissions & PF_W;

    if (!canRead && !canWrite) {
        return seL4_AllRights;
    }

    return seL4_CapRights_new(false, false, canRead, canWrite);
}

/*
 * Load an elf segment into the given vspace.
 *
 * TODO: The current implementation maps the frames into the loader vspace AND the target vspace
 *       and leaves them there. Additionally, if the current implementation fails, it does not
 *       clean up after itself.
 *
 *       This is insufficient, as you will run out of resouces quickly, and will be completely fixed
 *       throughout the duration of the project, as different milestones are completed.
 *
 *       Be *very* careful when editing this code. Most students will experience at least one elf-loading
 *       bug.
 *
 * The content to load is either zeros or the content of the ELF
 * file itself, or both.
 * The split between file content and zeros is a follows.
 *
 * File content: [dst, dst + file_size)
 * Zeros:        [dst + file_size, dst + segment_size)
 *
 * Note: if file_size == segment_size, there is no zero-filled region.
 * Note: if file_size == 0, the whole segment is just zero filled.
 *
 * @param cspace        of the loader, to allocate slots with
 * @param loader        vspace of the loader
 * @param loadee        vspace to load the segment in to
 * @param src           pointer to the content to load
 * @param segment_size  size of segment to load
 * @param file_size     end of section that should be zero'd
 * @param dst           destination base virtual address to load
 * @param permissions   for the mappings in this segment
 * @return
 *
 */
static int load_segment_into_vspace(cspace_t *cspace, seL4_CPtr loadee, char *src, size_t segment_size,
                                    size_t file_size, uintptr_t dst, seL4_CapRights_t permissions)
{
    assert(file_size <= segment_size);

    /* We work a page at a time in the destination vspace. */
    unsigned int pos = 0;
    seL4_Error err = seL4_NoError;
    while (pos < segment_size) {
        uintptr_t loadee_vaddr = (ROUND_DOWN(dst, PAGE_SIZE_4K));

        /* create slot for the frame to load the data into */
        seL4_CPtr loadee_frame = cspace_alloc_slot(cspace);
        if (loadee_frame == seL4_CapNull) {
            ZF_LOGD("Failed to alloc slot");
            return -1;
        }

        /* allocate the untyped for the loadees address space */
        frame_ref_t frame = alloc_frame();
        if (frame == NULL_FRAME) {
            ZF_LOGD("Failed to alloc frame");
            return -1;
        }

        /* copy it */
        err = cspace_copy(cspace, loadee_frame, frame_table_cspace(), frame_page(frame), seL4_AllRights);
        if (err != seL4_NoError) {
            ZF_LOGD("Failed to untyped reypte");
            return -1;
        }

        /* map the frame into the loadee address space */
        err = map_frame(cspace, loadee_frame, loadee, loadee_vaddr, permissions,
                        seL4_ARM_Default_VMAttributes);

        /* A frame has already been mapped at this address. This occurs when segments overlap in
         * the same frame, which is permitted by the standard. That's fine as we
         * leave all the frames mapped in, and this one is already mapped. Give back
         * the ut we allocated and continue on to do the write.
         *
         * Note that while the standard permits segments to overlap, this should not occur if the segments
         * have different permissions - you should check this and return an error if this case is detected. */
        bool already_mapped = (err == seL4_DeleteFirst);

        if (already_mapped) {
            cspace_delete(cspace, loadee_frame);
            cspace_free_slot(cspace, loadee_frame);
            free_frame(frame);
        } else if (err != seL4_NoError) {
            ZF_LOGE("Failed to map into loadee at %p, error %u", (void *) loadee_vaddr, err);
            return -1;
        }

        /* finally copy the data */
        unsigned char *loader_data = frame_data(frame);

        /* Write any zeroes at the start of the block. */
        size_t leading_zeroes = dst % PAGE_SIZE_4K;
        memset(loader_data, 0, leading_zeroes);
        loader_data += leading_zeroes;

        /* Copy the data from the source. */
        size_t segment_bytes = PAGE_SIZE_4K - leading_zeroes;
        size_t file_bytes = MIN(segment_bytes, file_size - pos);
        if (pos < file_size) {
            memcpy(loader_data, src, file_bytes);
        } else {
            memset(loader_data, 0, file_bytes);
        }
        loader_data += file_bytes;

        /* Fill in the end of the frame with zereos */
        size_t trailing_zeroes = PAGE_SIZE_4K - (leading_zeroes + file_bytes);
        memset(loader_data, 0, trailing_zeroes);

        /* Flush the frame contents from loader caches out to memory. */
        flush_frame(frame);

        /* Invalidate the caches in the loadee forcing data to be loaded
         * from memory. */
        if (seL4_CapRights_get_capAllowWrite(permissions)) {
            seL4_ARM_Page_Invalidate_Data(loadee_frame, 0, PAGE_SIZE_4K);
        }
        seL4_ARM_Page_Unify_Instruction(loadee_frame, 0, PAGE_SIZE_4K);

        pos += segment_bytes;
        dst += segment_bytes;
        src += segment_bytes;
    }
    return 0;
}

int elf_load(cspace_t *cspace, seL4_CPtr loadee_vspace, elf_t *elf_file)
{

    int num_headers = elf_getNumProgramHeaders(elf_file);
    for (int i = 0; i < num_headers; i++) {

        /* Skip non-loadable segments (such as debugging data). */
        if (elf_getProgramHeaderType(elf_file, i) != PT_LOAD) {
            continue;
        }

        /* Fetch information about this segment. */
        char *source_addr = elf_file->elfFile + elf_getProgramHeaderOffset(elf_file, i);
        size_t file_size = elf_getProgramHeaderFileSize(elf_file, i);
        size_t segment_size = elf_getProgramHeaderMemorySize(elf_file, i);
        uintptr_t vaddr = elf_getProgramHeaderVaddr(elf_file, i);
        seL4_Word flags = elf_getProgramHeaderFlags(elf_file, i);

        /* Copy it across into the vspace. */
        ZF_LOGD(" * Loading segment %p-->%p\n", (void *) vaddr, (void *)(vaddr + segment_size));
        int err = load_segment_into_vspace(cspace, loadee_vspace, source_addr, segment_size, file_size, vaddr,
                                           get_sel4_rights_from_elf(flags));
        if (err) {
            ZF_LOGE("Elf loading failed!");
            return -1;
        }
    }

    return 0;
}
