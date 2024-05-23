


#define IPV6_STRLEN_MAX         (4 * 8 + 7 + 1 + 4)
#define IPV4_STRLEN_MAX         (3 * 4 + 3 + 1 + 4)

#define INET_ADDR_SIZE          (sizeof(struct in6_addr))

static int is_valid_ipaddr(char *ipstr, int ipver)
{
        unsigned char buf[sizeof(struct in6_addr)];

        return (inet_pton(ipver, ipstr, buf) == 1);
}
