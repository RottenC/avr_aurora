Import("env")

# Keep project diagnostics enabled, but silence warnings emitted while
# compiling dependency implementation files.
for lib_builder in env.GetLibBuilders():
    if lib_builder.name.lower() == "fastled":
        lib_builder.env.AppendUnique(CCFLAGS=["-w"])
