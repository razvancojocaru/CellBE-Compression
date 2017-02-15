/*
 * Computer Architectures - Assignment 3
 * Author: Cojocaru Mihail-Razvan 333CA
 */
#include <spu_intrinsics.h>
#include <spu_mfcio.h>
#include "../cmp.h"

#define MAX_DMA 16000
#define BLOCK_DMA 15776 // Must be multiple of BLOCK_SIZE * BLOCK_SIZE

#define wait_tag(t) mfc_write_tag_mask(1<<t); mfc_read_tag_status_all();


/* Scalar compress function */
void compress_scalar(unsigned char* data, struct block *blocks, struct spu_data d)
{
	int row, col, bl_col, bl_index;
	unsigned char min, max;
	float aux, factor;
	struct block *curr_block;

	bl_index = 0;
	for (bl_col = 0; bl_col < d.width; bl_col += BLOCK_SIZE){
		//process 1 block from input image

		curr_block = &blocks[bl_index];
		//compute min and max
		min = max = data[bl_col];
		for (row = 0; row < BLOCK_SIZE; row++){
			for (col = bl_col; col < bl_col + BLOCK_SIZE; col++){
				if (data[row * d.width + col] < min)
					min = data[row * d.width + col];
				if (data[row * d.width + col] > max)
					max = data[row * d.width + col];
			}
		}
		curr_block->min = min;
		curr_block->max = max;
		
		//compute factor
		factor = (max - min) / (float) (NUM_COLORS_PALETTE - 1);
		
		//compute index matrix
		if (factor != 0) {
			//min != max
			for (row = 0; row < BLOCK_SIZE; row++){
				for (col = bl_col; col < bl_col + BLOCK_SIZE; col++){
					aux =  (data[row * d.width + col] - min) / factor;
					curr_block->index_matrix[row * BLOCK_SIZE + col - bl_col] = 
							(unsigned char) (aux + 0.5);
				}
			}
		} else {
			// min == max
			// all colors represented with min => index = 0
			memset(curr_block->index_matrix, 0, BLOCK_SIZE * BLOCK_SIZE);
		}
				
		bl_index++;
	}
}


/* Vectorial compress function */
void compress_vect(unsigned char* data, struct block *blocks, struct spu_data d)
{
	int row, col, bl_col, bl_index;
	unsigned char min, max;
	struct block *curr_block;
	int index, i, vdata_index;

	vector float vdata[4];
	vector float aux;
	vector float factor;
	vector float vmin;

	bl_index = 0;
	for (bl_col = 0; bl_col < d.width; bl_col += BLOCK_SIZE){
		//process 1 block from input image

		curr_block = &blocks[bl_index];
		//compute min and max
		min = max = data[bl_col];
		for (row = 0; row < BLOCK_SIZE; row++){
			for (col = bl_col; col < bl_col + BLOCK_SIZE; col++){
				if (data[row * d.width + col] < min)
					min = data[row * d.width + col];
				if (data[row * d.width + col] > max)
					max = data[row * d.width + col];
			}
		}
		curr_block->min = min;
		curr_block->max = max;
		
		//compute factor vectorial
		factor = spu_splats((float)((max - min) / (float) (NUM_COLORS_PALETTE - 1)));
		vmin = spu_splats((float)min);

		//compute index matrix
		if ( factor[0] != 0 ) {
			//min != max
			for (row = 0; row < BLOCK_SIZE; row++){

				//cast scalar data to vectorial (one block size length)
				for (i = 0; i < 4; i++) {
					vdata_index = row * d.width + bl_col + 4 * i;
					vdata[i][0] = (float)data[vdata_index];
					vdata[i][1] = (float)data[vdata_index + 1];
					vdata[i][2] = (float)data[vdata_index + 2];
					vdata[i][3] = (float)data[vdata_index + 3];
				}

				for (col = bl_col; col < bl_col + BLOCK_SIZE; col+=4){
					//compute aux vectorial (4 at a time)
					aux = (vdata[(col - bl_col) / 4] - vmin) / factor;

					//results must be written serial
					index = row * BLOCK_SIZE + col - bl_col;
					curr_block->index_matrix[index] = 
							(unsigned char) (aux[0] + 0.5);
					curr_block->index_matrix[index + 1] = 
							(unsigned char) (aux[1] + 0.5);
					curr_block->index_matrix[index + 2] = 
							(unsigned char) (aux[2] + 0.5);
					curr_block->index_matrix[index + 3] = 
							(unsigned char) (aux[3] + 0.5);
				}
			}
		} else {
			// min == max
			// all colors represented with min => index = 0
			memset(curr_block->index_matrix, 0, BLOCK_SIZE * BLOCK_SIZE);
		}		
		bl_index++;
	}
}


/* Scalar decompress function */
void decompress_scalar(unsigned char* data, struct block *blocks, struct spu_data d)
{
	int block_row_start, block_col_start, i, j, nr_blocks, k;
	float factor;
	struct block* curr_block;
	
	nr_blocks = d.width / BLOCK_SIZE;

	block_row_start = block_col_start = 0;
	
	for (i=0; i<nr_blocks; i++){
		k = block_row_start * d.width + block_col_start;
		curr_block = &blocks[i];
		factor = (curr_block->max - curr_block->min) / (float) (NUM_COLORS_PALETTE - 1);
		for (j=0; j<BLOCK_SIZE * BLOCK_SIZE; j++){
			data[k++] = (unsigned char) (curr_block->min + 
					curr_block->index_matrix[j] * factor + 0.5);
			if ((j + 1) % BLOCK_SIZE == 0){
				k -= BLOCK_SIZE; //back to the first column of the block
				k += d.width ; //go to the next line
			}
		}
		block_col_start += BLOCK_SIZE;
		if (block_col_start >= d.width){
			block_col_start = 0;
			block_row_start += BLOCK_SIZE;
		}
	}
}


/* Vectorial decompress function */
void decompress_vect(unsigned char* data, struct block *blocks, struct spu_data d)
{
	int block_row_start, block_col_start, i, j, nr_blocks, k, l;
	struct block* curr_block;

	vector float vdata[64], index_matrix[64];
	vector float factor;
	vector float vmin;
	vector float vconst;

	vconst = spu_splats((float)0.5);
	
	nr_blocks = d.width / BLOCK_SIZE;

	block_row_start = block_col_start = 0;
	
	for (i=0; i<nr_blocks; i++){
		k = block_row_start * d.width + block_col_start;
		curr_block = &blocks[i];
		factor = spu_splats((curr_block->max - curr_block->min) / (float) (NUM_COLORS_PALETTE - 1));
		vmin = spu_splats((float)curr_block->min);

		//cast one block of data to vectors
		for (l = 0; l < 64; l++) {
			index_matrix[l][0] = (float)curr_block->index_matrix[4 * l];
			index_matrix[l][1] = (float)curr_block->index_matrix[4 * l + 1];
			index_matrix[l][2] = (float)curr_block->index_matrix[4 * l + 2];
			index_matrix[l][3] = (float)curr_block->index_matrix[4 * l + 3];
		}

		for (j=0; j<BLOCK_SIZE * BLOCK_SIZE; j+=4){
			//compute vectorial data 4 at a time
			vdata[j/4] = (vmin + index_matrix[j/4] * factor + vconst);

			//after each assignment, we must check if there is a block ending
			data[k++] = (unsigned char)vdata[j/4][0];
			if ((j + 1) % BLOCK_SIZE == 0){
				k -= BLOCK_SIZE; //back to the first column of the block
				k += d.width ; //go to the next line
			}
			data[k++] = (unsigned char)vdata[j/4][1];
			if ((j + 2) % BLOCK_SIZE == 0){
				k -= BLOCK_SIZE; //back to the first column of the block
				k += d.width ; //go to the next line
			}
			data[k++] = (unsigned char)vdata[j/4][2];
			if ((j + 3) % BLOCK_SIZE == 0){
				k -= BLOCK_SIZE; //back to the first column of the block
				k += d.width ; //go to the next line
			}
			data[k++] = (unsigned char)vdata[j/4][3];
			if ((j + 4) % BLOCK_SIZE == 0){
				k -= BLOCK_SIZE; //back to the first column of the block
				k += d.width ; //go to the next line
			}
		}
		block_col_start += BLOCK_SIZE;
		if (block_col_start >= d.width){
			block_col_start = 0;
			block_row_start += BLOCK_SIZE;
		}
	}
}



int main(unsigned long long speid, unsigned long long argp, unsigned long long envp)
{

	struct spu_data d __attribute__ ((aligned(16)));
	int i, j;
	int bytes_left;
	uint32_t tag_id = mfc_tag_reserve();
	if (tag_id==MFC_TAG_INVALID){
		printf("SPU: ERROR can't allocate tag ID\n"); 
		return -1;
	}

	/* Get through DMA transfer the information structure sent from PPU */ 
	mfc_get((void*)&d, argp, sizeof(struct spu_data), tag_id, 0, 0);
	wait_tag(tag_id);

	/* Alloc alligned internal structures. All of them must be < 256KB */
	unsigned char data[d.width * BLOCK_SIZE] __attribute__ ((aligned(16)));
	struct block *blocks = (struct block*) _alloc(
								d.width / BLOCK_SIZE * sizeof(struct block));
	char *current_read, *current_write;

	current_read = d.addr_read;
	current_write = d.addr_write;

	/* Check if compress or decompress is requested */
	if (d.operation == 0) {
		/* Perform compress */
		for (i = 0; i < d.no_lines; i++) {
			/* Get one block line and compute */

			bytes_left = d.width * BLOCK_SIZE;
			j = 0;
			while (bytes_left > MAX_DMA) {
				mfc_get((void*)(data + j * MAX_DMA), 
						(unsigned int)(current_read), MAX_DMA, tag_id, 0, 0);
				wait_tag(tag_id);

				j++;
				bytes_left -= MAX_DMA;
				current_read += MAX_DMA;
			}

			/* If received data is not multiple of MAX_DMA */
			if (bytes_left > 0) {
				mfc_get((void*)(data + j * MAX_DMA), 
						(unsigned int)(current_read), bytes_left, tag_id, 0, 0);
				wait_tag(tag_id);

				current_read += bytes_left;
			}

			/* Compute with required method */
			if (d.mode_vect == 0)
				compress_scalar(data, blocks, d);
			else if (d.mode_vect == 1)
				compress_vect(data, blocks, d);

			/* Put results in general memory */
			bytes_left = d.width / BLOCK_SIZE * sizeof(struct block);
			j = 0;
			while (bytes_left > BLOCK_DMA) {
				mfc_put((void*)((char*)blocks + j * BLOCK_DMA), 
						(unsigned int)(current_write), BLOCK_DMA, tag_id, 0, 0);
				wait_tag(tag_id);

				j++;
				bytes_left -= BLOCK_DMA;
				current_write += BLOCK_DMA;
			}

			if (bytes_left > 0) {
				mfc_put((void*)((char*)blocks + j * BLOCK_DMA), 
						(unsigned int)(current_write), bytes_left, tag_id, 0, 0);
				wait_tag(tag_id);

				current_write += bytes_left;
			}
		}

	} else if (d.operation == 1) {
		/* Perform decompress */
		for (i = 0; i < d.no_lines; i++) {
			/* Get one block line and compute */

			bytes_left = d.width / BLOCK_SIZE * sizeof(struct block);
			j = 0;
			while (bytes_left > BLOCK_DMA) {
				mfc_get((void*)((char*)blocks + j * BLOCK_DMA), 
						(unsigned int)(current_read), BLOCK_DMA, tag_id, 0, 0);
				wait_tag(tag_id);

				j++;
				bytes_left -= BLOCK_DMA;
				current_read += BLOCK_DMA;
			}

			if (bytes_left > 0) {
				mfc_get((void*)((char*)blocks + j * BLOCK_DMA), 
						(unsigned int)(current_read), bytes_left, tag_id, 0, 0);
				wait_tag(tag_id);

				current_read += bytes_left;
			}


			/* Compute with required method */
			if (d.mode_vect == 0)
				decompress_scalar(data, blocks, d);
			else if (d.mode_vect == 1)
				decompress_vect(data, blocks, d);

			/* Put results in general memory */
			bytes_left = d.width * BLOCK_SIZE;
			j = 0;
			while (bytes_left > MAX_DMA) {
				mfc_put((void*)(data + j * MAX_DMA), 
						(unsigned int)(current_write), MAX_DMA, tag_id, 0, 0);
				wait_tag(tag_id);

				j++;
				bytes_left -= MAX_DMA;
				current_write += MAX_DMA;
			}

			if (bytes_left > 0) {
				mfc_put((void*)(data + j * MAX_DMA), 
						(unsigned int)(current_write), bytes_left, tag_id, 0, 0);
				wait_tag(tag_id);

				current_write += bytes_left;
			}
		}
	}

	/* Free the internal structures */
	free_align(blocks);

	/* Free tag_id */
	mfc_tag_release(tag_id);

	return 0;
}
