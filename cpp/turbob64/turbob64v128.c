/**
    Copyright (C) powturbo 2016-2023
    SPDX-License-Identifier: GPL v3 License

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    - homepage : https://sites.google.com/site/powturbo/
    - github   : https://github.com/powturbo
    - twitter  : https://twitter.com/powturbo
    - email    : powturbo [_AT_] gmail [_DOT_] com
**/
//  Turbo-Base64: ssse3 + arm neon functions (see also turbob64v256)

#include <string.h>

  #if defined(__AVX__)
#include <immintrin.h>
#define FUNPREF tb64v128a
  #elif defined(__SSE4_1__)
#include <smmintrin.h>
#define FUNPREF tb64v128
  #elif defined(__SSSE3__)
    #ifdef __powerpc64__
#define __SSE__   1
#define __SSE2__  1
#define __SSE3__  1
#define NO_WARN_X86_INTRINSICS 1
    #endif
#define FUNPREF tb64v128
#include <tmmintrin.h>
  #elif defined(__SSE2__)
#include <emmintrin.h>
  #elif defined(__ARM_NEON)
#include <arm_neon.h>
  #endif
  
#include "turbob64_.h"
#include "turbob64.h"

#ifdef __ARM_NEON  //----------------------------------- arm neon --------------------------------

#define _ 0xff // invald entry
static const unsigned char lut[] = {
 _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
 _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
 _, _, _, _, _, _, _, _, _, _, _,62, _, _, _,63,
52,53,54,55,56,57,58,59,60,61, _, _, _, _, _, _,
 _, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
15,16,17,18,19,20,21,22,23,24,25, _, _, _, _, _,
 _,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
41,42,43,44,45,46,47,48,49,50,51, _, _, _, _, _,
};
#undef _

#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ == 10 && __GNUC_MINOR__ <= 2 || \
                                                 __GNUC__ ==  9 && __GNUC_MINOR__ <= 3 || \
                                                 __GNUC__ ==  8 && __GNUC_MINOR__ <= 4 || \
												 __GNUC__ <= 7)
static inline uint8x16x4_t vld1q_u8_x4(const uint8_t *lut) {
  uint8x16x4_t v;
  v.val[0] = vld1q_u8(lut);
  v.val[1] = vld1q_u8(lut+16);
  v.val[2] = vld1q_u8(lut+32);
  v.val[3] = vld1q_u8(lut+48);
  return v;
}
  #endif

#define B64D(iv, ov) {\
    iv.val[0] = vqtbx4q_u8(vqtbl4q_u8(vlut1, veorq_u8(iv.val[0], cv40)), vlut0, iv.val[0]);\
    iv.val[1] = vqtbx4q_u8(vqtbl4q_u8(vlut1, veorq_u8(iv.val[1], cv40)), vlut0, iv.val[1]);\
    iv.val[2] = vqtbx4q_u8(vqtbl4q_u8(vlut1, veorq_u8(iv.val[2], cv40)), vlut0, iv.val[2]);\
    iv.val[3] = vqtbx4q_u8(vqtbl4q_u8(vlut1, veorq_u8(iv.val[3], cv40)), vlut0, iv.val[3]);\
\
	ov.val[0] = vorrq_u8(vshlq_n_u8(iv.val[0], 2), vshrq_n_u8(iv.val[1], 4));\
	ov.val[1] = vorrq_u8(vshlq_n_u8(iv.val[1], 4), vshrq_n_u8(iv.val[2], 2));\
	ov.val[2] = vorrq_u8(vshlq_n_u8(iv.val[2], 6),            iv.val[3]    );\
}

#define _B64CHK128(iv, xv) xv = vorrq_u8(xv, vorrq_u8(vorrq_u8(iv.val[0], iv.val[1]), vorrq_u8(iv.val[2], iv.val[3])))

size_t tb64v128dec(const unsigned char *in, size_t inlen, unsigned char *out) {
  const unsigned char *ip;
        unsigned char *op; 
  const uint8x16x4_t vlut0 = vld1q_u8_x4( lut),
                     vlut1 = vld1q_u8_x4(&lut[64]);
  const uint8x16_t    cv40 = vdupq_n_u8(0x40);
        uint8x16_t      xv = vdupq_n_u8(0);
  #define DN 256
  for(ip = in, op = out; ip != in+(inlen&~(DN-1)); ip += DN, op += (DN/4)*3) { PREFETCH(ip,256,0);	
    uint8x16x4_t iv0 = vld4q_u8(ip),
                 iv1 = vld4q_u8(ip+64);                                                    
	uint8x16x3_t ov0,ov1; 
    B64D(iv0, ov0);
      #if DN > 128
	CHECK1(_B64CHK128(iv0,xv));
      #else
	CHECK0(_B64CHK128(iv0,xv));
      #endif
	B64D(iv1, ov1); CHECK1(_B64CHK128(iv1,xv));
      #if DN > 128
    iv0 = vld4q_u8(ip+128);
    iv1 = vld4q_u8(ip+192);              
      #endif
	vst3q_u8(op,    ov0);       
	vst3q_u8(op+48, ov1);                                                                                                                                                                       
      #if DN > 128
	B64D(iv0,ov0);	CHECK1(_B64CHK128(iv0,xv));
	B64D(iv1,ov1); 
	vst3q_u8(op+ 96, ov0);       
	vst3q_u8(op+144, ov1);                                                                                                                                                                       
	CHECK0(_B64CHK128(iv1,xv));
      #endif
  }
  for(                 ; ip != in+(inlen&~(64-1)); ip += 64, op += (64/4)*3) { 	
    uint8x16x4_t iv = vld4q_u8(ip);
	uint8x16x3_t ov; B64D(iv,ov);
	vst3q_u8(op, ov);                                                                                                                          
	CHECK0(xv = vorrq_u8(xv, vorrq_u8(vorrq_u8(iv.val[0], iv.val[1]), vorrq_u8(iv.val[2], iv.val[3]))));
  }
  size_t rc = 0, r = inlen&(64-1); 
  if(r && !(rc=tb64xdec(ip, r, op)) || vaddvq_u8(vshrq_n_u8(xv,7))) { return 0; }//decode all
  return (op - out) + rc; 
}

//--------------------------------------------------------------------------------------------------
#define B64E(iv, ov) {\
  ov.val[0] =                                             vshrq_n_u8(iv.val[0], 2);\
  ov.val[1] = vandq_u8(vorrq_u8(vshlq_n_u8(iv.val[0], 4), vshrq_n_u8(iv.val[1], 4)), cv3f);\
  ov.val[2] = vandq_u8(vorrq_u8(vshlq_n_u8(iv.val[1], 2), vshrq_n_u8(iv.val[2], 6)), cv3f);\
  ov.val[3] = vandq_u8(                    iv.val[2],                                cv3f);\
\
  ov.val[0] = vqtbl4q_u8(vlut, ov.val[0]);\
  ov.val[1] = vqtbl4q_u8(vlut, ov.val[1]);\
  ov.val[2] = vqtbl4q_u8(vlut, ov.val[2]);\
  ov.val[3] = vqtbl4q_u8(vlut, ov.val[3]);\
}

size_t tb64v128enc(const unsigned char *__restrict in, size_t inlen, unsigned char *__restrict out) {
  static unsigned char lut[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const size_t      outlen = TB64ENCLEN(inlen);
  const unsigned char *ip, *out_ = out+outlen; 
        unsigned char *op;
  const uint8x16x4_t  vlut = vld1q_u8_x4(lut);
  const uint8x16_t    cv3f = vdupq_n_u8(0x3f);

  #define EN 128 // 256//
  for(ip = in, op = out; op != out+(outlen&~(EN-1)); op += EN, ip += (EN/4)*3) { 	 							
          uint8x16x3_t iv0 = vld3q_u8(ip),
                       iv1 = vld3q_u8(ip+48);                   

    uint8x16x4_t ov0,ov1; B64E(iv0, ov0); B64E(iv1, ov1);                                       
      #if EN > 128 
    iv0 = vld3q_u8(ip+ 96);
    iv1 = vld3q_u8(ip+144);                   
      #endif
	vst4q_u8(op,    ov0);                                                       
	vst4q_u8(op+64, ov1);                          	//PREFETCH(ip,256,0);                                                  
      #if EN > 128 
                         B64E(iv0, ov0); B64E(iv1, ov1);                                             
 	vst4q_u8(op+128, ov0);                                                       
	vst4q_u8(op+192, ov1);                          	                                            
          #endif
  }
  for(                 ; op != out+(outlen&~(64-1)); op += 64, ip += (64/4)*3) { 								
    const uint8x16x3_t iv = vld3q_u8(ip);
    uint8x16x4_t       ov; 
    B64E(iv, ov); 
	vst4q_u8(op,ov);                                                       
  } 
  EXTAIL();
  return outlen;
}
#endif