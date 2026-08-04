#include <stdint.h>
#include <string.h>
#include <stdio.h>

extern "C" {
    // Basic IPv4 + TCP packet parser. Returns payload length or -1 on error.
    int packet_extract_payload(const char* raw_bytes, int length, char* payload_out) {
        if (length < 20) return -1; // Minimum IPv4 header length
        
        uint8_t version_ihl = (uint8_t)raw_bytes[0];
        uint8_t version = version_ihl >> 4;
        uint8_t ihl = version_ihl & 0x0F;
        
        if (version != 4) return -1; // Only IPv4 supported in this prototype
        
        int ip_header_len = ihl * 4;
        if (length < ip_header_len) return -1;
        
        uint8_t protocol = (uint8_t)raw_bytes[9];
        
        if (protocol == 6) { // TCP
            if (length < ip_header_len + 20) return -1;
            
            uint8_t data_offset = (uint8_t)raw_bytes[ip_header_len + 12];
            int tcp_header_len = (data_offset >> 4) * 4;
            
            int payload_offset = ip_header_len + tcp_header_len;
            if (payload_offset > length) return -1;
            
            int payload_length = length - payload_offset;
            if (payload_length > 0) {
                memcpy(payload_out, raw_bytes + payload_offset, payload_length);
            }
            return payload_length;
            
        } else if (protocol == 17) { // UDP
            if (length < ip_header_len + 8) return -1;
            
            int payload_offset = ip_header_len + 8;
            int payload_length = length - payload_offset;
            if (payload_length > 0) {
                memcpy(payload_out, raw_bytes + payload_offset, payload_length);
            }
            return payload_length;
        }
        
        return -1; // Unsupported protocol
    }
}
