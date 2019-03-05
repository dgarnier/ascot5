 co"""
MHD input IO.

File: mhd.py
"""
import h5py
import numpy as np

from . ascot5file import add_group
from . ascot5data import AscotData

def write_hdf5(fn, nmodes, mmodes, amplitude, omega, alpha, phi,
               psimin, psimax, tmin=None, tmax=None, desc=None):
    """
    Write MHD input to HDF5 file.

    Input is assumed to consist of n_modes (this number is deduced from input
    dimensions) of different modes. alpha and phi can be time-dependent in which
    case they should have additional dimension representing time. Ascot assumes
    these have time-dependency, so if none is given then the values are
    dublicated on a time grid before writing them into a file.

    Number of psi grid points (npsi) is deduced from the inputs.

    Args:
        fn : str <br>
            Full path to the HDF5 file.
        nmodes : array_like <br>
            Mode n (toroidal) numbers on an 1D array of length n_modes
        mmodes : array_like <br>
            Mode m (poloidal) numbers on an 1D array of length n_modes
        amplitude : array_like <br>
            Mode amplitudies on an 1D array of length n_modes
        omega : array_like <br>
            Mode frequencies on an 1D array of length n_modes
        alpha : array_like <br>
            Mode alpha on a 2D array of shape (n_modes, npsi) or 3D array with
            (n_modes, npsi, ntime).
        phi : array_like <br>
            Mode phi on a 2D array of shape (n_modes, npsi) or 3D array with
            (n_modes, npsi, ntime).
        psimin : float <br>
            Minimum value in psi grid.
        psimax : float <br>
            Maximum value in psi grid.
        tmin : float, optional <br>
            Minimum value in time grid. Must be given for non-stationary input.
        tmax : float, optional <br>
            Maximum value in time grid. Must be given for non-stationary input.
        desc : str, optional <br>
            Input's description.
    """

    assert alpha.ndim == 2 or alpha.ndim == 3), "Input data has incorrect shape"
    nmodes = alpha.shape[0]
    npsi   = alpha.shape[1]

    assert (nmodes.size    == n_modes and mmodes.size == n_modes and
            amplitude.size == n_modes and omega.size  == n_modes and
            phi.shape == alpha.shape), "Input data has incorrect shape"

    if alpha.ndim == 2:
        if tmin is None:
            tmin = 0
        if tmax is None:
            tmax = 100

        alpha = np.repeat(alpha[:, :, np.newaxis], 4, axis=2)
        phi   = np.repeat(phi[:, :, np.newaxis], 4, axis=2)
    else:
        assert(tmin is not None and tmax is not None)

    ntime = alpha.shape[2]

    parent = "mhd"
    group  = "MHD"

    with h5py.File(fn, "a") as f:
        g = add_group(f, parent, group, desc=desc)

        g.create_dataset("nmode",  (1,), data=nmode,  dtype="i8")
        g.create_dataset("npsi",   (1,), data=npsi,   dtype="i8")
        g.create_dataset("psimin", (1,), data=psimin, dtype="f8")
        g.create_dataset("psimax", (1,), data=psimax, dtype="f8")
        g.create_dataset("ntime",  (1,), data=ntime,  dtype="i8")
        g.create_dataset("tmin",   (1,), data=tmin,   dtype="f8")
        g.create_dataset("tmax",   (1,), data=tmax,   dtype="f8")

        g.create_dataset("nmodes", (nmode,), data=nmodes, dtype="i8")
        g.create_dataset("mmodes", (nmode,), data=mmodes, dtype="i8")

        g.create_dataset("amplitude", (nmode,), data=amplitude, dtype="f8")
        g.create_dataset("omega",     (nmode,), data=omega,     dtype="f8")

        g.create_dataset("alpha", (nmode,ntime,npsi), data=alpha, dtype="f8")
        g.create_dataset("phi",   (nmode,ntime,npsi), data=phi, dtype="f8")

    return g.name


def write_hdf5_dummy(fn, desc="Dummy"):
    """
    Write dummy MHD input.

    Args:
        fn : str <br>
            Full path to the HDF5 file.
    """
    n_modes = 2
    npsi    = 6

    nmodes    = np.array([1, 2])
    mmodes    = np.array([3, 4])
    amplitude = np.array([0.1, 2])
    omega     = np.array([1, 1.5])
    alpha     = np.ones((n_modes,npsi))
    phi       = np.ones((n_modes,npsi))
    psimin    = 0
    psimax    = 1
    write_hdf5(fn, nmodes, mmodes, amplitude, omega, alpha, phi, psimin, psimax,
               desc=desc)


def read_hdf5(fn, qid):
    """
    Read MHD input from HDF5 file.

    Args:
        fn : str <br>
            Full path to the HDF5 file.
        qid : str <br>
            QID of the data to be read.

    Returns:
        Dictionary containing input data.
    """

    path = "mhd/MHD_" + qid

    out = {}
    with h5py.File(fn,"r") as f:
        for key in f[path]:
            out[key] = f[path][key][:]

    return out


class MHD(AscotData):
    """
    Object representing MHD data.
    """

    def read(self):
        return read_hdf5(self._file, self.get_qid())
