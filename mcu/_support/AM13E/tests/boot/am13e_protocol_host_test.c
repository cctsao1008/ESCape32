/*
 * Stage D1: wire-level ESCape32 command framing through the real
 * boot/src/main.c dispatcher, boot/src/io.c framing helpers,
 * boot/mcu/AM13E/hal/flash_range.c and production flash.c state machine.
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
#include "common.h"
#include "dl_crcp.h"
#include "app_validity.h"
#include "update_staging.h"

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
extern bool boot_am13e_write_range(unsigned block, unsigned length, char **address);
extern int boot_am13e_flash_write(char *dst, const char *source, int length);

static uint8_t incoming[RX_CAPACITY];
static uint8_t transmitted[TX_CAPACITY];
static uint8_t expected_reply[TX_CAPACITY];
static size_t incoming_length, read_position, transmitted_length, expected_length;
static unsigned erases, programs;
static bool corrupt_readback;
static bool fail_next_erase, fail_next_program;
static bool receive_timeout;
static bool eligible_application;
extern bool boot_am13e_test_protected;
static uintptr_t logical_address(unsigned n) {
    return n < 4U ? MAP_ADDRESS + 0x4000U + n * BLOCK_BYTES :
        boot_am13e_test_first + (n - 4U) * BLOCK_BYTES;
}
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
static void queue_update_frame(const uint8_t *data,unsigned n,
                               bool bad_crc,int expected_ack)
{
    CHECK(data!=NULL&&n>=4U&&n<=BLOCK_BYTES&&(n&3U)==0U);
    input_val((uint8_t)(n/4U-1U));
    input_bytes(data,n);
    uint8_t crc[4];
    uint32_t check=software_crc(data,n);
    if(bad_crc)check^=1U;
    little_endian_32(check,crc);
    input_bytes(crc,4U);
    if(expected_ack>=0)expected_val((uint8_t)expected_ack);
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
    if (read_position == incoming_length) {
        if (receive_timeout || eligible_application) longjmp(end_of_stream, 1);
        receive_timeout = true;
        return 0; /* Let the parser reject the timed-out frame first. */
    }
    if ((size_t)len > incoming_length - read_position) {
        read_position = incoming_length; /* Truncated physical receive. */
        receive_timeout = true;
        return 0;
    }
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
/* Production boot/src/util.c supplies crc32; only CRCP is modeled. */
void init(void) {}
void initio(void) {}
bool boot_am13e_take_reboot_ack(void) { return false; }
uint32_t boot_am13e_device_id(void) { return UINT32_C(0x12345678); }
uint8_t boot_am13e_io_id(void) { return 4U; }
bool boot_am13e_application_valid(void) { return eligible_application; }
__attribute__((noreturn)) void boot_am13e_launch_application(void) {
    fprintf(stderr, "FAIL unexpected application jump during protocol test\n");
    exit(1);
}

/* Physical Flash model: 2 KiB sector erase, irreversible 1->0 program. */
uint32_t DL_Flash_eraseSector(uint32_t addr) {
    ++erases; /* Count every controller call, including illegal attempts. */
    if (fail_next_erase) { fail_next_erase = false; return DL_FLASH_ERROR; }
    if ((addr & (DL_FLASH_SECTOR_SIZE - 1U)) ||
        addr < MAP_ADDRESS + 0x4000U ||
        addr > boot_am13e_test_end - DL_FLASH_SECTOR_SIZE)
        return DL_FLASH_ERROR;
    memset((void *)(uintptr_t)addr, 0xff, DL_FLASH_SECTOR_SIZE);
    return DL_FLASH_SUCCESS;
}
uint32_t DL_Flash_program(uint32_t addr, uint8_t *src, uint32_t length) {
    ++programs;
    if (fail_next_program) { fail_next_program = false; return DL_FLASH_ERROR; }
    if (!src || !length || (addr & 15U) || (length & 15U) ||
        addr < MAP_ADDRESS + 0x4000U ||
        addr > boot_am13e_test_end - length)
        return DL_FLASH_ERROR;
    uint8_t *dst = (uint8_t *)(uintptr_t)addr;
    for (uint32_t i = 0U; i < length; ++i)
        if ((uint8_t)(dst[i] & src[i]) != src[i])
            return DL_FLASH_ERROR;
    for (uint32_t i = 0U; i < length; ++i) dst[i] &= src[i];
    if (corrupt_readback) {
        dst[0] ^= 1U;
        corrupt_readback = false;
    }
    return DL_FLASH_SUCCESS;
}

static void reset_stream(void) {
    incoming_length = read_position = transmitted_length = expected_length = 0U;
    receive_timeout = false;
}
static void run_stream(void) {
    if (setjmp(end_of_stream) == 0) {
        boot_am13e_protocol_entry();
        CHECK(false && "boot protocol unexpectedly returned");
    }
    CHECK(read_position == incoming_length);
    CHECK(transmitted_length == expected_length);
    for (size_t i = 0U; i < expected_length; ++i) {
        if (transmitted[i] != expected_reply[i]) {
            fprintf(stderr, "FAIL reply byte %zu: actual=%02x expected=%02x\n",
                    i, transmitted[i], expected_reply[i]);
            break;
        }
    }
    CHECK(memcmp(transmitted, expected_reply, expected_length) == 0);
}
static void rejected_stream(void) {
    const unsigned old_erases = erases, old_programs = programs;
    uint8_t snapshot[MAP_LENGTH];
    memcpy(snapshot, (const void *)(uintptr_t)MAP_ADDRESS, sizeof snapshot);
    run_stream();
    CHECK(erases == old_erases && programs == old_programs);
    CHECK(memcmp(snapshot, (const void *)(uintptr_t)MAP_ADDRESS,
                 sizeof snapshot) == 0);
}

/* R01: real dispatcher -> CRC framing -> range -> original writer body.
 * Each short write starts with non-erased bytes in BOTH sector halves.
 */
static void short_write_regressions(void) {
    const unsigned blocks[] = {0U, 1U, 2U, 3U, 4U, 5U, 6U, 256U, 260U, 491U};
    const uint8_t payloads[2][8] = {
        {0x12U, 0x34U, 0x56U, 0x78U, 0x9aU, 0xbcU, 0xdeU, 0xf0U},
        {0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU}
    };
    for (unsigned k = 0U; k < sizeof blocks / sizeof blocks[0]; ++k) {
        const unsigned block = blocks[k];
        uint8_t *dst = (uint8_t *)logical_address(block);
        uint8_t *sector = (uint8_t *)((uintptr_t)dst &
                                    ~((uintptr_t)DL_FLASH_SECTOR_SIZE - 1U));
        const size_t offset = (size_t)(dst - sector);
        for (unsigned p = 0U; p < 2U; ++p) {
            uint8_t expected_sector[DL_FLASH_SECTOR_SIZE];
            memset(sector, 0x5a, DL_FLASH_SECTOR_SIZE);
            memset(sector + (offset ? 0U : BLOCK_BYTES), 0xc3, BLOCK_BYTES);
            memcpy(expected_sector, sector, sizeof expected_sector);
            memcpy(expected_sector + offset, payloads[p], 8U);
            const unsigned old_erases = erases, old_programs = programs;
            reset_stream();
            queue_window(block >> 8U, RES_OK);
            queue_write(block & 255U, payloads[p], 8U, false, RES_OK);
            queue_read(block & 255U, 8U, payloads[p]);
            /* Illegal window must leave the selected window unchanged. */
            queue_window(2U, RES_ERROR);
            queue_write(block & 255U, payloads[p], 8U, false, RES_OK);
            queue_read(block & 255U, 8U, payloads[p]);
            /* Transition back to window0 and check its addressing. */
            queue_window(0U, RES_OK);
            queue_read(0U, 8U, block == 0U ? payloads[p] :
                       (const uint8_t *)logical_address(0U));
            run_stream();
            CHECK(erases == old_erases + 1U && programs == old_programs + 1U);
            CHECK(memcmp(sector, expected_sector, sizeof expected_sector) == 0);
            printf("PASS R01 block %u eight-byte %s write/read/retry\n",
                   block, p ? "FF" : "non-FF");
        }
    }
    puts("PASS R01 CFG 0..3 and APP 4/5/6/256/260/491: FF/non-FF, windows, sector preservation, read/retry");

    /* A successful controller status with corrupted data must NAK,
     * then a clean retry must repair and verify the sector.
     */
    uint8_t *dst = (uint8_t *)(boot_am13e_test_first + 2U * BLOCK_BYTES);
    uint8_t expected_sector[DL_FLASH_SECTOR_SIZE];
    memset(dst, 0x5a, sizeof expected_sector);
    memcpy(expected_sector, dst, sizeof expected_sector);
    memcpy(expected_sector, payloads[0], 8U);
    reset_stream();
    corrupt_readback = true;
    queue_write(6U, payloads[0], 8U, false, RES_ERROR);
    run_stream();
    CHECK(dst[0] != payloads[0][0]);
    reset_stream();
    queue_write(6U, payloads[0], 8U, false, RES_OK);
    queue_read(6U, 8U, payloads[0]);
    run_stream();
    CHECK(memcmp(dst, expected_sector, sizeof expected_sector) == 0);
    puts("PASS R01 readback corruption NAK and verified repair retry");

    for (unsigned block = 0U; block < 4U; ++block) {
        uint8_t *target = (uint8_t *)logical_address(block);
        uint8_t *sector = (uint8_t *)((uintptr_t)target &
            ~((uintptr_t)DL_FLASH_SECTOR_SIZE - 1U));
        for (unsigned failure = 0U; failure < 3U; ++failure) {
            memset(sector, 0x5a, DL_FLASH_SECTOR_SIZE);
            uint8_t recovered_sector[DL_FLASH_SECTOR_SIZE];
            memcpy(recovered_sector, sector, sizeof recovered_sector);
            memcpy(recovered_sector + (target-sector), payloads[0], 8U);
            reset_stream();
            fail_next_erase = failure == 0U;
            fail_next_program = failure == 1U;
            corrupt_readback = failure == 2U;
            queue_write(block, payloads[0], 8U, false, RES_ERROR);
            run_stream();
            reset_stream();
            queue_write(block, payloads[0], 8U, false, RES_OK);
            queue_read(block, 8U, payloads[0]);
            run_stream();
            CHECK(memcmp(target, payloads[0], 8U) == 0);
            CHECK(memcmp(sector, recovered_sector, sizeof recovered_sector) == 0);
            const unsigned verified_erases=erases,verified_programs=programs;
            reset_stream();
            queue_write(block, payloads[0], 8U, false, RES_OK);
            run_stream();
            CHECK(erases==verified_erases && programs==verified_programs);
        }
    }
    puts("PASS Config blocks0..3 erase/program/readback failures NAK and repair retry");

    for (unsigned block = 492U; block < 512U; ++block) {
        reset_stream();
        queue_window(1U, RES_OK);
        queue_write(block & 255U, payloads[0], 8U, false, RES_ERROR);
        rejected_stream();
    }
    for (unsigned block = 492U; block < 512U; ++block) {
        reset_stream();
        queue_window(1U, RES_OK);
        input_val(CMD_READ); input_val((uint8_t)(block & 255U)); input_val(255U);
        expected_val(RES_ERROR);
        input_val(CMD_PROBE); expected_val(RES_OK);
        eligible_application = true;
        rejected_stream();
        eligible_application = false;
    }
    boot_am13e_test_protected = true;
    for (unsigned block = 0U; block < 492U; ++block) {
        reset_stream();
        queue_window(block >> 8U, RES_OK);
        queue_write(block & 255U, payloads[0], 8U, false, RES_ERROR);
        rejected_stream();
    }
    boot_am13e_test_protected = false;
    puts("PASS invalid READ NAK/stay Boot with eligible App; all 492 protected writes: zero Flash calls");
    reset_stream();
    queue_window(1U, RES_OK);
    queue_write(235U, payloads[0], 8U, true, -1);
    input_val(CMD_PROBE); expected_val(RES_OK);
    rejected_stream();
    reset_stream();
    input_val(CMD_WRITE); input_val(2U);
    const uint8_t bad_count[2] = {0xffU, 0xffU};
    input_bytes(bad_count, sizeof bad_count);
    input_val(CMD_PROBE); expected_val(RES_OK);
    rejected_stream();
    /* Empty payload, partial payload and partial CRC after a valid8B count. */
    for (unsigned bytes = 0U; bytes <= 11U; ++bytes) {
        const uint8_t truncated[11] = {0U};
        reset_stream();
        input_val(CMD_WRITE); input_val(2U); input_val(1U);
        input_bytes(truncated, bytes);
        rejected_stream();
    }
    puts("PASS R01 blocks 492..511, CRC/count/empty/truncated frames: no Flash calls");

    /* Wire count encodes ONLY 4..1024 bytes; zero/nonmultiple/oversize
     * lengths and arbitrary pointers cannot be represented by CMD_WRITE.
     * Exercise the actual range and writer guards directly for those cases.
     */
    const unsigned bad_lengths[] = {0U, 1U, 3U, 7U, 1025U, 1028U, UINT32_MAX};
    const unsigned old_erases = erases, old_programs = programs;
    uint8_t snapshot[MAP_LENGTH];
    memcpy(snapshot, (const void *)(uintptr_t)MAP_ADDRESS, sizeof snapshot);
    char *address = NULL;
    for (unsigned i = 0U; i < sizeof bad_lengths / sizeof bad_lengths[0]; ++i) {
        CHECK(!boot_am13e_write_range(491U, bad_lengths[i], &address));
        if (bad_lengths[i] != UINT32_MAX)
            CHECK(!boot_am13e_flash_write((char *)dst,
                  (const char *)payloads[0], (int)bad_lengths[i]));
    }
    CHECK(!boot_am13e_flash_write((char *)dst, (const char *)payloads[0], -1));
    CHECK(!boot_am13e_write_range(2U, 8U, NULL));
    CHECK(!boot_am13e_write_range(UINT32_MAX, 8U, &address));
    CHECK(!boot_am13e_flash_write((char *)dst, NULL, 8));
    const uintptr_t illegal[] = {
        MAP_ADDRESS, MAP_ADDRESS + 0x3c00U, MAP_ADDRESS + 0x5000U,
        boot_am13e_test_first - 16U, boot_am13e_test_first + 1U,
        boot_am13e_test_first + 16U, boot_am13e_test_end - 16U,
        boot_am13e_test_end, boot_am13e_test_end + 16U
    };
    for (unsigned i = 0U; i < sizeof illegal / sizeof illegal[0]; ++i)
        CHECK(!boot_am13e_flash_write((char *)illegal[i],
                                      (const char *)payloads[0], 8));
    CHECK(!boot_am13e_flash_write((char *)(boot_am13e_test_end - 16U),
                                  (const char *)payloads[0], 32));
    /* Test end-overflow and physical-sector containment independently
     * of logical-block/alignment failures using narrowed mock bounds.
     */
    const uintptr_t saved_end = boot_am13e_test_end;
    boot_am13e_test_end = boot_am13e_test_first + 487U * BLOCK_BYTES + 4U;
    CHECK(!boot_am13e_write_range(491U, 8U, &address));
    CHECK(!boot_am13e_flash_write((char *)(boot_am13e_test_first + 487U * BLOCK_BYTES),
                                  (const char *)payloads[0], 8));
    boot_am13e_test_end = boot_am13e_test_first + BLOCK_BYTES;
    CHECK(boot_am13e_write_range(4U, 8U, &address));
    CHECK(!boot_am13e_flash_write(address, (const char *)payloads[0], 8));
    boot_am13e_test_end = saved_end;
    const uintptr_t saved_first = boot_am13e_test_first;
    boot_am13e_test_first += 1U;
    CHECK(!boot_am13e_write_range(4U, 8U, &address));
    boot_am13e_test_first = saved_first;
    CHECK(erases == old_erases && programs == old_programs);
    CHECK(memcmp(snapshot, (const void *)(uintptr_t)MAP_ADDRESS, sizeof snapshot) == 0);
    puts("PASS R01 length/address/alignment/end/sector guards: Boot/Reserved unchanged, no Flash calls");
    reset_stream();
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
    queue_read(235U,16U,erased16); /* Last valid logical block491 */
    queue_window(2U,RES_ERROR);    /* Remain window1 */
    queue_read(0U,16U,probe_upper);
    queue_write(236U,synthetic,16U,false,RES_ERROR);
    input_val(CMD_READ);input_val(236U);input_val(3U);expected_val(RES_ERROR);
    input_val(CMD_PROBE);expected_val(RES_OK);
    queue_window(0U,RES_OK);
    queue_read(4U,16U,erased16);

    /* Original Rel17 CMD_WRITE is not a new all-image state machine:
     * arbitrary APP 1KiB blocks are legal, independent of prior
     * CMD_WINDOW selection and of firmware linked size.
     */
    queue_write(6U,synthetic,16U,false,RES_OK);
    queue_write(6U,synthetic,16U,false,RES_OK);
    queue_read(6U,16U,synthetic);
    queue_write(6U,synthetic,16U,true,-1); /* Bad wire CRC: no Flash */
    queue_read(6U,16U,synthetic);

    /* Old host's invalidation frames are still ordinary FF writes. */
    queue_write(4U,erased,8U,false,RES_OK);
    queue_write(5U,erased,8U,false,RES_OK);

    unsigned window=0U;
    for(size_t offset=0U;offset<image_bytes;offset+=BLOCK_BYTES){
        const unsigned absolute=4U+(unsigned)(offset/BLOCK_BYTES);
        if((absolute>>8U)!=window){
            window=absolute>>8U;
            queue_window(window,RES_OK);
        }
        unsigned n=(unsigned)((image_bytes-offset>BLOCK_BYTES)?
                               BLOCK_BYTES:image_bytes-offset);
        queue_write(absolute&255U,image+offset,n,false,RES_OK);
        if(absolute==4U)queue_write(4U,image,n,false,RES_OK);
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
    queue_read(4U,32U,first32);
    /* Original CRC frames and per-block ACKs now reach a bounded SRAM
     * stage. Always reject final Boot update: Bank0 erase is DISABLED.
     */
    uint8_t boot_image[AM13E_BOOT_STAGE_BYTES];
    memset(boot_image,0xa6,sizeof boot_image);
    little_endian_32(UINT32_C(0x20018000),boot_image);
    little_endian_32(UINT32_C(0x00000101),boot_image+4);
    input_val(CMD_UPDATE);
    queue_update_frame(boot_image,4U,false,RES_OK);
    expected_val(RES_ERROR);
    input_val(CMD_PROBE);expected_val(RES_OK);
    input_val(CMD_UPDATE);
    for(unsigned k=0;k<AM13E_BOOT_STAGE_BLOCKS;k++)
        queue_update_frame(boot_image+k*BLOCK_BYTES,BLOCK_BYTES,false,RES_OK);
    expected_val(RES_ERROR);
    input_val(CMD_PROBE);expected_val(RES_OK);
    input_val(CMD_UPDATE);
    queue_update_frame(boot_image,1024U,false,RES_OK);
    queue_update_frame(boot_image+1024U,16U,true,-1);
    input_val(CMD_PROBE);expected_val(RES_OK);
    input_val(CMD_UPDATE);
    const uint8_t bad_count[2]={0x03U,0x03U};
    input_bytes(bad_count,sizeof bad_count);
    input_val(CMD_PROBE);expected_val(RES_OK);
    /* Reject all three upstream WRP modes and an invalid selector. */
    input_val(CMD_SETWRP);input_val(0x33U);expected_val(RES_ERROR);
    input_val(CMD_SETWRP);input_val(0x44U);expected_val(RES_ERROR);
    input_val(CMD_SETWRP);input_val(0x55U);expected_val(RES_ERROR);
    input_val(CMD_SETWRP);input_val(0x77U);expected_val(RES_ERROR);
    input_val(CMD_PROBE);expected_val(RES_OK);
}
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s path/to/AM13E-rev17.3.bin\n", argv[0]);
        return 2;
    }
    /* Reference golden vector, independent of the generated frames. */
    CHECK(software_crc((const uint8_t *)"123456789", 9U) ==
          UINT32_C(0xcbf43926));
    CHECK(crc32("1234",4)==UINT32_C(0x9be3e0a3));
    CHECK(crc32("12345678",8)==UINT32_C(0x9ae0daaf));
    CHECK(crc32("",0)==0U);
    const uint32_t crc_inits=am13e_test_crcp.initializations;
    CHECK(crc32(NULL,4)==0U && crc32("1234",-4)==0U &&
          crc32("123456789",9)==0U);
    CHECK(am13e_test_crcp.initializations==crc_inits);
    uint8_t crc_vector[1024];
    for(unsigned i=0;i<sizeof crc_vector;++i)crc_vector[i]=(uint8_t)i;
    for(unsigned n=4U;n<=sizeof crc_vector;n+=4U)
        CHECK(crc32((const char *)crc_vector,(int)n)==software_crc(crc_vector,n));
    size_t image_length = 0U;
    uint8_t *image = load_image(argv[1], &image_length);
    void *mapped = mmap((void *)(uintptr_t)MAP_ADDRESS, MAP_LENGTH,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                        -1, 0);
    if (mapped == MAP_FAILED) { perror("mmap"); free(image); return 2; }
    boot_am13e_test_first = MAP_ADDRESS + APP_OFFSET;
    boot_am13e_test_end = MAP_ADDRESS + MAP_LENGTH;
    memset(mapped, 0xff, MAP_LENGTH);
    short_write_regressions();
    memset((void *)(uintptr_t)boot_am13e_test_first, 0xff,
           (size_t)(boot_am13e_test_end - boot_am13e_test_first));
    /* Boot, original ESCape32 Cfg and the Reserved 4KiB must
     * survive arbitrary APP writes. Upper marker probes CMD_WINDOW.
     */
    memset((void *)(uintptr_t)MAP_ADDRESS,0x42,0x4000U);
    memset((void *)(uintptr_t)(MAP_ADDRESS+0x4000U),0x43,0x1000U);
    *(uint16_t *)(uintptr_t)(MAP_ADDRESS+0x4000U)=AM13E_BOOT_CFG_ID;
    memset((void *)(uintptr_t)(MAP_ADDRESS+0x5000U),0x44,0x1000U);
    memset((void *)(uintptr_t)(logical_address(256U)),0xa5,16U);
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
    puts("PASS CMD_WINDOW0/1: 4KiB Config prefix + max488KiB APP, Reserved skipped");
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
    CHECK(!boot_am13e_stage_complete());
    CHECK(boot_am13e_stage_length()==0U);
    puts("PASS CMD_UPDATE 16KiB/short/CRC ACK and final NAK; no Bank0 Flash");
    puts("PASS CMD_SETWRP modes reject safely");
    free(image);
    puts("PASS Stage D1 common ESCape32 Boot Protocol integration");
    return 0;
}
