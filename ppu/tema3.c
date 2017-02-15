#include <sys/time.h>
#include <math.h>
#include <libspe2.h>
#include <pthread.h>

#include "../cmp.h"

extern spe_program_handle_t tema3_spu;
#define PRINT_GRAPHS 0


/* Thread function used to spawn SPU context */
void *ppu_pthread_function(void *thread_arg) {

	spe_context_ptr_t ctx;
	struct spu_data *arg = (struct spu_data *) thread_arg;

	/* Create SPE context */
	if ((ctx = spe_context_create (0, NULL)) == NULL) {
		printf("Failed creating context");
		exit(1);
	}

	/* Load SPE program into context */
	if (spe_program_load (ctx, &tema3_spu)) {
		printf("Failed loading program");
		exit(1);
	}

	/* Run SPE context */
	unsigned int entry = SPE_DEFAULT_ENTRY;
	/* Transfer through argument the adress for initial DMA transfer */
	if (spe_context_run(ctx, &entry, 0, (void*)arg, NULL, NULL) < 0) {  
		printf("Failed running context");
		exit(1);
	}

	/* Destroy context */
	if (spe_context_destroy (ctx) != 0) {
		printf("Failed destroying context");
		exit(1);
	}

	pthread_exit(NULL);
}


/* Parallel compress function. Runs on given number of SPUs */
void compress_parallel(struct img* image, struct c_img* c_image,
						int mode_vect, int mode_dma, int num_spus)
{
	pthread_t threads[num_spus];
	unsigned int image_size;
	struct spu_data data[num_spus] __attribute__ ((aligned(16)));
	int i, nr_blocks, height_blocks, block_lines_spu, block_lines_first_spu;
	int size_spu, size_first_spu, blocks_spu, blocks_first_spu;

	c_image->width = image->width;
	c_image->height = image->height;

	image_size = image->width * image->height;

	/* Data is assigned to SPUs grouped in block lines.
	 * One block line = chunk of image of size BLOCK_SIZE * image->width
	 */
	height_blocks = image->height / BLOCK_SIZE;
	block_lines_spu = (int)ceil(((double)height_blocks) / num_spus);
	block_lines_first_spu = height_blocks - ((num_spus - 1) * block_lines_spu);

	/* Number of bytes and blocks assigned to each SPU */
	size_spu = block_lines_spu * image->width * BLOCK_SIZE;
	blocks_spu = size_spu / (BLOCK_SIZE * BLOCK_SIZE);
	
	/* First SPU can receive less data */
	size_first_spu = block_lines_first_spu * image->width * BLOCK_SIZE;
	blocks_first_spu = size_first_spu / (BLOCK_SIZE * BLOCK_SIZE);

	nr_blocks = image_size / (BLOCK_SIZE * BLOCK_SIZE);
	c_image->blocks = (struct block*) _alloc(nr_blocks * sizeof(struct block));

	/* First SPU will receive a little less data if image size not multiple
	 * of block size.
	 */
	data[0].addr_read = image->pixels;
	data[0].addr_write = c_image->blocks;
	data[0].mode_vect = mode_vect;
	data[0].mode_dma = mode_dma;
	data[0].operation = 0;
	data[0].size = size_first_spu;
	data[0].width = image->width;
	data[0].no_lines = block_lines_first_spu;
	data[0].no_blocks = blocks_first_spu;


	for (i = 1; i < num_spus; i++) {
		data[i].addr_read = data[i-1].addr_read + data[i-1].size;
		data[i].addr_write = data[i-1].addr_write + 
							data[i-1].no_blocks * sizeof(struct block);
		data[i].mode_vect = mode_vect;
		data[i].mode_dma = mode_dma;
		data[i].operation = 0;
		data[i].size = size_spu;
		data[i].width = image->width;
		data[i].no_lines = block_lines_spu;
		data[i].no_blocks = blocks_spu;
	}

	/* Create SPU threads */
	for (i = 0; i < num_spus; i++) {

		if (pthread_create (&threads[i], NULL, &ppu_pthread_function, &data[i])) {
			printf("Failed creating thread");
			exit(1);
		}
	}

	/* Waiit for SPU threads to finish */
	for (i = 0; i < num_spus; i++) {

		if (pthread_join (threads[i], NULL)) {
			printf("Failed pthread_join");
			exit(1);
		}
	}

}

/* Parallel decompress function. Runs on given number of SPUs */
void decompress_parallel(struct img* image, struct c_img* c_image,
						int mode_vect, int mode_dma, int num_spus)
{
	pthread_t threads[num_spus];
	unsigned int image_size;
	struct spu_data data[num_spus] __attribute__ ((aligned(16)));
	int i, nr_blocks, height_blocks, block_lines_spu, block_lines_first_spu;
	int size_spu, size_first_spu, blocks_spu, blocks_first_spu;

	/* Set image width and height. Alloc pixels array (alligned) */
	image_size = c_image->width * c_image->height;
	image->width = c_image->width;
	image->height = c_image->height;
	image->pixels = _alloc(image_size * sizeof(unsigned char));

	height_blocks = image->height / BLOCK_SIZE;
	block_lines_spu = (int)ceil(((double)height_blocks) / num_spus);
	block_lines_first_spu = height_blocks - ((num_spus - 1) * block_lines_spu);

	/* Number of bytes and blocks assigned to each SPU */
	size_spu = block_lines_spu * image->width * BLOCK_SIZE;
	blocks_spu = size_spu / (BLOCK_SIZE * BLOCK_SIZE);
	
	size_first_spu = block_lines_first_spu * image->width * BLOCK_SIZE;
	blocks_first_spu = size_first_spu / (BLOCK_SIZE * BLOCK_SIZE);

	nr_blocks = image_size / (BLOCK_SIZE * BLOCK_SIZE);

	/* First SPU will receive a little more data if image size not multiple
	of block size */
	data[0].addr_read = c_image->blocks;
	data[0].addr_write = image->pixels;
	data[0].mode_vect = mode_vect;
	data[0].mode_dma = mode_dma;
	data[0].operation = 1;
	data[0].size = size_first_spu;
	data[0].width = image->width;
	data[0].no_lines = block_lines_first_spu;
	data[0].no_blocks = blocks_first_spu;


	for (i = 1; i < num_spus; i++) {
		data[i].addr_read = data[i-1].addr_read + 
							data[i-1].no_blocks * sizeof(struct block);
		data[i].addr_write = data[i-1].addr_write + data[i-1].size;
		data[i].mode_vect = mode_vect;
		data[i].mode_dma = mode_dma;
		data[i].operation = 1;
		data[i].size = size_spu;
		data[i].width = image->width;
		data[i].no_lines = block_lines_spu;
		data[i].no_blocks = blocks_spu;

	}

	/* Create SPU threads */
	for (i = 0; i < num_spus; i++) {

		if (pthread_create (&threads[i], NULL, &ppu_pthread_function, &data[i])) {
			printf("Failed creating thread");
			exit(1);
		}
	}

	/* Waiit for SPU threads to finish */
	for (i = 0; i < num_spus; i++) {

		if (pthread_join (threads[i], NULL)) {
			printf("Failed pthread_join");
			exit(1);
		}
	}

}


int main(int argc, char** argv)
{
	struct img image;
	struct img image2;
	struct c_img c_image;
	struct timeval t1, t2, t3, t4;
	double total_time = 0, scale_time = 0;

	int mode_vect, mode_dma, num_spus;

	/* Check params */
	if (argc != 7){
		printf("Usage: %s mode_vect mode_dma num_spus file_in file_out_cmp file_out_pgm\n", argv[0]);
		return -1;
	}

	/* Make params a little more human readable */
	mode_vect = atoi(argv[1]);
	mode_dma = atoi(argv[2]);
	num_spus = atoi(argv[3]);

	if ((mode_vect < 0) || (mode_vect > 2)) {
		printf("mode_vect must be 0, 1 or 2\n");
		return -1;
	}

	if ((mode_dma < 0) || (mode_dma > 1)) {
		printf("mode_dma must be 0 or 1\n");
		return -1;
	}

	if ((num_spus != 1) && (num_spus != 2) && (num_spus != 4) && (num_spus != 8)) {
		printf("num_spus must be 1, 2, 4 or 8\n");
		return -1;
	}


	gettimeofday(&t3, NULL);	

	read_pgm(argv[4], &image);	

	gettimeofday(&t1, NULL);
	compress_parallel(&image, &c_image, mode_vect, mode_dma, num_spus);
	decompress_parallel(&image2, &c_image, mode_vect, mode_dma, num_spus);
	gettimeofday(&t2, NULL);	

	write_cmp(argv[5], &c_image);
	write_pgm(argv[6], &image2);

	free_cmp(&c_image);
	free_pgm(&image);
	free_pgm(&image2);

	gettimeofday(&t4, NULL);

	total_time += GET_TIME_DELTA(t3, t4);
	scale_time += GET_TIME_DELTA(t1, t2);

	if (PRINT_GRAPHS == 0) {
		printf("Compress / Decompress time: %lf\n", scale_time);
		printf("Total time: %lf\n", total_time);
	} else if (PRINT_GRAPHS == 1) {
		printf("%d %lf\n", num_spus, scale_time);
	} else if (PRINT_GRAPHS == 2) {
		printf("%d %lf\n", num_spus, total_time);
	}
	
	return 0;
}
