; A vertex shader that passes through all parameters with no manipulation, but
; only after executing a long chain of busywork that fills the maximum 136
; instruction slots. This is intended to measure costs that scale with
; vertex program length (e.g., CPU-side emulation of the vertex program).
;
; The busywork result is folded into oPos by multiplying it with #k.x, which
; must be set to 0.0, so the output is identical to passthrough.vsh while
; preventing the computation from being eliminated as dead code.
;
; #k must be set to (0.0, 0.5, 0.25, 1.0). All intermediate values are clamped
; to [0.25, 1.0] so that the ILU operations never see zero/negative inputs.

#k vector 96

max r0, iDiffuse, #k.x
min r0, r0, #k.w

mad r1, r0, #k.y, #k.z
dp4 r2.x, r1, #k.y
rcp r2.y, r2.x
mul r3, r1, r2.y
logp r4, r3.x
expp r5, r3.y
add r6, r4.z, r5.z
mad r6, r6, #k.y, r3
max r6, r6, #k.z
min r0, r6, #k.w

mad r1, r0, #k.y, #k.z
dp4 r2.x, r1, #k.y
rcp r2.y, r2.x
mul r3, r1, r2.y
logp r4, r3.x
expp r5, r3.y
add r6, r4.z, r5.z
mad r6, r6, #k.y, r3
max r6, r6, #k.z
min r0, r6, #k.w

mad r1, r0, #k.y, #k.z
dp4 r2.x, r1, #k.y
rcp r2.y, r2.x
mul r3, r1, r2.y
logp r4, r3.x
expp r5, r3.y
add r6, r4.z, r5.z
mad r6, r6, #k.y, r3
max r6, r6, #k.z
min r0, r6, #k.w

mad r1, r0, #k.y, #k.z
dp4 r2.x, r1, #k.y
rcp r2.y, r2.x
mul r3, r1, r2.y
logp r4, r3.x
expp r5, r3.y
add r6, r4.z, r5.z
mad r6, r6, #k.y, r3
max r6, r6, #k.z
min r0, r6, #k.w

mad r1, r0, #k.y, #k.z
dp4 r2.x, r1, #k.y
rcp r2.y, r2.x
mul r3, r1, r2.y
logp r4, r3.x
expp r5, r3.y
add r6, r4.z, r5.z
mad r6, r6, #k.y, r3
max r6, r6, #k.z
min r0, r6, #k.w

mad r1, r0, #k.y, #k.z
dp4 r2.x, r1, #k.y
rcp r2.y, r2.x
mul r3, r1, r2.y
logp r4, r3.x
expp r5, r3.y
add r6, r4.z, r5.z
mad r6, r6, #k.y, r3
max r6, r6, #k.z
min r0, r6, #k.w

mad r1, r0, #k.y, #k.z
dp4 r2.x, r1, #k.y
rcp r2.y, r2.x
mul r3, r1, r2.y
logp r4, r3.x
expp r5, r3.y
add r6, r4.z, r5.z
mad r6, r6, #k.y, r3
max r6, r6, #k.z
min r0, r6, #k.w

mad r1, r0, #k.y, #k.z
dp4 r2.x, r1, #k.y
rcp r2.y, r2.x
mul r3, r1, r2.y
logp r4, r3.x
expp r5, r3.y
add r6, r4.z, r5.z
mad r6, r6, #k.y, r3
max r6, r6, #k.z
min r0, r6, #k.w

mad r1, r0, #k.y, #k.z
dp4 r2.x, r1, #k.y
rcp r2.y, r2.x
mul r3, r1, r2.y
logp r4, r3.x
expp r5, r3.y
add r6, r4.z, r5.z
mad r6, r6, #k.y, r3
max r6, r6, #k.z
min r0, r6, #k.w

mad r1, r0, #k.y, #k.z
dp4 r2.x, r1, #k.y
rcp r2.y, r2.x
mul r3, r1, r2.y
logp r4, r3.x
expp r5, r3.y
add r6, r4.z, r5.z
mad r6, r6, #k.y, r3
max r6, r6, #k.z
min r0, r6, #k.w

mad r1, r0, #k.y, #k.z
dp4 r2.x, r1, #k.y
rcp r2.y, r2.x
mul r3, r1, r2.y
logp r4, r3.x
expp r5, r3.y
add r6, r4.z, r5.z
mad r6, r6, #k.y, r3
max r6, r6, #k.z
min r0, r6, #k.w

mad r1, r0, #k.y, #k.z
dp4 r2.x, r1, #k.y
rcp r2.y, r2.x
mul r3, r1, r2.y
logp r4, r3.x
expp r5, r3.y
add r6, r4.z, r5.z
mad r6, r6, #k.y, r3
max r6, r6, #k.z
min r0, r6, #k.w

dp4 r7.x, r0, r0
rsq r7.y, r7.x
mul r0, r0, r7.y

mad oPos, r0, #k.x, iPos
mov oDiffuse, iDiffuse
mov oSpecular, iSpecular
mov oFog, iFog
mov oPts, iPts
mov oBackDiffuse, iBackDiffuse
mov oBackSpecular, iBackSpecular
mov oTex0, iTex0
mov oTex1, iTex1
mov oTex2, iTex2
mov oTex3, iTex3
