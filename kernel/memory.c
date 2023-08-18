#include "memory.h"
#include "bitmap.h"
#include "stdint.h"
#include "global.h"
#include "debug.h"
#include "print.h"
#include "string.h"
#include "sync.h"

#define PAGE_SIZE 4096

// 位图地址
#define MEM_BITMAP_BASE 0xc009a000

// 内核使用的起始虚拟地址
// 跳过低端1MB内存，中间10为代表页表项偏移，即0x100，即256 * 4KB = 1MB
#define K_HEAD_START 0xc0100000

// 获取高10位页目录项标记
#define PDE_INDEX(addr) ((addr & 0xffc00000) >> 22)
// 获取中间10位页表标记
#define PTE_INDEX(addr) ((addr & 0x003ff000) >> 12)

// static functions declarations
static void printKernelPoolInfo(struct pool p);
static void printUserPoolInfo(struct pool p);
static void *vaddr_get(enum pool_flags pf, uint32_t pg_count);
static uint32_t *pte_ptr(uint32_t vaddr);
static uint32_t *pde_ptr(uint32_t vaddr);
static void *palloc(struct pool *m_pool);
static void page_table_add(void *_vaddr, void *_page_phyaddr);

struct pool
{
    struct bitmap pool_bitmap;
    uint32_t phy_addr_start;
    uint32_t pool_size;
    struct lock lock; // 申请内存时互斥
};

/* 内存仓库arena元信息 */
struct arena {
   struct mem_block_desc* desc;	 // 此arena关联的mem_block_desc
/* large为ture时,cnt表示的是页框数。
 * 否则cnt表示空闲mem_block数量 */
   uint32_t cnt;
   bool large;		   
};

struct mem_block_desc k_block_descs[DESC_CNT];	// 内核内存块描述符数组
struct pool kernel_pool, user_pool;      // 生成内核内存池和用户内存池
struct virtual_addr kernel_vaddr;	 // 此结构是用来给内核分配虚拟地址


/**
 * 初始化内存池.
 */
static void mem_pool_init(uint32_t all_memory)
{
    put_str("Start init Memory pool...\n");

    // 页表(一级和二级)占用的内存大小，256的由来:
    // 一页的页目录，页目录的第0和第768项指向一个页表，此页表分配了低端1MB内存(其实此页表中也只是使用了256个表项)，
    // 剩余的254个页目录项实际没有分配对应的真实页表，但是需要为内核预留分配的空间
    uint32_t page_table_size = PAGE_SIZE * 256;

    // 已经使用的内存为: 低端1MB内存 + 现有的页表和页目录占据的空间
    uint32_t used_mem = (page_table_size + 0x100000);

    uint32_t free_mem = (all_memory - used_mem);
    uint16_t free_pages = free_mem / PAGE_SIZE;

    uint16_t kernel_free_pages = (free_pages >> 1);
    uint16_t user_free_pages = (free_pages - kernel_free_pages);

    // 内核空间bitmap长度(字节)，每一位代表一页
    uint32_t kernel_bitmap_length = kernel_free_pages / 8;
    uint32_t user_bitmap_length = user_free_pages / 8;

    // 内核内存池起始物理地址，注意内核的虚拟地址占据地址空间的顶端，但是实际映射的物理地址是在这里
    uint32_t kernel_pool_start = used_mem;
    uint32_t user_pool_start = (kernel_pool_start + kernel_free_pages * PAGE_SIZE);

    kernel_pool.phy_addr_start = kernel_pool_start;
    user_pool.phy_addr_start = user_pool_start;

    kernel_pool.pool_size = kernel_free_pages * PAGE_SIZE;
    user_pool.pool_size = user_free_pages * PAGE_SIZE;

    kernel_pool.pool_bitmap.btmp_bytes_len = kernel_bitmap_length;
    user_pool.pool_bitmap.btmp_bytes_len = user_bitmap_length;

    // 内核bitmap和user bitmap bit数组的起始地址
    kernel_pool.pool_bitmap.bits = (void *)MEM_BITMAP_BASE;
    user_pool.pool_bitmap.bits = (void *)(MEM_BITMAP_BASE + kernel_bitmap_length);

    printKernelPoolInfo(kernel_pool);
    printUserPoolInfo(user_pool);

    bitmap_init(&kernel_pool.pool_bitmap);
    bitmap_init(&user_pool.pool_bitmap);

    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);

    kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kernel_bitmap_length;
    // 内核虚拟地址池仍然保存在低端内存以内
    kernel_vaddr.vaddr_bitmap.bits = (void *)(MEM_BITMAP_BASE + kernel_bitmap_length + user_bitmap_length);
    kernel_vaddr.vaddr_start = K_HEAD_START;

    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    put_str("Init memory pool done.\n");
}

static void printKernelPoolInfo(struct pool p)
{
    put_str("Kernel pool bitmap address: ");
    put_int(p.pool_bitmap.bits);
    put_str("; Kernel pool physical address: ");
    put_int(p.phy_addr_start);
    put_char('\n');
}

static void printUserPoolInfo(struct pool p)
{
    put_str("User pool bitmap address: ");
    put_int(p.pool_bitmap.bits);
    put_str("; User pool physical address: ");
    put_int(p.phy_addr_start);
    put_char('\n');
}

/**
 * 申请指定个数的虚拟页.返回虚拟页的起始地址，失败返回NULL.
 */
static void *vaddr_get(enum pool_flags pf, uint32_t pg_count)
{
    int vaddr_start = 0, bit_idx_start = -1;
    uint32_t count = 0;

    if (pf == PF_KERNEL)
    { // 内核内存池
        bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_count);
        if (bit_idx_start == -1)
        {
            // 申请失败，虚拟内存不足
            return NULL;
        }

        // 修改bitmap，占用虚拟内存
        while (count < pg_count)
        {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, (bit_idx_start + count), 1);
            ++count;
        }

        vaddr_start = (kernel_vaddr.vaddr_start + bit_idx_start * PAGE_SIZE);
    }
    else
    { // 用户内存池
        struct task_struct *cur = running_thread();
        bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap, pg_count);
        if (bit_idx_start == -1)
        {
            return NULL;
        }

        while (count < pg_count)
        {
            bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx_start + count++, 1);
        }
        vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;

        /* (0xc0000000 - PG_SIZE)做为用户3级栈已经在start_process被分配 */
        ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
    }

    return (void *)vaddr_start;
}

/* 在用户空间中申请4k内存,并返回其虚拟地址 */
void* get_user_pages(uint32_t pg_cnt) {
   lock_acquire(&user_pool.lock);
   void* vaddr = malloc_page(PF_USER, pg_cnt);
   memset(vaddr, 0, pg_cnt * PG_SIZE);
   lock_release(&user_pool.lock);
   return vaddr;
}

/* 将地址vaddr与pf池中的物理地址关联,仅支持一页空间分配 */
void* get_a_page(enum pool_flags pf, uint32_t vaddr) {
   struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
   lock_acquire(&mem_pool->lock);

   /* 先将虚拟地址对应的位图置1 */
   struct task_struct* cur = running_thread();
   int32_t bit_idx = -1;

/* 若当前是用户进程申请用户内存,就修改用户进程自己的虚拟地址位图 */
   if (cur->pgdir != NULL && pf == PF_USER) {
      bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
      ASSERT(bit_idx > 0);
      bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx, 1);

   } else if (cur->pgdir == NULL && pf == PF_KERNEL){
/* 如果是内核线程申请内核内存,就修改kernel_vaddr. */
      bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
      ASSERT(bit_idx > 0);
      bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx, 1);
   } else {
      PANIC("get_a_page:not allow kernel alloc userspace or user alloc kernelspace by get_a_page");
   }

   void* page_phyaddr = palloc(mem_pool);
   if (page_phyaddr == NULL) {
      return NULL;
   }
   page_table_add((void*)vaddr, page_phyaddr); 
   lock_release(&mem_pool->lock);
   return (void*)vaddr;
}

/* 得到虚拟地址映射到的物理地址 */
uint32_t addr_v2p(uint32_t vaddr) {
   uint32_t* pte = pte_ptr(vaddr);
/* (*pte)的值是页表所在的物理页框地址,
 * 去掉其低12位的页表项属性+虚拟地址vaddr的低12位 */
   return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

/**
 * 得到虚拟地址对应的PTE的指针.
 */
static uint32_t *pte_ptr(uint32_t vaddr)
{
    return (uint32_t *)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) + (PTE_INDEX(vaddr) << 2));
}

/**
 * 得到虚拟地址对应的PDE指针.
 */
static uint32_t *pde_ptr(uint32_t vaddr)
{
    return (uint32_t *)((0xfffff000) + (PDE_INDEX(vaddr) << 2));
}

/**
 * 在给定的物理内存池中分配一个物理页，返回其物理地址.
 */
static void *palloc(struct pool *m_pool)
{
    int bit_index = bitmap_scan(&m_pool->pool_bitmap, 1);
    if (bit_index == -1)
    {
        return NULL;
    }

    bitmap_set(&m_pool->pool_bitmap, bit_index, 1);
    uint32_t page_phyaddr = ((bit_index * PAGE_SIZE) + m_pool->phy_addr_start);
    return (void *)page_phyaddr;
}

/**
 * 通过页表建立虚拟页与物理页的映射关系.
 */
static void page_table_add(void *_vaddr, void *_page_phyaddr)
{
    uint32_t vaddr = (uint32_t)_vaddr, page_phyaddr = (uint32_t)_page_phyaddr;
    uint32_t *pde = pde_ptr(vaddr);
    uint32_t *pte = pte_ptr(vaddr);

    if (*pde & 0x00000001)
    {
        // 页目录项已经存在
        if (!(*pte & 0x00000001))
        {
            // 物理页必定不存在，使页表项指向我们新分配的物理页
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        }
    }
    else
    {
        // 新分配一个物理页作为页表
        uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);
        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        // 清理物理页
        memset((void *)((int)pte & 0xfffff000), 0, PAGE_SIZE);
        *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }
}

/**
 * 分配page_count个页空间，自动建立虚拟页与物理页的映射.
 */
void *malloc_page(enum pool_flags pf, uint32_t page_count)
{
    ASSERT(page_count > 0 && page_count < 3840);

    // 在虚拟地址池中申请虚拟内存
    void *vaddr_start = vaddr_get(pf, page_count);
    if (vaddr_start == NULL)
    {
        return NULL;
    }

    uint32_t vaddr = (uint32_t)vaddr_start, count = page_count;
    struct pool *mem_pool = (pf & PF_KERNEL) ? &kernel_pool : &user_pool;

    // 物理页不必连续，逐个与虚拟页做映射
    while (count > 0)
    {
        void *page_phyaddr = palloc(mem_pool);
        if (page_phyaddr == NULL)
        {
            return NULL;
        }

        page_table_add((void *)vaddr, page_phyaddr);
        vaddr += PAGE_SIZE;
        --count;
    }

    return vaddr_start;
}

/**
 * 在内核内存池中申请page_count个页.
 */
void *get_kernel_pages(uint32_t page_count)
{
    void *vaddr = malloc_page(PF_KERNEL, page_count);
    if (vaddr != NULL)
    {
        memset(vaddr, 0, page_count * PAGE_SIZE);
    }
    return vaddr;
}

/* 为malloc做准备 */
void block_desc_init(struct mem_block_desc* desc_array) {				   
   uint16_t desc_idx, block_size = 16;

   /* 初始化每个mem_block_desc描述符 */
   for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
      desc_array[desc_idx].block_size = block_size;

      /* 初始化arena中的内存块数量 */
      desc_array[desc_idx].blocks_per_arena = (PG_SIZE - sizeof(struct arena)) / block_size;	  

      list_init(&desc_array[desc_idx].free_list);

      block_size *= 2;         // 更新为下一个规格内存块
   }
}

/* 返回arena中第idx个内存块的地址 */
static struct mem_block* arena2block(struct arena* a, uint32_t idx) {
  return (struct mem_block*)((uint32_t)a + sizeof(struct arena) + idx * a->desc->block_size);
}

/* 返回内存块b所在的arena地址 */
static struct arena* block2arena(struct mem_block* b) {
   return (struct arena*)((uint32_t)b & 0xfffff000);
}

/* 在堆中申请size字节内存 */
void* sys_malloc(uint32_t size) {
   enum pool_flags PF;
   struct pool* mem_pool;
   uint32_t pool_size;
   struct mem_block_desc* descs;
   struct task_struct* cur_thread = running_thread();

/* 判断用哪个内存池*/
   if (cur_thread->pgdir == NULL) {     // 若为内核线程
      PF = PF_KERNEL; 
      pool_size = kernel_pool.pool_size;
      mem_pool = &kernel_pool;
      descs = k_block_descs;
   } else {				      // 用户进程pcb中的pgdir会在为其分配页表时创建
      PF = PF_USER;
      pool_size = user_pool.pool_size;
      mem_pool = &user_pool;
      descs = cur_thread->u_block_desc;
   }

   /* 若申请的内存不在内存池容量范围内则直接返回NULL */
   if (!(size > 0 && size < pool_size)) {
      return NULL;
   }
   struct arena* a;
   struct mem_block* b;	
   lock_acquire(&mem_pool->lock);

/* 超过最大内存块1024, 就分配页框 */
   if (size > 1024) {
      uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(struct arena), PG_SIZE);    // 向上取整需要的页框数

      a = malloc_page(PF, page_cnt);

      if (a != NULL) {
	 memset(a, 0, page_cnt * PG_SIZE);	 // 将分配的内存清0  

      /* 对于分配的大块页框,将desc置为NULL, cnt置为页框数,large置为true */
	 a->desc = NULL;
	 a->cnt = page_cnt;
	 a->large = true;
	 lock_release(&mem_pool->lock);
	 return (void*)(a + 1);		 // 跨过arena大小，把剩下的内存返回
      } else { 
	 lock_release(&mem_pool->lock);
	 return NULL; 
      }
   } else {    // 若申请的内存小于等于1024,可在各种规格的mem_block_desc中去适配
      uint8_t desc_idx;
      
      /* 从内存块描述符中匹配合适的内存块规格 */
      for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
	 if (size <= descs[desc_idx].block_size) {  // 从小往大后,找到后退出
	    break;
	 }
      }

   /* 若mem_block_desc的free_list中已经没有可用的mem_block,
    * 就创建新的arena提供mem_block */
      if (list_empty(&descs[desc_idx].free_list)) {
	 a = malloc_page(PF, 1);       // 分配1页框做为arena
	 if (a == NULL) {
	    lock_release(&mem_pool->lock);
	    return NULL;
	 }
	 memset(a, 0, PG_SIZE);

    /* 对于分配的小块内存,将desc置为相应内存块描述符, 
     * cnt置为此arena可用的内存块数,large置为false */
	 a->desc = &descs[desc_idx];
	 a->large = false;
	 a->cnt = descs[desc_idx].blocks_per_arena;
	 uint32_t block_idx;

	 enum intr_status* old_status = intr_disable();

	 /* 开始将arena拆分成内存块,并添加到内存块描述符的free_list中 */
	 for (block_idx = 0; block_idx < descs[desc_idx].blocks_per_arena; block_idx++) {
	    b = arena2block(a, block_idx);
	    ASSERT(!list_find(&a->desc->free_list, &b->free_elem));
	    list_append(&a->desc->free_list, &b->free_elem);	
	 }
	 intr_set_status(old_status);
      }    

   /* 开始分配内存块 */
      b = elem2entry(struct mem_block, free_elem, list_pop(&(descs[desc_idx].free_list)));
      memset(b, 0, descs[desc_idx].block_size);

      a = block2arena(b);  // 获取内存块b所在的arena
      a->cnt--;		   // 将此arena中的空闲内存块数减1
      lock_release(&mem_pool->lock);
      return (void*)b;
   }
}

/* 将物理地址pg_phy_addr回收到物理内存池 */
void pfree(uint32_t pg_phy_addr) {
   struct pool* mem_pool;
   uint32_t bit_idx = 0;
   if (pg_phy_addr >= user_pool.phy_addr_start) {     // 用户物理内存池
      mem_pool = &user_pool;
      bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
   } else {	  // 内核物理内存池
      mem_pool = &kernel_pool;
      bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
   }
   bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);	 // 将位图中该位清0
}

/* 去掉页表中虚拟地址vaddr的映射,只去掉vaddr对应的pte */
static void page_table_pte_remove(uint32_t vaddr) {
   uint32_t* pte = pte_ptr(vaddr);
   *pte &= ~PG_P_1;	// 将页表项pte的P位置0
   asm volatile ("invlpg %0"::"m" (vaddr):"memory");    //更新tlb
}

/* 在虚拟地址池中释放以_vaddr起始的连续pg_cnt个虚拟页地址 */
static void vaddr_remove(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {
   uint32_t bit_idx_start = 0, vaddr = (uint32_t)_vaddr, cnt = 0;

   if (pf == PF_KERNEL) {  // 内核虚拟内存池
      bit_idx_start = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
      while(cnt < pg_cnt) {
	 bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
      }
   } else {  // 用户虚拟内存池
      struct task_struct* cur_thread = running_thread();
      bit_idx_start = (vaddr - cur_thread->userprog_vaddr.vaddr_start) / PG_SIZE;
      while(cnt < pg_cnt) {
	 bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
      }
   }
}

/* 释放以虚拟地址vaddr为起始的cnt个物理页框 */
void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {
   uint32_t pg_phy_addr;
   uint32_t vaddr = (int32_t)_vaddr, page_cnt = 0;
   ASSERT(pg_cnt >=1 && vaddr % PG_SIZE == 0); 
   pg_phy_addr = addr_v2p(vaddr);  // 获取虚拟地址vaddr对应的物理地址

/* 确保待释放的物理内存在低端1M+1k大小的页目录+1k大小的页表地址范围外 */
   ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= 0x102000);
   
/* 判断pg_phy_addr属于用户物理内存池还是内核物理内存池 */
   if (pg_phy_addr >= user_pool.phy_addr_start) {   // 位于user_pool内存池
      vaddr -= PG_SIZE;
      while (page_cnt < pg_cnt) {
	 vaddr += PG_SIZE;
	 pg_phy_addr = addr_v2p(vaddr);

	 /* 确保物理地址属于用户物理内存池 */
	 ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= user_pool.phy_addr_start);

	 /* 先将对应的物理页框归还到内存池 */
	 pfree(pg_phy_addr);

         /* 再从页表中清除此虚拟地址所在的页表项pte */
	 page_table_pte_remove(vaddr);

	 page_cnt++;
      }
   /* 清空虚拟地址的位图中的相应位 */
      vaddr_remove(pf, _vaddr, pg_cnt);

   } else {	     // 位于kernel_pool内存池
      vaddr -= PG_SIZE;	      
      while (page_cnt < pg_cnt) {
	 vaddr += PG_SIZE;
	 pg_phy_addr = addr_v2p(vaddr);
      /* 确保待释放的物理内存只属于内核物理内存池 */
	 ASSERT((pg_phy_addr % PG_SIZE) == 0 && \
	       pg_phy_addr >= kernel_pool.phy_addr_start && \
	       pg_phy_addr < user_pool.phy_addr_start);
	
	 /* 先将对应的物理页框归还到内存池 */
	 pfree(pg_phy_addr);

         /* 再从页表中清除此虚拟地址所在的页表项pte */
	 page_table_pte_remove(vaddr);

	 page_cnt++;
      }
   /* 清空虚拟地址的位图中的相应位 */
      vaddr_remove(pf, _vaddr, pg_cnt);
   }
}

/* 回收内存ptr */
void sys_free(void* ptr) {
   ASSERT(ptr != NULL);
   if (ptr != NULL) {
      enum pool_flags PF;
      struct pool* mem_pool;

   /* 判断是线程还是进程 */
      if (running_thread()->pgdir == NULL) {
	 ASSERT((uint32_t)ptr >= K_HEAD_START);
	 PF = PF_KERNEL; 
	 mem_pool = &kernel_pool;
      } else {
	 PF = PF_USER;
	 mem_pool = &user_pool;
      }

      lock_acquire(&mem_pool->lock);   
      struct mem_block* b = ptr;
      struct arena* a = block2arena(b);	     // 把mem_block转换成arena,获取元信息
      ASSERT(a->large == 0 || a->large == 1);
      if (a->desc == NULL && a->large == true) { // 大于1024的内存
	 mfree_page(PF, a, a->cnt); 
      } else {				 // 小于等于1024的内存块
	 /* 先将内存块回收到free_list */
	 list_append(&a->desc->free_list, &b->free_elem);

	 /* 再判断此arena中的内存块是否都是空闲,如果是就释放arena */
	 if (++a->cnt == a->desc->blocks_per_arena) {
	    uint32_t block_idx;
	    for (block_idx = 0; block_idx < a->desc->blocks_per_arena; block_idx++) {
	       struct mem_block*  b = arena2block(a, block_idx);
	       ASSERT(list_find(&a->desc->free_list, &b->free_elem));
	       list_remove(&b->free_elem);
	    }
	    mfree_page(PF, a, 1); 
	 } 
      }   
      lock_release(&mem_pool->lock); 
   }
}

void mem_init(void)
{
    put_str("Init memory start.\n");
    uint32_t total_memory = (*(uint32_t *)(0xb00));
    mem_pool_init(total_memory);
    put_str("Init memory done.\n");
}
