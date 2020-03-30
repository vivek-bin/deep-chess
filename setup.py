from distutils.core import setup, Extension
import numpy

search = Extension("csearch", sources=["deepchess/cengine.c", "deepchess/csearch.c"], include_dirs=[numpy.get_include()])
setup(name="csearch", version="1.0", description="Monte carlo tree search", ext_modules=[search])

engine = Extension("cengine", sources=["deepchess/cengine.c"])
setup(name="cengine", version="1.0", description="C Chess engine", ext_modules=[engine])
