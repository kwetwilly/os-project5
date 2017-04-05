/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/
//Thomas Franceschi
//Kyle Williams

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

const char *algorithm;
struct disk *disk;
char *physmem;
int nframes;
int npages;
int freeFrames[1024] = {0};

void page_fault_handler( struct page_table *pt, int page )
{
	int frame, bits;
	//Check type of error
	page_table_get_entry( pt, page, &frame, &bits );

	//No permissions -- not in memory
	if(bits == 0){
		//set just read
		int myframe = page%nframes;
		int newFrame, newBits;
		page_table_get_entry( pt, page, &newFrame, &newBits );
		
		//check if destination frame contains different page
		if(freeFrames[myframe] != page && newBits != 0){
			disk_write( disk, freeFrames[myframe], &physmem[page*PAGE_SIZE]);
			disk_read( disk, page, &physmem[page*PAGE_SIZE]);
			page_table_set_entry( pt, page, myframe, PROT_READ);	//Give rights to new page
			page_table_set_entry( pt, freeFrames[myframe], 0, 0);	//strip acces from old page
		}

		else{
			page_table_set_entry(pt,page,myframe,PROT_READ);
			disk_read( disk, page,&physmem[myframe*PAGE_SIZE]);
		}
		freeFrames[page%nframes] = page; //mark used
	}
	//Read only
	else if(bits == 1){
		//set read + write
		page_table_set_entry(pt,page,page%nframes,PROT_READ|PROT_WRITE);
		freeFrames[page%nframes] = page; //mark used
	}
	
	page_table_get_entry( pt, page, &frame, &bits );
	//page_table_print( pt );
	printf("page fault on page #%d\n",page);
}

int main( int argc, char *argv[] )
{
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|lru|custom> <sort|scan|focus>\n");
		return 1;
	}

	npages = atoi(argv[1]);
	nframes = atoi(argv[2]);
	algorithm = argv[3]; //algorithm to use
	const char *program = argv[4];

	//Check is nframes>npages
	if(nframes>npages) nframes = npages;

	//set delimeter for free frames
	freeFrames[nframes] = -1;
	

	disk = disk_open("myvirtualdisk",npages);
	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}


	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	char *virtmem = page_table_get_virtmem(pt);

	physmem = page_table_get_physmem(pt);

	if(!strcmp(program,"sort")) {
		sort_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"scan")) {
		scan_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"focus")) {
		focus_program(virtmem,npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[3]);
		return 1;
	}

	page_table_delete(pt);
	disk_close(disk);

	return 0;
}
