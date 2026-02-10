.SUFFIXES: .hlsl

# Vertex Shaders

$(OBJ_PATH)\$(O)\custom_shader_vs.h: custom_shader_vs.hlsl
    @$(ECHO) Compiling shader $(*F)...
    $(FXC) $(FXC_FLAGS) /Tvs_$(FXC_PROFILE_VERSION) /Fh"$@" /Vn$(*B) $?

$(OBJ_PATH)\$(O)\shader_vs.h: shader_vs.hlsl
    @$(ECHO) Compiling shader $(*F)...
    $(FXC) $(FXC_FLAGS) /Tvs_$(FXC_PROFILE_VERSION) /Fh"$@" /Vn$(*B) $?

# Pixel Shaders

$(OBJ_PATH)\$(O)\custom_shader_ps.h: custom_shader_ps.hlsl
    @$(ECHO) Compiling shader $(*F)...
    $(FXC) $(FXC_FLAGS) /Tps_$(FXC_PROFILE_VERSION) /Fh"$@" /Vn$(*B) $?

$(OBJ_PATH)\$(O)\shader_ps.h: shader_ps.hlsl
    @$(ECHO) Compiling shader $(*F)...
    $(FXC) $(FXC_FLAGS) /Tps_$(FXC_PROFILE_VERSION) /Fh"$@" /Vn$(*B) $?
