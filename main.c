#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

struct Pixel {
	unsigned int red, green, blue;
};

struct Image {
	unsigned int height, width, max_value;
	struct Pixel **matrix;
};

void freeImage(struct Image *image) {
	int i;
	for (i = 0; i < image->height; ++i) {
		free(image->matrix[i]);
	}
	free(image->matrix);
	free(image);
}

struct ThreadArgument
{
	struct Image *image, **result;
	unsigned int from, to;
};

int readInt(const unsigned char *c, int *cur_pos, const int max_len) {
	if (*cur_pos == max_len)
		return 0;

	bool skip_until_new_line = 0, seen_digit = 0;
	int result = 0;
	while (*cur_pos != max_len) {
		unsigned char ch = c[*cur_pos];

		if (ch == '\n' && !seen_digit) {
			++(*cur_pos);
			skip_until_new_line = 0;
		} else if (skip_until_new_line) {
			++(*cur_pos);
		} else if (ch == '#' && !seen_digit) {
			++(*cur_pos);
			skip_until_new_line = 1;
		} else if (ch >= '0' && ch <= '9') {
			seen_digit = 1;
			result = result * 10 + ch - '0';
			++(*cur_pos);
		} else if (seen_digit) {
			return result;
		} else if (ch == ' ' || ch == '\t')
			++(*cur_pos);
		else {
			return 0;
		}
	}

	return seen_digit ? result : 0;
}

struct Image *readImage(const char *filename) {
	int fd = -1;
	ssize_t bytes_read;

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		perror("Could not open file\n");
		exit(1);
	}

	struct stat fileStat;
	if (fstat(fd, &fileStat)) {
		perror("Error while reading file\n");
		exit(1);
	}

	char *signed_buff = (char*)malloc(fileStat.st_size + 1);

	if (!read(fd, signed_buff, fileStat.st_size + 1)) {
		perror("Error while reading file\n");
		free(signed_buff);
		exit(1);
	}

	unsigned char *buff = (unsigned char*) malloc(fileStat.st_size + 1);

	int i;
	for (i = 0; i < fileStat.st_size + 1; ++i)
		buff[i] = (unsigned char)signed_buff[i];

	if (fileStat.st_size < 4) {
		perror("Incorrect file format\n");
		free(signed_buff);
		free(buff);
		exit(1);
	}

	if (buff[0] != 'P' || buff[1] != '6' || (buff[2] != ' ' && buff[2] != '\n' && buff[2] != '\t')) {
		perror("Image format must be \"P6\"\n");
		free(signed_buff);
		free(buff);
		exit(1);
	}

	int it = 3;

	int width = readInt(buff, &it, fileStat.st_size + 1);
	if (width == 0) {
		perror("Image size is incorrect\n");
		free(signed_buff);
		free(buff);
		exit(1);
	}

	int height = readInt(buff, &it, fileStat.st_size + 1);
	if (height == 0) {
		perror("Image size is incorrect\n");
		free(signed_buff);
		free(buff);
		exit(1);
	}

	if (buff[it] != '\n' && buff[it] != '#' && buff[it] != ' ' && buff[it] != '\t') {
		perror("Error while reading image size\n");
		free(signed_buff);
		free(buff);
		exit(1);
	}

	if (buff[it] != '\n' && buff[it] != '#' && buff[it] != ' ' && buff[it] != '\t') {
		perror("Error while reading image size\n");
		free(signed_buff);
		free(buff);
		exit(1);
	}

	int max_value = readInt(buff, &it, fileStat.st_size + 1);
	if (max_value == 0 || max_value >= 65536) {
		perror("Maximum color value is incorrect\n");
		free(signed_buff);
		free(buff);
		exit(1);	
	}

	if (buff[it] != '\n' && buff[it] != ' ' && buff[it] != '\t') {
		perror("Error while reading image size\n");
		free(signed_buff);
		free(buff);
		exit(1);
	}

	++it;

	struct Image *image = (struct Image*)malloc(sizeof(struct Image));
	image->height = height;
	image->width = width;
	image->max_value = max_value;

	image->matrix = (struct Pixel**)malloc(height * sizeof(struct Pixel*));
	for (i = 0; i < height; ++i)
		image->matrix[i] = (struct Pixel*)malloc(width * sizeof(struct Pixel));

	int cnt = 0;
	while (it < fileStat.st_size + 1) {
		if (buff[it] > max_value) {
			perror("Image color element is more than maximim value\n");
			free(signed_buff);
			free(buff);
			freeImage(image);
			exit(1);
		}

		if (max_value < 256) {
			int pixel = cnt / 3;
			if (pixel / width == height)
				break;

			if (cnt % 3 == 0)
				image->matrix[pixel / width][pixel % width].red = buff[it];
			else if (cnt % 3 == 1)
				image->matrix[pixel / width][pixel % width].green = buff[it];
			else 
				image->matrix[pixel / width][pixel % width].blue = buff[it];
		}
		else {
			int pixel = cnt / 6;
			if (pixel / width == height)
				break;

			if (cnt % 6 < 2) {
				image->matrix[pixel / width][pixel % width].red *= 256;
				image->matrix[pixel / width][pixel % width].red += buff[it];
			}
			else if (cnt % 6 < 4) {
				image->matrix[pixel / width][pixel % width].green *= 256;
				image->matrix[pixel / width][pixel % width].green += buff[it];
			}
			else {
				image->matrix[pixel / width][pixel % width].blue *= 256;
				image->matrix[pixel / width][pixel % width].blue += buff[it];
			}
		}

		++it;
		++cnt;
	}

	close(fd);
	free(buff);
	free(signed_buff);
	return image;
}

void printInteger(int fd, int x) {
	if (x == 0)
		return;
	printInteger(fd, x / 10);
	char c[1];
	c[0] = x % 10 + '0';
	write(fd, c, sizeof(c));
	return;
}

void writeImage(const char* filename, struct Image *image) {
	if (!image)
		return;

	int fd = -1;

	fd = open(filename, O_WRONLY);
	if (fd == -1) {
		perror("Could not open file\n");
		freeImage(image);
		exit(1);
	}

	char format[2];
	format[0] = 'P', format[1] = '6';
	char cc[1];
	cc[0] = '\n';

	write(fd, format, sizeof(format));
	write(fd, cc, sizeof(cc));
	printInteger(fd, image->width);
	write(fd, cc, sizeof(cc));
	printInteger(fd, image->height);
	write(fd, cc, sizeof(cc));
	printInteger(fd, image->max_value);
	write(fd, cc, sizeof(cc));

	int i, j;
	for (i = 0; i < image->height; ++i) {
		for (j = 0; j < image->width; ++j) {
			if (image->max_value < 256) {
				char c[3];
				c[0] = image->matrix[i][j].red;
				c[1] = image->matrix[i][j].green;
				c[2] = image->matrix[i][j].blue;
				write(fd, c, sizeof(c));
			} else {
				char c[6];
				c[0] = image->matrix[i][j].red / 256;
				c[1] = image->matrix[i][j].red % 256;
				c[2] = image->matrix[i][j].green / 256;
				c[3] = image->matrix[i][j].green % 256;
				c[4] = image->matrix[i][j].blue / 256;
				c[5] = image->matrix[i][j].blue % 256;
				write(fd, c, sizeof(c));
			}
		}
	}

	close(fd);
}

struct Image* convertToWB(struct Image *image) {
	if (!image)
		return NULL;

	struct Image *result = (struct Image*)malloc(sizeof(struct Image));
	result->height = image->height;
	result->width = image->width;
	result->max_value = image->max_value;
	result->matrix = (struct Pixel**)malloc(result->height * sizeof(struct Pixel*));
	int i;
	for (i = 0; i < image->height; ++i)
		result->matrix[i] = (struct Pixel*)malloc(result->width * sizeof(struct Pixel));

	int j;
	for (i = 0; i < image->height; ++i) {
		for (j = 0; j < image->width; ++j) {
			int red = image->matrix[i][j].red;
			int green = image->matrix[i][j].green;
			int blue = image->matrix[i][j].blue;
			int gr = (299 * red + 587 * green + 114 * blue) / 1000;
			result->matrix[i][j].red = result->matrix[i][j].green = result->matrix[i][j].blue = gr;
		}
	}

	freeImage(image);
	return result;
}

void *applySobelOnThread(void *);

struct Image *applySobel(struct Image *image, int threads) {
	if (!image || !threads) 
		return NULL;

	image = convertToWB(image);
	struct Image *result = (struct Image*)malloc(sizeof(struct Image));
	result->height = image->height - 2;
	result->width = image->width - 2;
	result->max_value = image->max_value;
	result->matrix = (struct Pixel**)malloc(result->height * sizeof(struct Pixel*));
	int i;
	for (i = 0; i < image->height; ++i)
		result->matrix[i] = (struct Pixel*)malloc(result->width * sizeof(struct Pixel));

	unsigned int pixels = result->height * result->width;
	threads = threads <	pixels ? threads : pixels;

	struct ThreadArgument args[threads];
	pthread_t thr[threads];
	for (i = 0; i < threads; ++i) {
		args[i].from = pixels / threads * i;
		args[i].to = pixels / threads * (i + 1);
		args[i].image = image;
		args[i].result = &result;

	   if (pthread_create(&thr[i], NULL , applySobelOnThread, (void *)&args[i])) {
	   		perror("Could not create thread\n");
	   		freeImage(result);
	   		freeImage(image);
	   		exit(1);
	   }
	}

	for (i = 0; i < threads; ++i) {
		if (pthread_join(thr[i], NULL)) {
			perror("Error while running in thread\n");
			freeImage(image);
			freeImage(result);
			exit(1);
		}
	}

	int j;
	unsigned int max_value = 0;
	for (i = 0; i < result->height; ++i) {
		for (j = 0; j < result->width; ++j) {
			if (max_value < result->matrix[i][j].red)
				max_value = result->matrix[i][j].red;
		}
	}

	result->max_value = max_value;

	freeImage(image);
	return result;
}

void *applySobelOnThread(void *threadArg) {
	struct ThreadArgument *arg;
	arg = (struct ThreadArgument *)threadArg;

	if (!arg->image || arg->image->height <= 2 || arg->image->width <= 2) {
		pthread_exit(NULL);
	}

	int it;
	for (it = arg->from; it < arg->to; ++it) {
		int i = it / (*arg->result)->width;
		int j = it % (*arg->result)->width;
		if (i == (*arg->result)->height)
			break;

		int gx = -arg->image->matrix[i][j].red -
			2 * arg->image->matrix[i + 1][j].red - 
			arg->image->matrix[i + 2][j].red +
			arg->image->matrix[i][j + 2].red  + 
			2 * arg->image->matrix[i + 1][j + 2].red  + 
			arg->image->matrix[i + 2][j + 2].red;

		int gy = -arg->image->matrix[i][j].red - 
			2 * arg->image->matrix[i][j + 1].red - 
			arg->image->matrix[i][j + 2].red + 
			arg->image->matrix[i + 2][j].red + 
			2 * arg->image->matrix[i + 2][j + 1].red + 
			arg->image->matrix[i + 2][j + 2].red;
        int sum = sqrt(gx * gx + gy * gy);

        (*arg->result)->matrix[i][j].red = (*arg->result)->matrix[i][j].green = (*arg->result)->matrix[i][j].blue = sum;
	}

	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	if (argc != 4) {
		perror("Wrong arguments count\n");
		exit(1);
	}

	if (!atoi(argv[3])) {
		perror("Wrong number of threads\n");
		exit(1);
	}

	struct Image *image = readImage(argv[1]);
	clock_t  start = clock();
	image = applySobel(image, atoi(argv[3]));
	clock_t finish = clock();
	printf("Time spent to aaply Sobel operator using %d thread(s) is %f seconds\n", atoi(argv[3]), (double)(finish - start) / CLOCKS_PER_SEC);
	writeImage(argv[2], image);

	freeImage(image);
	return 0;
}