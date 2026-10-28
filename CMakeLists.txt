cmake_minimum_required(VERSION 3.22)

project(MasterPack VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Poner ON para compilar tambien el .exe Standalone de cada plugin (pruebas)
option(MASTERPACK_STANDALONE "Compilar tambien las versiones Standalone" OFF)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.2
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(JUCE)

# -----------------------------------------------------------------------------
# Un proyecto, varios plugins: cada llamada crea un target con su propio .vst3
#   add_master_plugin(<carpeta/target> "<nombre en el DAW>" <PLUGIN_CODE> "<categorias VST3>")
# -----------------------------------------------------------------------------
function(add_master_plugin target product code categories)
    set(formats VST3)
    if(MASTERPACK_STANDALONE)
        list(APPEND formats Standalone)
    endif()

    juce_add_plugin(${target}
        COMPANY_NAME                "PerreoTrap"
        PLUGIN_MANUFACTURER_CODE    Mpck
        PLUGIN_CODE                 ${code}
        FORMATS                     ${formats}
        PRODUCT_NAME                "${product}"
        IS_SYNTH                    FALSE
        NEEDS_MIDI_INPUT            FALSE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD     FALSE
        VST3_CATEGORIES             ${categories}
    )

    target_sources(${target}
        PRIVATE
            Plugins/${target}/${target}.cpp
            Common/Dsp.h
            Common/MasterBase.h
            Common/MasterEditor.h
    )

    target_include_directories(${target} PRIVATE Common)

    target_compile_definitions(${target}
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
    )

    target_compile_options(${target}
        PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/MP>
            $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    )

    target_link_libraries(${target}
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )
endfunction()

# -----------------------------------------------------------------------------
# Los plugins del pack (el PLUGIN_CODE de cada uno debe ser unico, 4 letras,
# con al menos una mayuscula)
# -----------------------------------------------------------------------------
add_master_plugin(MasterEQ       "Master EQ"       Meqp "Fx;EQ")
add_master_plugin(MasterComp     "Master Comp"     Mcop "Fx;Dynamics")
add_master_plugin(MasterSat      "Master Sat"      Msap "Fx;Distortion")
add_master_plugin(MasterImager   "Master Imager"   Mimp "Fx;Spatial")
add_master_plugin(MasterLimiter  "Master Limiter"  Mlmp "Fx;Dynamics")
