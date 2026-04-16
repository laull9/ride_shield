# EmbedResource.cmake — 将二进制资源嵌入 C++ 可执行文件
#
# 用法：
#   rideshield_embed_resource(<target>
#       <symbol_name> <file_path>
#       [<symbol_name2> <file_path2> ...]
#   )
#
# 生成的符号：
#   extern const unsigned char <symbol_name>_data[];
#   extern const unsigned char <symbol_name>_end[];
#   extern const unsigned int  <symbol_name>_size;
#
# 使用 rideshield/embedded_resource.h 中的辅助函数获取 span。

function(rideshield_embed_resource target_name)
    set(_args ${ARGN})
    list(LENGTH _args _arg_count)
    math(EXPR _remainder "${_arg_count} % 2")
    if(_remainder)
        message(FATAL_ERROR "rideshield_embed_resource: arguments must be <symbol> <file> pairs")
    endif()

    set(_generated_sources "")
    set(_idx 0)

    while(_idx LESS _arg_count)
        list(GET _args ${_idx} _symbol)
        math(EXPR _file_idx "${_idx} + 1")
        list(GET _args ${_file_idx} _file)
        math(EXPR _idx "${_idx} + 2")

        get_filename_component(_abs_file "${_file}" ABSOLUTE)
        if(NOT EXISTS "${_abs_file}")
            message(FATAL_ERROR "rideshield_embed_resource: file not found: ${_abs_file}")
        endif()

        set(_asm_file "${CMAKE_CURRENT_BINARY_DIR}/embedded_${_symbol}.S")

        if(WIN32 AND MSVC)
            # MSVC: 使用 incbin 指令的 MASM 语法
            file(WRITE "${_asm_file}"
".section .rodata\n"
".global ${_symbol}_data\n"
".global ${_symbol}_end\n"
".global ${_symbol}_size\n"
".align 16\n"
"${_symbol}_data:\n"
".incbin \"${_abs_file}\"\n"
"${_symbol}_end:\n"
".align 4\n"
"${_symbol}_size:\n"
".int ${_symbol}_end - ${_symbol}_data\n"
            )
        else()
            # GCC / Clang
            file(WRITE "${_asm_file}"
".section .rodata,\"a\",@progbits\n"
".global ${_symbol}_data\n"
".global ${_symbol}_end\n"
".global ${_symbol}_size\n"
".align 16\n"
"${_symbol}_data:\n"
".incbin \"${_abs_file}\"\n"
"${_symbol}_end:\n"
".align 4\n"
"${_symbol}_size:\n"
".int ${_symbol}_end - ${_symbol}_data\n"
            )
        endif()

        set_source_files_properties("${_asm_file}" PROPERTIES
            GENERATED TRUE
            LANGUAGE ASM
        )
        list(APPEND _generated_sources "${_asm_file}")
    endwhile()

    target_sources(${target_name} PRIVATE ${_generated_sources})

    # 确保目标启用 ASM
    get_property(_langs GLOBAL PROPERTY ENABLED_LANGUAGES)
    if(NOT "ASM" IN_LIST _langs)
        enable_language(ASM)
    endif()
endfunction()
