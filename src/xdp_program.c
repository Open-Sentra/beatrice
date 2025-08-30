#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

// XDP action: redirect to user space
#define XDP_REDIRECT 4

// Map to store redirect information
struct {
    __uint(type, BPF_MAP_TYPE_XSKMAP);
    __uint(max_entries, 1);
    __uint(key_size, sizeof(int));
    __uint(value_size, sizeof(int));
} xsks_map SEC(".maps");

// XDP program entry point
SEC("xdp")
int xdp_prog(struct xdp_md *ctx)
{
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;
    
    // Basic packet validation
    if (data + sizeof(struct ethhdr) > data_end)
        return XDP_PASS;
    
    struct ethhdr *eth = data;
    
    // Check if it's an IP packet
    if (eth->h_proto != bpf_htons(ETH_P_IP))
        return XDP_PASS;
    
    // Validate IP header
    if (data + sizeof(struct ethhdr) + sizeof(struct iphdr) > data_end)
        return XDP_PASS;
    
    struct iphdr *ip = (void *)(eth + 1);
    
    // Basic IP validation
    if (ip->ihl < 5 || ip->version != 4)
        return XDP_PASS;
    
    // Check if we have enough data for the full IP header
    if (data + sizeof(struct ethhdr) + (ip->ihl * 4) > data_end)
        return XDP_PASS;
    
    // Redirect to user space (queue 0)
    int key = 0;
    return bpf_redirect_map(&xsks_map, key, 0);
}

// License for the program
char _license[] SEC("license") = "GPL"; 