#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

struct Pixel {
	unsigned int red, green, blue;
};

struct Image {
	unsigned int height, width, max_value;
	struct Pixel **matrix;
};

int readInt(const unsigned char *c, int *cur_pos, const int max_len) {
	if (*cur_pos == max_len)
		return 0;

	bool skip_until_new_line = 0, seen_digit = 0;
	int result = 0;
	while (*cur_pos != max_len) {
		unsigned char ch = c[*cur_pos];

		if (ch == '\n' && !seen_digit) {
			++*cur_pos;
			skip_until_new_line = 0;
		} else if (skip_until_new_line) {
			++*cur_pos;
		} else if (ch == '#' && !seen_digit) {
			++*cur_pos;
			skip_until_new_line = 1;
		} else if (ch >= '0' && ch <= '9') {
			seen_digit = 1;
			result = result * 10 + ch - '0';
			++*cur_pos;
		} else if (seen_digit) {
			printf("%d\n", result);
			return result;
		} else if (ch == ' ' || ch == '\t')
			++*cur_pos;
		else {
			printf("kek %d %c %d %d\n", *cur_pos, ch, skip_until_new_line, seen_digit);
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
		exit(1);
	}

	unsigned char *buff = (unsigned char*) malloc(fileStat.st_size + 1);
	printf("Bytes read=%d\n", fileStat.st_size);

	int i;
	for (i = 0; i < fileStat.st_size + 1; ++i)
		buff[i] = (unsigned char)signed_buff[i];

	if (fileStat.st_size < 4) {
		perror("Incorrect file format\n");
		exit(1);
	}

	if (buff[0] != 'P' || buff[1] != '6' || (buff[2] != ' ' && buff[2] != '\n' && buff[2] != '\t')) {
		perror("Image format must be \"P6\"\n");
		exit(1);
	}

	int it = 3;
	int height = readInt(buff, &it, fileStat.st_size + 1);
	printf("kek\n");
	if (height == 0) {
		printf("%d\n", it);
		perror("Image size is incorrect\n");
		exit(1);
	}

	if (buff[it] != '\n' && buff[it] != '#' && buff[it] != ' ' && buff[it] != '\t') {
		perror("Error while reading image size\n");
		exit(1);
	}

	int width = readInt(buff, &it, fileStat.st_size + 1);
	if (width == 0) {
		perror("Image size is incorrect\n");
		exit(1);
	}

	if (buff[it] != '\n' && buff[it] != '#' && buff[it] != ' ' && buff[it] != '\t') {
		perror("Error while reading image size\n");
		exit(1);
	}

	int max_value = readInt(buff, &it, fileStat.st_size + 1);
	if (max_value == 0 || max_value >= 65536) {
		perror("Maximum color value is incorrect\n");
		exit(1);	
	}

	if (buff[it] != '\n' && buff[it] != ' ' && buff[it] != '\t') {
		perror("Error while reading image size\n");
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

			if (cnt % 6 < 2)
				image->matrix[pixel / width][pixel % width].red = buff[it];
			else if (cnt % 6 < 4) 
				image->matrix[pixel / width][pixel % width].green = buff[it];
			else 
				image->matrix[pixel / width][pixel % width].blue = buff[it];
		}

		++it;
		++cnt;
	}

	close(fd);
	free(buff);
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
		exit(1);
	}

	char format[2];
	format[0] = 'P', format[1] = '6';
	char cc[1];
	cc[0] = '\n';

	write(fd, format, sizeof(format));
	write(fd, cc, sizeof(cc));
	printInteger(fd, image->height);
	write(fd, cc, sizeof(cc));
	printInteger(fd, image->width);
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
			int grey = (299 * red + 587 * green + 114 * blue) / 1000;
			result->matrix[i][j].red = result->matrix[i][j].green = result->matrix[i][j].blue = grey;
		}
	}

	return result;
}



struct Image* applySobel(struct Image *image) {
	if (!image || image->height <= 2 || image->width <= 2)
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

	int j;
	for (i = 1; i < image->height - 1; ++i) {
		for (j = 1; j < image->width - 1; ++j) {
			int gx = image->matrix[i - 1][j - 1].red + 
				2 * image->matrix[i][j - 1].red + 
				image->matrix[i + 1][j - 1].red -
                image->matrix[i - 1][j + 1].red -
                2 * image->matrix[i][j + 1].red -
                image->matrix[i + 1][j + 1].red;

            int gy = image->matrix[i - 1][j - 1].red + 
				2 * image->matrix[i - 1][j].red + 
				image->matrix[i - 1][j + 1].red -
                image->matrix[i + 1][j - 1].red -
                2 * image->matrix[i + 1][j].red -
                image->matrix[i + 1][j + 1].red;

            int sum = abs(gx) + abs(gy);

            sum = sum < 0 ? 0 : sum;
            sum = sum > image->max_value ? image->max_value : sum;

            result->matrix[i - 1][j - 1].red = result->matrix[i - 1][j - 1].green = result->matrix[i - 1][j - 1].blue = sum;
		}
	}

	return result;
}

int main(int argc, char *argv[]) {
	struct Image *image = readImage(argv[1]);
	image = applySobel(image);
	writeImage(argv[2], image);
	return 0;
}