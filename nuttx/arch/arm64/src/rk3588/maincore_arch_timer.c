#include <nuttx/config.h>
#include <debug.h>
#include <assert.h>
#include <stdio.h>

#include <nuttx/arch.h>
#include <arch/irq.h>
#include <arch/chip/chip.h>
#include <nuttx/spinlock.h>
#include <nuttx/timers/arch_alarm.h>

#include "../common/arm64_arch.h"
#include "../common/arm64_internal.h"
#include "../common/arm64_gic.h"
#include "../common/arm64_arch_timer.h"

#include "jihuoma.h"

#define CONFIG_ARM_TIMER_VIRTUAL_IRQ        (GIC_PPI_INT_BASE + 11)
#define ARM_ARCH_TIMER_IRQ     CONFIG_ARM_TIMER_VIRTUAL_IRQ

void maincore_arm64_arch_timer_set_compare(uint64_t value)
{
  write_sysreg(value, cntv_cval_el0);
}

uint64_t maincore_arm64_arch_timer_get_compare(void)
{
  return read_sysreg(cntv_cval_el0);
}

void maincore_arm64_arch_timer_enable(bool enable)
{
  uint64_t value;

  value = read_sysreg(cntv_ctl_el0);

  if (enable)
    {
      value |= CNTV_CTL_ENABLE_BIT;
    }
  else
    {
      value &= ~CNTV_CTL_ENABLE_BIT;
    }

  write_sysreg(value, cntv_ctl_el0);
}

void maincore_arm64_arch_timer_set_irq_mask(bool mask)
{
  uint64_t value;

  value = read_sysreg(cntv_ctl_el0);

  if (mask)
    {
      value |= CNTV_CTL_IMASK_BIT;
    }
  else
    {
      value &= ~CNTV_CTL_IMASK_BIT;
    }

  write_sysreg(value, cntv_ctl_el0);
}

uint64_t maincore_arm64_arch_timer_count(void)
{
  return read_sysreg(cntvct_el0);
}

uint64_t maincore_arm64_arch_timer_get_cntfrq(void)
{
  return read_sysreg(cntfrq_el0);
}

static int maincore_arm64_arch_timer_compare_isr(int irq, void *regs, void *arg)
{
  maincore_arm64_arch_timer_set_irq_mask(true);

  return OK;
}

void maincore_arm64_arch_timer_init(void)
{
	irq_attach(ARM_ARCH_TIMER_IRQ,
             maincore_arm64_arch_timer_compare_isr, NULL);
             
	up_disable_irq(ARM_ARCH_TIMER_IRQ);
  
	maincore_arm64_arch_timer_enable(true);
    
	maincore_arm64_arch_timer_set_compare(0xffffffffffffffffULL);
  
	maincore_arm64_arch_timer_set_irq_mask(false);
}

#if (ENABLE_JIHUOMA > 0)	

#define ECB 1

#define AES128 1

#define AES_KEYLEN 16   // Key length in bytes
#define AES_keyExpSize 176

struct AES_ctx
{
  uint8_t RoundKey[AES_keyExpSize];
};

#define Nb 4

#define Nk 4        
#define Nr 10

typedef uint8_t state_t[4][4];

static const uint8_t sbox[256] = {
  //0     1    2      3     4    5     6     7      8    9     A      B    C     D     E     F
  0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
  0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
  0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
  0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
  0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
  0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
  0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
  0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
  0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
  0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
  0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
  0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
  0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
  0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
  0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
  0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16 };
  
static const uint8_t rsbox[256] = {
  0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
  0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
  0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
  0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
  0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
  0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
  0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
  0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
  0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
  0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
  0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
  0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
  0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
  0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
  0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
  0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d };
  
static const uint8_t Rcon[11] = {
  0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36 };
  
#define getSBoxValue(num) (sbox[(num)])

static void KeyExpansion(uint8_t* RoundKey, const uint8_t* Key)
{
  unsigned i, j, k;
  uint8_t tempa[4]; // Used for the column/row operations
  
  // The first round key is the key itself.
  for (i = 0; i < Nk; ++i)
  {
    RoundKey[(i * 4) + 0] = Key[(i * 4) + 0];
    RoundKey[(i * 4) + 1] = Key[(i * 4) + 1];
    RoundKey[(i * 4) + 2] = Key[(i * 4) + 2];
    RoundKey[(i * 4) + 3] = Key[(i * 4) + 3];
  }

  // All other round keys are found from the previous round keys.
  for (i = Nk; i < Nb * (Nr + 1); ++i)
  {
    {
      k = (i - 1) * 4;
      tempa[0]=RoundKey[k + 0];
      tempa[1]=RoundKey[k + 1];
      tempa[2]=RoundKey[k + 2];
      tempa[3]=RoundKey[k + 3];

    }

    if (i % Nk == 0)
    {
      // This function shifts the 4 bytes in a word to the left once.
      // [a0,a1,a2,a3] becomes [a1,a2,a3,a0]

      // Function RotWord()
      {
        const uint8_t u8tmp = tempa[0];
        tempa[0] = tempa[1];
        tempa[1] = tempa[2];
        tempa[2] = tempa[3];
        tempa[3] = u8tmp;
      }

      // SubWord() is a function that takes a four-byte input word and 
      // applies the S-box to each of the four bytes to produce an output word.

      // Function Subword()
      {
        tempa[0] = getSBoxValue(tempa[0]);
        tempa[1] = getSBoxValue(tempa[1]);
        tempa[2] = getSBoxValue(tempa[2]);
        tempa[3] = getSBoxValue(tempa[3]);
      }

      tempa[0] = tempa[0] ^ Rcon[i/Nk];
    }

    j = i * 4; k=(i - Nk) * 4;
    RoundKey[j + 0] = RoundKey[k + 0] ^ tempa[0];
    RoundKey[j + 1] = RoundKey[k + 1] ^ tempa[1];
    RoundKey[j + 2] = RoundKey[k + 2] ^ tempa[2];
    RoundKey[j + 3] = RoundKey[k + 3] ^ tempa[3];
  }
}

static void AES_init_ctx(struct AES_ctx* ctx, const uint8_t* key)
{
  KeyExpansion(ctx->RoundKey, key);
}

static void AddRoundKey(uint8_t round, state_t* state, const uint8_t* RoundKey)
{
  uint8_t i,j;
  for (i = 0; i < 4; ++i)
  {
    for (j = 0; j < 4; ++j)
    {
      (*state)[i][j] ^= RoundKey[(round * Nb * 4) + (i * Nb) + j];
    }
  }
}

static void SubBytes(state_t* state)
{
  uint8_t i, j;
  for (i = 0; i < 4; ++i)
  {
    for (j = 0; j < 4; ++j)
    {
      (*state)[j][i] = getSBoxValue((*state)[j][i]);
    }
  }
}

static void ShiftRows(state_t* state)
{
  uint8_t temp;

  // Rotate first row 1 columns to left  
  temp           = (*state)[0][1];
  (*state)[0][1] = (*state)[1][1];
  (*state)[1][1] = (*state)[2][1];
  (*state)[2][1] = (*state)[3][1];
  (*state)[3][1] = temp;

  // Rotate second row 2 columns to left  
  temp           = (*state)[0][2];
  (*state)[0][2] = (*state)[2][2];
  (*state)[2][2] = temp;

  temp           = (*state)[1][2];
  (*state)[1][2] = (*state)[3][2];
  (*state)[3][2] = temp;

  // Rotate third row 3 columns to left
  temp           = (*state)[0][3];
  (*state)[0][3] = (*state)[3][3];
  (*state)[3][3] = (*state)[2][3];
  (*state)[2][3] = (*state)[1][3];
  (*state)[1][3] = temp;
}

static uint8_t xtime(uint8_t x)
{
  return ((x<<1) ^ (((x>>7) & 1) * 0x1b));
}

static void MixColumns(state_t* state)
{
  uint8_t i;
  uint8_t Tmp, Tm, t;
  for (i = 0; i < 4; ++i)
  {  
    t   = (*state)[i][0];
    Tmp = (*state)[i][0] ^ (*state)[i][1] ^ (*state)[i][2] ^ (*state)[i][3] ;
    Tm  = (*state)[i][0] ^ (*state)[i][1] ; Tm = xtime(Tm);  (*state)[i][0] ^= Tm ^ Tmp ;
    Tm  = (*state)[i][1] ^ (*state)[i][2] ; Tm = xtime(Tm);  (*state)[i][1] ^= Tm ^ Tmp ;
    Tm  = (*state)[i][2] ^ (*state)[i][3] ; Tm = xtime(Tm);  (*state)[i][2] ^= Tm ^ Tmp ;
    Tm  = (*state)[i][3] ^ t ;              Tm = xtime(Tm);  (*state)[i][3] ^= Tm ^ Tmp ;
  }
}

#define Multiply(x, y)                                \
      (  ((y & 1) * x) ^                              \
      ((y>>1 & 1) * xtime(x)) ^                       \
      ((y>>2 & 1) * xtime(xtime(x))) ^                \
      ((y>>3 & 1) * xtime(xtime(xtime(x)))) ^         \
      ((y>>4 & 1) * xtime(xtime(xtime(xtime(x))))))   \

#define getSBoxInvert(num) (rsbox[(num)])

static void InvMixColumns(state_t* state)
{
  int i;
  uint8_t a, b, c, d;
  for (i = 0; i < 4; ++i)
  { 
    a = (*state)[i][0];
    b = (*state)[i][1];
    c = (*state)[i][2];
    d = (*state)[i][3];

    (*state)[i][0] = Multiply(a, 0x0e) ^ Multiply(b, 0x0b) ^ Multiply(c, 0x0d) ^ Multiply(d, 0x09);
    (*state)[i][1] = Multiply(a, 0x09) ^ Multiply(b, 0x0e) ^ Multiply(c, 0x0b) ^ Multiply(d, 0x0d);
    (*state)[i][2] = Multiply(a, 0x0d) ^ Multiply(b, 0x09) ^ Multiply(c, 0x0e) ^ Multiply(d, 0x0b);
    (*state)[i][3] = Multiply(a, 0x0b) ^ Multiply(b, 0x0d) ^ Multiply(c, 0x09) ^ Multiply(d, 0x0e);
  }
}

static void InvSubBytes(state_t* state)
{
  uint8_t i, j;
  for (i = 0; i < 4; ++i)
  {
    for (j = 0; j < 4; ++j)
    {
      (*state)[j][i] = getSBoxInvert((*state)[j][i]);
    }
  }
}

static void InvShiftRows(state_t* state)
{
  uint8_t temp;

  // Rotate first row 1 columns to right  
  temp = (*state)[3][1];
  (*state)[3][1] = (*state)[2][1];
  (*state)[2][1] = (*state)[1][1];
  (*state)[1][1] = (*state)[0][1];
  (*state)[0][1] = temp;

  // Rotate second row 2 columns to right 
  temp = (*state)[0][2];
  (*state)[0][2] = (*state)[2][2];
  (*state)[2][2] = temp;

  temp = (*state)[1][2];
  (*state)[1][2] = (*state)[3][2];
  (*state)[3][2] = temp;

  // Rotate third row 3 columns to right
  temp = (*state)[0][3];
  (*state)[0][3] = (*state)[1][3];
  (*state)[1][3] = (*state)[2][3];
  (*state)[2][3] = (*state)[3][3];
  (*state)[3][3] = temp;
}

static void Cipher(state_t* state, const uint8_t* RoundKey)
{
  uint8_t round = 0;

  // Add the First round key to the state before starting the rounds.
  AddRoundKey(0, state, RoundKey);

  // There will be Nr rounds.
  // The first Nr-1 rounds are identical.
  // These Nr rounds are executed in the loop below.
  // Last one without MixColumns()
  for (round = 1; ; ++round)
  {
    SubBytes(state);
    ShiftRows(state);
    if (round == Nr) {
      break;
    }
    MixColumns(state);
    AddRoundKey(round, state, RoundKey);
  }
  // Add round key to last round
  AddRoundKey(Nr, state, RoundKey);
}

static void InvCipher(state_t* state, const uint8_t* RoundKey)
{
  uint8_t round = 0;

  // Add the First round key to the state before starting the rounds.
  AddRoundKey(Nr, state, RoundKey);

  // There will be Nr rounds.
  // The first Nr-1 rounds are identical.
  // These Nr rounds are executed in the loop below.
  // Last one without InvMixColumn()
  for (round = (Nr - 1); ; --round)
  {
    InvShiftRows(state);
    InvSubBytes(state);
    AddRoundKey(round, state, RoundKey);
    if (round == 0) {
      break;
    }
    InvMixColumns(state);
  }

}

static void AES_ECB_encrypt(const struct AES_ctx* ctx, uint8_t* buf)
{
  // The next function call encrypts the PlainText with the Key using AES algorithm.
  Cipher((state_t*)buf, ctx->RoundKey);
}

static void AES_ECB_decrypt(const struct AES_ctx* ctx, uint8_t* buf)
{
  // The next function call decrypts the PlainText with the Key using AES algorithm.
  InvCipher((state_t*)buf, ctx->RoundKey);
}

struct AES_ctx g_RTOSStrcut;
uint8_t g_RTOSText[16] = {0};

static void prepare_soemthing(const uint8_t* key)
{
	AES_init_ctx(&g_RTOSStrcut, key);
}

static void __attribute__((unused)) before_soemthing(uint8_t* buf)
{
	AES_ECB_encrypt(&g_RTOSStrcut, buf);
}

static void after_soemthing(uint8_t* buf)
{
	AES_ECB_decrypt(&g_RTOSStrcut, buf);
}

extern int rtos_printf(const char *fmt, ...);

void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        rtos_printf("%02x ", data[i]);
    }
    rtos_printf("\n");
}

#define USER_SHARED_BUF_FIXVAR_SIZE 1024
#define OFFSET_TO_RTONBOOT_SHARE_BUFFER 0xc00000

uint32_t do_soemthing(void)
{
	uint8_t key[16] = {
        0x31, 0x39, 0x37, 0x31, 0x30, 0x33, 0x30, 0x32,
        0x31, 0x39, 0x37, 0x38, 0x31, 0x31, 0x32, 0x34
    };
    uint8_t plaintext[16] = {0};
    uint8_t * ptr_ciper_serial = (uint8_t *)( ((unsigned long long)CONFIG_RAM_START) + OFFSET_TO_RTONBOOT_SHARE_BUFFER 
		+ USER_SHARED_BUF_FIXVAR_SIZE - 16);
	uint32_t value;	 
    
	prepare_soemthing(key);
    
    memcpy(plaintext, ptr_ciper_serial, sizeof(plaintext));
    
    /*print_hex(plaintext, sizeof(plaintext));*/
    
    after_soemthing(plaintext);
		
	value = *((uint32_t *)(&plaintext[0]));
	
	return value;		
}

uint8_t g_soemThing[32];	

typedef struct {
	// Do not rely on the size or contents of this type,
	// for they may change without notice.
	uint64_t hash[8];
	uint64_t input_offset[2];
	uint64_t input[16];
	size_t   input_idx;
	size_t   hash_size;
} crypto_blake2b_ctx;

typedef int8_t   i8;
typedef uint8_t  u8;
typedef int16_t  i16;
typedef uint32_t u32;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint64_t u64;

#define FOR_T(type, i, start, end) for (type i = (start); i < (end); i++)
#define FOR(i, start, end)         FOR_T(size_t, i, start, end)
#define COPY(dst, src, size)       FOR(_i_, 0, size) (dst)[_i_] = (src)[_i_]
#define ZERO(buf, size)            FOR(_i_, 0, size) (buf)[_i_] = 0

static const u64 iv[8] = {
	0x6a09e667f3bcc908, 0xbb67ae8584caa73b,
	0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1,
	0x510e527fade682d1, 0x9b05688c2b3e6c1f,
	0x1f83d9abfb41bd6b, 0x5be0cd19137e2179,
};

static u32 load32_le(const u8 s[4])
{
	return
		((u32)s[0] <<  0) |
		((u32)s[1] <<  8) |
		((u32)s[2] << 16) |
		((u32)s[3] << 24);
}

static u64 load64_le(const u8 s[8])
{
	return load32_le(s) | ((u64)load32_le(s+4) << 32);
}

static void load64_le_buf (u64 *dst, const u8 *src, size_t size) {
	FOR(i, 0, size) { dst[i] = load64_le(src + i*8); }
}

static size_t gap(size_t x, size_t pow_2)
{
	return (~x + 1) & (pow_2 - 1);
}

static void store32_le(u8 out[4], u32 in)
{
	out[0] =  in        & 0xff;
	out[1] = (in >>  8) & 0xff;
	out[2] = (in >> 16) & 0xff;
	out[3] = (in >> 24) & 0xff;
}

static void store64_le(u8 out[8], u64 in)
{
	store32_le(out    , (u32)in );
	store32_le(out + 4, in >> 32);
}

static void store64_le_buf(u8 *dst, const u64 *src, size_t size) {
	FOR(i, 0, size) { store64_le(dst + i*8, src[i]); }
}

static u64 rotr64(u64 x, u64 n) { return (x >> n) ^ (x << (64 - n)); }

static void crypto_wipe(void *secret, size_t size)
{
	volatile u8 *v_secret = (u8*)secret;
	ZERO(v_secret, size);
}

#define WIPE_CTX(ctx)              crypto_wipe(ctx   , sizeof(*(ctx)))

void crypto_blake2b_keyed_init(crypto_blake2b_ctx *ctx, size_t hash_size,
                               const u8 *key, size_t key_size)
{
	// initial hash
	COPY(ctx->hash, iv, 8);
	ctx->hash[0] ^= 0x01010000 ^ (key_size << 8) ^ hash_size;

	ctx->input_offset[0] = 0;  // beginning of the input, no offset
	ctx->input_offset[1] = 0;  // beginning of the input, no offset
	ctx->hash_size       = hash_size;
	ctx->input_idx       = 0;
	ZERO(ctx->input, 16);

	// if there is a key, the first block is that key (padded with zeroes)
	if (key_size > 0) {
		u8 key_block[128] = {0};
		COPY(key_block, key, key_size);
		// same as calling crypto_blake2b_update(ctx, key_block , 128)
		load64_le_buf(ctx->input, key_block, 16);
		ctx->input_idx = 128;
	}
}

static void blake2b_compress(crypto_blake2b_ctx *ctx, int is_last_block)
{
	static const u8 sigma[12][16] = {
		{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
		{ 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },
		{ 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 },
		{  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 },
		{  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 },
		{  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 },
		{ 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 },
		{ 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 },
		{  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 },
		{ 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0 },
		{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
		{ 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },
	};

	// increment input offset
	u64   *x = ctx->input_offset;
	size_t y = ctx->input_idx;
	x[0] += y;
	if (x[0] < y) {
		x[1]++;
	}

	// init work vector
	u64 v0 = ctx->hash[0];  u64 v8  = iv[0];
	u64 v1 = ctx->hash[1];  u64 v9  = iv[1];
	u64 v2 = ctx->hash[2];  u64 v10 = iv[2];
	u64 v3 = ctx->hash[3];  u64 v11 = iv[3];
	u64 v4 = ctx->hash[4];  u64 v12 = iv[4] ^ ctx->input_offset[0];
	u64 v5 = ctx->hash[5];  u64 v13 = iv[5] ^ ctx->input_offset[1];
	u64 v6 = ctx->hash[6];  u64 v14 = iv[6] ^ (u64)~(is_last_block - 1);
	u64 v7 = ctx->hash[7];  u64 v15 = iv[7];

	// mangle work vector
	u64 *input = ctx->input;
#define BLAKE2_G(a, b, c, d, x, y)	\
	a += b + x;  d = rotr64(d ^ a, 32); \
	c += d;      b = rotr64(b ^ c, 24); \
	a += b + y;  d = rotr64(d ^ a, 16); \
	c += d;      b = rotr64(b ^ c, 63)
#define BLAKE2_ROUND(i)	\
	BLAKE2_G(v0, v4, v8 , v12, input[sigma[i][ 0]], input[sigma[i][ 1]]); \
	BLAKE2_G(v1, v5, v9 , v13, input[sigma[i][ 2]], input[sigma[i][ 3]]); \
	BLAKE2_G(v2, v6, v10, v14, input[sigma[i][ 4]], input[sigma[i][ 5]]); \
	BLAKE2_G(v3, v7, v11, v15, input[sigma[i][ 6]], input[sigma[i][ 7]]); \
	BLAKE2_G(v0, v5, v10, v15, input[sigma[i][ 8]], input[sigma[i][ 9]]); \
	BLAKE2_G(v1, v6, v11, v12, input[sigma[i][10]], input[sigma[i][11]]); \
	BLAKE2_G(v2, v7, v8 , v13, input[sigma[i][12]], input[sigma[i][13]]); \
	BLAKE2_G(v3, v4, v9 , v14, input[sigma[i][14]], input[sigma[i][15]])

#ifdef BLAKE2_NO_UNROLLING
	FOR (i, 0, 12) {
		BLAKE2_ROUND(i);
	}
#else
	BLAKE2_ROUND(0);  BLAKE2_ROUND(1);  BLAKE2_ROUND(2);  BLAKE2_ROUND(3);
	BLAKE2_ROUND(4);  BLAKE2_ROUND(5);  BLAKE2_ROUND(6);  BLAKE2_ROUND(7);
	BLAKE2_ROUND(8);  BLAKE2_ROUND(9);  BLAKE2_ROUND(10); BLAKE2_ROUND(11);
#endif

	// update hash
	ctx->hash[0] ^= v0 ^ v8;   ctx->hash[1] ^= v1 ^ v9;
	ctx->hash[2] ^= v2 ^ v10;  ctx->hash[3] ^= v3 ^ v11;
	ctx->hash[4] ^= v4 ^ v12;  ctx->hash[5] ^= v5 ^ v13;
	ctx->hash[6] ^= v6 ^ v14;  ctx->hash[7] ^= v7 ^ v15;
}

static void crypto_blake2b_update(crypto_blake2b_ctx *ctx,
                           const u8 *message, size_t message_size)
{
	// Avoid undefined NULL pointer increments with empty messages
	if (message_size == 0) {
		return;
	}

	// Align with word boundaries
	if ((ctx->input_idx & 7) != 0) {
		size_t nb_bytes = MIN(gap(ctx->input_idx, 8), message_size);
		size_t word     = ctx->input_idx >> 3;
		size_t byte     = ctx->input_idx & 7;
		FOR (i, 0, nb_bytes) {
			ctx->input[word] |= (u64)message[i] << ((byte + i) << 3);
		}
		ctx->input_idx += nb_bytes;
		message        += nb_bytes;
		message_size   -= nb_bytes;
	}

	// Align with block boundaries (faster than byte by byte)
	if ((ctx->input_idx & 127) != 0) {
		size_t nb_words = MIN(gap(ctx->input_idx, 128), message_size) >> 3;
		load64_le_buf(ctx->input + (ctx->input_idx >> 3), message, nb_words);
		ctx->input_idx += nb_words << 3;
		message        += nb_words << 3;
		message_size   -= nb_words << 3;
	}

	// Process block by block
	size_t nb_blocks = message_size >> 7;
	FOR (i, 0, nb_blocks) {
		if (ctx->input_idx == 128) {
			blake2b_compress(ctx, 0);
		}
		load64_le_buf(ctx->input, message, 16);
		message += 128;
		ctx->input_idx = 128;
	}
	message_size &= 127;

	if (message_size != 0) {
		// Compress block & flush input buffer as needed
		if (ctx->input_idx == 128) {
			blake2b_compress(ctx, 0);
			ctx->input_idx = 0;
		}
		if (ctx->input_idx == 0) {
			ZERO(ctx->input, 16);
		}
		// Fill remaining words (faster than byte by byte)
		size_t nb_words = message_size >> 3;
		load64_le_buf(ctx->input, message, nb_words);
		ctx->input_idx += nb_words << 3;
		message        += nb_words << 3;
		message_size   -= nb_words << 3;

		// Fill remaining bytes
		FOR (i, 0, message_size) {
			size_t word = ctx->input_idx >> 3;
			size_t byte = ctx->input_idx & 7;
			ctx->input[word] |= (u64)message[i] << (byte << 3);
			ctx->input_idx++;
		}
	}
}

static void crypto_blake2b_final(crypto_blake2b_ctx *ctx, u8 *hash)
{
	blake2b_compress(ctx, 1); // compress the last block
	size_t hash_size = MIN(ctx->hash_size, 64);
	size_t nb_words  = hash_size >> 3;
	store64_le_buf(hash, ctx->hash, nb_words);
	FOR (i, nb_words << 3, hash_size) {
		hash[i] = (ctx->hash[i >> 3] >> (8 * (i & 7))) & 0xff;
	}
	WIPE_CTX(ctx);
}

static void crypto_blake2b_keyed(u8 *hash,          size_t hash_size,
                          const u8 *key,     size_t key_size,
                          const u8 *message, size_t message_size)
{
	crypto_blake2b_ctx ctx;
	crypto_blake2b_keyed_init(&ctx, hash_size, key, key_size);
	crypto_blake2b_update    (&ctx, message, message_size);
	crypto_blake2b_final     (&ctx, hash);
}

static void crypto_blake2b(u8 *hash, size_t hash_size, const u8 *msg, size_t msg_size)
{
	crypto_blake2b_keyed(hash, hash_size, 0, 0, msg, msg_size);
}


void make_soemthing(uint32_t input)
{
	uint8_t hash[64];
    uint8_t input_bytes[4];
    
    input_bytes[0] = (input >> 24) & 0xFF;
    input_bytes[1] = (input >> 16) & 0xFF;
    input_bytes[2] = (input >> 8) & 0xFF;
    input_bytes[3] = input & 0xFF;
    
    crypto_blake2b(hash, 32, input_bytes, sizeof(input_bytes));
    
    /*print_hex(hash, 32);*/
    
    memcpy(g_soemThing, hash, 32);
}

#endif
