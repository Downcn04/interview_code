import ctypes
from ctypes import wintypes
import tkinter as tk

user32 = ctypes.windll.user32
WDA_EXCLUDEFROMCAPTURE = 0x11
SetWindowDisplayAffinity = user32.SetWindowDisplayAffinity
SetWindowDisplayAffinity.restype = wintypes.BOOL
SetWindowDisplayAffinity.argtypes = [wintypes.HWND, wintypes.DWORD]
GetAncestor = user32.GetAncestor
GetAncestor.restype = wintypes.HWND
GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
GA_ROOT = 2


def make_unshareable(root: tk.Tk) -> None:
    hwnd = GetAncestor(root.winfo_id(), GA_ROOT)
    SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)