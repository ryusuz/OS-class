#include <stdio.h>
#define MAX_PCB_SIZE 64

typedef struct ku_pte {
    unsigned char present : 1;
    unsigned char check : 1;
    unsigned char pfn : 6;

    // char pte;
} ku_pte;

typedef struct _PCB {
    char pid;
    ku_pte *pdba;   // page directory base address
    struct _PCB *next;
} PCB;

typedef struct _LinkedList {
    PCB *head;
} LinkedList;

ku_pte *ku_mmu_p_mem;
int *ku_mmu_swap_space;

LinkedList *ku_mmu_pcb_list;

int *ku_mmu_free_list;
int ku_mmu_page_num;
int ku_mmu_swap_num;
int ku_mmu_count = 0;   // 0 이라는 것은 할당이 안 된 상태, 1 이상은 할당, -1은 할당됐지만 스왑 불가능
int swap_count = 0;     // 0 : 아무것도 없음, 1 : swap-in
int swap_index = 1;

LinkedList *ku_mmu_init_list() {
    LinkedList *list = (LinkedList *)malloc(sizeof(LinkedList));
    PCB *newPCB = (PCB *)malloc(sizeof(PCB));
    list->head = newPCB;
    newPCB->next = NULL;
    return list;
}

PCB *ku_mmu_last_node() {
    PCB *tmp = ku_mmu_pcb_list->head;
    
    while(tmp->next != NULL) {
        tmp = tmp->next;
    }

    return tmp; // 라스트 노드!!~!!
}

PCB *ku_mmu_insert_list(char pid, ku_pte *pdba) {
    PCB *newPCB = (PCB *)malloc(sizeof(PCB));
    newPCB->pid = pid;
    newPCB->pdba = pdba;
    newPCB->next = NULL;

    PCB *last = ku_mmu_last_node();
    last->next = newPCB;

    return newPCB;
}

PCB *ku_mmu_find_pid(char pid){
    PCB *tmp = ku_mmu_pcb_list->head;

    while(tmp->next != NULL) {
        tmp = tmp->next;

        if(pid == tmp->pid) return tmp;
    }
    return NULL;
}


// p_mem의 빈 공간의 주소를 리턴해주는 함수
ku_pte *get_free_mem(int check_swapable) {
    int i;
    unsigned int swapable_count = ku_mmu_page_num;

    // 프리리스트 탐색하면서 값이 0인 값을 찾아서(빈공간)
    // 스왑가능한 페이지면 ++한 값을, 스왑불가능한 페이지면 -1을 넣어준다.
    for(i = 0; i < ku_mmu_page_num; i++) {
        if(ku_mmu_free_list[i] == -1) {
            swapable_count--;  // 스왑불가능한 공간 찾아서 할당가능 페이지 개수 줄여주며 체크
        }
        else if(ku_mmu_free_list[i] == 0) {
            ku_mmu_free_list[i] =  check_swapable == -1 ? -1 : ++ku_mmu_count;
            return &ku_mmu_p_mem[i * 4];
        }
    }

    int min = ku_mmu_count+1;
    int index_of_min=-1;

    if(swapable_count > 0) {
        //swap_out
        // 스왑가능한 페이지들 중 가장 먼저 할당된 페이지(가장 작은 인덱스)를 찾아줌 
        for(i = 1; i < ku_mmu_page_num; i++) {
            if(ku_mmu_free_list[i] > 0 && min >= ku_mmu_free_list[i]) {
                min = ku_mmu_free_list[i];
                index_of_min = i;
            }
        }

        // page의 pfn을 가지는 녀석을  pmem에서 선형으로 돌면서 page == pfn이면 pte의 내용을 다시 바꿔줌
        int i;
        for(i=4;i < ku_mmu_page_num*4;i++){
            if(ku_mmu_p_mem[i].pfn == index_of_min) {
                ku_mmu_p_mem[i].present = 0;
                ku_mmu_p_mem[i].check = swap_index & 1;
                ku_mmu_p_mem[i].pfn = (swap_index & 126) >> 1; 
            }
        }

        ku_mmu_swap_space[(swap_index++ % ku_mmu_swap_num-1)+1] = 1;    //swap-in
        ku_mmu_free_list[index_of_min] = check_swapable == -1 ? -1 : ++ku_mmu_count;
        return &ku_mmu_p_mem[index_of_min * 4];     
    }
    else
        return NULL; //스왑가능한 페이지 없음

}

int ku_page_fault(char pid, char va) {
    ku_pte *PD = ku_mmu_find_pid(pid)->pdba;
    ku_pte *PMD;
    ku_pte *PT;
    ku_pte *page;
    
    int pd_offset = (va & 192) >> 6;
    ku_pte *PDE = (PD + pd_offset);
    char swap_offset = PDE->pfn << 1| PDE->check;

    if(PDE->present == 0 && PDE->check == 0) {
        // 매핑이 안됨
        PMD = get_free_mem(-1);
        if(PMD == NULL) return -1;
        // PDE
        PDE->present = 1;
        PDE->pfn = (PMD - ku_mmu_p_mem) / 4;
    } else {
        // PFN 알고 있는 곳
        PMD = ku_mmu_p_mem + (PDE->pfn * 4);
    }

    // PDE가 매핑이 되어있는 곳
    int pmd_offset = (va & 48) >> 4;    
    ku_pte *PMDE = (PMD + pmd_offset);
    swap_offset = PMDE->pfn  << 1| PMDE->check;

    if(PMDE->present == 0 && PMDE->check == 0) {
        PT = get_free_mem(-1);
        if(PT == NULL) return -1;
        PMDE->present = 1;
        PMDE->pfn = (PT - ku_mmu_p_mem) / 4;
    } else {
        PT = ku_mmu_p_mem + (PMDE->pfn * 4);
    }

    int pt_offset = (va & 12) >> 2;
    ku_pte *PTE = (PT + pt_offset);
    swap_offset = PTE->pfn  << 1| PTE->check;

    if((PTE->present == 0 && PTE->check == 0)) {
        page = get_free_mem(0);
        if(page == NULL) return -1; 
 
        PTE->present = 1;
        PTE->pfn = (page - ku_mmu_p_mem) / 4;
    } else if(PTE->present == 0 && swap_offset != 0){ 
        //스왑아웃 되어있다면?
        page = get_free_mem(0);
        if(page == NULL) return -1;
        
        PTE->present = 1;
        PTE->check = 0;
        PTE->pfn = (page - ku_mmu_p_mem) / 4;
    } else {
        page = ku_mmu_p_mem + (PTE->pfn * 4);
    }

    return 0;
}

void *ku_mmu_init(unsigned int mem_size, unsigned int swap_size) {
    ku_mmu_page_num = mem_size / 4;
    ku_mmu_swap_num = swap_size / 4;

    ku_mmu_p_mem = (ku_pte *)malloc(sizeof(ku_pte) *4 * ku_mmu_page_num);  // 메모리를 페이지 개수 * pte 만큼 동적할당
    ku_mmu_swap_space = (int *)malloc(sizeof(int) * ku_mmu_swap_num);   // 스왑스페이스 생성
    ku_mmu_free_list = (int *)malloc(sizeof(int) * ku_mmu_page_num);  // 프리리스트 생성, 안의 값들은 모두 0이기 때문에 비할당 상태
    ku_mmu_pcb_list = ku_mmu_init_list();

    ku_mmu_free_list[0] = -1;   // 0번 인덱스는 OS영역으로 사용불가, 스왑 불가 => -1
    
    return ku_mmu_p_mem;
}

int ku_run_proc(char pid, struct ku_pte **ku_cr3) { // pid와 ku_cr3는 next process의 것들
    PCB *tmp = ku_mmu_find_pid(pid);

    if(tmp == NULL) {   //새로운 PCB 생성

        ku_pte *newPage = get_free_mem(-1); // 새로운 공간 할당

        if(newPage == NULL) return -1;

        tmp = ku_mmu_insert_list(pid, newPage);
    }

    *ku_cr3 = tmp->pdba;

    return 0;
}