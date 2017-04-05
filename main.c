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
#include <time.h>

const char *algorithm;
struct disk *disk;
char *physmem;
int nframes;
int npages;
int framemap[1024] = {0};

int isFull(){
	int i = 0;
	for( i = 0; i < nframes; i++ ){
		if (framemap[i] == -1){
			return 0;
		}
	}
	return 1;
}

int nextOpen(){
	int j = 0;
	for( j = 0; j < nframes; j++){
		if (framemap[j] == -1){
			return j;
		}
	}
	return -1; //error: should not get here
}

int random_algo(int page){
	//int newFrame = rand() % nframes;
	int newFrame = (page + 1) % nframes;
	return newFrame;
}

void page_fault_handler( struct page_table *pt, int page )
{
	int frame, bits;
	//Check type of error
	page_table_get_entry( pt, page, &frame, &bits );

	//No permissions -- not in memory
	if(bits == 0){
		int myframe;

		//If frames all full, kick out a page
		if(isFull()){
			//Decide which frame to put page into
			if(!strcmp(algorithm,"rand")) {
				myframe = random_algo(page);

			} else if(!strcmp(algorithm,"fifo")) {
				myframe = 0; //implement later

			} else if(!strcmp(algorithm,"custom")) {
				myframe = 0; //implement later

			} else {
				printf("unknown algo\n");
				myframe = page%nframes; //DEFAULT for now
			}
			
			printf("EVICTED!\n");
			disk_write( disk, framemap[myframe], &physmem[myframe*PAGE_SIZE]);
			disk_read( disk, page, &physmem[myframe*PAGE_SIZE]);
			page_table_set_entry( pt, page, myframe, PROT_READ);	//Give rights to new page
			page_table_set_entry( pt, framemap[myframe], 0, 0);		//strip acces from old page
		}
		//Else frame is empty
		else{
			//pick next open frame
			myframe = nextOpen();
			page_table_set_entry(pt,page,myframe,PROT_READ);
			disk_read( disk, page,&physmem[myframe*PAGE_SIZE]);
		}
	}
	//Read only -- data already in memory
	else if(bits == 1){
		//Add Write permissions
		page_table_set_entry(pt,page,page%nframes,PROT_READ|PROT_WRITE);
	}
	
	//Update which page is in the frame in physical memory
	framemap[page%nframes] = page;

	page_table_get_entry( pt, page, &frame, &bits );
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

	//initilaize frame tracker
	int i = 0;
	for(i = 0; i < 1024; i++){
		framemap[i] = -1;
	}
	
	//seedrand
	srand(time(NULL));
	

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
