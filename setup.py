from distutils.core import setup, Extension

engine = Extension("cengine", sources=["deepchess/cengine.c"])
setup(name="cengine", version="1.0", description="C Chess engine", ext_modules=[engine])

search = Extension("csearch", sources=["deepchess/csearch.c"])
setup(name="csearch", version="1.0", description="Monte carlo tree search", ext_modules=[search])
