add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", { outputdir = "build" })

-- Global settings for all targets
add_requires("libsndfile", "fftw")
add_files("audio.c", "encoder.c", "decoder.c", "file.c", "fft.c")
add_packages("libsndfile", "fftw")

-- Target definitions
target("sandbox")
add_files("sandbox.c")

target("dtmf_encdec")
add_files("main.c")
