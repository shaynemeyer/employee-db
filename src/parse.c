#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "common.h"
#include "parse.h"

int create_db_header(struct dbheader_t **headerOut) {
	if (headerOut == NULL) {
		printf("ERROR: Invalid arguments supplied.\n");
		return STATUS_ERROR;
	}

	struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
	if (header == (void*)-1) {
		printf("Malloc failed to create db header\n");
		free(header);
		return STATUS_ERROR;
	}

	header->version = 0x1;
	header->count = 0;
	header->magic = HEADER_MAGIC;
	header->filesize = sizeof(struct dbheader_t);

	*headerOut = header;

	return STATUS_SUCCESS;
}

int validate_db_header(int fd, struct dbheader_t **headerOut) {
	if (fd < 0) {
		printf("Got a bad FD from the user\n");
		return STATUS_ERROR;
	}

	if (headerOut == NULL) {
		printf("ERROR: Invalid arguments supplied.\n");
		return STATUS_ERROR;
	}

	struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
	if (header == (void*)-1) {
		printf("Malloc failed create a db header\n");
		free(header);
		return STATUS_ERROR;
	}

	if (read(fd, header, sizeof(struct dbheader_t)) != sizeof(struct dbheader_t)) {
		perror("read");
		free(header);
		return STATUS_ERROR;
	}

	header->version = ntohs(header->version);
	header->count = ntohs(header->count);
	header->magic = ntohl(header->magic);
	header->filesize = ntohl(header->filesize);

	if (header->magic != HEADER_MAGIC) {
		printf("Impromper header magic\n");
		free(header);
		return STATUS_ERROR;
	}


	if (header->version != 1) {
		printf("Impromper header version\n");
		free(header);
		return STATUS_ERROR;
	}

	struct stat dbstat = {0};
	fstat(fd, &dbstat);
	if (header->filesize != dbstat.st_size) {
		printf("Corrupted database\n");
		free(header);
		return STATUS_ERROR;
	}

	*headerOut = header;

	return STATUS_SUCCESS;
}

int output_file(int fd, struct dbheader_t *dbhdr, struct employee_t *employees) {
	if (fd < 0) {
		printf("Got a bad FD from the user\n");
		return STATUS_ERROR;
	}

	int realcount = dbhdr->count;

	dbhdr->magic = htonl(dbhdr->magic);
	dbhdr->filesize = htonl(sizeof(struct dbheader_t) + (sizeof(struct employee_t) * realcount));
	dbhdr->count = htons(dbhdr->count);
	dbhdr->version = htons(dbhdr->version);

	lseek(fd, 0, SEEK_SET);

	write(fd, dbhdr, sizeof(struct dbheader_t));

	int i = 0;
	for (; i < realcount; i++) {
		employees[i].hours = htonl(employees[i].hours);
		write(fd, &employees[i], sizeof(struct employee_t));
	}

	return STATUS_SUCCESS;

}	

int read_employees(int fd, struct dbheader_t *dbhdr, struct employee_t **employeesOut) {
	if (fd < 0) {
		printf("Got a bad FD from the user\n");
		return STATUS_ERROR;
	}

	int count = dbhdr->count;

	struct employee_t *employees = calloc(count, sizeof(struct employee_t));
	if (employees == (void*)-1) {
		printf("Malloc failed\n");
		free(employees);
		return STATUS_ERROR;
	}

	read(fd, employees, count*sizeof(struct employee_t));

	int i = 0;
	for (; i < count; i++) {
		employees[i].hours = ntohl(employees[i].hours);
	}

	*employeesOut = employees;
	return STATUS_SUCCESS;

}

int add_employee(struct dbheader_t *dbhdr, struct employee_t **employees, char *addstring) {
	if (dbhdr == NULL || employees == NULL || addstring == NULL || addstring[0] == '\0') {
		printf("ERROR: Invalid arguments supplied.\n");
		return STATUS_ERROR;
	}

	// Check if there is space for a new employee
	if (dbhdr->count >= MAX_EMPLOYEES) {
			printf("Error: Employee database is full.\n");
			return STATUS_ERROR; // Handle full database
	}

	int count = dbhdr->count;

	// Tokenize the input string
	char *name = strtok(addstring, ",");
	char *addr = strtok(NULL, ",");
	char *hours = strtok(NULL, ",");

	// Check if all tokens are present
	if (name == NULL || addr == NULL || hours == NULL) {
			printf("Error: Missing fields in input string.\n");
			return STATUS_ERROR; // Handle missing fields
	}
	printf("%s %s %s\n", name, addr, hours);

	printf("Count: %d\n", count);

	int cursor_position = count - 1;

	// Safely copy name and address
	snprintf(employees[cursor_position].name, sizeof(employees[cursor_position].name), "%s", name);
	snprintf(employees[cursor_position].address, sizeof(employees[cursor_position].address), "%s", addr);

	// employees[cursor_position].hours = atoi(hours);

	// Convert hours with error checking
	char *endptr;
	employees[cursor_position].hours = strtol(hours, &endptr, 10);
	if (*endptr != '\0') {
			printf("Error: Invalid hours format.\n");
			return STATUS_ERROR; // Handle invalid hours
	}

	dbhdr->count++;
	
	return STATUS_SUCCESS;
}