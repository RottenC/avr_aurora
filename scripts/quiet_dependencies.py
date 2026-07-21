Import("env")

env.Append(CCFLAGS=["-Wall", "-Wextra", "-Wpedantic"])
env.Append(CXXFLAGS=["-Wall", "-Wextra", "-Wpedantic"])
env.Append(CPPDEFINES=[("FASTLED_INTERNAL", 1)])
