/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 *  with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

#include <dev_tree.h>

#define ROUNDUP(a, b) (((a) + ((b)-1)) & ~((b)-1))
#define ROUNDDOWN(a, b) ((a) & ~((b)-1))

#define PAGE_SIZE_DEF  2048
static int m_page_size = PAGE_SIZE_DEF;

off_t fdsize(int fd) {
    off_t off;

    off = lseek(fd, 0L, SEEK_END);
    lseek(fd, 0L, SEEK_SET);

    return off;
}

off_t fdpos(int fd) {
    return lseek(fd, 0L, SEEK_CUR);
}

/* Returns 0 if the device tree is valid. */
int dev_tree_validate(struct dt_table *table, unsigned int page_size, uint32_t *dt_hdr_size)
{
	int dt_entry_size;
	uint64_t hdr_size;

	/* Validate the device tree table header */
	if(table->magic != DEV_TREE_MAGIC) {
		fprintf(stderr, "Bad magic in device tree table \n");
		return -1;
	}

	if (table->version == DEV_TREE_VERSION_V1) {
		dt_entry_size = sizeof(struct dt_entry_v1);
	} else if (table->version == DEV_TREE_VERSION_V2) {
		dt_entry_size = sizeof(struct dt_entry_v2);
	} else if (table->version == DEV_TREE_VERSION_V3) {
		dt_entry_size = sizeof(struct dt_entry);
	} else {
		fprintf(stderr, "Unsupported version (%d) in DT table \n",
				table->version);
		return -1;
	}

	hdr_size = (uint64_t)table->num_entries * dt_entry_size + DEV_TREE_HEADER_SIZE;

	/* Roundup to page_size. */
	hdr_size = ROUNDUP(hdr_size, page_size);

	if (hdr_size > UINT_MAX)
		return -1;
	else
		*dt_hdr_size = hdr_size & UINT_MAX;

	return 0;
}

int dev_tree_generate(const char* directory, const char* directory_decomp, struct dt_table *table) {
	uint32_t i;
    int rc;
	unsigned char *table_ptr = NULL;
    struct dt_entry dt_entry_buf_1;
	struct dt_entry_v1 *dt_entry_v1 = NULL;
	struct dt_entry_v2 *dt_entry_v2 = NULL;
    struct dt_entry *cur_dt_entry = NULL;

    table_ptr = (unsigned char *)table + DEV_TREE_HEADER_SIZE;
	cur_dt_entry = &dt_entry_buf_1;

	fprintf(stdout, "DTB Total entry: %d, DTB version: %d\n", table->num_entries, table->version);
	for(i = 0; i < table->num_entries; i++) {
        switch(table->version) {
		case DEV_TREE_VERSION_V1:
			dt_entry_v1 = (struct dt_entry_v1 *)table_ptr;
			cur_dt_entry->platform_id = dt_entry_v1->platform_id;
			cur_dt_entry->variant_id = dt_entry_v1->variant_id;
			cur_dt_entry->soc_rev = dt_entry_v1->soc_rev;
			cur_dt_entry->board_hw_subtype = (dt_entry_v1->variant_id >> 0x18);
			/*cur_dt_entry->pmic_rev[0] = board_pmic_target(0);
			cur_dt_entry->pmic_rev[1] = board_pmic_target(1);
			cur_dt_entry->pmic_rev[2] = board_pmic_target(2);
			cur_dt_entry->pmic_rev[3] = board_pmic_target(3);*/
			cur_dt_entry->offset = dt_entry_v1->offset;
			cur_dt_entry->size = dt_entry_v1->size;
			table_ptr += sizeof(struct dt_entry_v1);
			break;
		case DEV_TREE_VERSION_V2:
			dt_entry_v2 = (struct dt_entry_v2*)table_ptr;
			cur_dt_entry->platform_id = dt_entry_v2->platform_id;
			cur_dt_entry->variant_id = dt_entry_v2->variant_id;
			cur_dt_entry->soc_rev = dt_entry_v2->soc_rev;
			/* For V2 version of DTBs we have platform version field as part
			 * of variant ID, in such case the subtype will be mentioned as 0x0
			 * As the qcom, board-id = <0xSSPMPmPH, 0x0>
			 * SS -- Subtype
			 * PM -- Platform major version
			 * Pm -- Platform minor version
			 * PH -- Platform hardware CDP/MTP
			 * In such case to make it compatible with LK algorithm move the subtype
			 * from variant_id to subtype field
			 */
			if (dt_entry_v2->board_hw_subtype == 0)
				cur_dt_entry->board_hw_subtype = (cur_dt_entry->variant_id >> 0x18);
			else
				cur_dt_entry->board_hw_subtype = dt_entry_v2->board_hw_subtype;
			/*cur_dt_entry->pmic_rev[0] = board_pmic_target(0);
			cur_dt_entry->pmic_rev[1] = board_pmic_target(1);
			cur_dt_entry->pmic_rev[2] = board_pmic_target(2);
			cur_dt_entry->pmic_rev[3] = board_pmic_target(3);*/
			cur_dt_entry->offset = dt_entry_v2->offset;
			cur_dt_entry->size = dt_entry_v2->size;
			table_ptr += sizeof(struct dt_entry_v2);
			break;
		case DEV_TREE_VERSION_V3:
			memcpy(cur_dt_entry, (struct dt_entry *)table_ptr,
				   sizeof(struct dt_entry));
			/* For V3 version of DTBs we have platform version field as part
			 * of variant ID, in such case the subtype will be mentioned as 0x0
			 * As the qcom, board-id = <0xSSPMPmPH, 0x0>
			 * SS -- Subtype
			 * PM -- Platform major version
			 * Pm -- Platform minor version
			 * PH -- Platform hardware CDP/MTP
			 * In such case to make it compatible with LK algorithm move the subtype
			 * from variant_id to subtype field
			 */
			if (cur_dt_entry->board_hw_subtype == 0)
				cur_dt_entry->board_hw_subtype = (cur_dt_entry->variant_id >> 0x18);

			table_ptr += sizeof(struct dt_entry);
			break;
		default:
			fprintf(stderr, "ERROR: Unsupported version (%d) in DT table \n",
					table->version);
			return -1;
		}

        fprintf(stdout, "entry: <%u %u 0x%x>\n",
					cur_dt_entry->platform_id,
					cur_dt_entry->variant_id,
					cur_dt_entry->soc_rev);

        // build filename
        char filename[PATH_MAX];
        rc = snprintf(filename, sizeof(filename), "%s/%u.dts", directory, i);
        if(rc<0 || (size_t)rc>=sizeof(filename)) {
            fprintf(stderr, "Can't build filename\n");
            return rc;
        }

        // open file
        FILE* f = fopen(filename, "w+");
        if(!f) {
            fprintf(stderr, "Can't open file %s\n", filename);
            return -1;
        }

        // build filename2
        char filename2[PATH_MAX];
        rc = snprintf(filename2, sizeof(filename2), "%s/%u.dts", directory_decomp, i);
        if(rc<0 || (size_t)rc>=sizeof(filename2)) {
            fprintf(stderr, "Can't build filename\n");
            return rc;
        }

        // open file
        FILE* forig = fopen(filename2, "r");
        if(!f) {
            fprintf(stderr, "Can't open file %s\n", filename);
            return -1;
        }

        fprintf(f, "/dts-v1/;\n\n/ {\n");

        // add simple root nodes
        char line[4096];
        while (fgets(line, sizeof(line), forig)) {
            size_t len = strlen(line);

            if(len<5) continue;
            if(line[len-1]=='\n') line[(len--)-1] = 0;
            if(line[0]!='\t') continue;
            if(isspace(line[1])) continue;
            if(line[len-1]!=';') continue;

            fprintf(f, "%s\n", line);
        }

        // memory
        fprintf(f, "\n\tmemory {\n");
        fprintf(f, "\t\t#address-cells = <0x1>;\n");
        fprintf(f, "\t\t#size-cells = <0x1>;\n");
        fprintf(f, "\t\tdevice_type = \"memory\";\n");
        fprintf(f, "\t\treg = <0x0 0x0 0x0 0x0>;\n");
        fprintf(f, "\t};\n\n");

        // chosen
        fprintf(f, "\tchosen {\n\t};\n");

        fprintf(f, "};\n");

        // close files
        if(fclose(f)) {
            fprintf(stderr, "Can't close file %s\n", filename);
            return -1;
        }
        if(fclose(forig)) {
            fprintf(stderr, "Can't close file %s\n", filename);
            return -1;
        }
	}

    return 0;
}

int main(int argc, char** argv) {
    int rc;
    off_t off;
    void* dtimg = NULL;
    ssize_t ssize;

    // validate arguments
    if(argc!=4) {
        fprintf(stderr, "Usage: %s dt.img outdir origdecompdir\n", argv[0]);
        return -EINVAL;
    }

    // open file
    const char* filename = argv[1];
    int fd = open(filename, O_RDONLY);
    if(fd<0) {
        fprintf(stderr, "Can't open file %s\n", filename);
        return fd;
    }

    // get filesize
    off = fdsize(fd);
    if(off<0) {
        fprintf(stderr, "Can't get size of file %s\n", filename);
        rc = (int)off;
        goto close_file;
    }

    // allocate buffer
    dtimg = malloc(off);
    if(!dtimg) {
        fprintf(stderr, "Can't allocate buffer of size %lu\n", off);
        rc = -ENOMEM;
        goto close_file;
    }

    // read file into memory
    ssize = read(fd, dtimg, off);
    if(ssize!=off) {
        fprintf(stderr, "Can't read file %s into buffer\n", filename);
        rc = (int)ssize;
        goto free_buffer;
    }

    // validate devicetree
    uint32_t dt_hdr_size;
    rc = dev_tree_validate(dtimg, m_page_size, &dt_hdr_size);
    if (rc) {
        fprintf(stderr, "Cannot validate Device Tree Table \n");
        goto free_buffer;
    }

    // generate devtree
    struct dt_table* table = dtimg;
    rc = dev_tree_generate(argv[2], argv[3], table);
    if (rc) {
        fprintf(stderr, "Cannot process table\n");
        goto free_buffer;
    }

free_buffer:
    free(dtimg);

close_file:
    // close file
    if(close(fd)) {
        fprintf(stderr, "Can't close file %s\n", filename);
        return rc;
    }

    if(rc) {
        fprintf(stderr, "ERROR: %s\n", strerror(-rc));
        return rc;
    }

    return rc;
}
