from distutils.core import setup, Extension

module1 = Extension("cengine", sources=["deepchess/cengine.c"])
setup(name="cengine", version="1.0", description="C Chess engine", ext_modules=[module1])
