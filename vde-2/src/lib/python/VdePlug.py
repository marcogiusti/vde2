from vdeplug import VdeStream, VdePlug
import warnings


__all__ = ["VdeStream", "VdePlug"]

msg = """The module VdePlug is deprecated. Please use the module \
vdeplug. Change your code as following:

    try:
        from vdeplug import VdePlug, VdeStream
    except ImportError:
        from VdePlug import VdePlug, VdeStream
"""
warnings.warn(msg, DeprecationWarning)
