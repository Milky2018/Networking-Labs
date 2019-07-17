#ifndef __MMACROS_H__
#define __MMACROS_H__

#include <arpa/inet.h>

#define hton(x) _Generic((x), \
	u16 : htons(x), \
	u32 : htonl(x))

#define ntoh(x) _Generic((x), \
	u16 : ntohs(x), \
	u32 : ntohl(x))

#define attr_use(typename) \
    static typename __##typename##_attr_use;

#define attr_get(typename, property) \
    static inline typeof(__##typename##_attr_use.property) \
	typename##_get_##property(typename *__this) \
    { \
        return ntoh(__this->property); \
    }

#define attr_set(typename, property) \
    static inline void \
	typename##_set_##property(typename *__this, typeof(__##typename##_attr_use.property) __arg) \
    { \
        __this->property = hton(__arg); \
    }

#define attr_access(typename, property) \
    attr_get(typename, property) \
    attr_set(typename, property)

typedef struct sock_addr sock_addr;
attr_use(sock_addr)
attr_access(sock_addr, ip)
attr_access(sock_addr, port)

#endif