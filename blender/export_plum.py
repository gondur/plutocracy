#!BPY

"""
Name: 'Plutocracy Model (.plum)...'
Blender: 245
Group: 'Export'
Tooltip: 'Save a Plutocracy Model File'
"""

__author__ = "Michael Levin, Campbell Barton, Jiri Hnidek"
__url__ = ['www.blender.org', 'blenderartists.org']
__version__ = "1.0"

__bpydoc__ = """\
This script is an exporter to Plutocracy Model file format.
(Based on the OBJ exporter by Campbell Barton and Jiri Hnidek.)

Usage:

Select the objects you wish to export and run this script from "File->Export" menu.
Selecting the default options from the popup box will be good in most cases.
All objects that can be represented as a mesh (mesh, curve, metaball, surface, text3d)
will be exported as mesh data.
"""


# --------------------------------------------------------------------------
# Plutocracy Model Exporter by Michael "risujin" Levin
# based on OBJ Export v1.1 by Campbell Barton (AKA Ideasman)
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------


import Blender
from Blender import Mesh, Scene, Window, sys, Image, Draw
import BPyMesh
import BPyObject
import BPySys
import BPyMessages


def vec_eq(a, b):
	for i in range(0, len(a)):
		if a[i] != b[i]:
			return False
	return True


def sanitize(s):
	return s.replace('"', '\\"')


# Materials can have ".001" etc appended to their names when the name is
# already taken, this should be stripped
def sanitize_strip(s):
	s = s.rstrip(".0123456789");
	return sanitize(s)


def write_frame(file, objects):
	scn = Scene.GetCurrent()

	# Get all meshes
	for ob_main in objects:
		for ob, ob_mat in BPyObject.getDerivedObjects(ob_main):
			me = BPyMesh.getMeshFromObject(ob, None, True, False, scn)

			# Make sure there is something to write
			if not me or not me.faces or not (len(me.faces) + len(me.verts)):
				continue

			# High quality normals
			BPyMesh.meshCalcNormals(me)

			# Write object and material shader name
			file.write('\no "%s"\n' % sanitize(ob.name))
			if ob.getData(1):
				file.write('m "%s"\n' % sanitize_strip(ob.getData(1)))

			# Get the material name
			#materials = ob.getMaterials()
			#if materials:
			#	file.write('m "%s"\n' % sanitize_strip(materials[0].getName()))

			verts = [ ]
			for f in me.faces:
				inds = [ 0, 0, 0 ]
				reused = False
				for j in range(0, 3):
					vec = [ f.v[j].co.x, f.v[j].co.y, f.v[j].co.z, \
					        f.no.x, f.no.y, f.no.z, f.uv[j].x, f.uv[j].y ]
					if f.smooth:
						vec[4] = f.v[j].no[0]
						vec[5] = f.v[j].no[1]
						vec[6] = f.v[j].no[2]

					# See if this vertex line has already been written
					for i in range(0, len(verts)):
						if vec_eq(verts[i], vec):
							inds[j] = i
							reused = True
							break
					else:
						inds[j] = len(verts)
						verts.append(vec)
						file.write('v %g %g %g %g %g %g %g %g\n' % tuple(vec))
				if reused:
					file.write('i %d %d %d\n' % tuple(inds))

			# Cleanup (?)
			me.verts = None


def write(filename):
	if not filename.lower().endswith('.plum'):
		filename += '.plum'
	if not BPyMessages.Warning_SaveOver(filename):
		return

	# TODO: When overwriting a file, preserve any "anim" entries

	Window.EditMode(0)
	Window.WaitCursor(1)

	# Export an animation up to and including the current frame
	scn = orig_scene = Scene.GetCurrent()
	orig_frame = Blender.Get('curframe')
	scene_frames = range(1, orig_frame + 1)

	file = open(filename, "w")
	file.write(('########################################' + \
		        '########################################\n' + \
	            '# Blender3D v%s PLUM file: %s\n' + \
	            '# www.blender3d.org\n' + \
	            '########################################' + \
		        '########################################\n\n') % \
		       (Blender.Get('version'), \
	           Blender.Get('filename').split('/')[-1].split('\\')[-1] ))

	# Animation definitions
	file.write('# anim [name] [start] [end] [fps]\n' + \
	           'anim "idle" 1 %d 30\n' % orig_frame)

	# Write all frames into one file
	for frame in scene_frames:
		file.write(('\n########################################' + \
		            '########################################\n' + \
		            'frame %d\n' + \
		            '########################################' + \
		            '########################################\n') % frame);
		Blender.Set('curframe', frame)
		write_frame(file, scn.objects)
	file.close()

	Blender.Set('curframe', orig_frame)

if __name__ == '__main__':
	Window.FileSelector(write, 'Export Plutocracy PLUM', \
	                    sys.makename(ext='.plum'))

