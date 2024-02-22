@echo off
rem Copyright (c) Microsoft Corporation.
rem Licensed under the MIT License.

setlocal
set error=0

set FXCOPTS=/nologo /WX /Ges /Zi /Zpc /Qstrip_reflect /Qstrip_debug

if %1.==xbox. goto continuexbox
if %1.==. goto continuepc
echo usage: CompileShaders [xbox]
exit /b

:continuexbox
set XBOXOPTS=/D__XBOX_DISABLE_SHADER_NAME_EMPLACEMENT
if NOT %2.==noprecompile. goto skipnoprecompile
set XBOXOPTS=%XBOXOPTS% /D__XBOX_DISABLE_PRECOMPILE=1
:skipnoprecompile

set XBOXFXC="%XboxOneXDKLatest%\xdk\FXC\amd64\FXC.exe"
if exist %XBOXFXC% goto continue
set XBOXFXC="%XboxOneXDKLatest%xdk\FXC\amd64\FXC.exe"
if exist %XBOXFXC% goto continue
set XBOXFXC="%XboxOneXDKBuild%xdk\FXC\amd64\FXC.exe"
if exist %XBOXFXC% goto continue
set XBOXFXC="%DurangoXDK%xdk\FXC\amd64\FXC.exe"
if not exist %XBOXFXC% goto needxdk
goto continue

:continuepc

set PCFXC="%WindowsSdkVerBinPath%x86\fxc.exe"
if exist %PCFXC% goto continue
set PCFXC="%WindowsSdkBinPath%%WindowsSDKVersion%\x86\fxc.exe"
if exist %PCFXC% goto continue
set PCFXC="%WindowsSdkDir%bin\%WindowsSDKVersion%\x86\fxc.exe"
if exist %PCFXC% goto continue

set PCFXC=fxc.exe

:continue
if not defined CompileShadersOutput set CompileShadersOutput=Compiled
set StrTrim=%CompileShadersOutput%##
set StrTrim=%StrTrim: ##=%
set CompileShadersOutput=%StrTrim:##=%
@if not exist "%CompileShadersOutput%" mkdir "%CompileShadersOutput%"
call :CompileShader%1 AlphaTestEffect vs VSAlphaTest
call :CompileShader%1 AlphaTestEffect vs VSAlphaTestNoFog
call :CompileShader%1 AlphaTestEffect vs VSAlphaTestVc
call :CompileShader%1 AlphaTestEffect vs VSAlphaTestVcNoFog

call :CompileShader%1 AlphaTestEffect ps PSAlphaTestLtGt
call :CompileShader%1 AlphaTestEffect ps PSAlphaTestLtGtNoFog
call :CompileShader%1 AlphaTestEffect ps PSAlphaTestEqNe
call :CompileShader%1 AlphaTestEffect ps PSAlphaTestEqNeNoFog

call :CompileShader%1 BasicEffect vs VSBasic
call :CompileShader%1 BasicEffect vs VSBasicNoFog
call :CompileShader%1 BasicEffect vs VSBasicVc
call :CompileShader%1 BasicEffect vs VSBasicVcNoFog
call :CompileShader%1 BasicEffect vs VSBasicTx
call :CompileShader%1 BasicEffect vs VSBasicTxNoFog
call :CompileShader%1 BasicEffect vs VSBasicTxVc
call :CompileShader%1 BasicEffect vs VSBasicTxVcNoFog

call :CompileShader%1 BasicEffect vs VSBasicVertexLighting
call :CompileShader%1 BasicEffect vs VSBasicVertexLightingBn
call :CompileShader%1 BasicEffect vs VSBasicVertexLightingVc
call :CompileShader%1 BasicEffect vs VSBasicVertexLightingVcBn
call :CompileShader%1 BasicEffect vs VSBasicVertexLightingTx
call :CompileShader%1 BasicEffect vs VSBasicVertexLightingTxBn
call :CompileShader%1 BasicEffect vs VSBasicVertexLightingTxVc
call :CompileShader%1 BasicEffect vs VSBasicVertexLightingTxVcBn

call :CompileShader%1 BasicEffect vs VSBasicOneLight
call :CompileShader%1 BasicEffect vs VSBasicOneLightBn
call :CompileShader%1 BasicEffect vs VSBasicOneLightVc
call :CompileShader%1 BasicEffect vs VSBasicOneLightVcBn
call :CompileShader%1 BasicEffect vs VSBasicOneLightTx
call :CompileShader%1 BasicEffect vs VSBasicOneLightTxBn
call :CompileShader%1 BasicEffect vs VSBasicOneLightTxVc
call :CompileShader%1 BasicEffect vs VSBasicOneLightTxVcBn

call :CompileShader%1 BasicEffect vs VSBasicPixelLighting
call :CompileShader%1 BasicEffect vs VSBasicPixelLightingBn
call :CompileShader%1 BasicEffect vs VSBasicPixelLightingVc
call :CompileShader%1 BasicEffect vs VSBasicPixelLightingVcBn
call :CompileShader%1 BasicEffect vs VSBasicPixelLightingTx
call :CompileShader%1 BasicEffect vs VSBasicPixelLightingTxBn
call :CompileShader%1 BasicEffect vs VSBasicPixelLightingTxVc
call :CompileShader%1 BasicEffect vs VSBasicPixelLightingTxVcBn

call :CompileShader%1 BasicEffect ps PSBasic
call :CompileShader%1 BasicEffect ps PSBasicNoFog
call :CompileShader%1 BasicEffect ps PSBasicTx
call :CompileShader%1 BasicEffect ps PSBasicTxNoFog

call :CompileShader%1 BasicEffect ps PSBasicVertexLighting
call :CompileShader%1 BasicEffect ps PSBasicVertexLightingNoFog
call :CompileShader%1 BasicEffect ps PSBasicVertexLightingTx
call :CompileShader%1 BasicEffect ps PSBasicVertexLightingTxNoFog

call :CompileShader%1 BasicEffect ps PSBasicPixelLighting
call :CompileShader%1 BasicEffect ps PSBasicPixelLightingTx

call :CompileShader%1 DualTextureEffect vs VSDualTexture
call :CompileShader%1 DualTextureEffect vs VSDualTextureNoFog
call :CompileShader%1 DualTextureEffect vs VSDualTextureVc
call :CompileShader%1 DualTextureEffect vs VSDualTextureVcNoFog

call :CompileShader%1 DualTextureEffect ps PSDualTexture
call :CompileShader%1 DualTextureEffect ps PSDualTextureNoFog

call :CompileShader%1 EnvironmentMapEffect vs VSEnvMap
call :CompileShader%1 EnvironmentMapEffect vs VSEnvMapBn
call :CompileShader%1 EnvironmentMapEffect vs VSEnvMapFresnel
call :CompileShader%1 EnvironmentMapEffect vs VSEnvMapFresnelBn
call :CompileShader%1 EnvironmentMapEffect vs VSEnvMapOneLight
call :CompileShader%1 EnvironmentMapEffect vs VSEnvMapOneLightBn
call :CompileShader%1 EnvironmentMapEffect vs VSEnvMapOneLightFresnel
call :CompileShader%1 EnvironmentMapEffect vs VSEnvMapOneLightFresnelBn
call :CompileShader%1 EnvironmentMapEffect vs VSEnvMapPixelLighting
call :CompileShader%1 EnvironmentMapEffect vs VSEnvMapPixelLightingBn
call :CompileShaderSM4%1 EnvironmentMapEffect vs VSEnvMapPixelLightingSM4
call :CompileShaderSM4%1 EnvironmentMapEffect vs VSEnvMapPixelLightingBnSM4

call :CompileShader%1 EnvironmentMapEffect ps PSEnvMap
call :CompileShader%1 EnvironmentMapEffect ps PSEnvMapNoFog
call :CompileShader%1 EnvironmentMapEffect ps PSEnvMapSpecular
call :CompileShader%1 EnvironmentMapEffect ps PSEnvMapSpecularNoFog
call :CompileShader%1 EnvironmentMapEffect ps PSEnvMapPixelLighting
call :CompileShader%1 EnvironmentMapEffect ps PSEnvMapPixelLightingNoFog
call :CompileShader%1 EnvironmentMapEffect ps PSEnvMapPixelLightingFresnel
call :CompileShader%1 EnvironmentMapEffect ps PSEnvMapPixelLightingFresnelNoFog

call :CompileShader%1 EnvironmentMapEffect ps PSEnvMapSpherePixelLighting
call :CompileShader%1 EnvironmentMapEffect ps PSEnvMapSpherePixelLightingNoFog
call :CompileShader%1 EnvironmentMapEffect ps PSEnvMapSpherePixelLightingFresnel
call :CompileShader%1 EnvironmentMapEffect ps PSEnvMapSpherePixelLightingFresnelNoFog

call :CompileShaderSM4%1 EnvironmentMapEffect ps PSEnvMapDualParabolaPixelLighting
call :CompileShaderSM4%1 EnvironmentMapEffect ps PSEnvMapDualParabolaPixelLightingNoFog
call :CompileShaderSM4%1 EnvironmentMapEffect ps PSEnvMapDualParabolaPixelLightingFresnel
call :CompileShaderSM4%1 EnvironmentMapEffect ps PSEnvMapDualParabolaPixelLightingFresnelNoFog

call :CompileShader%1 SkinnedEffect vs VSSkinnedVertexLightingOneBone
call :CompileShader%1 SkinnedEffect vs VSSkinnedVertexLightingOneBoneBn
call :CompileShader%1 SkinnedEffect vs VSSkinnedVertexLightingTwoBones
call :CompileShader%1 SkinnedEffect vs VSSkinnedVertexLightingTwoBonesBn
call :CompileShader%1 SkinnedEffect vs VSSkinnedVertexLightingFourBones
call :CompileShader%1 SkinnedEffect vs VSSkinnedVertexLightingFourBonesBn

call :CompileShader%1 SkinnedEffect vs VSSkinnedOneLightOneBone
call :CompileShader%1 SkinnedEffect vs VSSkinnedOneLightOneBoneBn
call :CompileShader%1 SkinnedEffect vs VSSkinnedOneLightTwoBones
call :CompileShader%1 SkinnedEffect vs VSSkinnedOneLightTwoBonesBn
call :CompileShader%1 SkinnedEffect vs VSSkinnedOneLightFourBones
call :CompileShader%1 SkinnedEffect vs VSSkinnedOneLightFourBonesBn

call :CompileShader%1 SkinnedEffect vs VSSkinnedPixelLightingOneBone
call :CompileShader%1 SkinnedEffect vs VSSkinnedPixelLightingOneBoneBn
call :CompileShader%1 SkinnedEffect vs VSSkinnedPixelLightingTwoBones
call :CompileShader%1 SkinnedEffect vs VSSkinnedPixelLightingTwoBonesBn
call :CompileShader%1 SkinnedEffect vs VSSkinnedPixelLightingFourBones
call :CompileShader%1 SkinnedEffect vs VSSkinnedPixelLightingFourBonesBn

call :CompileShader%1 SkinnedEffect ps PSSkinnedVertexLighting
call :CompileShader%1 SkinnedEffect ps PSSkinnedVertexLightingNoFog
call :CompileShader%1 SkinnedEffect ps PSSkinnedPixelLighting

call :CompileShaderSM4%1 NormalMapEffect vs VSNormalPixelLightingTx
call :CompileShaderSM4%1 NormalMapEffect vs VSNormalPixelLightingTxBn
call :CompileShaderSM4%1 NormalMapEffect vs VSNormalPixelLightingTxVc
call :CompileShaderSM4%1 NormalMapEffect vs VSNormalPixelLightingTxVcBn

call :CompileShaderSM4%1 NormalMapEffect vs VSNormalPixelLightingTxInst
call :CompileShaderSM4%1 NormalMapEffect vs VSNormalPixelLightingTxBnInst
call :CompileShaderSM4%1 NormalMapEffect vs VSNormalPixelLightingTxVcInst
call :CompileShaderSM4%1 NormalMapEffect vs VSNormalPixelLightingTxVcBnInst

call :CompileShaderSM4%1 NormalMapEffect vs VSSkinnedPixelLightingTx
call :CompileShaderSM4%1 NormalMapEffect vs VSSkinnedPixelLightingTxBn

call :CompileShaderSM4%1 NormalMapEffect ps PSNormalPixelLightingTx
call :CompileShaderSM4%1 NormalMapEffect ps PSNormalPixelLightingTxNoFog
call :CompileShaderSM4%1 NormalMapEffect ps PSNormalPixelLightingTxNoSpec
call :CompileShaderSM4%1 NormalMapEffect ps PSNormalPixelLightingTxNoFogSpec

call :CompileShaderSM4%1 PBREffect vs VSConstant
call :CompileShaderSM4%1 PBREffect vs VSConstantInst
call :CompileShaderSM4%1 PBREffect vs VSConstantVelocity
call :CompileShaderSM4%1 PBREffect vs VSConstantBn
call :CompileShaderSM4%1 PBREffect vs VSConstantBnInst
call :CompileShaderSM4%1 PBREffect vs VSConstantVelocityBn
call :CompileShaderSM4%1 PBREffect vs VSSkinned
call :CompileShaderSM4%1 PBREffect vs VSSkinnedBn

call :CompileShaderSM4%1 PBREffect ps PSConstant
call :CompileShaderSM4%1 PBREffect ps PSTextured
call :CompileShaderSM4%1 PBREffect ps PSTexturedEmissive
call :CompileShaderSM4%1 PBREffect ps PSTexturedVelocity
call :CompileShaderSM4%1 PBREffect ps PSTexturedEmissiveVelocity

call :CompileShaderSM4%1 DebugEffect vs VSDebug
call :CompileShaderSM4%1 DebugEffect vs VSDebugBn
call :CompileShaderSM4%1 DebugEffect vs VSDebugVc
call :CompileShaderSM4%1 DebugEffect vs VSDebugVcBn

call :CompileShaderSM4%1 DebugEffect vs VSDebugInst
call :CompileShaderSM4%1 DebugEffect vs VSDebugBnInst
call :CompileShaderSM4%1 DebugEffect vs VSDebugVcInst
call :CompileShaderSM4%1 DebugEffect vs VSDebugVcBnInst

call :CompileShaderSM4%1 DebugEffect ps PSHemiAmbient
call :CompileShaderSM4%1 DebugEffect ps PSRGBNormals
call :CompileShaderSM4%1 DebugEffect ps PSRGBTangents
call :CompileShaderSM4%1 DebugEffect ps PSRGBBiTangents

call :CompileShader%1 SpriteEffect vs SpriteVertexShader
call :CompileShader%1 SpriteEffect ps SpritePixelShader

call :CompileShader%1 DGSLEffect vs main
call :CompileShader%1 DGSLEffect vs mainVc
call :CompileShader%1 DGSLEffect vs main1Bones
call :CompileShader%1 DGSLEffect vs main1BonesVc
call :CompileShader%1 DGSLEffect vs main2Bones
call :CompileShader%1 DGSLEffect vs main2BonesVc
call :CompileShader%1 DGSLEffect vs main4Bones
call :CompileShader%1 DGSLEffect vs main4BonesVc

call :CompileShaderHLSL%1 DGSLUnlit ps main
call :CompileShaderHLSL%1 DGSLLambert ps main
call :CompileShaderHLSL%1 DGSLPhong ps main

call :CompileShaderHLSL%1 DGSLUnlit ps mainTk
call :CompileShaderHLSL%1 DGSLLambert ps mainTk
call :CompileShaderHLSL%1 DGSLPhong ps mainTk

call :CompileShaderHLSL%1 DGSLUnlit ps mainTx
call :CompileShaderHLSL%1 DGSLLambert ps mainTx
call :CompileShaderHLSL%1 DGSLPhong ps mainTx

call :CompileShaderHLSL%1 DGSLUnlit ps mainTxTk
call :CompileShaderHLSL%1 DGSLLambert ps mainTxTk
call :CompileShaderHLSL%1 DGSLPhong ps mainTxTk

call :CompileShaderSM4%1 PostProcess vs VSQuad
call :CompileShaderSM4%1 PostProcess ps PSCopy
call :CompileShaderSM4%1 PostProcess ps PSMonochrome
call :CompileShaderSM4%1 PostProcess ps PSSepia
call :CompileShaderSM4%1 PostProcess ps PSDownScale2x2
call :CompileShaderSM4%1 PostProcess ps PSDownScale4x4
call :CompileShaderSM4%1 PostProcess ps PSGaussianBlur5x5
call :CompileShaderSM4%1 PostProcess ps PSBloomExtract
call :CompileShaderSM4%1 PostProcess ps PSBloomBlur
call :CompileShaderSM4%1 PostProcess ps PSMerge
call :CompileShaderSM4%1 PostProcess ps PSBloomCombine

call :CompileShaderSM4%1 ToneMap vs VSQuad
call :CompileShaderSM4%1 ToneMap ps PSCopy
call :CompileShaderSM4%1 ToneMap ps PSSaturate
call :CompileShaderSM4%1 ToneMap ps PSReinhard
call :CompileShaderSM4%1 ToneMap ps PSACESFilmic
call :CompileShaderSM4%1 ToneMap ps PS_SRGB
call :CompileShaderSM4%1 ToneMap ps PSSaturate_SRGB
call :CompileShaderSM4%1 ToneMap ps PSReinhard_SRGB
call :CompileShaderSM4%1 ToneMap ps PSACESFilmic_SRGB
call :CompileShaderSM4%1 ToneMap ps PSHDR10

if NOT %1.==xbox. goto skipxboxonly

call :CompileShaderSM4xbox ToneMap ps PSHDR10_Saturate
call :CompileShaderSM4xbox ToneMap ps PSHDR10_Reinhard
call :CompileShaderSM4xbox ToneMap ps PSHDR10_ACESFilmic
call :CompileShaderSM4xbox ToneMap ps PSHDR10_Saturate_SRGB
call :CompileShaderSM4xbox ToneMap ps PSHDR10_Reinhard_SRGB
call :CompileShaderSM4xbox ToneMap ps PSHDR10_ACESFilmic_SRGB

:skipxboxonly

echo.

if %error% == 0 (
    echo Shaders compiled ok
) else (
    echo There were shader compilation errors!
    exit /b 1
)

endlocal
exit /b 0

:CompileShader
set fxc=%PCFXC% "%1.fx" %FXCOPTS% /T%2_4_0_level_9_1 /E%3 "/Fh%CompileShadersOutput%\%1_%3.inc" "/Fd%CompileShadersOutput%\%1_%3.pdb" /Vn%1_%3
echo.
echo %fxc%
%fxc% || set error=1
exit /b

:CompileShaderSM4
set fxc=%PCFXC% "%1.fx" %FXCOPTS% /T%2_4_0 /E%3 "/Fh%CompileShadersOutput%\%1_%3.inc" "/Fd%CompileShadersOutput%\%1_%3.pdb" /Vn%1_%3
echo.
echo %fxc%
%fxc% || set error=1
exit /b

:CompileShaderHLSL
set fxc=%PCFXC% "%1.hlsl" %FXCOPTS% /T%2_4_0_level_9_1 /E%3 "/Fh%CompileShadersOutput%\%1_%3.inc" "/Fd%CompileShadersOutput%\%1_%3.pdb" /Vn%1_%3
echo.
echo %fxc%
%fxc% || set error=1
exit /b

:CompileShaderxbox
:CompileShaderSM4xbox
set fxc=%XBOXFXC% "%1.fx" %FXCOPTS% /T%2_5_0 %XBOXOPTS% /E%3 "/Fh%CompileShadersOutput%\XboxOne%1_%3.inc" "/Fd%CompileShadersOutput%\XboxOne%1_%3.pdb" /Vn%1_%3
echo.
echo %fxc%
%fxc% || set error=1
exit /b

:CompileShaderHLSLxbox
set fxc=%XBOXFXC% "%1.hlsl" %FXCOPTS% /T%2_5_0 %XBOXOPTS% /E%3 "/Fh%CompileShadersOutput%\XboxOne%1_%3.inc" "/Fd%CompileShadersOutput%\XboxOne%1_%3.pdb" /Vn%1_%3
echo.
echo %fxc%
%fxc% || set error=1
exit /b

:needxdk
echo ERROR: CompileShaders xbox requires the Microsoft Xbox One XDK
echo        (try re-running from the XDK Command Prompt)
