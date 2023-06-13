"""Python interface to libascot.so.
"""
import ctypes
import unyt
import numpy as np

from .libsimulate import LibSimulate
from .libascot    import LibAscot

from .libascot import _LIBASCOTFOUND
from a5py.exceptions import AscotInitException

class Ascotpy(LibAscot, LibSimulate):
    """Class with methods to initialize and access the data via Python.

    This class will be inherited by Ascot class. Here we hide all the dirty
    implementation of the libascot.so interface in private methods and
    attributes so that the Ascot front is clean for the users.

    MPI parallelism has not been tested.

    Attributes
    ----------
    _sim
        Simulation offload data struct.
    _offload_ready
        Flag indicating if inputs are packed.
    _nmrk
        Number of markers currently in the marker array.
    _markers
        Marker array for interactive simulations.
    _diag_occupied
        Flag indicating if diagnostics array contains data.
    _diag_offload_array
        Diagnostics data offload array.
    _offload_array
        Offload array containing the input data when this instance is packed.
    _int_offload_array
        Offload array for integers containing the input data when this instance
        is packed.
    _bfield_offload_array
        Offload data for the magnetic field input.
    _efield_offload_array
        Offload data for the electric field input.
    _plasma_offload_array
        Offload data for the plasma input.
    _neutral_offload_array
        Offload data for the neutral input.
    _wall_offload_array
        Offload data for the wall (float) input.
    _wall_int_offload_array
        Offload data for the wall (int) input.
    _boozer_offload_array
        Offload data for the Boozer input.
    _mhd_offload_array
        Offload data for the MHD input.
    _mpi_root
        Rank of the root MPI process.
    _mpi_rank
        Rank of this MPI process.
    _mpi_size
        Number of MPI processes.
    """

    DUMMY_QID = "".encode('UTF-8')
    """Flag to use in _sim.qid_* to indicate that the input is not initialized.
    """

    def __init__(self):
        """Initialize pointers to data that is being passed between Python and C
        via libascot.
        """
        if not _LIBASCOTFOUND:
            return

        # Initialize attributes
        self._offload_ready    = False
        self._nmrk             = ctypes.c_int32()
        self._diag_occupied    = False
        self._offload_data      = ascot2py.struct_c__SA_offload_package()
        self._offload_array     = ctypes.POINTER(ctypes.c_double)()
        self._int_offload_array = ctypes.POINTER(ctypes.c_int   )()
        self._markers = ctypes.POINTER(ascot2py.struct_c__SA_particle_state)()

        self._sim = ascot2py.struct_c__SA_sim_offload_data()
        self._bfield_offload_array  = ctypes.POINTER(ctypes.c_double)()
        self._efield_offload_array  = ctypes.POINTER(ctypes.c_double)()
        self._plasma_offload_array  = ctypes.POINTER(ctypes.c_double)()
        self._neutral_offload_array = ctypes.POINTER(ctypes.c_double)()
        self._wall_offload_array    = ctypes.POINTER(ctypes.c_double)()
        self._boozer_offload_array  = ctypes.POINTER(ctypes.c_double)()
        self._mhd_offload_array     = ctypes.POINTER(ctypes.c_double)()
        self._diag_offload_array    = ctypes.POINTER(ctypes.c_double)()

        self._wall_int_offload_array = ctypes.POINTER(ctypes.c_int)()

        # MPI init needs to be called (only) once so that we can run simulations
        argc       = ctypes.c_int32()
        argc.value = 0
        argv       = ctypes.POINTER(ctypes.c_char)()
        self._mpi_rank = ctypes.c_int32()
        self._mpi_root = ctypes.c_int32()
        self._mpi_size = ctypes.c_int32()
        ascot2py.mpi_interface_init(
            argc, ctypes.byref(argv), ctypes.byref(self._sim),
            ctypes.byref(self._mpi_rank), ctypes.byref(self._mpi_size),
            ctypes.byref(self._mpi_root))

    def _init(self, data, bfield=None, efield=None, plasma=None,
              wall=None, neutral=None, boozer=None, mhd=None,
              switch=False):
        """Read, offload, and initialize input data so it can be accessed
        by libascot.

        Assumes data is present and the provided QIDs are valid.

        Parameters
        ----------
        data : Ascot5IO
            Ascot5 data on disk which is used in initialization.
        bfield : str
            QID of the magnetic field to be initialized.
        efield : str
            QID of the electric field to be initialized.
        plasma : str
            QID of the plasma data to be initialized.
        wall : str
            QID of the wall data to be initialized.
        neutral : str
            QID of the neutral data to be initialized.
        boozer : str
            QID of the boozer to be initialized.
        mhd : str
            QID of the MHD data to be initialized.
        switch : bool
            If ``True``, free input that has been
        """
        if self._offload_ready:
            raise AscotInitException("This instance has been packed")

        # Iterate through all inputs and mark those that are initialized
        inputs2read = ctypes.c_int32()
        args = locals() # Contains function arguments and values in a dictionary
        for inp in ["bfield", "efield", "plasma", "wall", "neutral", "boozer",
                    "mhd"]:
            if args[inp] is None:
                # This input is not initialized
                continue

            # Convert QID strings to bytes
            args[inp] = args[inp].encode("UTF-8")

            currentqid = getattr(self._sim, "qid_" + inp)
            if args[inp] == currentqid:
                # This input is already initialized
                continue

            if currentqid != Ascotpy.DUMMY_QID:
                # Other input of same type is initialized. Raise error or free
                # the old input.
                if not switch:
                    raise AscotInitException(
                        "Cannot initialize " + inp
                        + ": other input is already initialized.\n"
                        "Either use \"switch=True\" or free(" + inp + "=True)")

                self._free(**{inp : True})

            # Mark this input to be read
            inputs2read.value |= getattr(ascot2py, "hdf5_input_" + inp)

        # Separate loop to mark QIDs of the inputs that are read. (We couldn't
        # do this in the previous loop as that might get interrupted by
        # an exception, in which case sim.qid_* would point to data which is not
        # initialized.
        for inp in ["bfield", "efield", "plasma", "wall", "neutral", "boozer",
                    "mhd"]:
            if inputs2read.value & getattr(ascot2py, "hdf5_input_" + inp):
                setattr(self._sim, "qid_" + inp, args[inp])

        ascot2py.hdf5_interface_read_input(
            ctypes.byref(self._sim),
            inputs2read,
            ctypes.byref(self._bfield_offload_array),
            ctypes.byref(self._efield_offload_array),
            ctypes.byref(self._plasma_offload_array),
            ctypes.byref(self._neutral_offload_array),
            ctypes.byref(self._wall_offload_array),
            ctypes.byref(self._wall_int_offload_array),
            ctypes.byref(self._boozer_offload_array),
            ctypes.byref(self._mhd_offload_array),
            None, # Marker array (ignore)
            None  # Number of markers that were read (ignore)
            )


    def _free(self, bfield=False, efield=False, plasma=False, wall=False,
              neutral=False, boozer=False, mhd=False):
        """Free input data initialized in C-side.
        """
        if self._offload_ready:
            raise AscotInitException("This instance has been packed")

        args = locals() # Contains function arguments and values in a dictionary

        # Iterate through all inputs and free the data if the corresponding
        # argument is True
        for inp in ["bfield", "efield", "plasma", "wall", "neutral", "boozer",
                    "mhd"]:
            if args[inp] and \
               getattr(self._sim, "qid_" + inp) != Ascotpy.DUMMY_QID:

                # Deallocate the data allocated in C side
                array = getattr(self, "_" + inp + "_offload_array")
                ascot2py.libascot_deallocate(array)
                if inp == "wall":
                    array = getattr(self, "_" + inp + "_int_offload_array")
                    ascot2py.libascot_deallocate(array)

                # Set the offload array length to zero
                if inp == "bfield":
                    self._sim.B_offload_data.offload_array_length = 0
                elif inp == "efield":
                    self._sim.E_offload_data.offload_array_length = 0
                else:
                    data = getattr(self._sim, inp + "_offload_data")
                    data.offload_array_length = 0
                    if inp == "wall":
                        data.int_offload_array_length = 0

                # Set QID to dummy value
                setattr(self._sim, "qid_" + inp, Ascotpy.DUMMY_QID)

    def _pack(self):
        """Pack offload arrays as one making this instance ready for simulation.

        Note that inputs cannot be changed or freed before calling _unpack. Make
        sure all required data is initialized before packing.
        """
        if self._offload_ready:
            raise AscotInitException("This instance is already packed")

        # This call internally frees individual offload arrays and initializes
        # the common ones.
        ascot2py.pack_offload_array(
            ctypes.byref(self._sim), ctypes.byref(self._offload_data),
            self._bfield_offload_array, self._efield_offload_array,
            self._plasma_offload_array, self._neutral_offload_array,
            self._wall_offload_array,   self._wall_int_offload_array,
            self._boozer_offload_array, self._mhd_offload_array,
            ctypes.byref(self._offload_array),
            ctypes.byref(self._int_offload_array))
        self._offload_ready = True

        # Set pointers to correct locations (based on the order arrays are
        # packed) so that we can continue using evaluation routines.
        def advance(prevptr, increment):
            ptr = ctypes.addressof(prevptr) + 2 * increment
            ptr = ctypes.cast(ctypes.c_void_p(ptr),
                              ctypes.POINTER(ctypes.c_double))
            return ptr

        self._bfield_offload_array = self._offload_array
        self._efield_offload_array = advance(
            self._bfield_offload_array.contents,
            self._sim.B_offload_data.offload_array_length)
        self._plasma_offload_array = advance(
            self._efield_offload_array.contents,
            self._sim.E_offload_data.offload_array_length)
        self._neutral_offload_array = advance(
            self._plasma_offload_array.contents,
            self._sim.plasma_offload_data.offload_array_length)
        self._wall_offload_array = advance(
            self._neutral_offload_array.contents,
            self._sim.neutral_offload_data.offload_array_length)
        self._boozer_offload_array = advance(
            self._wall_offload_array.contents,
            self._sim.wall_offload_data.offload_array_length)
        self._mhd_offload_array = advance(
            self._boozer_offload_array.contents,
            self._sim.boozer_offload_data.offload_array_length)

        self._wall_int_offload_array = self._int_offload_array


    def _unpack(self, bfield=True, efield=True, plasma=True, wall=True,
               neutral=True, boozer=True, mhd=True):
        """Unpack simulation arrays, i.e. free offload array and re-read data.

        After unpacking the inputs can be changed or freed again but simulations
        cannot be performed.
        """
        if not self._offload_ready:
            raise AscotInitException("This instance hasn't been packed")

        ascot2py.libascot_deallocate(self._offload_array)
        ascot2py.libascot_deallocate(self._int_offload_array)
        self._offload_data.offload_array_length     = 0
        self._offload_data.int_offload_array_length = 0
        self._offload_data.unpack_pos               = 0
        self._offload_data.int_unpack_pos           = 0

        inputs2read = {}
        def readornot(name, read, qid):
            if read:
                inputs2read[name] = qid.decode("utf-8")
            return Ascotpy.DUMMY_QID

        self._sim.qid_bfield  = readornot("bfield",  bfield,
                                          self._sim.qid_bfield)
        self._sim.qid_efield  = readornot("efield",  efield,
                                          self._sim.qid_efield)
        self._sim.qid_plasma  = readornot("plasma",  plasma,
                                          self._sim.qid_plasma)
        self._sim.qid_neutral = readornot("neutral", neutral,
                                          self._sim.qid_neutral)
        self._sim.qid_wall    = readornot("wall",    wall,
                                          self._sim.qid_wall)
        self._sim.qid_boozer  = readornot("boozer",  boozer,
                                          self._sim.qid_boozer)
        self._sim.qid_mhd     = readornot("mhd",     mhd,
                                          self._sim.qid_mhd)

        self._offload_ready = False
        self.input_init(**inputs2read)

    def input_initialized(self):
        """Get inputs that are currently initialized.

        Returns
        -------
        outputs : list [str]
            List of outputs currently initialized.
        """
        out = {}
        for inp in ["bfield", "efield", "plasma", "wall", "neutral", "boozer",
                    "mhd"]:
            qid = getattr(self._sim, "qid_" + inp)
            if qid != Ascotpy.DUMMY_QID:
                out[inp] = qid.decode("utf-8")
        return out

    def input_eval_list(self, show=True):
        """Return quantities that can be evaluated from inputs.

        The returned quantities are those that can be evaluated from the inputs
        with `input_eval` method. Units are not listed as that function returns
        quantities with units included.

        The number ion species in plasma input is not fixed so "ni" refers to
        i'th plasma species as listed by `input_getplasmaspecies` (index starts
        from 1).

        Parameters
        ----------
        show : bool, optional
            Print output.

        Returns
        -------
        quantities : dict [str, str]
            Name of each quantity and a short description.
        """

        out = {
            "rho":
            "Square root of normalized poloidal flux",
            "psi":
            "Poloidal flux",
            "br":
            "Magnetic field R component (not including MHD)",
            "bphi":
            "Magnetic field phi component (not including MHD)",
            "bz":
            "Magnetic field z component (not including MHD)",
            "bnorm":
            "Magnetic field norm (not including MHD)",
            "brdr":
            "d/dR of magnetic field R component (not including MHD)",
            "brdphi":
            "d/dphi of magnetic field R component (not including MHD)",
            "brdz":
            "d/dz of magnetic field R component (not including MHD)",
            "bphidr":
            "d/dR of magnetic field phi component (not including MHD)",
            "bphidphi":
            "d/dphi of magnetic field phi component (not including MHD)",
            "bphidz":
            "d/dz of magnetic field phi component (not including MHD)",
            "bzdr":
            "d/dR of magnetic field z component (not including MHD)",
            "bzdphi":
            "d/dphi of magnetic field z component (not including MHD)",
            "bzdz":
            "d/dz of magnetic field z component (not including MHD)",
            "divergence":
            "Magnetic field divergence (not including MHD)",
            "jr":
            "Current density R component",
            "jphi":
            "Current density phi component",
            "jz":
            "Current density z component",
            "jnorm":
            "Current density",
            "er":
            "Electric field R component (not including MHD)",
            "ephi":
            "Electric field phi component (not including MHD)",
            "ez":
            "Electric field z component (not including MHD)",
            "ne":
            "Electron density",
            "te":
            "Electron temperature",
            "n0":
            "Neutral density",
            "alpha":
            "Eigenfunction of the magnetic field MHD perturbation",
            "phi":
            "Eigenfunction of the electric field MHD perturbation",
            "mhd_br":
            "R component of B-field perturbation due to MHD",
            "mhd_bphi":
            "phi component of B-field perturbation due to MHD",
            "mhd_bz":
            "z component of B-field perturbation due to MHD",
            "mhd_er":
            "R component of E-field perturbation due to MHD",
            "mhd_ephi":
            "phi component of E-field perturbation due to MHD",
            "mhd_ez":
            "z component of E-field perturbation due to MHD",
            "mhd_phi":
            "Electric potential due to MHD",
            "db/b (mhd)":
            """|B_MHD| / |B| where B is evaluated from the magnetic
            field input and both contain all components""",
            "psi (bzr)":
            "Boozer poloidal flux",
            "rho (bzr)":
            "Boozer radial coordinate",
            "theta":
            "Boozer poloidal coordinate",
            "zeta":
            "Boozer toroidal coordinate",
            "dpsidr (bzr)":
            "d/dR of Boozer psi",
            "dpsidphi (bzr)":
            "d/dphi of Boozer psi",
            "dpsidz (bzr)":
            "d/dz of Boozer psi",
            "dthetadr":
            "d/dr of Boozer theta",
            "dthetadphi":
            "d/dphi of Boozer theta",
            "dthetadz":
            "d/dz of Boozer theta",
            "dzetadr":
            "d/dr of Boozer zeta",
            "dzetadphi":
            "d/dphi of Boozer zeta",
            "dzetadz":
            "d/dz of Boozer zeta",
            "qprof":
            "Local value of the q-profile (safety factor)",
            "bjac":
            "Determinant of the Jacobian in Boozer coordinate transformation",
            "bjacxb2":
            "bjac multiplied by B^2 which is constant along flux surfaces",
            "ne":
            "Electron density",
            "te":
            "Electron temperature",
            "nij, where j >= 1":
            "Ion species 'j' density",
            "tij, where j >= 1":
            "Ion species 'j' temperature",
        }
        if show:
            for name, desc in out.items():
                print(name.ljust(15) + " : " + desc)

        return out

    def input_eval(self, r, phi, z, t, qnt, grid=False):
        """Evaluate input quantities at given coordinates.

        This method uses ASCOT5 C-routines for evaluating inputs exactly as
        they are evaluated during a simulation. See ``input_eval_list`` for
        a complete list of available input and derived quantities.

        Parameters
        ----------
        r : array_like
            R coordinates where data is evaluated [m].
        phi : array_like
            phi coordinates where data is evaluated [rad].
        z : array_like
            z coordinates where data is evaluated [m].
        t : array_like
            Time coordinates where data is evaluated [s].
        qnt : str
            Name of the evaluated quantity.
        grid : boolean, optional
            Treat input coordinates as abscissae and return the evaluated
            quantities on a grid instead.

        Returns
        -------
        val : array_like (n,) or (nr,nphi,nz,nt)
            Quantity evaluated in each queried point.

            NaN is returned at those points where the evaluation failed.

            4D matrix is returned when grid=``True``.

        Raises
        ------
        AssertionError
            If required data has not been initialized.
        RuntimeError
            If evaluation in libascot.so failed.
        """
        R   = np.asarray(R).ravel()
        phi = np.asarray(phi).ravel()
        z   = np.asarray(z).ravel()
        t   = np.asarray(t).ravel()

        if grid:
            arrsize = (R.size, phi.size, z.size, t.size)
            R, phi, z, t = np.meshgrid(R, phi, z, t)
            R   = R.ravel()
            phi = phi.ravel()
            z   = z.ravel()
            t   = t.ravel()
        else:
            # Not a grid so check that dimensions are correct (and make
            # single-valued vectors correct size)
            arrsize = np.amax(np.array([R.size, phi.size, z.size, t.size]))
            errmsg = "Input arrays have inconsistent sizes ({}, {}, {}, {})"
            assert (R.size == 1 or R.size == arrsize) and  \
                (phi.size == 1 or phi.size == arrsize) and \
                (z.size == 1 or z.size == arrsize) and     \
                (t.size == 1 or t.size == arrsize),        \
                errmsg.format(R.size, phi.size, z.size, t.size)

            if R.size == 1:
                R = R[0]*np.ones((arrsize,))
            if phi.size == 1:
                phi = phi[0]*np.ones((arrsize,))
            if z.size == 1:
                z = z[0]*np.ones((arrsize,))
            if t.size == 1:
                t = t[0]*np.ones((arrsize,))

        out = None
        if quantity in ["rho", "psi"]:
            out = self._eval_bfield(R, phi, z, t, evalrho=True)[quantity]
        if quantity in ["br", "bphi", "bz", "brdr", "brdphi", "brdz", "bphidr",
                        "bphidphi", "bphidz", "bzdr", "bzdphi", "bzdz"]:
            out = self._eval_bfield(R, phi, z, t, evalb=True)[quantity]
        if quantity == "divergence":
            out = self._eval_bfield(R, phi, z, t, evalb=True)
            out = out["br"]/R + out["brdr"] + out["bphidphi"]/R + out["bzdz"]
        if quantity == "axis":
            out = self._eval_bfield(R, phi, z, t, evalaxis=True)
        if quantity == "bnorm":
            out = self._eval_bfield(R, phi, z, t, evalb=True)
            out = np.sqrt( out["br"]**2 + out["bphi"]**2 + out["bz"]**2 )
        if quantity in ["jnorm", "jr", "jphi", "jz"]:
            b  = self._eval_bfield(R, phi, z, t, evalb=True)
            out = {}
            out["jr"]    = b["bzdphi"]/R - b["bphidz"]
            out["jphi"]  = b["brdz"] - b["bzdr"]
            out["jz"]    = b["bphi"]/R + b["bphidr"] - b["brdphi"]/R
            out["jnorm"] = np.sqrt(out["jr"]**2 + out["jphi"]**2 + out["jz"]**2)
            for k in out.keys():
                out[k] /= unyt.mu_0
            out = out[quantity]
        if quantity in ["er", "ephi", "ez"]:
            out = self._eval_efield(R, phi, z, t)[quantity]
        if quantity in ["n0"]:
            out = self._eval_neutral(R, phi, z, t)[quantity]
        if quantity in ["psi (bzr)", "theta", "zeta", "dpsidr (bzr)",
                        "dpsidphi (bzr)", "dpsidz (bzr)", "dthetadr",
                        "dthetadphi", "dthetadz", "dzetadr", "dzetadphi",
                        "dzetadz", "rho (bzr)"]:
            quantity = quantity.split()[0]
            out = self._eval_boozer(R, phi, z, t)[quantity]
        if quantity in ["qprof", "bjac", "bjacxb2"]:
            out = self._eval_boozer(R, phi, z, t, evalfun=True)[quantity]
        if quantity in ["alpha", "phi"]:
            out = self._eval_mhd(R, phi, z, t, evalpot=True)[quantity]
        if quantity in ["mhd_br", "mhd_bphi", "mhd_bz", "mhd_er", "mhd_ephi",
                        "mhd_ez", "mhd_phi"]:
            out = self._eval_mhd(R, phi, z, t)[quantity]
        if quantity in ["db/b (mhd)"]:
            bpert = self._eval_mhd(R, phi, z, t)
            b = self._eval_bfield(R, phi, z, t, evalb=True)
            bpert = np.sqrt(  bpert["mhd_br"]**2
                            + bpert["mhd_bphi"]**2
                            + bpert["mhd_bz"]**2)
            b = np.sqrt(b["br"]**2 + b["bphi"]**2 + b["bz"]**2)
            out = bpert/b
        if quantity in ["ne", "te"]:
            out = self._eval_plasma(R, phi, z, t)[quantity]

        ni = ["ni" + str(i+1) for i in range(99)]
        ti = ["ti" + str(i+1) for i in range(99)]
        if quantity in ni or quantity in ti:
            nspec = self.input_getplasmaspecies()[0]
            if int(quantity[1:]) <= nspec:
                out = self._eval_plasma(R, phi, z, t)[quantity]

        if grid:
            out = np.reshape(out, arrsize)

        return out

    def get_plasmaquantities(self):
        """Return species present in plasma input.
        """

        spec = self.get_plasmaspecies()
        for i in range(1,spec["nspecies"]):
            quantities.append("ni" + str(i))
            quantities.append("ti" + str(i))

        return quantities

    def _evaluateripple(self, R, z, t, nphi):
        """TBD
        """
        nin = R.size
        phigrid = np.linspace(0, 2*np.pi, nphi+1)[:-1]
        R = np.meshgrid(R, phigrid, indexing="ij")[0]
        z = np.meshgrid(z, phigrid, indexing="ij")[0]
        t, phigrid = np.meshgrid(t, phigrid, indexing="ij")
        out = np.abs(self.eval_bfield(R.ravel(), phigrid.ravel(), z.ravel(),
                                      t.ravel(), evalb=True)["bphi"])
        out = out.reshape(nin, nphi)
        bmax = np.nanmax(out, axis=1)
        bmin = np.nanmin(out, axis=1)
        return (bmax - bmin) / (bmax + bmin)


    def _evaluateripplewell(self, R, z, t, nphi):
        """TBD
        """
        nin = R.size
        phigrid = np.linspace(0, 2*np.pi, nphi+1)[:-1]
        R = np.meshgrid(R, phigrid, indexing="ij")[0]
        z = np.meshgrid(z, phigrid, indexing="ij")[0]
        t, phigrid = np.meshgrid(t, phigrid, indexing="ij")
        out = self.eval_bfield(R.ravel(), phigrid.ravel(), z.ravel(),
                               t.ravel(), evalb=True)

        bnorm = np.sqrt( out["br"]*out["br"] + out["bphi"]*out["bphi"]
                         + out["bz"]*out["bz"] )
        bhat_r   = out["br"]   / bnorm
        bhat_phi = out["bphi"] / bnorm
        bhat_z   = out["bz"]   / bnorm

        bbar = (bhat_r * out["bphidr"] +  bhat_z * out["bphidz"]).reshape(nin, nphi)
        btil = (bhat_phi * out["bphidphi"] / R.ravel()).reshape(nin, nphi)

        return np.nanmean(np.abs(bbar), axis=1) \
            / np.nanmax(np.abs(btil), axis=1)

    def _plotripple(self, Rgrid, zgrid, time, nphi, axes=None):
        """TBD
        """
        R, z, t = np.meshgrid(Rgrid, zgrid, time, indexing="ij")
        rip = self.evaluateripple(R, z, t, nphi).reshape(Rgrid.size, zgrid.size)

        newfig = axes is None
        if newfig:
            plt.figure()
            axes = plt.gca()

        CS = axes.contour(Rgrid, zgrid, 100 * rip.transpose(),
                          [0.01, 0.05, 0.1, 0.5, 1, 5])
        axes.clabel(CS)

        axes.set_aspect("equal", adjustable="box")
        axes.set_xlim(Rgrid[0], Rgrid[-1])
        axes.set_ylim(zgrid[0], zgrid[-1])

        if newfig:
            plt.show(block=False)

    def _plotripplewell(self, Rgrid, zgrid, time, nphi, axes=None,
                       clevel=[-1,0,1], clabel=True, **kwargs):
        """TBD
        """
        R, z, t = np.meshgrid(Rgrid, zgrid, time, indexing="ij")
        rip = self.evaluateripplewell(R, z, t, nphi).reshape(Rgrid.size, zgrid.size)

        newfig = axes is None
        if newfig:
            plt.figure()
            axes = plt.gca()

        CS = axes.contour(Rgrid, zgrid, np.log10(rip.transpose()),
                          clevel, **kwargs)
        if clabel:
            axes.clabel(CS)

        axes.set_aspect("equal", adjustable="box")
        axes.set_xlim(Rgrid[0], Rgrid[-1])
        axes.set_ylim(zgrid[0], zgrid[-1])

        if newfig:
            plt.show(block=False)
