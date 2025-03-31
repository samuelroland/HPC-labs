add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", { outputdir = "build" })

-- Global settings for all targets
add_requires("libsndfile", "fftw")
add_files("audio.c", "encoder.c", "decoder.c", "file.c", "fft.c")
add_packages("libsndfile", "fftw")
set_policy("check.target_package_licenses", false)

-- Necessary for likwid
add_links("likwid")
add_defines("LIKWID_PERFMON")

set_warnings("all")
set_optimize("none") -- -O0
add_cflags("-g", "-fno-inline")

-- Target definitions
target("sandbox")
add_files("sandbox.c")

target("dtmf_encdec-buffers")
add_defines("DECODER_VARIANT=1")
add_files("main.c")

target("dtmf_encdec-fft")
add_defines("DECODER_VARIANT=2")
add_files("main.c")
