"""Compatibility imports for the Qt timeline widget layer.

Core code should import TimelineHistory and TimelineSample from timeline_model
so importing core modules never loads PySide6. Importing this module is a GUI
operation and may load Qt through timeline_widget.
"""
from .timeline_model import TimelineHistory, TimelineSample
from .timeline_widget import TimelineWidget

__all__ = ["TimelineHistory", "TimelineSample", "TimelineWidget"]
