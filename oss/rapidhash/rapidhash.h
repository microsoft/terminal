/*
 * rapidhash - Very fast, high quality, platform-independent hashing algorithm.
 * Copyright (C) 2024 Nicolas De Carli
 *
 * Based on 'wyhash', by Wang Yi <godspeed_china@yeah.net>
 *
 * BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You can contact the author at:
 *   - rapidhash source repository: https://github.com/Nicoshev/rapidhash
 */

/*
 *  Includes.
 */
#include <stdint.h>
#include <string.h>
#if defined(_MSC_VER)
  #include <intrin.h>
  #if defined(_M_X64) && !defined(_M_ARM64EC)
    #pragma intrinsic(_umul128)
  #endif
#endif

/*
 *  Protection macro, alters behaviour of rapid_mum multiplication function.
 *  
 *  RAPIDHASH_FAST: Normal behavior, max speed.
 *  RAPIDHASH_PROTECTED: Extra protection against entropy loss.
 */
#ifndef RAPIDHASH_PROTECTED
  #define RAPIDHASH_FAST
#elif defined(RAPIDHASH_FAST)
  #error "cannot define RAPIDHASH_PROTECTED and RAPIDHASH_FAST simultaneously."
#endif

/*
 *  Unrolling macros, changes code definition for main hash function.
 *  
 *  RAPIDHASH_COMPACT: Legacy variant, each loop process 48 bytes.
 *  RAPIDHASH_UNROLLED: Unrolled variant, each loop process 96 bytes.
 *
 *  Most modern CPUs should benefit from having RAPIDHASH_UNROLLED.
 *
 *  These macros do not alter the output hash.
 */
#ifndef RAPIDHASH_COMPACT
  #define RAPIDHASH_UNROLLED
#elif defined(RAPIDHASH_UNROLLED)
  #error "cannot define RAPIDHASH_COMPACT and RAPIDHASH_UNROLLED simultaneously."
#endif

/*
 *  Likely and unlikely macros.
 */
#if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
  #define _likely_(x)  __builtin_expect(x,1)
  #define _unlikely_(x)  __builtin_expect(x,0)
#else
  #define _likely_(x) (x)
  #define _unlikely_(x) (x)
#endif

/*
 *  Endianness macros.
 */
#ifndef RAPIDHASH_LITTLE_ENDIAN
  #if defined(_WIN32) || defined(__LITTLE_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    #define RAPIDHASH_LITTLE_ENDIAN
  #elif defined(__BIG_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    #define RAPIDHASH_BIG_ENDIAN
  #else
    #warning "could not determine endianness! Falling back to little endian."
    #define RAPIDHASH_LITTLE_ENDIAN
  #endif
#endif

/*
 *  Default seed.
 */
#define RAPID_SEED (0xbdd89aa982704029ull)

/*
 *  Default secret parameters.
 */
static const uint64_t rapid_secret[3] = {0x2d358dccaa6c78a5ull, 0x8bb84b93962eacc9ull, 0x4b33a62ed433d4a3ull};

/*
 *  64*64 -> 128bit multiply function.
 *  
 *  @param A  Address of 64-bit number.
 *  @param B  Address of 64-bit number.
 *
 *  Calculates 128-bit C = *A * *B.
 *
 *  When RAPIDHASH_FAST is defined:
 *  Overwrites A contents with C's low 64 bits.
 *  Overwrites B contents with C's high 64 bits.
 *
 *  When RAPIDHASH_PROTECTED is defined:
 *  Xors and overwrites A contents with C's low 64 bits.
 *  Xors and overwrites B contents with C's high 64 bits.
 */
static inline void rapid_mum(uint64_t *A, uint64_t *B){
#if defined(__SIZEOF_INT128__)
  __uint128_t r=*A; r*=*B; 
  #ifdef RAPIDHASH_PROTECTED
  *A^=(uint64_t)r; *B^=(uint64_t)(r>>64);
  #else
  *A=(uint64_t)r; *B=(uint64_t)(r>>64);
  #endif
#elif defined(_MSC_VER) && (defined(_WIN64) || defined(_M_HYBRID_CHPE_ARM64))
  #if defined(_M_X64)
    #ifdef RAPIDHASH_PROTECTED
    uint64_t  a,  b;
    a=_umul128(*A,*B,&b);
    *A^=a;  *B^=b;
    #else
    *A=_umul128(*A,*B,B);
    #endif
  #else
    #ifdef RAPIDHASH_PROTECTED
    uint64_t a, b;
    b = __umulh(*A, *B);
    a = *A * *B;
    *A^=a;  *B^=b;
    #else
    uint64_t c;
    c = __umulh(*A, *B);
    *A = *A * *B;
    *B = c;
    #endif
  #endif
#else
  uint64_t ha=*A>>32, hb=*B>>32, la=(uint32_t)*A, lb=(uint32_t)*B, hi, lo;
  uint64_t rh=ha*hb, rm0=ha*lb, rm1=hb*la, rl=la*lb, t=rl+(rm0<<32), c=t<rl;
  lo=t+(rm1<<32); c+=lo<t; hi=rh+(rm0>>32)+(rm1>>32)+c;
  #ifdef RAPIDHASH_PROTECTED
  *A^=lo;  *B^=hi;
  #else
  *A=lo;  *B=hi;
  #endif
#endif
}

/*
 *  Multiply and xor mix function.
 *  
 *  @param A  64-bit number.
 *  @param B  64-bit number.
 *
 *  Calculates 128-bit C = A * B.
 *  Returns 64-bit xor between high and low 64 bits of C.
 */
static inline uint64_t rapid_mix(uint64_t A, uint64_t B){ rapid_mum(&A,&B); return A^B; }

/*
 *  Read functions.
 */
#ifdef RAPIDHASH_LITTLE_ENDIAN
static inline uint64_t rapid_read64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v;}
static inline uint64_t rapid_read32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v;}
#elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
static inline uint64_t rapid_read64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return __builtin_bswap64(v);}
static inline uint64_t rapid_read32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return __builtin_bswap32(v);}
#elif defined(_MSC_VER)
static inline uint64_t rapid_read64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return _byteswap_uint64(v);}
static inline uint64_t rapid_read32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return _byteswap_ulong(v);}
#else
static inline uint64_t rapid_read64(const uint8_t *p) {
  uint64_t v; memcpy(&v, p, 8);
  return (((v >> 56) & 0xff)| ((v >> 40) & 0xff00)| ((v >> 24) & 0xff0000)| ((v >>  8) & 0xff000000)| ((v <<  8) & 0xff00000000)| ((v << 24) & 0xff0000000000)| ((v << 40) & 0xff000000000000)| ((v << 56) & 0xff00000000000000));
}
static inline uint64_t rapid_read32(const uint8_t *p) {
  uint32_t v; memcpy(&v, p, 4);
  return (((v >> 24) & 0xff)| ((v >>  8) & 0xff00)| ((v <<  8) & 0xff0000)| ((v << 24) & 0xff000000));
}
#endif

/*
 *  Reads and combines 3 bytes of input.
 *
 *  @param p  Buffer to read from.
 *  @param k  Length of @p, in bytes.
 *
 *  Always reads and combines 3 bytes from memory.
 *  Guarantees to read each buffer position at least once.
 *  
 *  Returns a 64-bit value containing all three bytes read. 
 */
static inline uint64_t rapid_readSmall(const uint8_t *p, size_t k) { return (((uint64_t)p[0])<<56)|(((uint64_t)p[k>>1])<<32)|p[k-1];}

/*
 *  rapidhash main function.
 *
 *  @param key     Buffer to be hashed.
 *  @param len     @key length, in bytes.
 *  @param seed    64-bit seed used to alter the hash result predictably.
 *  @param secret  Triplet of 64-bit secrets used to alter hash result predictably.
 *
 *  Returns a 64-bit hash.
 */
static inline uint64_t rapidhash_internal(const void *key, size_t len, uint64_t seed, const uint64_t* secret){
  const uint8_t *p=(const uint8_t *)key; seed^=rapid_mix(seed^secret[0],secret[1])^len;  uint64_t  a,  b;
  if(_likely_(len<=16)){
    if(_likely_(len>=4)){ 
      const uint8_t * plast = p + len - 4;
      a = (rapid_read32(p) << 32) | rapid_read32(plast);
      const uint64_t delta = ((len&24)>>(len>>3));
      b = ((rapid_read32(p + delta) << 32) | rapid_read32(plast - delta)); }
    else if(_likely_(len>0)){ a=rapid_readSmall(p,len); b=0;}
    else a=b=0;
  }
  else{
    size_t i=len; 
    if(_unlikely_(i>48)){
      uint64_t see1=seed, see2=seed;
#ifdef RAPIDHASH_UNROLLED
      while(_likely_(i>=96)){
        seed=rapid_mix(rapid_read64(p)^secret[0],rapid_read64(p+8)^seed);
        see1=rapid_mix(rapid_read64(p+16)^secret[1],rapid_read64(p+24)^see1);
        see2=rapid_mix(rapid_read64(p+32)^secret[2],rapid_read64(p+40)^see2);
        seed=rapid_mix(rapid_read64(p+48)^secret[0],rapid_read64(p+56)^seed);
        see1=rapid_mix(rapid_read64(p+64)^secret[1],rapid_read64(p+72)^see1);
        see2=rapid_mix(rapid_read64(p+80)^secret[2],rapid_read64(p+88)^see2);
        p+=96; i-=96;
      }
      if(_unlikely_(i>=48)){
        seed=rapid_mix(rapid_read64(p)^secret[0],rapid_read64(p+8)^seed);
        see1=rapid_mix(rapid_read64(p+16)^secret[1],rapid_read64(p+24)^see1);
        see2=rapid_mix(rapid_read64(p+32)^secret[2],rapid_read64(p+40)^see2);
        p+=48; i-=48;
      }
#else
      do {
        seed=rapid_mix(rapid_read64(p)^secret[0],rapid_read64(p+8)^seed);
        see1=rapid_mix(rapid_read64(p+16)^secret[1],rapid_read64(p+24)^see1);
        see2=rapid_mix(rapid_read64(p+32)^secret[2],rapid_read64(p+40)^see2);
        p+=48; i-=48;
      } while (_likely_(i>=48));
#endif
      seed^=see1^see2;
    }
    if(i>16){
      seed=rapid_mix(rapid_read64(p)^secret[2],rapid_read64(p+8)^seed^secret[1]);
      if(i>32)
        seed=rapid_mix(rapid_read64(p+16)^secret[2],rapid_read64(p+24)^seed);
    }
    a=rapid_read64(p+i-16);  b=rapid_read64(p+i-8);
  }
  a^=secret[1]; b^=seed;  rapid_mum(&a,&b);
  return  rapid_mix(a^secret[0]^len,b^secret[1]);
}

/*
 *  rapidhash default seeded hash function.
 *
 *  @param key     Buffer to be hashed.
 *  @param len     @key length, in bytes.
 *  @param seed    64-bit seed used to alter the hash result predictably.
 *
 *  Calls rapidhash_internal using provided parameters and default secrets.
 *
 *  Returns a 64-bit hash.
 */
static inline uint64_t rapidhash_withSeed(const void *key, size_t len, uint64_t seed) {
  return rapidhash_internal(key, len, seed, rapid_secret);
}

/*
 *  rapidhash default hash function.
 *
 *  @param key     Buffer to be hashed.
 *  @param len     @key length, in bytes.
 *
 *  Calls rapidhash_withSeed using provided parameters and the default seed.
 *
 *  Returns a 64-bit hash.
 */
static inline uint64_t rapidhash(const void *key, size_t len) {
  return rapidhash_withSeed(key, len, RAPID_SEED);
}
