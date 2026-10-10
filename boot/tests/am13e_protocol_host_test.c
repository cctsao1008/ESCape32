/*
 * Stage D1: wire-level ESCape32 command framing through the real
 * boot/src/main.c dispatcher, boot/src/io.c framing helpers,
 * boot/mcu/AM13E/flash_range.c and production flash.c state machine.
 *
 * The receive/send transport and Flash controller are MOCKS, not PB14 hardware.
 * Host CRC is a software model of CRC-32/ISO-HDLC, not TI CRCP qualification.
 */
#define _GNU_SOURCE
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <sys/mman.h>

#include "dl_flash.h"
#include "app_validity.h"

#define MAP_ADDRESS UINT32_C(0x10000000)
#define MAP_LENGTH  UINT32_C(0x00080000)
#define APP_OFFSET  UINT32_C(0x00006000)
#define BLOCK_BYTES 1024U
#define CMD_PROBE 0U
#define CMD_INFO 1U
#define CMD_READ 2U
#define CMD_WRITE 3U
#define CMD_UPDATE 4U
#define CMD_SETWRP 5U
#define CMD_WINDOW 6U
#define RES_OK 0U
#define RES_ERROR 1U
#define RX_CAPACITY (AM13E_FLASH_APP_BYTES + 32768U)
#define TX_CAPACITY 8192U
#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    exit(1); } } while (0)

uintptr_t boot_am13e_test_first;
uintptr_t boot_am13e_test_end;
extern void boot_am13e_protocol_entry(void);
extern void boot_am13e_test_reset_update_state(void);

static uint8_t incoming[RX_CAPACITY];
static uint8_t transmitted[TX_CAPACITY];
static uint8_t expected_reply[TX_CAPACITY];
static size_t incoming_length, read_position, transmitted_length, expected_length;
static unsigned erases, programs;
static jmp_buf end_of_stream;

static void input_bytes(const uint8_t *bytes, size_t n) {
    CHECK(n <= sizeof incoming - incoming_length);
    memcpy(incoming + incoming_length, bytes, n);
    incoming_length += n;
}
static void expected_bytes(const uint8_t *bytes, size_t n) {
    CHECK(n <= sizeof expected_reply - expected_length);
    memcpy(expected_reply + expected_length, bytes, n);
    expected_length += n;
}
static void input_val(uint8_t val) {
    uint8_t b[2] = {val, (uint8_t)~val};
    input_bytes(b, sizeof b);
}
static void expected_val(uint8_t val) {
    uint8_t b[2] = {val, (uint8_t)~val};
    expected_bytes(b, sizeof b);
}
static uint32_t software_crc(const uint8_t *buffer, size_t n) {
    uint32_t crc = UINT32_C(0xffffffff);
    for (size_t i = 0; i < n; ++i) {
        crc ^= buffer[i];
        for (unsigned bit = 0; bit < 8U; ++bit)
            crc = (crc >> 1U) ^
                  (UINT32_C(0xedb88320) & (0U - (crc & 1U)));
    }
    return ~crc;
}
static void little_endian_32(uint32_t value, uint8_t *out) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}
static void queue_write(unsigned block, const uint8_t *payload,
                        unsigned length, bool wrong_crc, int expected_ack) {
    CHECK(block <= 255U && length >= 4U && length <= BLOCK_BYTES &&
          (length & 3U) == 0U);
    input_val(CMD_WRITE);
    input_val((uint8_t)block);
    input_val((uint8_t)(length / 4U - 1U));
    input_bytes(payload, length);
    uint8_t crc[4];
    uint32_t value = software_crc(payload, length);
    if (wrong_crc) value ^= 1U;
    little_endian_32(value, crc);
    input_bytes(crc, 4U);
    if (expected_ack >= 0) expected_val((uint8_t)expected_ack);
}
static void queue_window(unsigned requested,int expected_ack) {
    CHECK(requested<=255U);
    input_val(CMD_WINDOW);
    input_val((uint8_t)requested);
    if(expected_ack>=0)expected_val((uint8_t)expected_ack);
}
static void expected_data(const uint8_t *data, unsigned n) {
    CHECK((n & 3U) == 0U && n >= 4U && n <= BLOCK_BYTES);
    expected_val((uint8_t)(n / 4U - 1U));
    expected_bytes(data, n);
    uint8_t checksum[4];
    little_endian_32(software_crc(data, n), checksum);
    expected_bytes(checksum, sizeof checksum);
}
static void queue_read(unsigned block, unsigned length,
                       const uint8_t *expected_payload) {
    CHECK(block <= 255U && (length & 3U) == 0U && length > 0U);
    input_val(CMD_READ);
    input_val((uint8_t)block);
    input_val((uint8_t)(length / 4U - 1U));
    expected_data(expected_payload, length);
}

/* The next two functions are the actual framing layer's physical backend. */
int recvbuf(char *buf, int len) {
    CHECK(buf != NULL && len >= 0);
    if (read_position == incoming_length) longjmp(end_of_stream, 1);
    CHECK((size_t)len <= incoming_length - read_position);
    memcpy(buf, incoming + read_position, (size_t)len);
    read_position += (size_t)len;
    return 1;
}
void sendbuf(const char *buf, int len) {
    CHECK(buf != NULL && len > 0);
    CHECK((size_t)len <= sizeof transmitted - transmitted_length);
    memcpy(transmitted + transmitted_length, buf, (size_t)len);
    transmitted_length += (size_t)len;
}
/* The host emulates ESCape32 command CRC; hardware CRCP still unqualified. */
uint32_t crc32(const char *buf, int len) {
    CHECK(buf != NULL && len >= 0);
    return software_crc((const uint8_t *)(const void *)buf, (size_t)len);
}
void init(void) {}
void initio(void) {}
bool boot_am13e_take_reboot_ack(void) { return false; }
uint32_t boot_am13e_device_id(void) { return UINT32_C(0x12345678); }
uint8_t boot_am13e_io_id(void) { return 4U; }
bool boot_am13e_application_valid(void) { return false; }
__attribute__((noreturn)) void boot_am13e_launch_application(void) {
    fprintf(stderr, "FAIL unexpected application jump during protocol test\n");
    exit(1);
}

/* Physical Flash model: 2 KiB sector erase, irreversible 1->0 program. */
uint32_t DL_Flash_eraseSector(uint32_t addr) {
    if ((addr & (DL_FLASH_SECTOR_SIZE - 1U)) ||
        addr < boot_am13e_test_first ||
        addr > boot_am13e_test_end - DL_FLASH_SECTOR_SIZE)
        return DL_FLASH_ERROR;
    ++erases;
    memset((void *)(uintptr_t)addr, 0xff, DL_FLASH_SECTOR_SIZE);
    return DL_FLASH_SUCCESS;
}
uint32_t DL_Flash_program(uint32_t addr, uint8_t *src, uint32_t length) {
    if (!src || !length || (addr & 15U) || (length & 15U) ||
        addr < boot_am13e_test_first ||
        addr > boot_am13e_test_end - length)
        return DL_FLASH_ERROR;
    uint8_t *dst = (uint8_t *)(uintptr_t)addr;
    for (uint32_t i = 0U; i < length; ++i)
        if ((uint8_t)(dst[i] & src[i]) != src[i])
            return DL_FLASH_ERROR;
    ++programs;
    for (uint32_t i = 0U; i < length; ++i) dst[i] &= src[i];
    return DL_FLASH_SUCCESS;
}

static uint8_t *load_image(const char *filename, size_t *size_out) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) { perror(filename); exit(2); }
    CHECK(fseek(fp, 0, SEEK_END) == 0);
    const long n = ftell(fp);
    CHECK(n >= 8L && n <= (long)AM13E_FLASH_APP_BYTES &&
          (n & 3L) == 0L);
    CHECK(fseek(fp, 0, SEEK_SET) == 0);
    uint8_t *image = malloc((size_t)n);
    CHECK(image != NULL);
    CHECK(fread(image, 1U, (size_t)n, fp) == (size_t)n);
    CHECK(fclose(fp) == 0);
    *size_out = (size_t)n;
    return image;
}
static void queue_protocol(size_t image_bytes,const uint8_t *image){
    const uint8_t erased[8]={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
    uint8_t erased16[16],probe_upper[16],synthetic[16];
    memset(erased16,0xff,sizeof erased16);
    memset(probe_upper,0xa5,sizeof probe_upper);
    memset(synthetic,0x3b,sizeof synthetic);
    const uint8_t bad_pair[2]={CMD_PROBE,CMD_PROBE};
    input_bytes(bad_pair,sizeof bad_pair); /* Bad complement, no ACK */
    input_val(CMD_PROBE); expected_val(RES_OK);
    input_val(CMD_INFO);
    const uint8_t info[32]={4U,4U,0x78U,0x56U,0x34U,0x12U};
    expected_data(info,32U);

    queue_window(1U,RES_OK);
    queue_read(0U,16U,probe_upper); /* Effective block256 */
    queue_read(231U,16U,erased16); /* Last valid block487 */
    queue_window(2U,RES_ERROR);    /* Remain window1 */
    queue_read(0U,16U,probe_upper);
    queue_write(232U,synthetic,16U,false,RES_ERROR);
    input_val(CMD_READ);input_val(232U);input_val(3U);
    input_val(CMD_PROBE);expected_val(RES_OK);
    queue_window(0U,RES_OK);
    queue_read(0U,16U,erased16);

    /* Original Rel17 CMD_WRITE is not a new all-image state machine:
     * arbitrary APP 1KiB blocks are legal, independent of prior
     * CMD_WINDOW selection and of firmware linked size.
     */
    queue_write(2U,synthetic,16U,false,RES_OK);
    queue_write(2U,synthetic,16U,false,RES_OK);
    queue_read(2U,16U,synthetic);
    queue_write(2U,synthetic,16U,true,-1); /* Bad wire CRC: no Flash */
    queue_read(2U,16U,synthetic);

    /* Old host's invalidation frames are still ordinary FF writes. */
    queue_write(0U,erased,8U,false,RES_OK);
    queue_write(1U,erased,8U,false,RES_OK);

    unsigned window=0U;
    for(size_t offset=0U;offset<image_bytes;offset+=BLOCK_BYTES){
        const unsigned absolute=(unsigned)(offset/BLOCK_BYTES);
        if((absolute>>8U)!=window){
            window=absolute>>8U;
            queue_window(window,RES_OK);
        }
        unsigned n=(unsigned)((image_bytes-offset>BLOCK_BYTES)?
                               BLOCK_BYTES:image_bytes-offset);
        queue_write(absolute&255U,image+offset,n,false,RES_OK);
        if(absolute==0U)queue_write(0U,image,n,false,RES_OK);
    }
    if(window)queue_window(0U,RES_OK);
    /* Short ELF-link smoke binaries can be smaller than the 32-byte
     * READ payload. CMD_READ returns the programmed prefix followed by
     * erased (0xff) Flash, not bytes past the end of the BIN buffer.
     */
    uint8_t first32[32];
    memset(first32,0xff,sizeof first32);
    const size_t available=image_bytes<sizeof first32?
                           image_bytes:sizeof first32;
    memcpy(first32,image,available);
    queue_read(0U,32U,first32);
    /* Source-gap guard: CMD_UPDATE rejects before consuming an update
     * payload, so the next command remains framed and Boot stays intact.
     * Reject all three upstream WRP modes and an invalid selector.
     */
    input_val(CMD_UPDATE);expected_val(RES_ERROR);
    input_val(CMD_PROBE);expected_val(RES_OK);
    input_val(CMD_SETWRP);input_val(0x33U);expected_val(RES_ERROR);
    input_val(CMD_SETWRP);input_val(0x44U);expected_val(RES_ERROR);
    input_val(CMD_SETWRP);input_val(0x55U);expected_val(RES_ERROR);
    input_val(CMD_SETWRP);input_val(0x77U);expected_val(RES_ERROR);
    input_val(CMD_PROBE);expected_val(RES_OK);
}
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s path/to/AM13E_APP_SMOKE.am13e-smoke.bin\n", argv[0]);
        return 2;
    }
    /* Reference golden vector, independent of the generated frames. */
    CHECK(software_crc((const uint8_t *)"123456789", 9U) ==
          UINT32_C(0xcbf43926));
    size_t image_length = 0U;
    uint8_t *image = load_image(argv[1], &image_length);
    void *mapped = mmap((void *)(uintptr_t)MAP_ADDRESS, MAP_LENGTH,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                        -1, 0);
    if (mapped == MAP_FAILED) { perror("mmap"); free(image); return 2; }
    boot_am13e_test_first = MAP_ADDRESS + APP_OFFSET;
    boot_am13e_test_end = MAP_ADDRESS + MAP_LENGTH;
    memset((void *)(uintptr_t)boot_am13e_test_first, 0xff,
           (size_t)(boot_am13e_test_end - boot_am13e_test_first));
    /* Boot, original ESCape32 Cfg and the Reserved 4KiB must
     * survive arbitrary APP writes. Upper marker probes CMD_WINDOW.
     */
    memset((void *)(uintptr_t)MAP_ADDRESS,0x42,0x4000U);
    memset((void *)(uintptr_t)(MAP_ADDRESS+0x4000U),0x43,0x1000U);
    *(uint16_t *)(uintptr_t)(MAP_ADDRESS+0x4000U)=AM13E_BOOT_CFG_ID;
    memset((void *)(uintptr_t)(MAP_ADDRESS+0x5000U),0x44,0x1000U);
    memset((void *)(uintptr_t)(boot_am13e_test_first+256U*1024U),0xa5,16U);
    boot_am13e_test_reset_update_state();

    queue_protocol(image_length, image);
    if (setjmp(end_of_stream) == 0) {
        boot_am13e_protocol_entry();
        CHECK(false && "boot protocol unexpectedly returned");
    }

    CHECK(read_position == incoming_length);
    CHECK(transmitted_length == expected_length);
    if(memcmp(transmitted,expected_reply,expected_length)!=0) {
        for(size_t at=0U;at<expected_length;++at) {
            if(transmitted[at]!=expected_reply[at]) {
                fprintf(stderr,
                    "FAIL protocol byte %zu/%zu actual=%02x expected=%02x, image_length=%zu\n",
                    at,expected_length,transmitted[at],expected_reply[at],
                    image_length);
                break;
            }
        }
        CHECK(false && "framed Boot reply mismatch");
    }
    puts("PASS CMD_PROBE/INFO 32byte, complements and CRC32 wire framing");
    puts("PASS CMD_WINDOW0/1 and 488KiB APP physical bounds");
    puts("PASS original random/duplicate/short CMD_WRITE, no signature gate");
    CHECK(erases>=1U && programs>=1U);
    CHECK(memcmp((const void *)boot_am13e_test_first,image,
                 image_length)==0);
    uint32_t sp=0U,pc=0U;
    CHECK(boot_am13e_app_validity(
        (void *)(uintptr_t)(MAP_ADDRESS+0x4000U),
        (void *)boot_am13e_test_first,&sp,&pc));
    for(unsigned i=0;i<0x4000U;++i)
        CHECK(*((uint8_t *)(uintptr_t)(MAP_ADDRESS+i))==0x42U);
    for(unsigned i=2U;i<0x1000U;++i)
        CHECK(*((uint8_t *)(uintptr_t)(MAP_ADDRESS+0x4000U+i))==0x43U);
    for(unsigned i=0;i<0x1000U;++i)
        CHECK(*((uint8_t *)(uintptr_t)(MAP_ADDRESS+0x5000U+i))==0x44U);
    puts("PASS original Cfg.id + M33 vector enables launch, no image CRC");
    puts("PASS Boot/Config/Reserved preserved, linked-sized flat image");
    puts("PASS CMD_UPDATE rejects before erase; CMD_SETWRP modes reject safely");
    free(image);
    puts("PASS Stage D1 common ESCape32 Boot Protocol integration");
    return 0;
}
