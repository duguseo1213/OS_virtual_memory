/**********************************************************************
 * Copyright (c) 2020-2022
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "types.h"
#include "list_head.h"
#include "vm.h"

#define NONE 100000

/**
 * Ready queue of the system
 */
extern struct list_head processes;

/**
 * Currently running process
 */
extern struct process *current;

/**
 * Page Table Base Register that MMU will walk through for address translation
 */
extern struct pagetable *ptbr;

/**
 * TLB of the system.
 */
extern struct tlb_entry tlb[1UL << (PTES_PER_PAGE_SHIFT * 2)];


/**
 * The number of mappings for each page frame. Can be used to determine how
 * many processes are using the page frames.
 */
extern unsigned int mapcounts[];


/**
 * lookup_tlb(@vpn, @pfn)
 *
 * DESCRIPTION
 *   Translate @vpn of the current process through TLB. DO NOT make your own
 *   data structure for TLB, but use the defined @tlb data structure
 *   to translate. If the requested VPN exists in the TLB, return true
 *   with @pfn is set to its PFN. Otherwise, return false.
 *   The framework calls this function when needed, so do not call
 *   this function manually.
 *
 * RETURN
 *   Return true if the translation is cached in the TLB.
 *   Return false otherwise
 */
bool lookup_tlb(unsigned int vpn, unsigned int *pfn)
{
	struct tlb_entry *t = tlb;
	for(int i=0; i<256; i++)
	{
		t=tlb+i;
		if(t->vpn==vpn && t->valid==true)
		{
			*pfn=t->pfn;
			
			return true;
		}
	}
	return false;
}


/**
 * insert_tlb(@vpn, @pfn)
 *
 * DESCRIPTION
 *   Insert the mapping from @vpn to @pfn into the TLB. The framework will call
 *   this function when required, so no need to call this function manually.
 *
 */
void insert_tlb(unsigned int vpn, unsigned int pfn)
{
	struct tlb_entry *t = tlb;
	for(int i=0; i<256; i++)
	{
		t=tlb+i;
		if(t->valid==false)
		{
			t->valid=true;
			t->pfn=pfn;
			t->vpn=vpn;
			printf("afdfa %u\n",t->vpn=vpn);
			return;
		}
	}
}


/**
 * alloc_page(@vpn, @rw)
 *
 * DESCRIPTION
 *   Allocate a page frame that is not allocated to any process, and map it
 *   to @vpn. When the system has multiple free pages, this function should
 *   allocate the page frame with the **smallest pfn**.
 *   You may construct the page table of the @current process. When the page
 *   is allocated with RW_WRITE flag, the page may be later accessed for writes.
 *   However, the pages populated with RW_READ only should not be accessed with
 *   RW_WRITE accesses.
 *
 * RETURN
 *   Return allocated page frame number.
 *   Return -1 if all page frames are allocated.
 */
unsigned int alloc_page(unsigned int vpn, unsigned int rw)
{
	int pd_index = vpn / NR_PTES_PER_PAGE;
	int pte_index = vpn % NR_PTES_PER_PAGE;
	struct pagetable *pt = ptbr;
	for(int i=0; i<128; i++)
	{
		if(mapcounts[i]==0)
		{
			mapcounts[i]++;
			struct pte *temp=malloc(1000);
			temp->pfn=i;
			temp->valid=true;
			temp->private=NONE;
			if(rw==3) temp->writable=true;
			struct pte_directory *pd;
			pd = pt->outer_ptes[pd_index];
			if(!pd)
			{
				printf("1\n");
				pd=malloc(1000);
				pd->ptes[pte_index]=*temp;
				ptbr->outer_ptes[pd_index]=pd;
			}
			else
			{
				printf("2\n");
				
				pd->ptes[pte_index]=*temp;
				ptbr->outer_ptes[pd_index]=pd;
			}	
			
			/*
			struct pagetable *pt = ptbr;
			if(!pt)
			{
				printf("1\n");
				pt=malloc(10000);
				//a.*outer_ptes[pd_index]=pd;
				pt->outer_ptes[pd_index]=pd;
				current->pagetable=*pt;
				ptbr=pt;
			}
			else
			{
				printf("2\n");
				pt->outer_ptes[pd_index]=pd;
			}*/

			return i;
		}
		else
		{
			continue;
		}
	}
	return -1;	
}


/**
 * free_page(@vpn)
 *
 * DESCRIPTION
 *   Deallocate the page from the current processor. Make sure that the fields
 *   for the corresponding PTE (valid, writable, pfn) is set @false or 0.
 *   Also, consider carefully for the case when a page is shared by two processes,
 *   and one process is to free the page.
 */
void free_page(unsigned int vpn)
{
	int pd_index = vpn / NR_PTES_PER_PAGE;
	int pte_index = vpn % NR_PTES_PER_PAGE;

	struct pagetable *pt = ptbr;
	struct pte_directory *pd;
	struct pte *pte;

	pd = pt->outer_ptes[pd_index];
	pte = &pd->ptes[pte_index];
	
	unsigned int pagefn=pte->pfn;
	mapcounts[pagefn]=mapcounts[pagefn]-1;

	pte->valid=false;
	pte->writable=false;
	pte->pfn=0;

	struct tlb_entry *t = tlb;
	for(int i=0; i<256; i++)
	{
		t=tlb+i;
		if(t->pfn==pagefn)
		{
			t->valid=false;
			t->pfn=0;
			t->vpn=0;

			return;
		}
	}

	/*
	if(mapcounts[pagefn]>0)
	{
		printf("don't erase\n");
		pte->valid=false;
		pte->writable=false;
		pte->pfn=0;
	}
	else
	{
		printf("erase\n");
		pte->valid=false;
		pte->writable=false;
		pte->pfn=0;
	}
	*/

}


/**
 * handle_page_fault()
 *
 * DESCRIPTION
 *   Handle the page fault for accessing @vpn for @rw. This function is called
 *   by the framework when the __translate() for @vpn fails. This implies;
 *   0. page directory is invalid
 *   1. pte is invalid
 *   2. pte is not writable but @rw is for write
 *   This function should identify the situation, and do the copy-on-write if
 *   necessary.
 *
 * RETURN
 *   @true on successful fault handling
 *   @false otherwise
 */
bool handle_page_fault(unsigned int vpn, unsigned int rw)
{
	int pd_index = vpn / NR_PTES_PER_PAGE;
	int pte_index = vpn % NR_PTES_PER_PAGE;
	struct process *temp,*n,*temptemp;
	struct pagetable *pt = ptbr;
	struct pte_directory *pd;
	struct pte *pte;
	int private_temp; // write신청 들어온애 임시저장. 얘를 찾을때 까지 반복

	

	pd = pt->outer_ptes[pd_index];
	pte = &pd->ptes[pte_index];
	printf("%d %d %d\n",pte->private,pte->writable,mapcounts[pte->pfn]);
	if (rw == RW_WRITE) 
	{
		
		if (pte->writable==false  && pte->private !=NONE)
		{
			printf("copy on write\n");
			
			

			private_temp=pte->private;
			
			mapcounts[pte->pfn]=mapcounts[pte->pfn]-1;
			
			list_for_each_entry_safe(temp,n,&processes,list)
			{
				struct pte_directory *parent = temp->pagetable.outer_ptes[pd_index];
				struct pte *pten=&parent->ptes[pte_index];

				if(pten->private==current->pid)
				{
					pten->private=pte->private;
					
					if(mapcounts[pte->pfn]==1)
					{
						// pten->writable=true; 왜 true 로 안바꿔줄까?
						//pten->private=NONE;
					}
				}
			}
			
			
			for(int i=0; i<128; i++)
			{
				if(mapcounts[i]==0)
				{
					mapcounts[i]++;
					struct pte_directory *pd2;
					struct pte *pte2=malloc(1000);

					pd2 = pt->outer_ptes[pd_index];
					pte2 = &pd2->ptes[pte_index];

					pd = pt->outer_ptes[pd_index];

					pte2->pfn=i;
					pte2->valid=true;
					pte2->writable=true;
					pte2->private=NONE;

					pd->ptes[pte_index]=*pte2;
					return true;
				}
			}
			
		}
		else
		{
			return false;
		}


	}
	if(!pd)
	{
		return false;
	}
	if(pte->valid==false)
	{
		return false;
	}
	return false;
}


/**
 * switch_process()
 *
 * DESCRIPTION
 *   If there is a process with @pid in @processes, switch to the process.
 *   The @current process at the moment should be put into the @processes
 *   list, and @current should be replaced to the requested process.
 *   Make sure that the next process is unlinked from the @processes, and
 *   @ptbr is set properly.
 *
 *   If there is no process with @pid in the @processes list, fork a process
 *   from the @current. This implies the forked child process should have
 *   the identical page table entry 'values' to its parent's (i.e., @current)
 *   page table. 
 *   To implement the copy-on-write feature, you should manipulate the writable
 *   bit in PTE and mapcounts for shared pages. You may use pte->private for 
 *   storing some useful information :-)
 */
void switch_process(unsigned int pid)
{
	struct process *temp,*n,*temptemp;

	struct pagetable *pt1 = ptbr;
	struct pagetable *pt2=malloc(1000);
	struct pte_directory *pd1;
	struct pte_directory *pd2;
	struct pte *pte1;
	struct pte *pte2;
	int found=0;

	struct tlb_entry *t = tlb; //tlb flush
	for(int i=0; i<256; i++)
	{
		t=tlb+i;
		t->pfn=0;
		t->valid=false;
		t->vpn=0;
	}

	list_add(&current->list,&processes);
	list_for_each_entry_safe(temp,n,&processes,list)
	{
		if(temp->pid==pid)
		{
			temptemp=temp;
			found=1;
		}
	}
	if(found==1)
	{
		printf("nofork\n");
		current=temptemp;
		list_del(&temptemp->list);
		ptbr=&current->pagetable;
	}
	else
	{
		printf("fork\n");
		
		struct process *new=malloc(1000);
		new->pid=pid;

		ptbr=&new->pagetable;
		for(int i=0; i<16;i++)
		{
			
			if(pt1->outer_ptes[i])
			{
				
				pd1 = pt1->outer_ptes[i];
					pd2=malloc(1000);
					for(int j=0; j<16; j++)
					{
						
						
						pte1 = &pd1->ptes[j];
						
						if(pte1->valid==true)
						{
								
							struct pte *pte2=malloc(1000);
							
							pte2->valid=true;
							

							if(pte1->writable==true || pte1->private !=NONE)
							{
								if(mapcounts[pte1->pfn]==1)
								{
									pte1->private=new->pid;
									pte2->private=current->pid;

								}
								else if(mapcounts[pte1->pfn]>1)
								{
									pte1->private=new->pid;
									pte2->private=pte1->private;
								}
								else{}
							}
							else
							{
								pte1->private=NONE;
								pte2->private=NONE;
							}
							
							pte2->writable=false;
							pte1->writable=false;

							pte2->pfn=pte1->pfn;
							
							mapcounts[pte1->pfn]++;

							pd2->ptes[j]=*pte2;
						}
						/*
						pte2->valid=pte1->valid;
						pte2->writable=pte1->writable;*/
						
						
					}
				ptbr->outer_ptes[i]=pd2;
			}
			
			
			
		}
		current=new;
				

	}
	
	
}

/*if(mapcounts[pte->pfn]>0)
				{

					mapcounts[pte->pfn]++;
				}*/
