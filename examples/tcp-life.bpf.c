#include <vmlinux.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h>
#include "maps.bpf.h"

#define MAX_ENTRIES 8192

#define AF_INET 2
#define AF_INET6 10

#define MAX_BUCKET_SLOT 11

struct ipv4_key_t {
    u32 saddr;
    u32 saddr_ns;
    u32 saddr_az;
    u32 daddr;
    u32 daddr_ns;
    u32 daddr_az;
};

struct ipv4_bucket_key_t {
    u32 saddr;
    u32 saddr_ns;
    u32 saddr_az;
    u32 daddr;
    u32 daddr_ns;
    u32 daddr_az;
    u64 bucket;
};

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_ENTRIES * (MAX_BUCKET_SLOT + 2));
    __type(key, struct ipv4_bucket_key_t);
    __type(value, u64);
} tcp_ipv4_srtt_usec SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_LRU_HASH);
    __uint(max_entries, MAX_ENTRIES);
    __type(key, struct ipv4_key_t);
    __type(value, u64);
} tcp_ipv4_bytes_sent SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_LRU_HASH);
    __uint(max_entries, MAX_ENTRIES);
    __type(key, struct ipv4_key_t);
    __type(value, u64);
} tcp_ipv4_bytes_recv SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_LRU_HASH);
    __uint(max_entries, MAX_ENTRIES);
    __type(key, struct ipv4_key_t);
    __type(value, u64);
} tcp_ipv4_retrans SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_LRU_HASH);
    __uint(max_entries, MAX_ENTRIES);
    __type(key, struct ipv4_key_t);
    __type(value, u64);
} tcp_ipv4_lost SEC(".maps");

SEC("tp_btf/inet_sock_set_state")
int BPF_PROG(inet_sock_set_state, struct sock *sk, int oldstate, int newstate)
{
    if (newstate != BPF_TCP_CLOSE)
		return 0;

    struct tcp_sock *tp = (struct tcp_sock *)sk;
    struct ipv4_key_t key = {};
    struct ipv4_bucket_key_t bucket_key = {};

    switch (sk->__sk_common.skc_family) {
    case AF_INET:
        bucket_key.saddr_az = bucket_key.saddr_ns = bucket_key.saddr = key.saddr_az = key.saddr_ns = key.saddr = sk->__sk_common.skc_rcv_saddr;
        bucket_key.daddr_az = bucket_key.daddr_ns = bucket_key.daddr = key.daddr_az = key.daddr_ns = key.daddr = sk->__sk_common.skc_daddr;
        increment_map(&tcp_ipv4_bytes_sent, &key, BPF_CORE_READ(tp, bytes_sent));
        increment_map(&tcp_ipv4_bytes_recv, &key, BPF_CORE_READ(tp, bytes_received));
        increment_map(&tcp_ipv4_retrans, &key, BPF_CORE_READ(tp, total_retrans));
        increment_map(&tcp_ipv4_lost, &key, BPF_CORE_READ(tp, lost));
        increment_exp2_histogram(&tcp_ipv4_srtt_usec, bucket_key, BPF_CORE_READ(tp, srtt_us) / 1000, MAX_BUCKET_SLOT);
        break;
        
    case AF_INET6:
        break;
    }

    return 0;
}

char LICENSE[] SEC("license") = "GPL";
