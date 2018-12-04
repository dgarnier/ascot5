"""
Main module for reading ASCOT5 HDF5 files.
"""
import numpy as np
import h5py
import datetime

from . import ascot5group

from . import B_2D
from . import B_3D
from . import B_ST
from . import B_TC
from . import B_GS

from . import E_TC
from . import E_1D
from . import E_3D

from . import wall_2D
from . import wall_3D

from . import plasma_1D

from . import metadata

from . import markers

from . import options

from . import orbits
from . import dists
from . import states

def read_hdf5(fn, groups="all"):
    """
    Read all or specified data groups that are present in ASCOT5 HDF5 file.

    Parameters
    ----------

    fn : str
        Full path to the HDF5 file to be read.
    groups: str list, optional
        List of groups to be read. Default is all.

    Returns
    -------

    Dictionary filled with data. Structure is similar as
    the HDF5 file.
    """

    if groups == "all":
        groups = ["bfield", "efield", "options", "wall", "plasma",
                  "marker", "metadata", "states", "orbits", "dists",
                  "results"]

    with h5py.File(fn, "r") as f:
        # Read the requested input if present.
        out = {}

        out["options"] = {}
        if "options" in f and "options" in groups:
            qids,dates = get_qids(fn, "options")
            for qid in qids:
                out["options"]["opt-"+qid] = options.read_hdf5(fn,qid)

        out["bfield"] = {}
        if "bfield" in f and "bfield" in groups:
            qids,dates = get_qids(fn, "bfield")
            for qid in qids:
                if ("B_2DS-"+qid) in f["bfield"]:
                    out["bfield"]["B_2DS-"+qid] = B_2D.read_hdf5(fn,qid)
                if ("B_3DS-"+qid) in f["bfield"]:
                    out["bfield"]["B_3DS-"+qid] = B_3D.read_hdf5(fn,qid)
                if ("B_TC-"+qid) in f["bfield"]:
                    out["bfield"]["B_TC-"+qid]  = B_TC.read_hdf5(fn,qid)
                if ("B_GS-"+qid) in f["bfield"]:
                    out["bfield"]["B_GS-"+qid]  = B_GS.read_hdf5(fn,qid)
                if ("B_ST-"+qid) in f["bfield"]:
                    out["bfield"]["B_ST-"+qid]  = B_ST.read_hdf5(fn,qid)

        out["efield"] = {}
        if "efield" in f and "efield" in groups:
            qids,dates = get_qids(fn, "efield")
            for qid in qids:
                if ("E_1D-"+qid) in f["efield"]:
                    out["efield"]["E_1D-"+qid] = E_1D.read_hdf5(fn,qid)
                if ("E_TC-"+qid) in f["efield"]:
                    out["efield"]["E_TC-"+qid] = E_TC.read_hdf5(fn,qid)
                if ("E_3D-"+qid) in f["efield"]:
                    out["efield"]["E_3D-"+qid] = E_3D.read_hdf5(fn,qid)

        out["wall"] = {}
        if "wall" in f and "wall" in groups:
            qids,dates = get_qids(fn, "wall")
            for qid in qids:
                if ("wall_2D-"+qid) in f["wall"]:
                    out["wall"]["wall_2D-"+qid] = wall_2D.read_hdf5(fn,qid)
                if ("wall_3D-"+qid) in f["wall"]:
                    out["wall"]["wall_3D-"+qid] = wall_3D.read_hdf5(fn,qid)

        out["plasma"] = {}
        if "plasma" in f and "plasma" in groups:
            qids,dates = get_qids(fn, "plasma")
            for qid in qids:
                if ("plasma_1D-"+qid) in f["plasma"]:
                    out["plasma"]["plasma_1D-"+qid] = plasma_1D.read_hdf5(fn,qid)

        out["marker"] = {}
        if "marker" in f and "marker" in groups:
            qids,dates = get_qids(fn, "marker")
            for qid in qids:
                if ("particle-"+qid) in f["marker"]:
                    out["marker"]["particle-"+qid]       = markers.read_hdf5_particles(fn, qid)
                if ("guiding_center-"+qid) in f["marker"]:
                    out["marker"]["guiding_center-"+qid] = markers.read_hdf5_guidingcenters(fn, qid)
                if ("field_line-"+qid) in f["marker"]:
                    out["marker"]["field_line-"+qid]     = markers.read_hdf5_fieldlines(fn, qid)

        out["metadata"] = {}
        if "metadata" in f and "metadata" in groups:
            out["metadata"] = metadata.read_hdf5(fn)

        if "results" in f and "results" in groups:
            qids,dates = get_qids(fn, "results")
            for qid in qids:
                if not "results" in out:
                    out["results"] = {}

                path = "run-"+qid
                out["results"][path] = {}

                # Metadata.
                out["results"][path]["qid"]  = qid
                out["results"][path]["date"] = f["results"][path].attrs["date"]
                out["results"][path]["description"] = f["results"][path].attrs["description"]

                out["results"][path]["qid_bfield"]  = f["results"][path].attrs["qid_bfield"]
                out["results"][path]["qid_efield"]  = f["results"][path].attrs["qid_efield"]
                out["results"][path]["qid_marker"]  = f["results"][path].attrs["qid_marker"]
                out["results"][path]["qid_options"] = f["results"][path].attrs["qid_options"]
                out["results"][path]["qid_plasma"]  = f["results"][path].attrs["qid_plasma"]
                out["results"][path]["qid_wall"]    = f["results"][path].attrs["qid_wall"]

                # Actual data
                if "inistate" in f["results"][path] and "results" in groups:
                    st = states.read_hdf5(fn,qid,["inistate"])
                    out["results"][path]["inistate"] = st["inistate"]
                if "endstate" in f["results"][path] and "results" in groups:
                    st = states.read_hdf5(fn,qid,["endstate"])
                    out["results"][path]["endstate"] = st["endstate"]
                if "dists" in f["results"][path] and "results" in groups:
                    out["results"][path]["dists"] = dists.read_hdf5(fn,qid)
                if "orbits" in f["results"][path] and "results" in groups:
                    out["results"][path]["orbits"] = orbits.read_hdf5(fn,qid)

    return out


def write_hdf5(fn, a5):
    """
    Generate ASCOT5 HDF5 file.

    TODO implement

    Parameters
    ----------

    fn : str
        Full path to HDF5 file.
    a5 : dictionary
        ASCOT5 HDF5 file in dictionary format (as given by the
        read_hdf5 function).
    """

    with h5py.File(fn,"a") as f:
        pass


def get_qids(fn, group):
    """
    Get all qids that are present in a mastergroup.

    The qids are sorted with first one being the active one,
    and the rest are sorted by date from newest to oldest.

    Parameters
    ----------

    fn : str
        Full path to HDF5 file.
    group : str
        The group from which qids are extracted.
    Return
    ----------

    qids : str array
        Sorted qids as strings.
    dates : str array
        Dates associated with qids.
    """

    with h5py.File(fn,"r") as f:
        group = f[group]

        # Find all qids and note the date when they were created.
        qidsraw = []
        datesraw = []
        for grp in group:
            qidsraw.append(grp[-10:])
            datesraw.append(group[grp].attrs["date"])

        #print(datetime.datetime.strptime(datesraw[0].decode('utf-8'), "%Y-%m-%d %H:%M:%S."))
        qids  = [None]*len(qidsraw)
        dates = [None]*len(qidsraw)

        # If qids exist, we sort them.
        if len(qidsraw) > 0:
            # The first qid is supposed to be the one that is active.
            if group.attrs["active"].decode('utf-8') in qidsraw:
                qids[0] = group.attrs["active"].decode('utf-8')

                # Remove associated date.
                dates[0] = datesraw[qidsraw.index(qids[0])]
                datesraw.remove(dates[0])
                qidsraw.remove(qids[0])

            # Sort by date
            temp = [datetime.datetime.strptime(d.decode('utf-8')[:19], "%Y-%m-%d %H:%M:%S") for d in datesraw]
            idx = sorted(range(len(temp)), key=lambda k: temp[k])

            qids[1:] = [qidsraw[i] for i in reversed(idx)]
            dates[1:] = [datesraw[i] for i in reversed(idx)]

    return qids, dates

def set_active(fn, mastergroup, qid):
    """
    Set given qid active in the qiven mastergroup
    """
    with h5py.File(fn,"a") as f:
        ascot5group.setactiveqid(f, mastergroup, qid)

def get_description(fn, mastergroup, qid):
    """
    Get description of a group.

    Args:
        fn:          str Name of the HDF5 file
        mastergroup: str Master group where the requested group resides
        qid        : str QID of group whose description is requested

    Return:
        str Description of a group
    """
    with h5py.File(fn,"r") as f:
        for group in f[mastergroup]:
            groupqid = group[-10:]
            if groupqid == qid:
                return "%s" % f[mastergroup][group].attrs["description"].decode('utf-8')
