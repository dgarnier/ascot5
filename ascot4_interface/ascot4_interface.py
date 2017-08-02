import h5py
import os.path
from ascot4_particles import *
from ascot4_magn_bkg import *
from ascot4_plasma import *
from ascot4_erad import *
from ascot4_wall_2d import *
from ascot4_wall_3d import *

import sys
sys.path.append('../ui')
import ui_E_TC

def main():
    overwrite_fields = False
    h5file = 'ascot.h5'
    f = h5py.File(h5file, 'a') # Open for reading or writing    

    # Particle input
    if overwrite_fields or (not 'markers' in f):
        fname = 'input.particles'
        if (os.path.isfile(fname)):
            data = read_particles(fname)
            if 'markers' in f:
                del f['markers']
            #f.close()
            write_particles(h5file, data)

    #TODO inputs for all kinds of different ASCOT4 magn_bkg
    # at least 2D and 3D
    fname = 'input.magn_bkg'
    if (os.path.isfile(fname)):
        data = read_magn_bkg(fname)
        write_magn_bkg(f, data)

    # Plasma profile
    if overwrite_fields or (not 'plasma' in f):
        fname = 'input.plasma_1d'
        if (os.path.isfile(fname)):
            data = read_plasma(fname)
            if 'plasma' in f:
                del f['plasma']
            f.close()
            write_plasma_1d(h5file, data)

    # Radial electric field
    if overwrite_fields or (not 'efield/erad' in f):
        fname = 'input.erad'
        if (os.path.isfile(fname)):
            data = read_erad(fname)
            if 'efield/erad' in f:
                del f['efield/erad']
            write_erad(f, data)
            f['efield'].attrs['type'] = 'erad'
        else:
            E = np.array([0.0, 0, 0])
            ui_E_TC.write_hdf5(h5file, E) 
            
    # 2D wall
    if overwrite_fields or (not 'wall/2D' in f):
        fname = 'input.wall_2d'
        if (os.path.isfile(fname)):
            data = read_wall_2d(fname)
            if 'wall/2D' in f:
                del f['wall/2D']
            f = h5py.File(h5file, 'a')
            write_wall_2d(f, data)
            f['wall'].attrs['type'] = np.string_("2D")

    # 3D wall
    if overwrite_fields or (not 'wall/3D' in f):
        fname = 'input.wall_3d'
        if (os.path.isfile(fname)):
            data = read_wall_3d(fname)
            if 'wall/3D' in f:
                del f['wall/3D']
            write_wall_3d(f, data)
            f['wall'].attrs['type'] = '3D'
            
if __name__ == '__main__':
    main()
