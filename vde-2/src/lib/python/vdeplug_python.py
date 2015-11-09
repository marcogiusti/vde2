from vdeplug import open, ctlfd, datafd, close
import warnings


__all__ = ["open", "ctlfd", "datafd", "close"]

msg = """The module vdeplug_python is deprecated. Please use the module \
vdeplug. Change your code as following:

    try:
        import vdeplug as vp
    except ImportError:
        import vdeplug_python as vp
"""
warnings.warn(msg, DeprecationWarning)
