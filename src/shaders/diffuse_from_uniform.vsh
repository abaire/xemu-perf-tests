; A vertex shader that just passes through all parameters except diffuse,
; which is taken from the shader uniforms.

#diffuse vector     96  ; The input field that should be assigned to oDiffuse.

mov oPos, iPos
mov oDiffuse, #diffuse
mov oSpecular, iSpecular
mov oFog, iFog
mov oPts, iPts
mov oBackDiffuse, iBackDiffuse
mov oBackSpecular, iBackSpecular
mov oTex0, iTex0
mov oTex1, iTex1
mov oTex2, iTex2
mov oTex3, iTex3
