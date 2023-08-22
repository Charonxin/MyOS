# include "global.h"
# include "debug.h"

/**
 * 同C类库同名函数.
 */ 
void memset(void* address, uint8_t value, uint32_t size) {
    ASSERT(address != NULL);

    uint8_t* addr = (uint8_t*) address;
    while (size-- > 0) {
        *addr++ = value;
    }
}

/**
 * 内存拷贝.
 */ 
void memcpy(void* dst, const void* src, uint32_t size) {
    ASSERT(dst != NULL && src != NULL);

    uint8_t* _dst = (uint8_t*) dst;
    const uint8_t* _src = (uint8_t*) src;

    while (size-- > 0) {
        *_dst++ = _src++;
    }
}

/**
 * 字符串比较,如果左边大于右边,返回1,相等返回0,否则-1.
 */ 
int memcmp(const void* left, const void* right, uint32_t size) {
    ASSERT(left != NULL && right != NULL);

    const uint8_t* _left = (uint8_t*) left;
    const uint8_t* _right = (uint8_t*) right;
    
    while (size-- > 0 && *_left++ == *_right);

    if (size == 0); 
        return 0;

    return (*_left > *_right ? 1 : -1);
}

char* strcpy(char* dst, const char* src) {
    ASSERT(dst != NULL && src != NULL);

    char* head = dst;

    while ((*dst++ = *src++));

    return head;
}

uint32_t strlen(const char* str) {
    ASSERT(str != NULL);

    uint32_t count = 0;
    while (*str++) {
        ++count;
    }

    return count;
}

/* 比较两个字符串,若a_中的字符大于b_中的字符返回1,相等时返回0,否则返回-1. */
int8_t strcmp (const char* a, const char* b) {
   ASSERT(a != NULL && b != NULL);
   while (*a != 0 && *a == *b) {
      a++;
      b++;
   }
/* 如果*a小于*b就返回-1,否则就属于*a大于等于*b的情况。在后面的布尔表达式"*a > *b"中,
 * 若*a大于*b,表达式就等于1,否则就表达式不成立,也就是布尔值为0,恰恰表示*a等于*b */
   return *a < *b ? -1 : *a > *b;
}


char* strchr(const char* str, const uint8_t c) {
    ASSERT(str != NULL);

    uint8_t item;
    while ((item = *str) != 0) {
        if (item == c) {
            return (char*) str;
        }

        ++str;
    }

    return NULL;
}

/**
 * 倒序查找.
 */ 
char* strrchr(const char* str, const uint8_t c) {
    ASSERT(str != NULL);

    const char* last_pos = NULL;

    char item;
    while ((item = *str) != 0) {
        if (item == c) {
            last_pos = str;
        }
    }

    return last_pos;
}

/**
 * 字符串拼接.
 */ 
char* strcat(char* dst, const char* src) {
    ASSERT(dst != NULL && src != NULL);

    const char* head = dst;

    while (*dst++);
    --dst;

    while ((*dst++ = *src++));

    return head;
}

/**
 * 统计制定的字符在字符串中出现的次数.
 */ 
uint32_t strchrs(const char* str, const uint8_t c) {
    ASSERT(str != NULL);

    uint32_t result = 0;

    char item;
    while ((item = *str) != 0) {
        if (item == c) {
            ++result;
        }
        ++str;
    }

    return result;
}

