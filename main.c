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
int *framemap;
int fifo_counter;
int *lru_counter;
int num_reads = 0;
int num_writes = 0;
int num_faults = 0;
int reset_counter = 0;

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

int random_algo(){
	int newFrame = rand() % nframes;
	return newFrame;
}

int fifo_algo(){
    if(fifo_counter == nframes-1){
        fifo_counter = 0;
    }
    else{
        fifo_counter++;
    }
    
    return fifo_counter;
}

int custom_algo(){
	//printf("run algo\n");
	//periodically reste all use counts
	if(reset_counter == 20){
		int x = 0;
		for(x=0; x < nframes; x++ ){
			lru_counter[x] = 0;
		}
		reset_counter = 0;
	}
	int lru = lru_counter[0];
	int frame = 0;
	int i;
	for( i = 0; i < nframes; i++){
		//printf("frame %d count: %d\n", i, lru_counter[i]);
		if(lru_counter[i] < lru){
			lru = lru_counter[i];
			frame = i;
		}
	}

	lru_counter[frame] = 1;
	//printf("frame: %d\n", frame);
	reset_counter++;
	return frame;
}

void page_fault_handler( struct page_table *pt, int page )
{
    int frame, bits;
	int myframe = -1;
	//Check type of error
	page_table_get_entry( pt, page, &frame, &bits );
    
	//No permissions -- not in memory
	if(bits == 0){
		num_faults++;
		//If frames all full, kick out a page
		if(isFull()){
			//Decide which frame to put page into
			if(!strcmp(algorithm,"rand")) {
				myframe = random_algo();

			} else if(!strcmp(algorithm,"fifo")) {
				myframe = fifo_algo(); //implement later

			} else if(!strcmp(algorithm,"custom")) {
				myframe = custom_algo();

			} else {
				printf("unknown algo\n");
				myframe = page%nframes; //DEFAULT for now
			}
			
			// printf("EVICTED!\n");
			disk_write( disk, framemap[myframe], &physmem[myframe*PAGE_SIZE]);
			disk_read( disk, page, &physmem[myframe*PAGE_SIZE]);
			page_table_set_entry( pt, page, myframe, PROT_READ);	//Give rights to new page
			page_table_set_entry( pt, framemap[myframe], 0, 0);		//strip acces from old page
			num_reads++;
			num_writes++;
			if(!strcmp(algorithm,"custom")) {
				//reset all write permissions
				int k = 0;
				for( k = 0; k < nframes; k++){
					//printf("resetting frame %d\n", k);
                    if(k == myframe){
                        page_table_set_entry( pt, framemap[myframe], 0, 0);		//strip acces from old page
                    }
                    else{
                        page_table_set_entry( pt, framemap[k], k, 1);
                    }
				}
			}
		}
		//Else frame is empty
		else{
			//pick next open frame
			myframe = nextOpen();
			page_table_set_entry(pt,page,myframe,PROT_READ);
			disk_read( disk, page,&physmem[myframe*PAGE_SIZE]);
			num_reads++;
		}
	}
	//Read only -- data already in memory
	else if(bits == 1){
		//Add Write permissions
		page_table_set_entry(pt,page,frame,PROT_READ|PROT_WRITE);
		if(!strcmp(algorithm,"custom")) {
			//increment "uses"
			lru_counter[frame]++;
		}
	}
	
	//Update which page is in the frame in physical memory
    if(myframe != -1){
        framemap[myframe] = page;
    }
    
	//page_table_get_entry( pt, page, &frame, &bits );
	// printf("page fault on page #%d\n",page);
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

	
	
	//seedrand
	srand(time(NULL));

	//malloc arrays
	framemap = (int *)malloc(sizeof(int)*nframes);
	lru_counter = (int *)malloc(sizeof(int)*nframes);

	//initilaize frame tracker and lru counter
	int i = 0;
	for(i = 0; i < nframes; i++){
		framemap[i] = -1;
		lru_counter[i] = 0;
	}

    
    // initialize FIFO counter
    fifo_counter = -1;
	

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

	printf("reads: %d\nwrites: %d\nfaults: %d\n", num_reads, num_writes, num_faults);

	//free arrays
	free(lru_counter);
	free(framemap);

	return 0;
}
