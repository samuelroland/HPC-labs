add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "build"})

add_requires("libsndfile", "fftw")


add_files("audio.c", "encoder.c", "decoder.c", "file.c", "fft.c")

target("sandbox")
    add_files("sandbox.c")
    add_packages("libsndfile", "fftw")

target("dtmf_encdec")
    add_files("main.c")
    add_packages("libsndfile", "fftw")