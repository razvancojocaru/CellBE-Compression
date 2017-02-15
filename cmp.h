/*
 * Computer Architectures - Assignment 3
 * Author: Cojocaru Mihail-Razvan 333CA
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <libmisc.h>

#define BUF_SIZE 		256
#define BLOCK_SIZE 		16
#define NUM_COLORS_PALETTE 16

//macro for easily getting how much time has passed between two events
#define GET_TIME_DELTA(t1, t2) ((t2).tv_sec - (t1).tv_sec + \
		((t2).tv_usec - (t1).tv_usec) / 1000000.0)


struct spu_data {
	// 16 byte alligned for DMA transfer
	void *addr_read;
	void *addr_write;
	int mode_vect;
	int mode_dma;
	unsigned char operation;
	int width;
	unsigned int size;
	int no_lines;
	int no_blocks;
} __attribute__ ((aligned(16)));

struct img {
	//regular image
	unsigned char* pixels;
	int width, height;
};

struct block {
	//min and max values for the block
	unsigned char min, max;
	//index matrix for the pixels in the block
	unsigned char index_matrix[BLOCK_SIZE * BLOCK_SIZE];
} __attribute__((aligned(16)));

struct c_img{
	//compressed image
	struct block* blocks;
	int width, height;
};

struct nibbles {
	unsigned first_nibble : 4;
	unsigned second_nibble: 4;
};

/* Utils */
void* _alloc(int size)
{
	void *res;

	res = malloc_align(size,4);
	if (!res){
		fprintf(stderr, "%s: Failed to allocated %d bytes\n", __func__,
				size);
		exit(0);
	}

	return res;
}

void _read_buffer(int fd, void* buf, int size)
{
	char *ptr;
	int left_to_read, bytes_read;

	ptr = (char*) buf;
	left_to_read = size;

	while (left_to_read > 0){
		bytes_read = read(fd, ptr, left_to_read);
		if (bytes_read <= 0){
			fprintf(stderr, "%s: Error reading buffer. "
					"fd=%d left_to_read=%d size=%d bytes_read=%d\n", 
					__func__, fd, left_to_read, size, bytes_read);
			exit(0);
		}
		left_to_read -= bytes_read;
		ptr += bytes_read;
	}
}

void _write_buffer(int fd, void* buf, int size)
{
	char *ptr;
	int left_to_write, bytes_written;

	ptr = (char*)buf;
	left_to_write = size;

	while (left_to_write > 0){
		bytes_written = write(fd, ptr, left_to_write);
		if (bytes_written <= 0){
			fprintf(stderr, "%s: Error writing buffer. "
					"fd=%d left_to_write=%d size=%d bytes_written=%d\n", 
					__func__, fd, left_to_write, size, bytes_written);
			exit(0);
		}
		left_to_write -= bytes_written;
		ptr += bytes_written;
	}
}

int _open_for_write(char* path)
{
	int fd;

	fd = open(path, O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if (fd < 0){
		fprintf(stderr, "%s: Error opening %s\n", __func__, path);
		exit(0);
	}
	return fd;
}

int _open_for_read(char* path)
{
	int fd;

	fd = open(path, O_RDONLY);
	if (fd < 0){
		fprintf(stderr, "%s: Error opening %s\n", __func__, path);
		exit(0);
	}
	return fd;
}

void read_line(int fd, char* path, char* buf, int buf_size)
{
	char c = 0;
	int i = 0;
	while (c != '\n'){
		if (read(fd, &c, 1) == 0){
			fprintf(stderr, "Error reading from %s\n", path);
			exit(0);
		}
		if (i == buf_size){
			fprintf(stderr, "Unexpected input in %s\n", path);
			exit(0);
		}
		buf[i++] = c;
	}
	buf[i] = '\0';
}


/* read_cmp */
void read_cmp(char* path, struct c_img* out_img)
{	
	int fd, nr_blocks, i, j = 0, k, file_size;
	char *big_buf, small_buf[BUF_SIZE];
	struct nibbles nb;
	
	fd = _open_for_read(path);
	
	//read width and height
	read_line(fd, path, small_buf, BUF_SIZE);
	out_img->width = atoi(small_buf);
	read_line(fd, path, small_buf, BUF_SIZE);
	out_img->height = atoi(small_buf);
	
	nr_blocks = out_img->width * out_img->height / (BLOCK_SIZE * BLOCK_SIZE);
	out_img->blocks = (struct block*) _alloc(nr_blocks * sizeof(struct block));
	file_size = nr_blocks * (2 + BLOCK_SIZE * BLOCK_SIZE / 2);
	
	big_buf = (char*) _alloc(file_size);

	_read_buffer(fd, big_buf, file_size);

	for (i=0; i<nr_blocks; i++){
		//read a and b
		out_img->blocks[i].min = big_buf[j++];
		out_img->blocks[i].max = big_buf[j++];
		//read index_matrix
		k = 0;
		while (k < BLOCK_SIZE * BLOCK_SIZE){
			nb = *((struct nibbles*) &big_buf[j++]);
			out_img->blocks[i].index_matrix[k++] = nb.first_nibble;
			out_img->blocks[i].index_matrix[k++] = nb.second_nibble;
		}
	}
	free_align(big_buf);
	close(fd);
}

void write_cmp(char* path, struct c_img* out_img)
{
	int i, nr_blocks, j, fd, k, file_size;
	char *buf, small_buf[BUF_SIZE];
	struct nibbles nb;
	
	fd = _open_for_write(path);
	
	//write width and height
	sprintf(small_buf, "%d\n%d\n", out_img->width, out_img->height);
	_write_buffer(fd, small_buf, strlen(small_buf));
	
	nr_blocks = out_img->width * out_img->height / (BLOCK_SIZE * BLOCK_SIZE);
	file_size = nr_blocks * (2 + BLOCK_SIZE * BLOCK_SIZE / 2);
	buf = _alloc(file_size);

	k = 0;
	for (i=0; i<nr_blocks; i++){
		//write min and max
		buf[k++] = out_img->blocks[i].min;
		buf[k++] = out_img->blocks[i].max;		
		//write index matrix
		j = 0;
		while (j < BLOCK_SIZE * BLOCK_SIZE){
			nb.first_nibble = out_img->blocks[i].index_matrix[j++];
			nb.second_nibble = out_img->blocks[i].index_matrix[j++];
			buf[k++] = *((char*) &nb);
		}
	}
	_write_buffer(fd, buf, file_size);

	free_align(buf);
	close(fd);
}

void free_cmp(struct c_img* image)
{
	free_align(image->blocks);
}


/* read_pgm */
void read_pgm(char* path, struct img* in_img)
{
	int fd;
	char buf[BUF_SIZE], *token;

	fd = _open_for_read(path);

	//read file type; expecting P5
	read_line(fd, path, buf, BUF_SIZE);
	if (strncmp(buf, "P5", 2)){
		fprintf(stderr, "Expected binary PGM (P5 type) when reading from %s\n", path);
		exit(0);
	}

	//read comment line
	read_line(fd, path, buf, BUF_SIZE);

	//read image width and height
	read_line(fd, path, buf, BUF_SIZE);
	token = strtok(buf, " ");
	if (token == NULL){
		fprintf(stderr, "Expected token when reading from %s\n", path);
		exit(0);
	}	
	in_img->width = atoi(token);
	token = strtok(NULL, " ");
	if (token == NULL){
		fprintf(stderr, "Expected token when reading from %s\n", path);
		exit(0);
	}
	in_img->height = atoi(token);
	if (in_img->width < 0 || in_img->height < 0){
		fprintf(stderr, "Invalid width or height when reading from %s\n", path);
		exit(0);
	}

	//read max value
	read_line(fd, path, buf, BUF_SIZE);

	in_img->pixels = (unsigned char*) _alloc(in_img->width * in_img->height *
			sizeof (unsigned char));
			
	_read_buffer(fd, in_img->pixels, in_img->width * in_img->height);

	close(fd);
}

void write_pgm(char* path, struct img* out_img)
{
	int fd; 
	char buf[BUF_SIZE];

	fd = _open_for_write(path);

	//write image type
	strcpy(buf, "P5\n");
	_write_buffer(fd, buf, strlen(buf));

	//write comment 
	strcpy(buf, "#Decompressed\n");
	_write_buffer(fd, buf, strlen(buf));

	//write image width and height
	sprintf(buf, "%d %d\n", out_img->width, out_img->height);
	_write_buffer(fd, buf, strlen(buf));

	//write max value
	strcpy(buf, "255\n");
	_write_buffer(fd, buf, strlen(buf));

	//write image pixels
	_write_buffer(fd, out_img->pixels, out_img->width * out_img->height);

	close(fd);
}

void free_pgm(struct img* image)
{
	free_align(image->pixels);
}

