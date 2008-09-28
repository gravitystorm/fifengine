import fife
from serializers import *

class XMLAnimationLoader(fife.ResourceLoader):
	def __init__(self, imagepool, vfs):
		fife.ResourceLoader.__init__(self)
		self.imagepool = imagepool
		self.vfs = vfs
		self.thisown = 0
		self.filename = ''
		self.node = None

	def loadResource(self, location):
		self.filename = location.getFilename()
		return self.do_load_resource()

	def do_load_resource(self):
		f = self.vfs.open(self.filename)
		f.thisown = 1
		tree = ET.parse(f)
		self.node = tree.getroot()

		animation = fife.Animation()
		common_frame_delay = int(self.node.get('delay', 0))
		x_offset = int(self.node.get('x_offset', 0))
		y_offset = int(self.node.get('y_offset', 0))
		animation.setActionFrame(int(self.node.get('action', 0)))

		frames = self.node.findall('frame')
		if not frames:
			raise InvalidFormat('animation without <frame>s')

		for frame in frames:
			source = frame.get('source')
			if not source:
				raise InvalidFormat('animation without <frame>s')

			frame_x_offset = int(frame.get('x_offset', x_offset))
			frame_y_offset = int(frame.get('y_offset', y_offset))
			frame_delay = int(frame.get('delay', common_frame_delay))

			# xml paths are relative to the directory of the file they're used in.
			path = self.filename.split('/')
			path.pop()
			path.append(str(source))

			image = self.imagepool.getImage(self.imagepool.getIndex('/'.join(path)))
			image.setXShift(frame_x_offset)
			image.setYShift(frame_y_offset)
			animation.addFrame(image, frame_delay);

		animation.thisown = 0
		return animation

