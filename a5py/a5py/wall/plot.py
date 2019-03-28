"""
Routines for plotting walls.

File: wall/plot.py
"""
import numpy as np

import importlib.util as util

import time

plt = util.find_spec("matplotlib")
if plt:
        import matplotlib.pyplot as plt

import a5py.postprocessing.mathlib as mathlib

def plot_segments(x, y, axes=None):
    """
    """
    newfig = axes is None
    if newfig:
        plt.figure()
        axes = plt.gca()

    x = np.append(x, x[0])
    y = np.append(y, y[0])
    axes.plot(x, y, color="black", linewidth=2)
    axes.axis("scaled")

    if newfig:
        plt.show(block=False)

def plot_projection(x1x2x3, y1y2y3, z1z2z3, axes=None):
    """
    """
    newfig = axes is None
    if newfig:
        plt.figure()
        axes = plt.gca()

    r1r2r3 = np.sqrt(x1x2x3 * x1x2x3 + y1y2y3 * y1y2y3)

    r1r2r3 = r1r2r3.ravel()
    z1z2z3 = z1z2z3.ravel()
    axes.plot(r1r2r3, z1z2z3, marker=".", markeredgecolor="black",
              linestyle="None")
    axes.axis("scaled")

    if newfig:
        plt.show(block=False)

def plot_intersection(x1x2x3, y1y2y3, z1z2z3, phi, axes=None):
    """
    """
    newfig = axes is None
    if newfig:
        plt.figure()
        axes = plt.gca()

    # Polar coordinates, p1p2p3 in range [-pi, pi]
    r1r2r3, p1p2p3 = mathlib.cart2pol(x1x2x3, y1y2y3)
    # Wrap phi to range [-pi, pi)
    phi = np.mod(phi + np.pi, 2*np.pi) - np.pi
    # Set p1p2p3 == phi to zero
    p1p2p3 = p1p2p3 - phi
    ind_crossing = np.amax(p1p2p3, 1) - np.amin(p1p2p3, 1) < np.pi
    # Intersection happens if (any(p1p2p3) < phi and any(p1p2p3) >= phi)
    p1p2p3_sign = np.copysign(1, p1p2p3)
    p1p2p3_polarity = np.sum(p1p2p3_sign, 1)
    ind_int = np.logical_and(np.abs(p1p2p3_polarity) < 3, ind_crossing)
    # Remove unnecessary data
    x1x2x3          = x1x2x3[ind_int, :]
    y1y2y3          = y1y2y3[ind_int, :]
    z1z2z3          = z1z2z3[ind_int, :]
    r1r2r3          = r1r2r3[ind_int, :]
    p1p2p3          = p1p2p3[ind_int, :]
    p1p2p3_polarity = p1p2p3_polarity[ind_int]
    p1p2p3_sign     = p1p2p3_sign[ind_int, :]
    n = x1x2x3.shape[0]
    # Find intersection line for all crossing triangles
    p1p2p3_polarity = np.tile(p1p2p3_polarity, (3,1)).T

    phi_diff = p1p2p3[p1p2p3_sign != p1p2p3_polarity]
    phi_same = np.reshape(p1p2p3[p1p2p3_sign == p1p2p3_polarity], (n,2))
    phi_diffs = (- phi_same / (phi_diff[:,None] - phi_same))

    r_diff = r1r2r3[p1p2p3_sign != p1p2p3_polarity]
    r_same = np.reshape(r1r2r3[p1p2p3_sign == p1p2p3_polarity], (n,2))
    r = r_same + (r_diff[:,None] - r_same ) * phi_diffs

    z_diff = z1z2z3[p1p2p3_sign != p1p2p3_polarity]
    z_same = np.reshape(z1z2z3[p1p2p3_sign == p1p2p3_polarity], (n,2))
    z = z_same + (z_diff[:,None] - z_same ) * phi_diffs

    axes.plot(r.T, z.T, 'k')
    axes.axis("scaled")

    if newfig:
        plt.show(block=False)
