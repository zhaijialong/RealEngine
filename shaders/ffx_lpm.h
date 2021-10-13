//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//
//                                           [LPM] LUMA PRESERVING MAPPER 1.20200225
//
//==============================================================================================================================
// MIT LICENSE
// ===========
// Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All rights reserved.
// -----------
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,
// modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
// -----------
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
// Software.
// -----------
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//------------------------------------------------------------------------------------------------------------------------------
// ABOUT
// =====
// LPM is an evoluation of the concepts in "A Blend of GCN Optimisation and Colour Processing" from GDC 2019.
// This presentation can be found here: https://gpuopen.com/gdc-2019-presentation-links/
// Changes from the GDC 2019 presentation,
//  (a.) Switched from using LUTs to doing the full mapping in ALU.
//       This was made possible by changing the algorithm used to "walk back in gamut".
//       There is no longer any need for "Fast" vs "Quality" modes.
//       While the prior "Quality" mode had good precision for luma, chroma precision was relatively poor.
//       Specifically having only 32 steps in the LUT wasn't enough to do a great job on the overexposure color shaping.
//       The ALU version has great precision, while being much easier to integrate and tune.
//       Likewise as most GPU workloads are not ALU-bound, this ALU version should be better for async integration.
//  (b.) Moved to only one step of "walk back in gamut" done towards the end of the mapper (not necessary to do twice).
//  (c.) Introduced a secondary optional matrix multiply conversion at the end (better for small to large gamut cases, etc).
//------------------------------------------------------------------------------------------------------------------------------
// FP16 PRECISION LIMITS
// =====================
// LPM supports a packed FP16 fast path.
// The lowest normal FP16 value is 2^-14 (or roughly 0.000061035).
// Converting this linear value to sRGB with input range {0 to 1} scaled {0 to 255} on output produces roughly 0.2.
// Maybe enough precision to convert for 8-bit, but not enough working precision for accumulating lighting.
// Converting 2^-14 to Gamma 2.2 with input range {0 to 1} scaled to {0 to 255} on output produces roughly 3.1.
// Not enough precision in normals values, depending on denormals for near zero precision.
//------------------------------------------------------------------------------------------------------------------------------
// TUNING THE CROSSTALK VALUE
// ==========================
// The crosstalk value shapes how color over-exposes.
// Suggest tuning the crosstalk value to get the look and feel desired by the content author.
// Here is the intuition on how to tune the crosstalk value passed into LpmSetup().
// Start with {1.0,1.0,1.0} -> this will fully desaturate on over-exposure (not good).
// Start reducing the blue {1.0,1.0,1.0/8.0} -> tint towards yellow on over-exposure (medium maintaining saturation).
// Reduce blue until the red->yellow->white transition is good {1.0,1.0,1.0/16.0} -> maintaining good saturation.
// It is possible to go extreme to {1.0,1.0,1.0/128.0} -> very high saturation on over-exposure (yellow tint).
// Then reduce red a little {1.0/2.0,1.0,1.0/32.0} -> to tint blues towards cyan on over-exposure.
// Or reduce green a little {1.0,1.0/2.0,1.0/32.0} -> to tint blues towards purple on over-exposure.
// If wanting a stronger blue->purple, drop both green and blue {1.0,1.0/4.0,1.0/128.0}.
// Note that crosstalk value should be tuned differently based on the color space.
// Suggest tuning crosstalk separately for Rec.2020 and Rec.709 primaries for example. 
//------------------------------------------------------------------------------------------------------------------------------
// SETUP THE MAPPER 
// ================
// This code can work on GPU or CPU, showing CPU version.
//  ...
//  // Define a function to store the control block.
//  #define A_CPU 1
//  #include "ffx_a.h"
//  ...
//  AU1 ctl[24*4]; // Upload this to a uint4[24] part of a constant buffer (for example 'constant.lpm[24]').
//  A_STATIC void LpmSetupOut(AU1 i,inAU4 v){ctl[i*4+0]=v[0];ctl[i*4+1]=v[1];ctl[i*4+2]=v[2];ctl[i*4+3]=v[3];}
//  ...
// Or a GPU version,
//  ...
//  A_STATIC void LpmSetupOut(AU1 i,inAU4 v){constant.lpm[i]=v;}
//  ...
// Then call the function to setup the control block,
//  ...
//  // Setup the control block.
//  varAF3(saturation)=initAF3(0.0,0.0,0.0);
//  varAF3(crosstalk)=initAF3(1.0,1.0/2.0,1.0/32.0);
//  LpmSetup(
//   false,LPM_CONFIG_709_709,LPM_COLORS_709_709, // <-- Using the LPM_ prefabs to make inputs easier.
//   0.0, // softGap
//   256.0, // hdrMax
//   8.0, // exposure
//   0.25, // contrast
//   1.0, // shoulder contrast
//   saturation,crosstalk);
//  ...
//------------------------------------------------------------------------------------------------------------------------------
// RUNNING THE MAPPER ON COLOR
// ===========================
// This part would be running per-pixel on the GPU.
// This is showing the no 'shoulder' adjustment fast path.
//  ...
//  // Define a function to load the control block.
//  #define A_HLSL 1
//  #define A_GPU 1
//  #include "ffx_a.h"
//  AU4 LpmFilterCtl(AU1 i){return constant.lpm[i];}
//  ...
//  // Define LPM_NO_SETUP before the "ffx_lpm.h" include if not using LpmSetup() on the GPU.
//  #define LPM_NO_SETUP 1
//  #include "ffx_lpm.h"
//  ... 
//  // Run the tone/gamut-mapper.
//  // The 'c.rgb' is the color to map, using the no-shoulder tuning option.
//  LpmFilter(c.r,c.g,c.b,false,LPM_CONFIG_709_709); // <-- Using the LPM_CONFIG_ prefab to make inputs easier.
//  ...
//  // Do the final linear to non-linear transform.
//  c.r=AToSrgbF1(c.r);
//  c.g=AToSrgbF1(c.g);
//  c.b=AToSrgbF1(c.b);
//  ...
//------------------------------------------------------------------------------------------------------------------------------
// CHANGE LOG
// ==========
// 20200225 - Fixed #ifdef A_HALF define and made sure LpmMapH() is wrapped as well, and wrapped float consts in AF1_().
// 20200224 - Fixed LPM_CONFIG_FS2RAW_709.
// 20200210 - Removed local dimming input and complexity in LpmFs2ScrgbScalar().
// 20200205 - Turn on scaleOnly for HDR10RAW_2020.
// 20200204 - Limited FP16 inputs, fixed 'softGap=0.0' case.
// 20200203 - Fixed typo to opAAddOne*().
// 20190716 - Code cleanup.
// 20190531 - Fix bug for saturation on CPU for LpmSetup().
// 20190530 - Converted to shared CPU/GPU code for LpmSetup().
// 20190425 - Initial GPU-only ALU-only prototype code drop (working 32-bit and 16-bit paths).
//==============================================================================================================================
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//
//                                                           COMMON
//
//------------------------------------------------------------------------------------------------------------------------------
// Code common to both the GPU and CPU version.
//==============================================================================================================================
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//                                                         HELPER CODE
//------------------------------------------------------------------------------------------------------------------------------
// Used by LpmSetup() to build constants.
//------------------------------------------------------------------------------------------------------------------------------
// Color math references, 
//  - http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
//  - https://en.wikipedia.org/wiki/SRGB#The_sRGB_transfer_function_.28.22gamma.22.29
//  - http://www.ryanjuckett.com/programming/rgb-color-space-conversion/
//==============================================================================================================================
// Low-precision solution.
A_STATIC void LpmMatInv3x3(outAF3 ox,outAF3 oy,outAF3 oz,inAF3 ix,inAF3 iy,inAF3 iz){
 AF1 i=ARcpF1(ix[0]*(iy[1]*iz[2]-iz[1]*iy[2])-ix[1]*(iy[0]*iz[2]-iy[2]*iz[0])+ix[2]*(iy[0]*iz[1]-iy[1]*iz[0]));
 ox[0]=(iy[1]*iz[2]-iz[1]*iy[2])*i;ox[1]=(ix[2]*iz[1]-ix[1]*iz[2])*i;ox[2]=(ix[1]*iy[2]-ix[2]*iy[1])*i;
 oy[0]=(iy[2]*iz[0]-iy[0]*iz[2])*i;oy[1]=(ix[0]*iz[2]-ix[2]*iz[0])*i;oy[2]=(iy[0]*ix[2]-ix[0]*iy[2])*i;
 oz[0]=(iy[0]*iz[1]-iz[0]*iy[1])*i;oz[1]=(iz[0]*ix[1]-ix[0]*iz[1])*i;oz[2]=(ix[0]*iy[1]-iy[0]*ix[1])*i;}
//------------------------------------------------------------------------------------------------------------------------------
// Transpose.
A_STATIC void LpmMatTrn3x3(outAF3 ox,outAF3 oy,outAF3 oz,inAF3 ix,inAF3 iy,inAF3 iz){
 ox[0]=ix[0];ox[1]=iy[0];ox[2]=iz[0];oy[0]=ix[1];oy[1]=iy[1];oy[2]=iz[1];oz[0]=ix[2];oz[1]=iy[2];oz[2]=iz[2];}
//------------------------------------------------------------------------------------------------------------------------------
A_STATIC void LpmMatMul3x3(outAF3 ox,outAF3 oy,outAF3 oz,inAF3 ax,inAF3 ay,inAF3 az,inAF3 bx,inAF3 by,inAF3 bz){
 varAF3(bx2);varAF3(by2);varAF3(bz2);LpmMatTrn3x3(bx2,by2,bz2,bx,by,bz);
 ox[0]=ADotF3(ax,bx2);ox[1]=ADotF3(ax,by2);ox[2]=ADotF3(ax,bz2);
 oy[0]=ADotF3(ay,bx2);oy[1]=ADotF3(ay,by2);oy[2]=ADotF3(ay,bz2);
 oz[0]=ADotF3(az,bx2);oz[1]=ADotF3(az,by2);oz[2]=ADotF3(az,bz2);}
//------------------------------------------------------------------------------------------------------------------------------
// D65 xy coordinates.
A_STATIC varAF2(lpmColD65)=initAF2(AF1_(0.3127),AF1_(0.3290));
//------------------------------------------------------------------------------------------------------------------------------
// Rec709 xy coordinates, (D65 white point).
A_STATIC varAF2(lpmCol709R)=initAF2(AF1_(0.64),AF1_(0.33));
A_STATIC varAF2(lpmCol709G)=initAF2(AF1_(0.30),AF1_(0.60));
A_STATIC varAF2(lpmCol709B)=initAF2(AF1_(0.15),AF1_(0.06));
//------------------------------------------------------------------------------------------------------------------------------
// DCI-P3 xy coordinates, (D65 white point).
A_STATIC varAF2(lpmColP3R)=initAF2(AF1_(0.680),AF1_(0.320));
A_STATIC varAF2(lpmColP3G)=initAF2(AF1_(0.265),AF1_(0.690));
A_STATIC varAF2(lpmColP3B)=initAF2(AF1_(0.150),AF1_(0.060));
//------------------------------------------------------------------------------------------------------------------------------
// Rec2020 xy coordinates, (D65 white point).
A_STATIC varAF2(lpmCol2020R)=initAF2(AF1_(0.708),AF1_(0.292));
A_STATIC varAF2(lpmCol2020G)=initAF2(AF1_(0.170),AF1_(0.797));
A_STATIC varAF2(lpmCol2020B)=initAF2(AF1_(0.131),AF1_(0.046));
//------------------------------------------------------------------------------------------------------------------------------
// Computes z from xy, returns xyz.
A_STATIC void LpmColXyToZ(outAF3 d,inAF2 s){d[0]=s[0];d[1]=s[1];d[2]=AF1_(1.0)-(s[0]+s[1]);}
//------------------------------------------------------------------------------------------------------------------------------
 // Returns conversion matrix, rgbw inputs are xy chroma coordinates.
A_STATIC void LpmColRgbToXyz(outAF3 ox,outAF3 oy,outAF3 oz,inAF2 r,inAF2 g,inAF2 b,inAF2 w){
 // Expand from xy to xyz.
 varAF3(rz);varAF3(gz);varAF3(bz);LpmColXyToZ(rz,r);LpmColXyToZ(gz,g);LpmColXyToZ(bz,b);
 varAF3(r3);varAF3(g3);varAF3(b3);LpmMatTrn3x3(r3,g3,b3,rz,gz,bz);
 // Convert white xyz to XYZ.
 varAF3(w3);LpmColXyToZ(w3,w);opAMulOneF3(w3,w3,ARcpF1(w[1]));
 // Compute xyz to XYZ scalars for primaries.
 varAF3(rv);varAF3(gv);varAF3(bv);LpmMatInv3x3(rv,gv,bv,r3,g3,b3);
 varAF3(s);s[0]=ADotF3(rv,w3);s[1]=ADotF3(gv,w3);s[2]=ADotF3(bv,w3);
 // Scale.
 opAMulF3(ox,r3,s);opAMulF3(oy,g3,s);opAMulF3(oz,b3,s);}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//                                                    SETUP CONTROL BLOCK
//------------------------------------------------------------------------------------------------------------------------------
// This is used to control LpmFilter*() functions.
//------------------------------------------------------------------------------------------------------------------------------
// STORE FUNCTION
// ==============
// A_STATIC void LpmSetupOut(AU1 index,inAU4 value){ . . . }
// Setup this function to accept the output of the control block from LpmSetup().
// The 'index' parameter ranges from {0 to 20} in this version, but might extend to 23 at some point.
//------------------------------------------------------------------------------------------------------------------------------
// CONTROL BLOCK
// =============
// LPM has an optimized constant|literal control block of 384 bytes.
// This control block should be 128-byte aligned (future-proof in case constant cache lines end up at 128-bytes/line).
// Much of this block is reserved for future usage, and to establish good alignment.
// Compile will dead-code remove things not used (no extra overhead).
// Content ordered and grouped for best performance in the common cases.
// Control block has both 32-bit and 16-bit values so that optimizations are possible on platforms supporting faster 16-bit.
//------------------------------------------------------------------------------------------------------------------------------
// 32-BIT PART
//     _______R________  _______G________  _______B________  _______A________
//  0  saturation.r      saturation.g      saturation.b      contrast
//  1  toneScaleBias.x   toneScaleBias.y   lumaT.r           lumaT.g
//  2  lumaT.b           crosstalk.r       crosstalk.g       crosstalk.b
//  3  rcpLumaT.r        rcpLumaT.g        rcpLumaT.b        con2R.r 
// --
//  4  con2R.g           con2R.b           con2G.r           con2G.g 
//  5  con2G.b           con2B.r           con2B.g           con2B.b
//  6  shoulderContrast  lumaW.r           lumaW.g           lumaW.b
//  7  softGap.x         softGap.y         conR.r            conR.g 
// --
//  8  conR.b            conG.r            conG.g            conG.b
//  9  conB.r            conB.g            conB.b            (reserved)
//  A  (reserved)        (reserved)        (reserved)        (reserved)
//  B  (reserved)        (reserved)        (reserved)        (reserved)
// --
//  C  (reserved)        (reserved)        (reserved)        (reserved)
//  D  (reserved)        (reserved)        (reserved)        (reserved)
//  E  (reserved)        (reserved)        (reserved)        (reserved)
//  F  (reserved)        (reserved)        (reserved)        (reserved)
// --
// PACKED 16-BIT PART
//          _______X________  _______Y________  _______X________  _______Y________
// 10.rg  saturation.r      saturation.g      saturation.b      contrast
// 10.ba  toneScaleBias.x   toneScaleBias.y   lumaT.r           lumaT.g
// 11.rg  lumaT.b           crosstalk.r       crosstalk.g       crosstalk.b
// 11.ba  rcpLumaT.r        rcpLumaT.g        rcpLumaT.b        con2R.r 
// 12.rg  con2R.g           con2R.b           con2G.r           con2G.g 
// 12.ba  con2G.b           con2B.r           con2B.g           con2B.b
// 13.rg  shoulderContrast  lumaW.r           lumaW.g           lumaW.b
// 13.ba  softGap.x         softGap.y         conR.r            conR.g 
// --
// 14.rg  conR.b            conG.r            conG.g            conG.b
// 14.ba  conB.r            conB.g            conB.b            (reserved)
// 15.rb  (reserved)        (reserved)        (reserved)        (reserved)
// 15.ba  (reserved)        (reserved)        (reserved)        (reserved)
// 16.rb  (reserved)        (reserved)        (reserved)        (reserved)
// 16.ba  (reserved)        (reserved)        (reserved)        (reserved)
// 17.rb  (reserved)        (reserved)        (reserved)        (reserved)
// 17.ba  (reserved)        (reserved)        (reserved)        (reserved)
//------------------------------------------------------------------------------------------------------------------------------
// IDEAS
// =====
//  - Some of this might benefit from double precision on the GPU.
//  - Can scaling factor in con2 be used to improve FP16 precision?
//==============================================================================================================================
#ifdef LPM_NO_SETUP
 A_STATIC void LpmSetupOut(AU1 i,inAU4 v){}
#endif
//------------------------------------------------------------------------------------------------------------------------------
// Output goes to the user-defined LpmSetupOut() function.
A_STATIC void LpmSetup(
// Path control.
AP1 shoulder, // Use optional extra shoulderContrast tuning (set to false if shoulderContrast is 1.0).
// Prefab start, "LPM_CONFIG_".
AP1 con, // Use first RGB conversion matrix, if 'soft' then 'con' must be true also.
AP1 soft, // Use soft gamut mapping.
AP1 con2, // Use last RGB conversion matrix.
AP1 clip, // Use clipping in last conversion matrix.
AP1 scaleOnly, // Scale only for last conversion matrix (used for 709 HDR to scRGB).
// Gamut control, "LPM_COLORS_".
inAF2 xyRedW,inAF2 xyGreenW,inAF2 xyBlueW,inAF2 xyWhiteW, // Chroma coordinates for working color space.
inAF2 xyRedO,inAF2 xyGreenO,inAF2 xyBlueO,inAF2 xyWhiteO, // For the output color space.
inAF2 xyRedC,inAF2 xyGreenC,inAF2 xyBlueC,inAF2 xyWhiteC,AF1 scaleC, // For the output container color space (if con2).
// Prefab end.
AF1 softGap, // Range of 0 to a little over zero, controls how much feather region in out-of-gamut mapping, 0=clip.
// Tonemapping control.
AF1 hdrMax, // Maximum input value.
AF1 exposure, // Number of stops between 'hdrMax' and 18% mid-level on input. 
AF1 contrast, // Input range {0.0 (no extra contrast) to 1.0 (maximum contrast)}.
AF1 shoulderContrast, // Shoulder shaping, 1.0 = no change (fast path). 
inAF3 saturation, // A per channel adjustment, use <0 decrease, 0=no change, >0 increase.
inAF3 crosstalk){ // One channel must be 1.0, the rest can be <= 1.0 but not zero.
//-----------------------------------------------------------------------------------------------------------------------------
 // Contrast needs to be 1.0 based for no contrast.
 contrast+=AF1_(1.0);
 // Saturation is based on contrast.
 opAAddOneF3(saturation,saturation,contrast);
//-----------------------------------------------------------------------------------------------------------------------------
 // The 'softGap' must actually be above zero.
 softGap=AMaxF1(softGap,AF1_(1.0/1024.0));
//-----------------------------------------------------------------------------------------------------------------------------
 AF1 midIn=hdrMax*AF1_(0.18)*AExp2F1(-exposure);
 AF1 midOut=AF1_(0.18);
//-----------------------------------------------------------------------------------------------------------------------------
 varAF2(toneScaleBias);
 AF1 cs=contrast*shoulderContrast;
 AF1 z0=-APowF1(midIn,contrast);
 AF1 z1=APowF1(hdrMax,cs)*APowF1(midIn,contrast);
 AF1 z2=APowF1(hdrMax,contrast)*APowF1(midIn,cs)*midOut;
 AF1 z3=APowF1(hdrMax,cs)*midOut;
 AF1 z4=APowF1(midIn,cs)*midOut;
 toneScaleBias[0]=-((z0+(midOut*(z1-z2))*ARcpF1(z3-z4))*ARcpF1(z4));
//-----------------------------------------------------------------------------------------------------------------------------
 AF1 w0=APowF1(hdrMax,cs)*APowF1(midIn,contrast);
 AF1 w1=APowF1(hdrMax,contrast)*APowF1(midIn,cs)*midOut;
 AF1 w2=APowF1(hdrMax,cs)*midOut;
 AF1 w3=APowF1(midIn,cs)*midOut;
 toneScaleBias[1]=(w0-w1)*ARcpF1(w2-w3);
//----------------------------------------------------------------------------------------------------------------------------- 
 varAF3(lumaW);varAF3(rgbToXyzXW);varAF3(rgbToXyzYW);varAF3(rgbToXyzZW);
 LpmColRgbToXyz(rgbToXyzXW,rgbToXyzYW,rgbToXyzZW,xyRedW,xyGreenW,xyBlueW,xyWhiteW);
 // Use the Y vector of the matrix for the associated luma coef.
 // For safety, make sure the vector sums to 1.0.
 opAMulOneF3(lumaW,rgbToXyzYW,ARcpF1(rgbToXyzYW[0]+rgbToXyzYW[1]+rgbToXyzYW[2]));
//----------------------------------------------------------------------------------------------------------------------------- 
 // The 'lumaT' for crosstalk mapping is always based on the output color space, unless soft conversion is not used.
 varAF3(lumaT);varAF3(rgbToXyzXO);varAF3(rgbToXyzYO);varAF3(rgbToXyzZO);
 LpmColRgbToXyz(rgbToXyzXO,rgbToXyzYO,rgbToXyzZO,xyRedO,xyGreenO,xyBlueO,xyWhiteO);
 if(soft)opACpyF3(lumaT,rgbToXyzYO);else opACpyF3(lumaT,rgbToXyzYW);
 opAMulOneF3(lumaT,lumaT,ARcpF1(lumaT[0]+lumaT[1]+lumaT[2]));
 varAF3(rcpLumaT);opARcpF3(rcpLumaT,lumaT);
//-----------------------------------------------------------------------------------------------------------------------------
 varAF2(softGap2)=initAF2(0.0,0.0);
 if(soft){softGap2[0]=softGap;softGap2[1]=(AF1_(1.0)-softGap)*ARcpF1(softGap*AF1_(0.693147180559));}
//-----------------------------------------------------------------------------------------------------------------------------
 // First conversion is always working to output.
 varAF3(conR)=initAF3(0.0,0.0,0.0);
 varAF3(conG)=initAF3(0.0,0.0,0.0);
 varAF3(conB)=initAF3(0.0,0.0,0.0);
 if(con){varAF3(xyzToRgbRO);varAF3(xyzToRgbGO);varAF3(xyzToRgbBO);
  LpmMatInv3x3(xyzToRgbRO,xyzToRgbGO,xyzToRgbBO,rgbToXyzXO,rgbToXyzYO,rgbToXyzZO);
  LpmMatMul3x3(conR,conG,conB,xyzToRgbRO,xyzToRgbGO,xyzToRgbBO,rgbToXyzXW,rgbToXyzYW,rgbToXyzZW);}
//-----------------------------------------------------------------------------------------------------------------------------
 // The last conversion is always output to container.
 varAF3(con2R)=initAF3(0.0,0.0,0.0);
 varAF3(con2G)=initAF3(0.0,0.0,0.0);
 varAF3(con2B)=initAF3(0.0,0.0,0.0);
 if(con2){varAF3(rgbToXyzXC);varAF3(rgbToXyzYC);varAF3(rgbToXyzZC);
  LpmColRgbToXyz(rgbToXyzXC,rgbToXyzYC,rgbToXyzZC,xyRedC,xyGreenC,xyBlueC,xyWhiteC);
  varAF3(xyzToRgbRC);varAF3(xyzToRgbGC);varAF3(xyzToRgbBC);
  LpmMatInv3x3(xyzToRgbRC,xyzToRgbGC,xyzToRgbBC,rgbToXyzXC,rgbToXyzYC,rgbToXyzZC);
  LpmMatMul3x3(con2R,con2G,con2B,xyzToRgbRC,xyzToRgbGC,xyzToRgbBC,rgbToXyzXO,rgbToXyzYO,rgbToXyzZO);
  opAMulOneF3(con2R,con2R,scaleC);opAMulOneF3(con2G,con2G,scaleC);opAMulOneF3(con2B,con2B,scaleC);}
 if(scaleOnly)con2R[0]=scaleC;
//-----------------------------------------------------------------------------------------------------------------------------
 // Debug force 16-bit precision for the 32-bit inputs, only works on the GPU.
 #ifdef A_GPU
  #ifdef LPM_DEBUG_FORCE_16BIT_PRECISION
   saturation=AF3(AH3(saturation));
   contrast=AF1(AH1(contrast));
   toneScaleBias=AF2(AH2(toneScaleBias));
   lumaT=AF3(AH3(lumaT));
   crosstalk=AF3(AH3(crosstalk));
   rcpLumaT=AF3(AH3(rcpLumaT));
   con2R=AF3(AH3(con2R));
   con2G=AF3(AH3(con2G));
   con2B=AF3(AH3(con2B));
   shoulderContrast=AF1(AH1(shoulderContrast));
   lumaW=AF3(AH3(lumaW));
   softGap2=AF2(AH2(softGap2));
   conR=AF3(AH3(conR));
   conG=AF3(AH3(conG));
   conB=AF3(AH3(conB));
  #endif
 #endif
//-----------------------------------------------------------------------------------------------------------------------------
 // Pack into control block.
 varAU4(map0);
 map0[0]=AU1_AF1(saturation[0]);
 map0[1]=AU1_AF1(saturation[1]);
 map0[2]=AU1_AF1(saturation[2]);
 map0[3]=AU1_AF1(contrast);
 LpmSetupOut(0,map0);
 varAU4(map1);
 map1[0]=AU1_AF1(toneScaleBias[0]);
 map1[1]=AU1_AF1(toneScaleBias[1]);
 map1[2]=AU1_AF1(lumaT[0]);
 map1[3]=AU1_AF1(lumaT[1]);
 LpmSetupOut(1,map1);
 varAU4(map2);
 map2[0]=AU1_AF1(lumaT[2]);
 map2[1]=AU1_AF1(crosstalk[0]); 
 map2[2]=AU1_AF1(crosstalk[1]); 
 map2[3]=AU1_AF1(crosstalk[2]); 
 LpmSetupOut(2,map2);
 varAU4(map3);
 map3[0]=AU1_AF1(rcpLumaT[0]);
 map3[1]=AU1_AF1(rcpLumaT[1]);
 map3[2]=AU1_AF1(rcpLumaT[2]);
 map3[3]=AU1_AF1(con2R[0]);
 LpmSetupOut(3,map3);
 varAU4(map4);
 map4[0]=AU1_AF1(con2R[1]);
 map4[1]=AU1_AF1(con2R[2]);
 map4[2]=AU1_AF1(con2G[0]);
 map4[3]=AU1_AF1(con2G[1]);
 LpmSetupOut(4,map4);
 varAU4(map5);
 map5[0]=AU1_AF1(con2G[2]);
 map5[1]=AU1_AF1(con2B[0]);
 map5[2]=AU1_AF1(con2B[1]);
 map5[3]=AU1_AF1(con2B[2]);
 LpmSetupOut(5,map5);
 varAU4(map6);
 map6[0]=AU1_AF1(shoulderContrast);
 map6[1]=AU1_AF1(lumaW[0]);
 map6[2]=AU1_AF1(lumaW[1]);
 map6[3]=AU1_AF1(lumaW[2]);
 LpmSetupOut(6,map6);
 varAU4(map7);
 map7[0]=AU1_AF1(softGap2[0]);
 map7[1]=AU1_AF1(softGap2[1]);
 map7[2]=AU1_AF1(conR[0]);
 map7[3]=AU1_AF1(conR[1]);
 LpmSetupOut(7,map7);
 varAU4(map8);
 map8[0]=AU1_AF1(conR[2]);
 map8[1]=AU1_AF1(conG[0]);
 map8[2]=AU1_AF1(conG[1]);
 map8[3]=AU1_AF1(conG[2]);
 LpmSetupOut(8,map8);
 varAU4(map9);
 map9[0]=AU1_AF1(conB[0]);
 map9[1]=AU1_AF1(conB[1]);
 map9[2]=AU1_AF1(conB[2]);
 map9[3]=AU1_(0);
 LpmSetupOut(9,map9);
//-----------------------------------------------------------------------------------------------------------------------------
 // Packed 16-bit part of control block.
 varAU4(map16);varAF2(map16x);varAF2(map16y);varAF2(map16z);varAF2(map16w);
 map16x[0]=saturation[0];
 map16x[1]=saturation[1];
 map16y[0]=saturation[2];
 map16y[1]=contrast;
 map16z[0]=toneScaleBias[0];
 map16z[1]=toneScaleBias[1];
 map16w[0]=lumaT[0];
 map16w[1]=lumaT[1];
 map16[0]=AU1_AH2_AF2(map16x);
 map16[1]=AU1_AH2_AF2(map16y);
 map16[2]=AU1_AH2_AF2(map16z);
 map16[3]=AU1_AH2_AF2(map16w);
 LpmSetupOut(16,map16);
 varAU4(map17);varAF2(map17x);varAF2(map17y);varAF2(map17z);varAF2(map17w);
 map17x[0]=lumaT[2];
 map17x[1]=crosstalk[0];
 map17y[0]=crosstalk[1];
 map17y[1]=crosstalk[2];
 map17z[0]=rcpLumaT[0];
 map17z[1]=rcpLumaT[1];
 map17w[0]=rcpLumaT[2];
 map17w[1]=con2R[0];
 map17[0]=AU1_AH2_AF2(map17x);
 map17[1]=AU1_AH2_AF2(map17y);
 map17[2]=AU1_AH2_AF2(map17z);
 map17[3]=AU1_AH2_AF2(map17w);
 LpmSetupOut(17,map17);
 varAU4(map18);varAF2(map18x);varAF2(map18y);varAF2(map18z);varAF2(map18w);
 map18x[0]=con2R[1];
 map18x[1]=con2R[2];
 map18y[0]=con2G[0];
 map18y[1]=con2G[1];
 map18z[0]=con2G[2];
 map18z[1]=con2B[0];
 map18w[0]=con2B[1];
 map18w[1]=con2B[2];
 map18[0]=AU1_AH2_AF2(map18x);
 map18[1]=AU1_AH2_AF2(map18y);
 map18[2]=AU1_AH2_AF2(map18z);
 map18[3]=AU1_AH2_AF2(map18w);
 LpmSetupOut(18,map18);
 varAU4(map19);varAF2(map19x);varAF2(map19y);varAF2(map19z);varAF2(map19w);
 map19x[0]=shoulderContrast;
 map19x[1]=lumaW[0];
 map19y[0]=lumaW[1];
 map19y[1]=lumaW[2];
 map19z[0]=softGap2[0];
 map19z[1]=softGap2[1];
 map19w[0]=conR[0];
 map19w[1]=conR[1];
 map19[0]=AU1_AH2_AF2(map19x);
 map19[1]=AU1_AH2_AF2(map19y);
 map19[2]=AU1_AH2_AF2(map19z);
 map19[3]=AU1_AH2_AF2(map19w);
 LpmSetupOut(19,map19);
 varAU4(map20);varAF2(map20x);varAF2(map20y);varAF2(map20z);varAF2(map20w);
 map20x[0]=conR[2];
 map20x[1]=conG[0];
 map20y[0]=conG[1];
 map20y[1]=conG[2];
 map20z[0]=conB[0];
 map20z[1]=conB[1];
 map20w[0]=conB[2];
 map20w[1]=0.0;
 map20[0]=AU1_AH2_AF2(map20x);
 map20[1]=AU1_AH2_AF2(map20y);
 map20[2]=AU1_AH2_AF2(map20z);
 map20[3]=AU1_AH2_AF2(map20w);
 LpmSetupOut(20,map20);}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//                                                 HDR10 RANGE LIMITING SCALAR
//------------------------------------------------------------------------------------------------------------------------------
// As of 2019, HDR10 supporting TVs typically have PQ tonal curves with near clipping long before getting to the peak 10K nits. 
// Unfortunately this clipping point changes per TV (requires some amount of user calibration).
// Some examples,
//  https://youtu.be/M7OsbpU4oCQ?t=875
//  https://youtu.be/8mlTElC2z2A?t=1159
//  https://youtu.be/B5V5hCVXBAI?t=975
// For this reason it can be useful to manually limit peak HDR10 output to some point before the clipping point.
// The following functions are useful to compute the scaling factor 'hdr10S' to use with LpmSetup() to manually limit peak.
//==============================================================================================================================
// Compute 'hdr10S' for raw HDR10 output, pass in peak nits (typically somewhere around 1000.0 to 2000.0).
A_STATIC AF1 LpmHdr10RawScalar(AF1 peakNits){return peakNits*(AF1_(1.0)/AF1_(10000.0));}
//------------------------------------------------------------------------------------------------------------------------------
// Compute 'hdr10S' for scRGB based HDR10 output, pass in peak nits (typically somewhere around 1000.0 to 2000.0).
A_STATIC AF1 LpmHdr10ScrgbScalar(AF1 peakNits){return peakNits*(AF1_(1.0)/AF1_(10000.0))*(AF1(10000.0)/AF1_(80.0));}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//                                                   FREESYNC2 SCRGB SCALAR
//------------------------------------------------------------------------------------------------------------------------------
// The more expensive scRGB mode for FreeSync2 requires a complex scale factor based on display properties.
//==============================================================================================================================
// This computes the 'fs2S' factor used in LpmSetup().
// TODO: Is this correct????????????????????????????????????????????????????????????????????????????????????????????????????????
A_STATIC AF1 LpmFs2ScrgbScalar(
AF1 minLuma,AF1 maxLuma){ // Queried display properties.
 return ((maxLuma-minLuma)+minLuma)*(AF1_(1.0)/AF1_(80.0));}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//                                                   CONFIGURATION PREFABS
//------------------------------------------------------------------------------------------------------------------------------
// Use these to simplify some of the input(s) to the LpmSetup() and LpmFilter() functions.
// The 'LPM_CONFIG_<destination>_<source>' defines are used for the path control.
// The 'LPM_COLORS_<destination>_<source>' defines are used for the gamut control.
// This contains expected common configurations, anything else will need to be made by the user.
//------------------------------------------------------------------------------------------------------------------------------
//                WORKING COLOR SPACE
//                ===================
// 2020 ......... Rec.2020
// 709 .......... Rec.709
// P3 ........... DCI-P3 with D65 white-point
// -------------- 
//                OUTPUT COLOR SPACE
//                ==================
// FS2RAW ....... Faster 32-bit/pixel FreeSync2 raw gamma 2.2 output (native display primaries)
// FS2SCRGB ..... Slower 64-bit/pixel FreeSync2 via the scRGB option (Rec.709 primaries with possible negative color)
// HDR10RAW ..... Faster 32-bit/pixel HDR10 raw (10:10:10:2 PQ output with Rec.2020 primaries)
// HDR10SCRGB ... Slower 64-bit/pixel scRGB (linear FP16, Rec.709 primaries with possible negative color)
// 709 .......... Rec.709, sRGB, Gamma 2.2, or traditional displays with Rec.709-like primaries
//------------------------------------------------------------------------------------------------------------------------------
// FREESYNC2 VARIABLES
// ===================
// fs2R ..... Queried xy coordinates for display red
// fs2G ..... Queried xy coordinates for display green
// fs2B ..... Queried xy coordinates for display blue
// fs2W ..... Queried xy coordinates for display white point
// fs2S ..... Computed by LpmFs2ScrgbScalar()
//------------------------------------------------------------------------------------------------------------------------------
// HDR10 VARIABLES
// ===============
// hdr10S ... Use LpmHdr10<Raw|Scrgb>Scalar() to compute this value
//==============================================================================================================================
                           // CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2RAW_709 A_FALSE,A_FALSE,A_TRUE, A_TRUE, A_FALSE
#define LPM_COLORS_FS2RAW_709 lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,\
                              lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,\
                              fs2R,fs2G,fs2B,fs2W,AF1_(1.0)
//------------------------------------------------------------------------------------------------------------------------------
// FreeSync2 min-spec is larger than sRGB, so using 709 primaries all the way through as an optimization.
                             // CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2SCRGB_709 A_FALSE,A_FALSE,A_FALSE,A_FALSE,A_TRUE
#define LPM_COLORS_FS2SCRGB_709 lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,\
                                lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,\
                                lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,fs2S
//------------------------------------------------------------------------------------------------------------------------------
                             // CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_HDR10RAW_709 A_FALSE,A_FALSE,A_TRUE, A_TRUE, A_FALSE
#define LPM_COLORS_HDR10RAW_709 lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,\
                                lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,\
                                lpmCol2020R,lpmCol2020G,lpmCol2020B,lpmColD65,hdr10S
//------------------------------------------------------------------------------------------------------------------------------
                               // CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_HDR10SCRGB_709 A_FALSE,A_FALSE,A_FALSE,A_FALSE,A_TRUE
#define LPM_COLORS_HDR10SCRGB_709 lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,\
                                  lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,\
                                  lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,hdr10S
//------------------------------------------------------------------------------------------------------------------------------
                        // CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_709_709 A_FALSE,A_FALSE,A_FALSE,A_FALSE,A_FALSE
#define LPM_COLORS_709_709 lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,\
                           lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,\
                           lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,AF1_(1.0)
//==============================================================================================================================
                          // CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2RAW_P3 A_TRUE, A_TRUE, A_FALSE,A_FALSE,A_FALSE
#define LPM_COLORS_FS2RAW_P3 lpmColP3R,lpmColP3G,lpmColP3B,lpmColD65,\
                             fs2R,fs2G,fs2B,fs2W,\
                             fs2R,fs2G,fs2B,fs2W,AF1_(1.0)
//------------------------------------------------------------------------------------------------------------------------------
// FreeSync2 gamut can be smaller than P3.
                            // CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2SCRGB_P3 A_TRUE, A_TRUE, A_TRUE, A_FALSE,A_FALSE
#define LPM_COLORS_FS2SCRGB_P3 lpmColP3R,lpmColP3G,lpmColP3B,lpmColD65,\
                               fs2R,fs2G,fs2B,fs2W,\
                               lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,fs2S
//------------------------------------------------------------------------------------------------------------------------------
                            // CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_HDR10RAW_P3 A_FALSE,A_FALSE,A_TRUE, A_TRUE, A_FALSE
#define LPM_COLORS_HDR10RAW_P3 lpmColP3R,lpmColP3G,lpmColP3B,lpmColD65,\
                               lpmColP3R,lpmColP3G,lpmColP3B,lpmColD65,\
                               lpmCol2020R,lpmCol2020G,lpmCol2020B,lpmColD65,hdr10S
//------------------------------------------------------------------------------------------------------------------------------
                              // CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_HDR10SCRGB_P3 A_FALSE,A_FALSE,A_TRUE, A_FALSE,A_FALSE
#define LPM_COLORS_HDR10SCRGB_P3 lpmColP3R,lpmColP3G,lpmColP3B,lpmColD65,\
                                 lpmCol2020R,lpmCol2020G,lpmCol2020B,lpmColD65,\
                                 lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,hdr10S
//------------------------------------------------------------------------------------------------------------------------------
                       // CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_709_P3 A_TRUE, A_TRUE, A_FALSE,A_FALSE,A_FALSE
#define LPM_COLORS_709_P3 lpmColP3R,lpmColP3G,lpmColP3B,lpmColD65,\
                          lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,\
                          lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,AF1_(1.0)
//==============================================================================================================================
                            // CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2RAW_2020 A_TRUE, A_TRUE, A_FALSE,A_FALSE,A_FALSE
#define LPM_COLORS_FS2RAW_2020 lpmCol2020R,lpmCol2020G,lpmCol2020B,lpmColD65,\
                               fs2R,fs2G,fs2B,fs2W,\
                               fs2R,fs2G,fs2B,fs2W,1.0
//------------------------------------------------------------------------------------------------------------------------------
                              // CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2SCRGB_2020 A_TRUE, A_TRUE, A_TRUE, A_FALSE,A_FALSE
#define LPM_COLORS_FS2SCRGB_2020 lpmCol2020R,lpmCol2020G,lpmCol2020B,lpmColD65,\
                                 fs2R,fs2G,fs2B,fs2W,\
                                 lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,fs2S
//------------------------------------------------------------------------------------------------------------------------------
                              // CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_HDR10RAW_2020 A_FALSE,A_FALSE,A_FALSE,A_FALSE,A_TRUE
#define LPM_COLORS_HDR10RAW_2020 lpmCol2020R,lpmCol2020G,lpmCol2020B,lpmColD65,\
                                 lpmCol2020R,lpmCol2020G,lpmCol2020B,lpmColD65,\
                                 lpmCol2020R,lpmCol2020G,lpmCol2020B,lpmColD65,hdr10S
//------------------------------------------------------------------------------------------------------------------------------
                                // CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_HDR10SCRGB_2020 A_FALSE,A_FALSE,A_TRUE, A_FALSE,A_FALSE
#define LPM_COLORS_HDR10SCRGB_2020 lpmCol2020R,lpmCol2020G,lpmCol2020B,lpmColD65,\
                                   lpmCol2020R,lpmCol2020G,lpmCol2020B,lpmColD65,\
                                   lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,hdr10S
//------------------------------------------------------------------------------------------------------------------------------
                         // CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_709_2020 A_TRUE, A_TRUE, A_FALSE,A_FALSE,A_FALSE
#define LPM_COLORS_709_2020 lpmCol2020R,lpmCol2020G,lpmCol2020B,lpmColD65,\
                            lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,\
                            lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65,AF1_(1.0)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//
//                                                             GPU
//
//==============================================================================================================================
#ifdef A_GPU
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//                                                        HELPER CODE
//==============================================================================================================================
 // Visualize difference between two values, by bits of precision.
 // This is useful when doing approximation to reference comparisons. 
 AP1 LpmD(AF1 a,AF1 b){return abs(a-b)<1.0;}
//------------------------------------------------------------------------------------------------------------------------------
 AF1 LpmC(AF1 a,AF1 b){AF1 c=1.0; // 6-bits or less (the color)
  if(LpmD(a* 127.0,b* 127.0))c=0.875; // 7-bits
  if(LpmD(a* 255.0,b* 255.0))c=0.5; // 8-bits
  if(LpmD(a* 512.0,b* 512.0))c=0.125; // 9-bits
  if(LpmD(a*1024.0,b*1024.0))c=0.0; // 10-bits or better (black)
  return c;}
//------------------------------------------------------------------------------------------------------------------------------
 AF3 LpmViewDiff(AF3 a,AF3 b){return AF3(LpmC(a.r,b.r),LpmC(a.g,b.g),LpmC(a.b,b.b));}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//                                                           MAPPER
//------------------------------------------------------------------------------------------------------------------------------
// Do not call this directly, instead call the LpmFilter*() functions.
// This gets reconfigured based on inputs for all the various usage cases.
// Some of this has been explicitly ordered to increase precision.
//------------------------------------------------------------------------------------------------------------------------------
// IDEAS
// =====
//  - Use med3() for soft falloff and for [A] color conversions.
//  - Retry FP16 PQ conversion with different input range.
//  - Possibly skip some work if entire wave is in gamut.
//==============================================================================================================================
 // Use LpmFilter() instead of this.
 void LpmMap(inout AF1 colorR,inout AF1 colorG,inout AF1 colorB, // Input and output color.
 AF3 lumaW, // Luma coef for RGB working space.
 AF3 lumaT, // Luma coef for crosstalk mapping (can be working or output color-space depending on usage case).
 AF3 rcpLumaT, // 1/lumaT.
 AF3 saturation, // Saturation powers.
 AF1 contrast, // Contrast power.
 AP1 shoulder, // Using shoulder tuning (should be a compile-time immediate).
 AF1 shoulderContrast, // Shoulder power.
 AF2 toneScaleBias, // Other tonemapping parameters.
 AF3 crosstalk, // Crosstalk scaling for over-exposure color shaping.
 AP1 con, // Use first RGB conversion matrix (should be a compile-time immediate), if 'soft' then 'con' must be true also.
 AF3 conR,AF3 conG,AF3 conB, // RGB conversion matrix (working to output space conversion).
 AP1 soft, // Use soft gamut mapping (should be a compile-time immediate).
 AF2 softGap, // {x,(1-x)/(x*0.693147180559)}, where 'x' is gamut mapping soft fall-off amount. 
 AP1 con2, // Use last RGB conversion matrix (should be a compile-time immediate).
 AP1 clip, // Use clipping on last conversion matrix.
 AP1 scaleOnly, // Do scaling only (special case for 709 HDR to scRGB).
 AF3 con2R,AF3 con2G,AF3 con2B){ // Secondary RGB conversion matrix.
//------------------------------------------------------------------------------------------------------------------------------
  // Grab original RGB ratio (RCP, 3x MUL, MAX3).
  AF1 rcpMax=ARcpF1(AMax3F1(colorR,colorG,colorB));AF1 ratioR=colorR*rcpMax;AF1 ratioG=colorG*rcpMax;AF1 ratioB=colorB*rcpMax;
  // Apply saturation, ratio must be max 1.0 for this to work right (3x EXP2, 3x LOG2, 3x MUL).
  ratioR=pow(ratioR,AF1_(saturation.r));ratioG=pow(ratioG,AF1_(saturation.g));ratioB=pow(ratioB,AF1_(saturation.b));
//------------------------------------------------------------------------------------------------------------------------------
  // Tonemap luma, note this uses the original color, so saturation is luma preserving.
  // If not using 'con' this uses the output space luma directly to avoid needing extra constants.
  // Note 'soft' should be a compile-time immediate (so no branch) (3x MAD).
  AF1 luma;if(soft)luma=colorG*AF1_(lumaW.g)+(colorR*AF1_(lumaW.r)+(colorB*AF1_(lumaW.b)));
  else             luma=colorG*AF1_(lumaT.g)+(colorR*AF1_(lumaT.r)+(colorB*AF1_(lumaT.b)));
  luma=pow(luma,AF1_(contrast)); // (EXP2, LOG2, MUL).
  AF1 lumaShoulder=shoulder?pow(luma,AF1_(shoulderContrast)):luma; // Optional (EXP2, LOG2, MUL).
  luma=luma*ARcpF1(lumaShoulder*AF1_(toneScaleBias.x)+AF1_(toneScaleBias.y)); // (MAD, MUL, RCP).
//------------------------------------------------------------------------------------------------------------------------------
  // If running soft clipping (this should be a compile-time immediate so branch will not exist).
  if(soft){
   // The 'con' should be a compile-time immediate so branch will not exist.
   // Use of 'con' is implied if soft-falloff is enabled, but using the check here to make finding bugs easy.
   if(con){
    // Converting ratio instead of color. Change of primaries (9x MAD).
    colorR=ratioR;colorG=ratioG;colorB=ratioB;
    ratioR=colorR*AF1_(conR.r)+(colorG*AF1_(conR.g)+(colorB*AF1_(conR.b)));
    ratioG=colorG*AF1_(conG.g)+(colorR*AF1_(conG.r)+(colorB*AF1_(conG.b)));
    ratioB=colorB*AF1_(conB.b)+(colorG*AF1_(conB.g)+(colorR*AF1_(conB.r)));
    // Convert ratio to max 1 again (RCP, 3x MUL, MAX3).
    rcpMax=ARcpF1(AMax3F1(ratioR,ratioG,ratioB));ratioR*=rcpMax;ratioG*=rcpMax;ratioB*=rcpMax;}
//------------------------------------------------------------------------------------------------------------------------------
   // Absolute gamut mapping converted to soft falloff (maintains max 1 property).
   //  g = gap {0 to g} used for {-inf to 0} input range
   //          {g to 1} used for {0 to 1} input range
   //  x >= 0 := y = x * (1-g) + g
   //  x < 0  := g * 2^(x*h)
   //  Where h=(1-g)/(g*log(2)) --- where log() is the natural log
   // The {g,h} above is passed in as softGap.
   // Soft falloff (3x MIN, 3x MAX, 9x MAD, 3x EXP2).
   ratioR=min(max(AF1_(softGap.x),ASatF1(ratioR*AF1_(-softGap.x)+ratioR)),
    ASatF1(AF1_(softGap.x)*exp2(ratioR*AF1_(softGap.y))));
   ratioG=min(max(AF1_(softGap.x),ASatF1(ratioG*AF1_(-softGap.x)+ratioG)),
    ASatF1(AF1_(softGap.x)*exp2(ratioG*AF1_(softGap.y))));
   ratioB=min(max(AF1_(softGap.x),ASatF1(ratioB*AF1_(-softGap.x)+ratioB)),
    ASatF1(AF1_(softGap.x)*exp2(ratioB*AF1_(softGap.y))));}
//------------------------------------------------------------------------------------------------------------------------------
  // Compute ratio scaler required to hit target luma (4x MAD, 1 RCP).
  AF1 lumaRatio=ratioR*AF1_(lumaT.r)+ratioG*AF1_(lumaT.g)+ratioB*AF1_(lumaT.b);
  // This is limited to not clip.
  AF1 ratioScale=ASatF1(luma*ARcpF1(lumaRatio));
  // Assume in gamut, compute output color (3x MAD).
  colorR=ASatF1(ratioR*ratioScale);colorG=ASatF1(ratioG*ratioScale);colorB=ASatF1(ratioB*ratioScale);
  // Capability per channel to increase value (3x MAD).
  // This factors in crosstalk factor to avoid multiplies later.
  //  '(1.0-ratio)*crosstalk' optimized to '-crosstalk*ratio+crosstalk' 
  AF1 capR=AF1_(-crosstalk.r)*colorR+AF1_(crosstalk.r);
  AF1 capG=AF1_(-crosstalk.g)*colorG+AF1_(crosstalk.g);
  AF1 capB=AF1_(-crosstalk.b)*colorB+AF1_(crosstalk.b);
  // Compute amount of luma needed to add to non-clipped channels to make up for clipping (3x MAD).
  AF1 lumaAdd=ASatF1((-colorB)*AF1_(lumaT.b)+((-colorR)*AF1_(lumaT.r)+((-colorG)*AF1_(lumaT.g)+luma)));
  // Amount to increase keeping over-exposure ratios constant and possibly exceeding clipping point (4x MAD, 1 RCP).
  AF1 t=lumaAdd*ARcpF1(capG*AF1_(lumaT.g)+(capR*AF1_(lumaT.r)+(capB*AF1_(lumaT.b))));
  // Add amounts to base color but clip (3x MAD).
  colorR=ASatF1(t*capR+colorR);colorG=ASatF1(t*capG+colorG);colorB=ASatF1(t*capB+colorB);
  // Compute amount of luma needed to add to non-clipped channel to make up for clipping (3x MAD).
  lumaAdd=ASatF1((-colorB)*AF1_(lumaT.b)+((-colorR)*AF1_(lumaT.r)+((-colorG)*AF1_(lumaT.g)+luma)));
  // Add to last channel (3x MAD).
  colorR=ASatF1(lumaAdd*AF1_(rcpLumaT.r)+colorR);
  colorG=ASatF1(lumaAdd*AF1_(rcpLumaT.g)+colorG);
  colorB=ASatF1(lumaAdd*AF1_(rcpLumaT.b)+colorB);
//------------------------------------------------------------------------------------------------------------------------------
  // The 'con2' should be a compile-time immediate so branch will not exist.
  // Last optional place to convert from smaller to larger gamut (or do clipped conversion).
  // For the non-soft-falloff case, doing this after all other mapping saves intermediate re-scaling ratio to max 1.0.
  if(con2){
   // Change of primaries (9x MAD).
   ratioR=colorR;ratioG=colorG;ratioB=colorB;
   if(clip){
    colorR=ASatF1(ratioR*AF1_(con2R.r)+(ratioG*AF1_(con2R.g)+(ratioB*AF1_(con2R.b))));
    colorG=ASatF1(ratioG*AF1_(con2G.g)+(ratioR*AF1_(con2G.r)+(ratioB*AF1_(con2G.b))));
    colorB=ASatF1(ratioB*AF1_(con2B.b)+(ratioG*AF1_(con2B.g)+(ratioR*AF1_(con2B.r))));}
   else{
    colorR=ratioR*AF1_(con2R.r)+(ratioG*AF1_(con2R.g)+(ratioB*AF1_(con2R.b)));
    colorG=ratioG*AF1_(con2G.g)+(ratioR*AF1_(con2G.r)+(ratioB*AF1_(con2G.b)));
    colorB=ratioB*AF1_(con2B.b)+(ratioG*AF1_(con2B.g)+(ratioR*AF1_(con2B.r)));}}
//------------------------------------------------------------------------------------------------------------------------------
  if(scaleOnly){colorR*=AF1_(con2R.r);colorG*=AF1_(con2R.r);colorB*=AF1_(con2R.r);}}
//==============================================================================================================================
 #ifdef A_HALF
  // Packed FP16 version, see non-packed version above for all comments.
  // Use LpmFilterH() instead of this.
  void LpmMapH(inout AH2 colorR,inout AH2 colorG,inout AH2 colorB,AH3 lumaW,AH3 lumaT,AH3 rcpLumaT,AH3 saturation,AH1 contrast,
  AP1 shoulder,AH1 shoulderContrast,AH2 toneScaleBias,AH3 crosstalk,AP1 con,AH3 conR,AH3 conG,AH3 conB,AP1 soft,AH2 softGap,
  AP1 con2,AP1 clip,AP1 scaleOnly,AH3 con2R,AH3 con2G,AH3 con2B){
//------------------------------------------------------------------------------------------------------------------------------
   AH2 rcpMax=ARcpH2(AMax3H2(colorR,colorG,colorB));AH2 ratioR=colorR*rcpMax;AH2 ratioG=colorG*rcpMax;AH2 ratioB=colorB*rcpMax;
   ratioR=pow(ratioR,AH2_(saturation.r));ratioG=pow(ratioG,AH2_(saturation.g));ratioB=pow(ratioB,AH2_(saturation.b));
//------------------------------------------------------------------------------------------------------------------------------
   AH2 luma;if(soft)luma=colorG*AH2_(lumaW.g)+(colorR*AH2_(lumaW.r)+(colorB*AH2_(lumaW.b)));
   else             luma=colorG*AH2_(lumaT.g)+(colorR*AH2_(lumaT.r)+(colorB*AH2_(lumaT.b)));
   luma=pow(luma,AH2_(contrast));
   AH2 lumaShoulder=shoulder?pow(luma,AH2_(shoulderContrast)):luma;
   luma=luma*ARcpH2(lumaShoulder*AH2_(toneScaleBias.x)+AH2_(toneScaleBias.y));
//------------------------------------------------------------------------------------------------------------------------------
   if(soft){
    if(con){
     colorR=ratioR;colorG=ratioG;colorB=ratioB;
     ratioR=colorR*AH2_(conR.r)+(colorG*AH2_(conR.g)+(colorB*AH2_(conR.b)));
     ratioG=colorG*AH2_(conG.g)+(colorR*AH2_(conG.r)+(colorB*AH2_(conG.b)));
     ratioB=colorB*AH2_(conB.b)+(colorG*AH2_(conB.g)+(colorR*AH2_(conB.r)));
     rcpMax=ARcpH2(AMax3H2(ratioR,ratioG,ratioB));ratioR*=rcpMax;ratioG*=rcpMax;ratioB*=rcpMax;}
//------------------------------------------------------------------------------------------------------------------------------
    ratioR=min(max(AH2_(softGap.x),ASatH2(ratioR*AH2_(-softGap.x)+ratioR)),
     ASatH2(AH2_(softGap.x)*exp2(ratioR*AH2_(softGap.y))));
    ratioG=min(max(AH2_(softGap.x),ASatH2(ratioG*AH2_(-softGap.x)+ratioG)),
     ASatH2(AH2_(softGap.x)*exp2(ratioG*AH2_(softGap.y))));
    ratioB=min(max(AH2_(softGap.x),ASatH2(ratioB*AH2_(-softGap.x)+ratioB)),
     ASatH2(AH2_(softGap.x)*exp2(ratioB*AH2_(softGap.y))));}
//------------------------------------------------------------------------------------------------------------------------------
   AH2 lumaRatio=ratioR*AH2_(lumaT.r)+ratioG*AH2_(lumaT.g)+ratioB*AH2_(lumaT.b);
   AH2 ratioScale=ASatH2(luma*ARcpH2(lumaRatio));
   colorR=ASatH2(ratioR*ratioScale);colorG=ASatH2(ratioG*ratioScale);colorB=ASatH2(ratioB*ratioScale);
   AH2 capR=AH2_(-crosstalk.r)*colorR+AH2_(crosstalk.r);
   AH2 capG=AH2_(-crosstalk.g)*colorG+AH2_(crosstalk.g);
   AH2 capB=AH2_(-crosstalk.b)*colorB+AH2_(crosstalk.b);
   AH2 lumaAdd=ASatH2((-colorB)*AH2_(lumaT.b)+((-colorR)*AH2_(lumaT.r)+((-colorG)*AH2_(lumaT.g)+luma)));
   AH2 t=lumaAdd*ARcpH2(capG*AH2_(lumaT.g)+(capR*AH2_(lumaT.r)+(capB*AH2_(lumaT.b))));
   colorR=ASatH2(t*capR+colorR);colorG=ASatH2(t*capG+colorG);colorB=ASatH2(t*capB+colorB);
   lumaAdd=ASatH2((-colorB)*AH2_(lumaT.b)+((-colorR)*AH2_(lumaT.r)+((-colorG)*AH2_(lumaT.g)+luma)));
   colorR=ASatH2(lumaAdd*AH2_(rcpLumaT.r)+colorR);
   colorG=ASatH2(lumaAdd*AH2_(rcpLumaT.g)+colorG);
   colorB=ASatH2(lumaAdd*AH2_(rcpLumaT.b)+colorB);
//------------------------------------------------------------------------------------------------------------------------------
   if(con2){
    ratioR=colorR;ratioG=colorG;ratioB=colorB;
    if(clip){
     colorR=ASatH2(ratioR*AH2_(con2R.r)+(ratioG*AH2_(con2R.g)+(ratioB*AH2_(con2R.b))));
     colorG=ASatH2(ratioG*AH2_(con2G.g)+(ratioR*AH2_(con2G.r)+(ratioB*AH2_(con2G.b))));
     colorB=ASatH2(ratioB*AH2_(con2B.b)+(ratioG*AH2_(con2B.g)+(ratioR*AH2_(con2B.r))));}
    else{
     colorR=ratioR*AH2_(con2R.r)+(ratioG*AH2_(con2R.g)+(ratioB*AH2_(con2R.b)));
     colorG=ratioG*AH2_(con2G.g)+(ratioR*AH2_(con2G.r)+(ratioB*AH2_(con2G.b)));
     colorB=ratioB*AH2_(con2B.b)+(ratioG*AH2_(con2B.g)+(ratioR*AH2_(con2B.r)));}}
//------------------------------------------------------------------------------------------------------------------------------
   if(scaleOnly){colorR*=AH2_(con2R.r);colorG*=AH2_(con2R.r);colorB*=AH2_(con2R.r);}}
 #endif
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//                                                           FILTER
//------------------------------------------------------------------------------------------------------------------------------
// Requires user define: AU4 LpmFilterCtl(AU1 index){...} to load control block values.
// Entry point for per-pixel color tone+gamut mapping.
// Input is linear color {0 to hdrMax} ranged.
// Output is linear color {0 to 1} ranged, except for scRGB where outputs can end up negative and larger than one.
//==============================================================================================================================
 // 32-bit entry point.
 void LpmFilter(
 // Input and output color.
 inout AF1 colorR,inout AF1 colorG,inout AF1 colorB,
 // Path control should all be compile-time immediates.
 AP1 shoulder, // Using shoulder tuning.
 // Prefab "LPM_CONFIG_" start, use the same as used for LpmSetup().
 AP1 con, // Use first RGB conversion matrix, if 'soft' then 'con' must be true also.
 AP1 soft, // Use soft gamut mapping.
 AP1 con2, // Use last RGB conversion matrix.
 AP1 clip, // Use clipping in last conversion matrix.
 AP1 scaleOnly){ // Scale only for last conversion matrix (used for 709 HDR to scRGB).
  // Grab control block, what is unused gets dead-code removal.
  AU4 map0=LpmFilterCtl(0);AU4 map1=LpmFilterCtl(1);AU4 map2=LpmFilterCtl(2);AU4 map3=LpmFilterCtl(3);
  AU4 map4=LpmFilterCtl(4);AU4 map5=LpmFilterCtl(5);AU4 map6=LpmFilterCtl(6);AU4 map7=LpmFilterCtl(7);
  AU4 map8=LpmFilterCtl(8);AU4 map9=LpmFilterCtl(9);AU4 mapA=LpmFilterCtl(10);AU4 mapB=LpmFilterCtl(11);
  AU4 mapC=LpmFilterCtl(12);AU4 mapD=LpmFilterCtl(13);AU4 mapE=LpmFilterCtl(14);AU4 mapF=LpmFilterCtl(15);
  AU4 mapG=LpmFilterCtl(16);AU4 mapH=LpmFilterCtl(17);AU4 mapI=LpmFilterCtl(18);AU4 mapJ=LpmFilterCtl(19);
  AU4 mapK=LpmFilterCtl(20);AU4 mapL=LpmFilterCtl(21);AU4 mapM=LpmFilterCtl(22);AU4 mapN=LpmFilterCtl(23);
  LpmMap(colorR,colorG,colorB,
   AF3(AF4_AU4(map6).g,AF4_AU4(map6).b,AF4_AU4(map6).a), // lumaW
   AF3(AF4_AU4(map1).b,AF4_AU4(map1).a,AF4_AU4(map2).r), // lumaT
   AF3(AF4_AU4(map3).r,AF4_AU4(map3).g,AF4_AU4(map3).b), // rcpLumaT
   AF3(AF4_AU4(map0).r,AF4_AU4(map0).g,AF4_AU4(map0).b), // saturation
   AF4_AU4(map0).a, // contrast
   shoulder,
   AF4_AU4(map6).r, // shoulderContrast
   AF2(AF4_AU4(map1).r,AF4_AU4(map1).g), // toneScaleBias
   AF3(AF4_AU4(map2).g,AF4_AU4(map2).b,AF4_AU4(map2).a),// crosstalk
   con,
   AF3(AF4_AU4(map7).b,AF4_AU4(map7).a,AF4_AU4(map8).r), // conR
   AF3(AF4_AU4(map8).g,AF4_AU4(map8).b,AF4_AU4(map8).a), // conG
   AF3(AF4_AU4(map9).r,AF4_AU4(map9).g,AF4_AU4(map9).b), // conB
   soft,
   AF2(AF4_AU4(map7).r,AF4_AU4(map7).g), // softGap
   con2,clip,scaleOnly,
   AF3(AF4_AU4(map3).a,AF4_AU4(map4).r,AF4_AU4(map4).g), // con2R
   AF3(AF4_AU4(map4).b,AF4_AU4(map4).a,AF4_AU4(map5).r), // con2G
   AF3(AF4_AU4(map5).g,AF4_AU4(map5).b,AF4_AU4(map5).a));} // con2B
//------------------------------------------------------------------------------------------------------------------------------
 #ifdef A_HALF
  // Packed 16-bit entry point (maps 2 colors at the same time).
  void LpmFilterH(
  inout AH2 colorR,inout AH2 colorG,inout AH2 colorB,
  AP1 shoulder,AP1 con,AP1 soft,AP1 con2,AP1 clip,AP1 scaleOnly){
   // Grab control block, what is unused gets dead-code removal.
   AU4 map0=LpmFilterCtl(0);AU4 map1=LpmFilterCtl(1);AU4 map2=LpmFilterCtl(2);AU4 map3=LpmFilterCtl(3);
   AU4 map4=LpmFilterCtl(4);AU4 map5=LpmFilterCtl(5);AU4 map6=LpmFilterCtl(6);AU4 map7=LpmFilterCtl(7);
   AU4 map8=LpmFilterCtl(8);AU4 map9=LpmFilterCtl(9);AU4 mapA=LpmFilterCtl(10);AU4 mapB=LpmFilterCtl(11);
   AU4 mapC=LpmFilterCtl(12);AU4 mapD=LpmFilterCtl(13);AU4 mapE=LpmFilterCtl(14);AU4 mapF=LpmFilterCtl(15);
   AU4 mapG=LpmFilterCtl(16);AU4 mapH=LpmFilterCtl(17);AU4 mapI=LpmFilterCtl(18);AU4 mapJ=LpmFilterCtl(19);
   AU4 mapK=LpmFilterCtl(20);AU4 mapL=LpmFilterCtl(21);AU4 mapM=LpmFilterCtl(22);AU4 mapN=LpmFilterCtl(23);
   // Pre-limit inputs to provide enough head-room for computation in FP16.
   // TODO: Document this better!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
   colorR=min(colorR,AH2_(4096.0));
   colorG=min(colorG,AH2_(4096.0));
   colorB=min(colorB,AH2_(4096.0));
   // Apply filter.
   LpmMapH(colorR,colorG,colorB,
    AH3(AH2_AU1(mapJ.r).y,AH2_AU1(mapJ.g).x,AH2_AU1(mapJ.g).y), // lumaW
    AH3(AH2_AU1(mapG.a).x,AH2_AU1(mapG.a).y,AH2_AU1(mapH.r).x), // lumaT
    AH3(AH2_AU1(mapH.b).x,AH2_AU1(mapH.b).y,AH2_AU1(mapH.a).x), // rcpLumaT
    AH3(AH2_AU1(mapG.r).x,AH2_AU1(mapG.r).y,AH2_AU1(mapG.g).x), // saturation
    AH2_AU1(mapG.g).y, // contrast
    shoulder,
    AH2_AU1(mapJ.r).x, // shoulderContrast
    AH2_AU1(mapG.b), // toneScaleBias
    AH3(AH2_AU1(mapH.r).y,AH2_AU1(mapH.g).x,AH2_AU1(mapH.g).y), // crosstalk
    con,
    AH3(AH2_AU1(mapJ.a).x,AH2_AU1(mapJ.a).y,AH2_AU1(mapK.r).x), // conR
    AH3(AH2_AU1(mapK.r).y,AH2_AU1(mapK.g).x,AH2_AU1(mapK.g).y), // conG
    AH3(AH2_AU1(mapK.b).x,AH2_AU1(mapK.b).y,AH2_AU1(mapK.a).x), // conB
    soft,
    AH2_AU1(mapJ.b), // softGap
    con2,clip,scaleOnly,
    AH3(AH2_AU1(mapH.a).y,AH2_AU1(mapI.r).x,AH2_AU1(mapI.r).y), // con2R
    AH3(AH2_AU1(mapI.g).x,AH2_AU1(mapI.g).y,AH2_AU1(mapI.b).x), // con2G
    AH3(AH2_AU1(mapI.b).y,AH2_AU1(mapI.a).x,AH2_AU1(mapI.a).y));} // con2B
 #endif
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//                                                       END OF GPU CODE
//==============================================================================================================================
#endif
